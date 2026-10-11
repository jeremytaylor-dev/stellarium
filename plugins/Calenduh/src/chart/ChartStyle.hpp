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

#ifndef CALENDUH_CHARTSTYLE_HPP
#define CALENDUH_CHARTSTYLE_HPP

#include <QColor>
#include <QString>

namespace Calenduh {

//! Colours, line weights and text of the chart. Lengths are in points (1/72 inch).
//! Defaults are the values accepted in requirements.html.
struct ChartStyle
{
	QString title = QStringLiteral("Observation Planning Calendar");

	QColor daylight        { 0xFF, 0xFF, 0xFF };  //!< CAL-T-07
	QColor bandColor[4] = {                       //!< CAL-T-06, below each threshold
		QColor(0xBC, 0xD8, 0xF0),                 //!< sunset..civil: pale blue
		QColor(0x4F, 0x7F, 0xBF),                 //!< civil..nautical: mid blue
		QColor(0x16, 0x2F, 0x5E),                 //!< nautical..astronomical: dark blue
		QColor(0x00, 0x00, 0x00) };               //!< astronomical night: black

	QColor gridColor       { 0x80, 0x80, 0x80 };  //!< CAL-T-11
	double gridWidth = 0.4;

	QColor starLineColor   { 0xFF, 0x5A, 0x1F };  //!< CAL-S-05
	double starLineWidth = 0.5;

	QColor moonNightColor  { 0xFF, 0xF3, 0xA0 };  //!< CAL-M-02
	QColor moonDayColor    { 0xC9, 0x97, 0x00 };  //!< CAL-M-10, Sun above -6 degrees
	double moonMinWidth = 0.5;                    //!< CAL-M-03, thinnest crescent
	double moonMaxWidthFactor = 0.45;             //!< CAL-M-03, full Moon, as a fraction of row height

	QColor panelLineColor  { 0xA8, 0xA8, 0xA8 };  //!< CAL-G-06, grey target lines
	double panelLineWidth = 1.2;
	QColor panelTextColor  { 0x00, 0x00, 0x00 };  //!< CAL-G-06, black labels
	QColor panelRowColor   { 0xE0, 0xE0, 0xE0 };  //!< panel row separators
	QColor panelShadeColor { 0xED, 0xED, 0xED };  //!< CAL-G-16, rows without a period
	double panelColumnFactor = 2.3;               //!< CAL-G-14, column width in units of the base text size
	QString panelShadeNote = QStringLiteral("Shaded rows: the period does not exist");

	QColor textColor       { 0x20, 0x20, 0x20 };
	QColor phaseOutline    { 0x80, 0x80, 0x80 };
};

} // namespace Calenduh

#endif // CALENDUH_CHARTSTYLE_HPP
