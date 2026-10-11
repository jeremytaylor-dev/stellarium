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

#ifndef CALENDUH_EPHEMERISSAMPLER_HPP
#define CALENDUH_EPHEMERISSAMPLER_HPP

#include "ChartModel.hpp"
#include "ChartSettings.hpp"
#include "VecMath.hpp"

#include <QSharedPointer>
#include <QStringList>
#include <QTimeZone>
#include <QVector>

class Planet;

namespace calenduh {

//! Builds a ChartModel from Stellarium's ephemerides (architecture section 5).
//!
//! Altitudes are calculated in this class for the chart location (option B,
//! architecture section 3.4). The class reads Stellarium's orbit functions
//! (Planet::computePosition for a given date), its precession, nutation and
//! sidereal-time functions, and the catalogue positions of stars and deep-sky
//! objects. It never changes the Stellarium clock, observer or selection
//! (CAL-I-06), and it does not depend on the planet of the Stellarium observer
//! (CAL-I-09). Must run on the GUI thread (catalogue lookups).
//!
//! Use: prepare(), then computeNight(i) for each night (one per event-loop turn,
//! architecture section 5.6), then model().
class EphemerisSampler
{
public:
	explicit EphemerisSampler(const ChartSettings& settings);

	//! Resolve names, compute the Sun events of all nights and the time window.
	//! Returns false and sets @p error on failure.
	bool prepare(QString* error = nullptr);

	int nightCount() const { return model_.rows.size(); }

	//! Moon, star transits and target panel of night @p index.
	void computeNight(int index);

	//! The chart model; complete after computeNight() for every night.
	const ChartModel& model() const { return model_; }

	//! Star and target names that Stellarium could not find (CAL-S-07, CAL-G-12).
	const QStringList& unresolvedNames() const { return unresolved; }

private:
	//! A body whose altitude can be calculated: a fixed J2000 direction (star,
	//! deep-sky object) or a Solar System body.
	struct Body
	{
		QString name;
		Vec3d j2000Direction;               //!< unit vector, equatorial J2000
		QSharedPointer<Planet> planet;      //!< set for Solar System bodies
	};

	ChartSettings settings;
	ChartModel model_;
	QTimeZone timeZone;
	QStringList unresolved;

	double deltaTDays = 0.;                 //!< TT - UT for the chart dates
	Mat4d j2000ToDate;                      //!< precession-nutation of the current night
	QSharedPointer<Planet> earth, moon;
	Body sunBody, moonBody;
	QVector<Body> stars;
	QVector<Body> targets;

	double latitude = 0.;                   //!< radians
	Vec3d observerShape;                    //!< (rho cos phi', 0, rho sin phi') in AU

	int windowStartClock = 0;
	int windowEndClock = 0;

	bool resolveBodies(QString* error);
	void setNight(const QDate& evening);
	void computeSunEvents(NightRow& row);
	void chooseWindow();

	double localSiderealRadians(double jd) const;
	Vec3d heliocentric(const QSharedPointer<Planet>& p, double jde) const;
	Vec3d geocentricEquatorial(const Body& body, double jd) const;
	double altitude(const Body& body, double jd, bool refraction) const;
	double findCrossing(const Body& body, bool refraction, double jdA, double jdB, double level) const;
};

} // namespace calenduh

#endif // CALENDUH_EPHEMERISSAMPLER_HPP
