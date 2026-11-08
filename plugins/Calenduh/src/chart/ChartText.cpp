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

#include "ChartText.hpp"
#include "ChartSettings.hpp"

#include <QRegularExpression>
#include <cmath>

namespace calenduh {

namespace {

// N_() marks the texts for translation; the plugin translates them (CAL-I-10, LANG-3).
#ifndef N_
#define N_(str) str
#endif

//! English texts in ChartText::Id order, except the Sun events (from sunEventName()).
const char* const englishTexts[ChartText::Sunset] = {
	// TRANSLATORS: Calenduh: chart title, followed by the date range.
	N_("Observation Planning Calendar"),
	// TRANSLATORS: Calenduh: note below the target panel.
	N_("Shaded rows: the period does not exist"),
	// TRANSLATORS: Calenduh: north, after a latitude in the chart title.
	N_("N"),
	// TRANSLATORS: Calenduh: south, after a latitude in the chart title.
	N_("S"),
	// TRANSLATORS: Calenduh: east, after a longitude in the chart title.
	N_("E"),
	// TRANSLATORS: Calenduh: west, after a longitude in the chart title.
	N_("W"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart.
	N_("Total solar eclipse"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart.
	N_("Annular solar eclipse"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart.
	N_("Partial solar eclipse"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart.
	N_("Total lunar eclipse"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart.
	N_("Partial lunar eclipse"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart.
	N_("Penumbral lunar eclipse"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart; %1 is a planet name.
	N_("%1 opposition"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart; %1 is a planet name; E means east.
	N_("%1 greatest elongation E"),
	// TRANSLATORS: Calenduh: label of a notable event on the chart; %1 is a planet name; W means west.
	N_("%1 greatest elongation W"),
};

const char* const englishSummaryTexts[ChartText::Count - ChartText::AfterStart] = {
	// TRANSLATORS: Calenduh: target panel summary, the end of the observing period; %1 is a duration (h:mm).
	N_("(start + %1)"),
	// TRANSLATORS: Calenduh: target panel summary; %1 is a duration (h:mm).
	N_("for minimum %1"),
	// TRANSLATORS: Calenduh: target panel summary, when there is no minimum duration.
	N_("at any time"),
	// TRANSLATORS: Calenduh: target panel summary; %1 start, %2 end, %3 minimum altitude, %4 the duration text.
	N_("%1 to %2, above %3° %4"),
};

//! The long date format of @p locale without the weekday.
QString dateFormatWithoutWeekday(const QLocale& locale)
{
	QString f = locale.dateFormat(QLocale::LongFormat);
	static const QRegularExpression weekday(QStringLiteral("\\s*,?\\s*d{3,4}\\s*,?\\s*"));
	return f.replace(weekday, QStringLiteral(" ")).trimmed();
}

//! @p format without its quoted literals.
QString unquoted(const QString& format)
{
	static const QRegularExpression quoted(QStringLiteral("'[^']*'"));
	return QString(format).remove(quoted);
}

} // namespace

ChartText::ChartText()
	: loc(QLocale::English, QLocale::UnitedKingdom)
{
	for (int i = 0; i < Sunset; ++i)
		texts[i] = QString::fromUtf8(englishTexts[i]);
	for (int i = 0; i < SunEventCount; ++i)
		texts[Sunset + i] = sunEventName(static_cast<SunEvent>(i));
	for (int i = AfterStart; i < Count; ++i)
		texts[i] = QString::fromUtf8(englishSummaryTexts[i - AfterStart]);
}

ChartText ChartText::translated(const std::function<QString(const QString&)>& translate, const QLocale& locale) const
{
	ChartText t = *this;
	for (QString& s : t.texts)
		s = translate(s);
	t.loc = locale;
	return t;
}

QString ChartText::eventLabel(const NotableEvent& event) const
{
	switch (event.kind)
	{
		case EventKind::SolarEclipse:
			return texts[event.detail == EventDetail::Total     ? TotalSolarEclipse
				     : event.detail == EventDetail::Annular ? AnnularSolarEclipse
									    : PartialSolarEclipse];
		case EventKind::LunarEclipse:
			return texts[event.detail == EventDetail::Total     ? TotalLunarEclipse
				     : event.detail == EventDetail::Partial ? PartialLunarEclipse
									    : PenumbralLunarEclipse];
		case EventKind::Opposition:
			return texts[Opposition].arg(bodyName(event.body));
		case EventKind::GreatestElongation:
			break;
	}
	return texts[event.detail == EventDetail::West ? GreatestElongationWest : GreatestElongationEast].arg(bodyName(event.body));
}

QString ChartText::latitude(double degrees) const
{
	return QString("%1° %2").arg(loc.toString(std::fabs(degrees), 'f', 2), texts[degrees < 0. ? South : North]);
}

QString ChartText::longitude(double degrees) const
{
	return QString("%1° %2").arg(loc.toString(std::fabs(degrees), 'f', 2), texts[degrees < 0. ? West : East]);
}

QString ChartText::dateRange(const QDate& first, const QDate& last) const
{
	const bool sameYear = first.year() == last.year();
	const bool sameMonth = sameYear && first.month() == last.month();
	if (loc.language() == QLocale::English)
	{
		// The form of the requirements (CAL-L-05), for every English locale.
		const QLocale english(QLocale::English, QLocale::UnitedKingdom);
		if (sameMonth)
			return QString("%1–%2 %3").arg(first.day()).arg(last.day()).arg(english.toString(last, "MMMM yyyy"));
		if (sameYear)
			return QString("%1 – %2").arg(english.toString(first, "d MMMM"), english.toString(last, "d MMMM yyyy"));
		return QString("%1 – %2").arg(english.toString(first, "d MMMM yyyy"), english.toString(last, "d MMMM yyyy"));
	}

	// LANG-2: the locale's long date without the weekday. If it starts with the day and
	// ends with the year after the month, the first date is shortened as in English:
	// "1.–31. August 2026", "1. August – 30. September 2026". Otherwise both dates are complete.
	const QString format = dateFormatWithoutWeekday(loc);
	const QString full = loc.toString(last, format);
	const QString plain = unquoted(format);
	static const QRegularExpression dayFirst(QStringLiteral("^d{1,2}[^dMy]*M"));
	const int year = format.lastIndexOf(QStringLiteral("yy"));
	const int month = year < 0 ? -1 : format.lastIndexOf(QLatin1Char('M'), year);
	const bool yearLast = year >= 0 && !unquoted(format.mid(year)).contains(QRegularExpression(QStringLiteral("[dM]")));
	if (sameYear && format.startsWith(QLatin1Char('d')) && dayFirst.match(plain).hasMatch() && month >= 0 && yearLast)
	{
		if (sameMonth)
		{
			// The day and the characters after it up to the first space, e.g. "1." in German.
			static const QRegularExpression dayPart(QStringLiteral("^d{1,2}[^\\sdMy']*"));
			const QString dayFormat = dayPart.match(format).captured(0);
			return QString("%1–%2").arg(loc.toString(first, dayFormat), full);
		}
		return QString("%1 – %2").arg(loc.toString(first, format.left(month + 1)), full);
	}
	return QString("%1 – %2").arg(loc.toString(first, format), full);
}

bool ChartText::operator==(const ChartText& o) const
{
	for (int i = 0; i < Count; ++i)
		if (texts[i] != o.texts[i])
			return false;
	return loc == o.loc && bodies == o.bodies;
}

} // namespace calenduh
