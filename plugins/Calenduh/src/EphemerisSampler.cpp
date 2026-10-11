/*
 * Calenduh plug-in for Stellarium
 *
 * Copyright (C) 2026 Jeremy Taylor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "EphemerisSampler.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "SolarSystem.hpp"
#include "Planet.hpp"
#include "precession.h"
#include "sidereal_time.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace calenduh {

namespace {

constexpr double MINUTE = 1. / 1440.;
constexpr double SUN_STEP = 10. * MINUTE;    //!< Sun sampling, culmination to culmination
constexpr double SAMPLE_STEP = 5. * MINUTE;  //!< Moon and target sampling (architecture 5.4, 5.7)
constexpr double SIDEREAL_DEGREES_PER_DAY = 360.98564736629;
constexpr double EARTH_EQUATORIAL_RADIUS_KM = 6378.137;
constexpr double EARTH_FLATTENING = 1. / 298.257223563;
constexpr double ASTRONOMICAL_UNIT_KM = 149597870.7;
constexpr int MAX_NIGHTS = 400;

//! Apparent minus geometric altitude [degrees] for standard conditions (10 C, 1010 hPa).
//! Same formula and iteration as Skyfield's altaz('standard'), so that fixtures compare directly.
double refracted(double geometricDeg)
{
	auto refraction = [](double altDeg) {
		if (altDeg < -1. || altDeg > 89.9)
			return 0.;
		const double r = 0.016667 / std::tan(qDegreesToRadians(altDeg + 7.31 / (altDeg + 4.4)));
		return r * (0.28 * 1010. / (10. + 273.));
	};
	double alt = geometricDeg;
	for (int i = 0; i < 20; ++i)
	{
		const double next = geometricDeg + refraction(alt);
		if (std::fabs(next - alt) <= 3.e-5)
			return next;
		alt = next;
	}
	return alt;
}

double wrap180(double deg)
{
	deg = std::fmod(deg + 180., 360.);
	if (deg < 0.)
		deg += 360.;
	return deg - 180.;
}

//! Earth precession (Vondrák 2011) and nutation (IAU 2000B) from J2000 to the
//! equator and equinox of date, built as Planet::computeTransMatrix() does for Earth.
//! Independent of the Stellarium observer.
Mat4d earthJ2000ToDate(double jde)
{
	double epsA, chiA, omegaA, psiA, deltaPsi, deltaEps;
	getPrecessionAnglesVondrak(jde, &epsA, &chiA, &omegaA, &psiA);
	getNutationAngles(jde, &deltaPsi, &deltaEps);
	const Mat4d nutation = Mat4d::xrotation(epsA) * Mat4d::zrotation(-deltaPsi) * Mat4d::xrotation(-epsA - deltaEps);
	const Mat4d dateToVsop87 = Mat4d::zrotation(-psiA) * Mat4d::xrotation(-omegaA) * Mat4d::zrotation(chiA) * nutation;
	return (StelCore::matVsop87ToJ2000 * dateToVsop87).transpose();
}

} // namespace

EphemerisSampler::EphemerisSampler(const ChartSettings& settings)
	: settings(settings)
	, timeZone(settings.location.timeZone.toUtf8())
{
}

bool EphemerisSampler::prepare(QString* error)
{
	auto fail = [error](const QString& message) {
		if (error)
			*error = message;
		return false;
	};
	if (!timeZone.isValid())
		return fail(QString("Unknown time zone: %1").arg(settings.location.timeZone));
	const qint64 nights = settings.firstEvening.daysTo(settings.lastEvening) + 1;
	if (!settings.firstEvening.isValid() || !settings.lastEvening.isValid() || nights < 1 || nights > MAX_NIGHTS)
		return fail(QString("The date range must have 1 to %1 nights.").arg(MAX_NIGHTS));

	model_ = ChartModel();
	model_.location = settings.location;
	unresolved.clear();
	for (QDate d = settings.firstEvening; d <= settings.lastEvening; d = d.addDays(1))
	{
		NightRow row;
		row.eveningDate = d;
		model_.rows.append(row);
	}

	// Observer: geodetic to geocentric (WGS84), in AU.
	latitude = qDegreesToRadians(settings.location.latitude);
	const double u = std::atan((1. - EARTH_FLATTENING) * std::tan(latitude));
	const double h = settings.location.elevation / 1000. / EARTH_EQUATORIAL_RADIUS_KM;
	const double rhoSin = (1. - EARTH_FLATTENING) * std::sin(u) + h * std::sin(latitude);
	const double rhoCos = std::cos(u) + h * std::cos(latitude);
	const double scale = EARTH_EQUATORIAL_RADIUS_KM / ASTRONOMICAL_UNIT_KM;
	observerShape.set(rhoCos * scale, 0., rhoSin * scale);

	if (!resolveBodies(error))
		return false;

	for (NightRow& row : model_.rows)
	{
		setNight(row.eveningDate);
		computeSunEvents(row);
	}
	chooseWindow();
	return true;
}

bool EphemerisSampler::resolveBodies(QString* error)
{
	StelCore* core = StelApp::getInstance().getCore();
	SolarSystem* solarSystem = GETSTELMODULE(SolarSystem);
	StelObjectMgr* objectMgr = GETSTELMODULE(StelObjectMgr);
	if (!core || !solarSystem || !objectMgr)
	{
		if (error)
			*error = QStringLiteral("Stellarium modules are not available.");
		return false;
	}
	earth = solarSystem->getEarth();
	moon = solarSystem->getMoon();
	sunBody.name = QStringLiteral("Sun");
	sunBody.planet = solarSystem->getSun();
	moonBody.name = QStringLiteral("Moon");
	moonBody.planet = moon;

	const QDate middle = settings.firstEvening.addDays(settings.firstEvening.daysTo(settings.lastEvening) / 2);
	deltaTDays = core->computeDeltaT(localClockToJd(middle.addDays(1), 0, timeZone)) / 86400.;

	// Catalogue positions: J2000 directions of stars and deep-sky objects
	// (proper motion as Stellarium applies it for its current date; below 1"/year
	// for the default stars). Solar System bodies are computed for each instant.
	auto resolve = [&](const QStringList& names, QVector<Body>& bodies) {
		bodies.clear();
		for (const QString& name : names)
		{
			const StelObjectP obj = objectMgr->searchByName(name);
			if (!obj)
			{
				unresolved.append(name);
				continue;
			}
			Body body;
			body.name = name;
			body.planet = qSharedPointerDynamicCast<Planet>(obj);
			if (!body.planet)
			{
				body.j2000Direction = obj->getJ2000EquatorialPos(core);
				body.j2000Direction.normalize();
			}
			bodies.append(body);
		}
	};
	resolve(settings.stars, stars);
	if (settings.panel.enabled)
		resolve(settings.panel.targets, targets);

	for (const Body& s : std::as_const(stars))
		model_.stars.append(s.name);
	if (settings.panel.enabled)
	{
		TargetPanel panel;
		panel.title = settings.panel.title;
		panel.summary = settings.panel.summary();
		for (const Body& t : std::as_const(targets))
			panel.targets.append(t.name);
		model_.panel = panel;
	}
	return true;
}

void EphemerisSampler::setNight(const QDate& evening)
{
	// Precession and nutation change by less than 0.1" within a night.
	j2000ToDate = earthJ2000ToDate(localClockToJd(evening.addDays(1), 0, timeZone) + deltaTDays);
}

double EphemerisSampler::localSiderealRadians(double jd) const
{
	return qDegreesToRadians(get_apparent_sidereal_time(jd, jd + deltaTDays) + settings.location.longitude);
}

Vec3d EphemerisSampler::heliocentric(const QSharedPointer<Planet>& p, double jde) const
{
	Vec3d sum(0., 0., 0.);
	for (QSharedPointer<Planet> q = p; q && q->getParent(); q = q->getParent())
	{
		Vec3d pos, vel;
		q->computePosition(jde, pos, vel);
		sum += pos;
	}
	return sum;
}

Vec3d EphemerisSampler::geocentricEquatorial(const Body& body, double jd) const
{
	if (!body.planet)
		return j2000ToDate.multiplyWithoutTranslation(body.j2000Direction);
	const double jde = jd + deltaTDays;
	Vec3d ecliptic;
	if (body.planet->getParent() == earth)
	{
		// The Moon: its position relative to its parent is already geocentric.
		Vec3d vel;
		body.planet->computePosition(jde, ecliptic, vel);
	}
	else
		ecliptic = heliocentric(body.planet, jde) - heliocentric(earth, jde);
	return j2000ToDate.multiplyWithoutTranslation(StelCore::matVsop87ToJ2000.multiplyWithoutTranslation(ecliptic));
}

Vec3d EphemerisSampler::topocentricEquatorial(const Body& body, double jd, double theta) const
{
	Vec3d v = geocentricEquatorial(body, jd);
	if (body.planet)
		v -= Vec3d(observerShape[0] * std::cos(theta), observerShape[0] * std::sin(theta), observerShape[2]);
	v.normalize();
	return v;
}

double EphemerisSampler::altitude(const Body& body, double jd, bool refraction) const
{
	const double theta = localSiderealRadians(jd);
	const Vec3d v = topocentricEquatorial(body, jd, theta);
	const double dec = std::asin(std::clamp(v[2], -1., 1.));
	const double hourAngle = theta - std::atan2(v[1], v[0]);
	const double sinAlt = std::sin(latitude) * std::sin(dec) + std::cos(latitude) * std::cos(dec) * std::cos(hourAngle);
	const double alt = qRadiansToDegrees(std::asin(std::clamp(sinAlt, -1., 1.)));
	return refraction ? refracted(alt) : alt;
}

double EphemerisSampler::findCrossing(const Body& body, bool refraction, double jdA, double jdB, double level) const
{
	double fa = altitude(body, jdA, refraction) - level;
	for (int i = 0; i < 14; ++i) // 10 minutes / 2^14 < 0.05 s
	{
		const double mid = 0.5 * (jdA + jdB);
		const double fm = altitude(body, mid, refraction) - level;
		if ((fa < 0.) == (fm < 0.))
		{
			jdA = mid;
			fa = fm;
		}
		else
			jdB = mid;
	}
	return 0.5 * (jdA + jdB);
}

double EphemerisSampler::sunCulmination(const QDate& date) const
{
	// Upper culmination nearest to local clock noon: hour angle zero. The Sun's
	// hour angle changes by about 360 degrees per day.
	double jd = localClockToJd(date, 12 * 60, timeZone);
	for (int i = 0; i < 3; ++i)
	{
		const double theta = localSiderealRadians(jd);
		const Vec3d v = topocentricEquatorial(sunBody, jd, theta);
		jd -= wrap180(qRadiansToDegrees(theta - std::atan2(v[1], v[0]))) / 360.;
	}
	return jd;
}

void EphemerisSampler::computeSunEvents(NightRow& row)
{
	// Sun: geometric altitude of the centre; the thresholds include refraction (CAL-T-02).
	// The window runs from one upper culmination of the Sun to the next. Each window
	// then holds at most one setting and one rising per threshold, also near polar
	// night and day, where clock noon can be far from solar noon.
	const double start = sunCulmination(row.eveningDate);
	const double end = sunCulmination(row.eveningDate.addDays(1));
	// Even steps of about SUN_STEP including both ends (23-25 h with DST changes).
	const int steps = std::max(2, static_cast<int>(std::lround((end - start) / SUN_STEP)));
	QVector<double> t, alt;
	for (int i = 0; i <= steps; ++i)
	{
		t.append(start + i * (end - start) / steps);
		alt.append(altitude(sunBody, t.last(), false));
	}

	// Lowest and highest Sun altitude (CAL-T-09a, CAL-T-09b), golden-section refinement.
	// The highest altitude is at one end of the window (a culmination).
	const int lowIndex = static_cast<int>(std::min_element(alt.cbegin(), alt.cend()) - alt.cbegin());
	auto extreme = [&](bool lowest) {
		const int i = lowest ? lowIndex : static_cast<int>(std::max_element(alt.cbegin(), alt.cend()) - alt.cbegin());
		SunExtreme e{ t[i], alt[i] };
		if (i == 0 || i == t.size() - 1)
			return e;
		const double sign = lowest ? 1. : -1.;
		auto f = [&](double jd) { return sign * altitude(sunBody, jd, false); };
		const double ratio = (std::sqrt(5.) - 1.) / 2.;
		double a = t[i - 1], b = t[i + 1];
		double c = b - ratio * (b - a), d = a + ratio * (b - a);
		double fc = f(c), fd = f(d);
		for (int n = 0; n < 30; ++n)
		{
			if (fc < fd)
			{
				b = d;
				d = c;
				fd = fc;
				c = b - ratio * (b - a);
				fc = f(c);
			}
			else
			{
				a = c;
				c = d;
				fc = fd;
				d = a + ratio * (b - a);
				fd = f(d);
			}
		}
		e.jd = 0.5 * (a + b);
		e.altitude = altitude(sunBody, e.jd, false);
		return e;
	};
	row.sunLowest = extreme(true);
	row.sunHighest = extreme(false);

	for (int k = 0; k < ThresholdCount; ++k)
	{
		const double level = thresholdAltitude(static_cast<Threshold>(k));
		row.dusk[k].reset();
		row.dawn[k].reset();
		for (int i = 1; i < t.size(); ++i)
		{
			const bool belowA = alt[i - 1] < level;
			const bool belowB = alt[i] < level;
			if (belowA == belowB)
				continue;
			std::optional<double>& slot = belowB ? row.dusk[k] : row.dawn[k];
			if (!slot)
				slot = findCrossing(sunBody, false, t[i - 1], t[i], level);
		}
		// A dip below the threshold shorter than the sampling step: no sample is
		// below, but the refined minimum is. The crossings are on each side of it.
		if (!row.dusk[k] && !row.dawn[k] && alt[0] >= level && row.sunLowest.altitude < level
		    && lowIndex > 0 && lowIndex < t.size() - 1)
		{
			row.dusk[k] = findCrossing(sunBody, false, t[lowIndex - 1], row.sunLowest.jd, level);
			row.dawn[k] = findCrossing(sunBody, false, row.sunLowest.jd, t[lowIndex + 1], level);
		}
	}
}

void EphemerisSampler::chooseWindow()
{
	if (settings.autoWindow)
	{
		// CAL-L-07: whole hour before the earliest sunset to whole hour after the latest sunrise.
		// Clock times are measured from local noon, so that a sunset after midnight counts as late.
		double earliestSunset = -1., latestSunrise = -1.;
		auto fromNoon = [](double minutes) { return std::fmod(minutes - 12. * 60. + 1440., 1440.); };
		for (const NightRow& row : std::as_const(model_.rows))
		{
			if (row.dusk[0])
			{
				const double m = fromNoon(localClockMinutes(*row.dusk[0], timeZone));
				earliestSunset = (earliestSunset < 0.) ? m : std::min(earliestSunset, m);
			}
			if (row.dawn[0])
				latestSunrise = std::max(latestSunrise, fromNoon(localClockMinutes(*row.dawn[0], timeZone)));
		}
		const int start = (earliestSunset >= 0.) ? static_cast<int>(earliestSunset / 60.) * 60 : 6 * 60;
		const int end = (latestSunrise >= 0.) ? static_cast<int>(std::ceil(latestSunrise / 60.)) * 60 : 18 * 60;
		windowStartClock = (start + 12 * 60) % 1440;
		windowEndClock = (end + 12 * 60) % 1440;
	}
	else
	{
		windowStartClock = settings.windowStartClock;
		windowEndClock = settings.windowEndClock;
	}

	for (NightRow& row : model_.rows)
	{
		// A clock time before noon is on the next morning (as for the observing period).
		row.windowStart = nightClockToJd(row.eveningDate, windowStartClock, timeZone);
		row.windowEnd = nightClockToJd(row.eveningDate, windowEndClock, timeZone);
		if (row.windowEnd <= row.windowStart)
			row.windowEnd = nightClockToJd(row.eveningDate.addDays(1), windowEndClock, timeZone);
	}
}

void EphemerisSampler::computeNight(int index)
{
	NightRow& row = model_.rows[index];
	setNight(row.eveningDate);

	// Moon (architecture 5.4): apparent altitude of the centre.
	row.moonSegments.clear();
	QVector<double> t;
	for (int i = 0; row.windowStart + i * SAMPLE_STEP < row.windowEnd - 1e-9; ++i)
		t.append(row.windowStart + i * SAMPLE_STEP);
	t.append(row.windowEnd);
	QVector<double> alt(t.size());
	for (int i = 0; i < t.size(); ++i)
	{
		alt[i] = altitude(moonBody, t[i], true);
		model_.maxMoonAltitude = std::max(model_.maxMoonAltitude, alt[i]);
	}
	std::optional<MoonSegment> open;
	if (alt[0] > 0.)
	{
		open = MoonSegment();
		open->samples.append({ t[0], alt[0] });
	}
	for (int i = 1; i < t.size(); ++i)
	{
		if ((alt[i - 1] > 0.) != (alt[i] > 0.))
		{
			const double tc = findCrossing(moonBody, true, t[i - 1], t[i], 0.);
			if (alt[i] > 0.)
			{
				open = MoonSegment();
				open->rise = tc;
				open->samples.append({ tc, 0. });
			}
			else if (open)
			{
				open->set = tc;
				open->samples.append({ tc, 0. });
				row.moonSegments.append(*open);
				open.reset();
			}
		}
		if (open && alt[i] > 0.)
			open->samples.append({ t[i], alt[i] });
	}
	if (open)
		row.moonSegments.append(*open);

	// Phase at local midnight (CAL-P-04), from the geocentric Sun and Moon vectors.
	{
		const double jde = localClockToJd(row.eveningDate.addDays(1), 0, timeZone) + deltaTDays;
		const Vec3d sunGeo = -heliocentric(earth, jde);
		Vec3d moonGeo, vel;
		moon->computePosition(jde, moonGeo, vel);
		const Vec3d toSun = sunGeo - moonGeo;
		const double cosPhase = toSun.dot(-moonGeo) / (toSun.norm() * moonGeo.norm());
		row.moonIllumination = 0.5 * (1. + cosPhase);
		const double elongation = std::fmod(qRadiansToDegrees(std::atan2(moonGeo[1], moonGeo[0]) - std::atan2(sunGeo[1], sunGeo[0])) + 720., 360.);
		row.moonWaning = elongation > 180.;
	}

	// Star transits (architecture 5.5): hour angle zero inside the window.
	row.transits.clear();
	const double theta0 = qRadiansToDegrees(localSiderealRadians(row.windowStart));
	for (const Body& star : std::as_const(stars))
	{
		const Vec3d v = geocentricEquatorial(star, row.windowStart);
		const double ra = qRadiansToDegrees(std::atan2(v[1], v[0]));
		double jd = row.windowStart + std::fmod(ra - theta0 + 720., 360.) / SIDEREAL_DEGREES_PER_DAY;
		jd += wrap180(ra - qRadiansToDegrees(localSiderealRadians(jd))) / SIDEREAL_DEGREES_PER_DAY; // nutation-level correction
		if (jd >= row.windowStart && jd <= row.windowEnd)
			row.transits.insert(star.name, jd);
	}

	// Target panel (architecture 5.7, CAL-G-08).
	row.periodStart.reset();
	row.periodEnd.reset();
	row.targetQualifies = QVector<bool>(targets.size(), false);
	if (settings.panel.enabled)
	{
		const auto period = settings.panel.period(row, timeZone);
		if (period)
		{
			row.periodStart = period->first;
			row.periodEnd = period->second;
			for (int k = 0; k < targets.size(); ++k)
			{
				// Samples every SAMPLE_STEP and at the period end. The ends of each run
				// above the minimum altitude are refined to the crossing times.
				const Body& body = targets[k];
				const double minAlt = settings.panel.minAltitude;
				double longest = -1.;
				std::optional<double> runStart;
				double prevJd = 0.;
				bool prevAbove = false;
				for (int i = 0;; ++i)
				{
					const double jd = std::min(period->first + i * SAMPLE_STEP, period->second);
					const bool above = altitude(body, jd, true) >= minAlt;
					if (above && !prevAbove)
						runStart = (i == 0) ? jd : findCrossing(body, true, prevJd, jd, minAlt);
					else if (!above && prevAbove)
					{
						longest = std::max(longest, findCrossing(body, true, prevJd, jd, minAlt) - *runStart);
						runStart.reset();
					}
					prevJd = jd;
					prevAbove = above;
					if (jd >= period->second)
						break;
				}
				if (runStart)
					longest = std::max(longest, period->second - *runStart);
				row.targetQualifies[k] = longest >= 0.
					&& longest * 24. >= settings.panel.minDurationHours - 1e-9;
			}
		}
	}
}

} // namespace calenduh
