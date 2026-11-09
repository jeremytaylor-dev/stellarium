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
#include <QTimeZone>
#include <QVector>
#include <optional>

namespace calenduh {

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

//! Notable event types (CAL-E-01), in settings order.
enum class EventKind { SolarEclipse = 0, LunarEclipse = 1, Opposition = 2, GreatestElongation = 3 };
constexpr int EventKindCount = 4;

//! JSON key of an event kind, e.g. "lunarEclipse".
QString eventKindKey(EventKind kind);
//! The event kind of a JSON key; empty if unknown.
std::optional<EventKind> eventKindFromKey(const QString& key);

//! Eclipse type, or side of a greatest elongation (CAL-E-08).
enum class EventDetail { None, Total, Annular, Partial, Penumbral, East, West };

//! JSON key of an event detail, e.g. "total"; empty for None.
QString eventDetailKey(EventDetail detail);
//! The event detail of a JSON key; None if unknown.
EventDetail eventDetailFromKey(const QString& key);

//! One notable event, marked by a ring on its row (requirements section 8a). The label is
//! made when the chart is drawn (ChartText::eventLabel), in the language of the chart.
struct NotableEvent
{
	EventKind kind = EventKind::Opposition;
	double jd = 0.;     //!< event time (CAL-E-04); can be outside the time window (CAL-E-06)
	QString body;       //!< English planet name for an opposition or elongation; empty for an eclipse
	EventDetail detail = EventDetail::None;
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

	SunExtreme sunLowest;         //!< lowest Sun altitude, culmination to culmination
	SunExtreme sunHighest;        //!< highest Sun altitude, culmination to culmination

	double moonIllumination = 0.; //!< 0..1 at local midnight (CAL-P-04)
	bool moonWaning = false;
	QVector<MoonSegment> moonSegments;

	QMap<QString, double> transits; //!< star name -> JD of upper transit inside the window

	//! Target-panel observing period (CAL-G-09). Empty if it does not exist on this night.
	std::optional<double> periodStart;
	std::optional<double> periodEnd;
	QVector<bool> targetQualifies;  //!< per panel target, in list order (CAL-G-08)

	QVector<NotableEvent> notableEvents; //!< in time order (requirements section 8a)

	bool hasPeriod() const { return periodStart.has_value() && periodEnd.has_value(); }
};

//! Target panel (requirements section 8).
struct TargetPanel
{
	QString title;          //!< CAL-G-11
	QString summary;        //!< one-line summary of the settings (CAL-G-11)
	QStringList targets;    //!< list order = column order (CAL-G-14)
};

struct ChartLocation
{
	QString name;
	double latitude = 0.;   //!< degrees, north positive
	double longitude = 0.;  //!< degrees, east positive
	double elevation = 0.;  //!< metres
	QString timeZone;       //!< IANA identifier, e.g. "Pacific/Auckland"

	bool operator==(const ChartLocation& o) const
	{
		return name == o.name && latitude == o.latitude && longitude == o.longitude
		       && elevation == o.elevation && timeZone == o.timeZone;
	}
	bool operator!=(const ChartLocation& o) const { return !(*this == o); }
};

struct ChartModel
{
	ChartLocation location;
	double maxMoonAltitude = 0.;  //!< degrees; the top of every row (CAL-M-04)
	QStringList stars;            //!< star names in display order
	QVector<NightRow> rows;
	std::optional<TargetPanel> panel; //!< absent when the panel is off (CAL-G-17)

	//! Read a model from JSON (schema 1). Returns false and sets errorMessage on failure.
	bool fromJson(const QByteArray& json, QString* errorMessage = nullptr);
	QByteArray toJson() const;
};

//! Row boundaries for notable events (CAL-E-10): element i is the start of row i, and the
//! last element is the end of the last row. A boundary between two rows is the midpoint
//! between the end of one window and the start of the next; the outer boundaries are half a
//! day before the first window centre and after the last window centre. Needs the windows.
QVector<double> eventRowBoundaries(const QVector<NightRow>& rows);

//! Conversion between UTC Julian Day and QDateTime.
QDateTime jdToDateTime(double jd);
double dateTimeToJd(const QDateTime& dateTime);

//! The time zone of a ChartLocation: an IANA identifier, or a fixed offset "UTC+hh:mm" /
//! "UTC-hh:mm" (Stellarium's LMST and LTST). Fixed offsets are made from the number of
//! seconds, because older Qt versions accept only some offsets by name.
QTimeZone chartTimeZone(const QString& id);

//! JD of a local clock time (minutes after midnight) on @p date in @p timeZone.
//! A time in a daylight-saving gap maps to a valid instant next to the gap;
//! an ambiguous time (clock set back) maps to one of its two instants.
double localClockToJd(const QDate& date, int minutes, const QTimeZone& timeZone);

//! JD of a local clock time on the night of @p evening: a time before noon is on the
//! next morning, a time from noon is on the evening date.
double nightClockToJd(const QDate& evening, int minutes, const QTimeZone& timeZone);

//! Local clock time of @p jd, in minutes after midnight (with fraction).
double localClockMinutes(double jd, const QTimeZone& timeZone);

} // namespace calenduh

#endif // CALENDUH_CHARTMODEL_HPP
