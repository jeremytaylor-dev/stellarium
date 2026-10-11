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

#ifndef CALENDUH_CHARTMODEL_HPP
#define CALENDUH_CHARTMODEL_HPP

// Plain chart data. No Stellarium dependency (architecture section 4, design rule).
// All times are Julian Days in UTC.

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace Calenduh {

//! The four Sun-altitude thresholds of the chart, from the brightest to the darkest band.
enum class Threshold { Sunset = 0, Civil = 1, Nautical = 2, Astronomical = 3 };
constexpr int ThresholdCount = 4;

//! Sun altitude [degrees] of each threshold (CAL-T-02).
double thresholdAltitude(Threshold t);

struct SunExtreme
{
	double jd = 0.;
	double altitude = 0.;   //!< degrees
};

struct MoonSample
{
	double jd = 0.;
	double altitude = 0.;   //!< degrees, apparent
};

//! One continuous interval with the Moon above the horizon, inside the time window.
struct MoonSegment
{
	std::optional<double> rise;   //!< JD, if the Moon rises inside the window (CAL-M-06)
	std::optional<double> set;    //!< JD, if the Moon sets inside the window
	QVector<MoonSample> samples;  //!< in time order; includes the 0-degree points at rise and set
};

//! One night (one chart row).
struct NightRow
{
	QDate eveningDate;
	double windowStart = 0.;      //!< JD of the time-window start on this night
	double windowEnd = 0.;        //!< JD of the time-window end on the next morning

	//! Dusk (evening) and dawn (morning) time of each threshold. Empty if the
	//! Sun does not cross the threshold on this night (CAL-T-09).
	std::optional<double> dusk[ThresholdCount];
	std::optional<double> dawn[ThresholdCount];

	SunExtreme sunLowest;         //!< lowest Sun altitude, noon to noon
	SunExtreme sunHighest;        //!< highest Sun altitude, noon to noon

	double moonIllumination = 0.; //!< 0..1 at local midnight (CAL-P-04)
	bool moonWaning = false;
	QVector<MoonSegment> moonSegments;

	QMap<QString, double> transits; //!< star name -> JD of upper transit inside the window
};

struct ChartLocation
{
	QString name;
	double latitude = 0.;   //!< degrees, north positive
	double longitude = 0.;  //!< degrees, east positive
	double elevation = 0.;  //!< metres
	QString timeZone;       //!< IANA identifier, e.g. "Pacific/Auckland"
};

struct ChartModel
{
	ChartLocation location;
	double maxMoonAltitude = 0.;  //!< degrees; the top of every row (CAL-M-04)
	QStringList stars;            //!< star names in display order
	QVector<NightRow> rows;

	//! Read a model from JSON (schema 1). Returns false and sets errorMessage on failure.
	bool fromJson(const QByteArray& json, QString* errorMessage = nullptr);
	QByteArray toJson() const;
};

//! Conversion between UTC Julian Day and QDateTime.
QDateTime jdToDateTime(double jd);
double dateTimeToJd(const QDateTime& dateTime);

} // namespace Calenduh

#endif // CALENDUH_CHARTMODEL_HPP
