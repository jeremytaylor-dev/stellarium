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
//! Use: prepare(), then computeNight(i) for each night, then computeEvents(),
//! then model(). To keep the user interface responsive (architecture section
//! 5.6), call the stages one per event-loop turn instead: begin(),
//! computeSunNight(i) for each night, chooseTimeWindow(); then computeNight(i)
//! one per turn; then computeEventStep(i) one per turn.
class EphemerisSampler
{
public:
	explicit EphemerisSampler(const ChartSettings& settings);

	//! Resolve names, compute the Sun events of all nights and the time window.
	//! Returns false and sets @p error on failure.
	bool prepare(QString* error = nullptr);

	//! First stage of prepare(): check the settings, make the rows, resolve names.
	bool begin(QString* error = nullptr);
	//! Second stage of prepare(): the Sun events of night @p index.
	void computeSunNight(int index);
	//! Last stage of prepare(): the time window, from the Sun events of all nights.
	void chooseTimeWindow() { chooseWindow(); }

	int nightCount() const { return model_.rows.size(); }

	//! Moon, star transits and target panel of night @p index.
	void computeNight(int index);

	//! Notable events of all nights (requirements section 8a). Needs the time
	//! window and the Moon curves, so call it after computeNight() for every night.
	//! Same as computeEventStep(i) for i = 0 .. eventStepCount() - 1, in order.
	void computeEvents();
	//! Number of event steps; known after begin().
	int eventStepCount() const;
	//! One part of computeEvents(): the start, one planet, one eclipse type, or
	//! the assignment to rows (the last step).
	void computeEventStep(int index);

	//! The chart model; complete after computeNight() for every night and computeEvents().
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
	QVector<Body> outerPlanets;             //!< oppositions (CAL-E-02)
	QVector<Body> innerPlanets;             //!< greatest elongations (CAL-E-02)

	double latitude = 0.;                   //!< radians
	Vec3d observerShape;                    //!< (rho cos phi', 0, rho sin phi') in AU

	int windowStartClock = 0;
	int windowEndClock = 0;

	bool resolveBodies(QString* error);
	void setNight(const QDate& evening);
	double sunCulmination(const QDate& date) const;
	void computeSunEvents(NightRow& row);
	void chooseWindow();

	double localSiderealRadians(double jd) const;
	Vec3d heliocentric(const QSharedPointer<Planet>& p, double jde) const;
	Vec3d geocentricEquatorial(const Body& body, double jd) const;
	Vec3d topocentricEquatorial(const Body& body, double jd, double theta) const;
	//! Observer's position from the Earth's centre [AU], equatorial of date, at local sidereal angle @p theta.
	Vec3d observerOffset(double theta) const;
	double altitude(const Body& body, double jd, bool refraction) const;
	double findCrossing(const Body& body, bool refraction, double jdA, double jdB, double level) const;

	// Notable events (EphemerisSampler.cpp, requirements section 8a).
	struct EventFound
	{
		NotableEvent event;
		const Body* planet;                 //!< checked for visibility in its row (CAL-E-03)
	};
	QVector<double> eventBounds;            //!< eventRowBoundaries() of the rows
	QVector<double> eventDays;              //!< daily search samples
	QVector<EventFound> eventsFound;        //!< before the assignment to rows

	template <class F>
	QVector<double> signChanges(F f) const;
	double apparentLongitudeDifference(const Body& body, double jd) const;
	void findOppositions(const Body& planet);
	void findGreatestElongations(const Body& planet);
	void findLunarEclipses();
	void findSolarEclipses();
	void assignEventRows();
	void setEpoch(double jd);
	//! Geocentric position [AU] in the VSOP87 J2000 ecliptic frame. Planets: corrected
	//! for light time; with @p apparent also for aberration. The Moon is geometric.
	Vec3d eventGeocentric(const Body& body, double jd, bool apparent) const;
	//! Topocentric position [AU] in the equatorial frame of the epoch set by setEpoch().
	Vec3d eventTopocentric(const Body& body, double jd, bool apparent) const;
	double eclipticLongitude(const Body& body, double jd, bool apparent) const;
	//! CAL-E-09: the time of the smallest @p separation in [c1, c4] with @p body above
	//! @p level; empty if the body is below it all the time.
	template <class Separation>
	std::optional<double> visibleBest(double c1, double c4, double tMax, Separation separation,
					  const Body& body, bool refraction, double level);
};

} // namespace calenduh

#endif // CALENDUH_EPHEMERISSAMPLER_HPP
