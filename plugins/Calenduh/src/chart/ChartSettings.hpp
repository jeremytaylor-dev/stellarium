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

#ifndef CALENDUH_CHARTSETTINGS_HPP
#define CALENDUH_CHARTSETTINGS_HPP

// Inputs of chart generation. No Stellarium dependency.

#include "ChartModel.hpp"

#include <QDate>
#include <QJsonObject>
#include <QTimeZone>
#include <QString>
#include <QStringList>
#include <optional>
#include <utility>

namespace calenduh {

//! The eight Sun events of a night, in time order (CAL-T-01).
enum class SunEvent { Sunset, CivilDusk, NauticalDusk, AstronomicalDusk,
		      AstronomicalDawn, NauticalDawn, CivilDawn, Sunrise };
constexpr int SunEventCount = 8;

//! JSON key of an event, e.g. "nauticalDusk" (same keys as the chart-model JSON).
QString sunEventKey(SunEvent e);
std::optional<SunEvent> sunEventFromKey(const QString& key);
//! Display name of an event, e.g. "nautical dusk".
QString sunEventName(SunEvent e);
//! Time of an event on a night, if it occurs.
std::optional<double> sunEventTime(const NightRow& row, SunEvent e);

//! One end of the target-panel observing period (CAL-G-09).
struct PeriodEdge
{
	enum class Kind { Clock, Event, Duration };
	Kind kind = Kind::Event;
	int clockMinutes = 0;           //!< Kind::Clock: minutes after local midnight
	SunEvent event = SunEvent::NauticalDusk;
	double offsetHours = 0.;        //!< Kind::Event: offset from the event
	double durationHours = 0.;      //!< Kind::Duration (end only): duration after the start

	static PeriodEdge atClock(int minutes);
	static PeriodEdge atEvent(SunEvent e, double offsetHours = 0.);
	static PeriodEdge afterStart(double hours);

	QString describe() const;
	QJsonObject toJson() const;
	static std::optional<PeriodEdge> fromJson(const QJsonObject& o);
};

//! Target-panel settings (requirements section 8).
struct PanelSettings
{
	bool enabled = false;
	QString title;
	PeriodEdge start;
	PeriodEdge end;
	double minAltitude = 0.;        //!< degrees (CAL-G-03)
	double minDurationHours = 0.;   //!< CAL-G-08
	QStringList targets;            //!< CAL-G-04, list order = column order

	static PanelSettings visualPreset();            //!< CAL-G-10
	static PanelSettings astrophotographyPreset();  //!< CAL-G-10

	//! One-line summary for the heading (CAL-G-11).
	QString summary() const;

	//! The observing period on a night, or empty if it does not exist (CAL-G-09).
	//! @p timeZone is used for clock times; a clock time before noon is on the next morning.
	std::optional<std::pair<double, double>> period(const NightRow& row, const QTimeZone& timeZone) const;

	QJsonObject toJson() const;
	static std::optional<PanelSettings> fromJson(const QJsonObject& o);
};

//! All inputs of chart generation.
struct ChartSettings
{
	ChartLocation location;         //!< CAL-L-09
	QDate firstEvening;             //!< CAL-L-06
	QDate lastEvening;
	bool autoWindow = true;         //!< CAL-L-07
	int windowStartClock = 18 * 60; //!< minutes after local midnight, if not automatic
	int windowEndClock = 6 * 60;
	QStringList stars = defaultStars();  //!< CAL-S-07
	PanelSettings panel;

	//! Default star list (CAL-S-08), in order of right ascension.
	static QStringList defaultStars();
};

} // namespace calenduh

#endif // CALENDUH_CHARTSETTINGS_HPP
