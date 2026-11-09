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

#ifndef CALENDUH_CHARTTEXT_HPP
#define CALENDUH_CHARTTEXT_HPP

#include "ChartModel.hpp"

#include <QLocale>
#include <QMap>
#include <QString>
#include <functional>

namespace calenduh {

//! The language-dependent text of the chart (CAL-I-11). The defaults are English, as the
//! requirements give them. The plugin makes a translated copy with translated(). Star,
//! target and panel names are not here: they stay as the user typed them (LANG-1).
class ChartText
{
public:
	enum Id
	{
		Title, ShadeNote,
		North, South, East, West,
		TotalSolarEclipse, AnnularSolarEclipse, PartialSolarEclipse,
		TotalLunarEclipse, PartialLunarEclipse, PenumbralLunarEclipse,
		Opposition, GreatestElongationEast, GreatestElongationWest, //!< %1: planet name
		Sunset, CivilDusk, NauticalDusk, AstronomicalDusk,           //!< in SunEvent order
		AstronomicalDawn, NauticalDawn, CivilDawn, Sunrise,
		AfterStart,     //!< %1: duration
		ForMinimum,     //!< %1: duration
		AtAnyTime,
		PanelSummary,   //!< %1 start, %2 end, %3 altitude, %4 duration text
		Count
	};

	//! English texts, English dates and numbers.
	ChartText();

	const QString& operator[](Id id) const { return texts[id]; }
	void set(Id id, const QString& text) { texts[id] = text; }

	//! A copy with every text passed through @p translate, and dates and numbers in @p locale.
	ChartText translated(const std::function<QString(const QString&)>& translate, const QLocale& locale) const;
	const QLocale& locale() const { return loc; }

	//! Shown names of Solar System bodies, by English name (LANG-1). A body that is not in
	//! the map keeps its English name.
	void setBodyNames(const QMap<QString, QString>& names) { bodies = names; }
	QString bodyName(const QString& englishName) const { return bodies.value(englishName, englishName); }

	//! CAL-E-08: the label of a notable event, without the time.
	QString eventLabel(const NotableEvent& event) const;
	//! CAL-L-05: "51.51° N", "0.13° W", in the locale's number format.
	QString latitude(double degrees) const;
	QString longitude(double degrees) const;
	//! CAL-L-05: the date range of the title, e.g. "1–31 August 2026".
	QString dateRange(const QDate& first, const QDate& last) const;
	//! A number in the locale's format, as QLocale::toString().
	QString number(double value, char format, int precision) const { return loc.toString(value, format, precision); }

	bool operator==(const ChartText& o) const;
	bool operator!=(const ChartText& o) const { return !(*this == o); }

private:
	QString texts[Count];
	QLocale loc;
	QMap<QString, QString> bodies;
};

} // namespace calenduh

#endif // CALENDUH_CHARTTEXT_HPP
