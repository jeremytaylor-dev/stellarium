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

#ifndef CALENDUH_USERSETTINGS_HPP
#define CALENDUH_USERSETTINGS_HPP

// User settings of the plugin and their storage in Stellarium's config.ini,
// group [Calenduh] (CAL-I-08). No Stellarium dependency.

#include "ChartSettings.hpp"
#include "ChartStyle.hpp"

#include <QPageLayout>
#include <QPageSize>
#include <QSizeF>

class QSettings;

namespace calenduh {

//! True if @p id names a time zone that Qt knows. Qt accepts an empty name
//! (as the system zone); this function does not.
bool isValidTimeZone(const QString& id);

//! Identifier of a fixed UTC offset, e.g. "UTC+12:45", which QTimeZone accepts.
QString fixedOffsetTimeZone(int minutesAheadOfUtc);

//! Export options (CAL-I-05).
struct ExportOptions
{
	enum class Format { Pdf, Svg, Png };
	Format format = Format::Pdf;
	QPageSize::PageSizeId pageSize = QPageSize::A4;
	QPageLayout::Orientation orientation = QPageLayout::Landscape;
	int pngDpi = 150;
	static constexpr int MaxPngDpi = 600;
	QString directory;              //!< last export directory

	//! Page size in points, in the selected orientation. Also used for SVG and PNG.
	QSizeF pagePoints() const;
	static QString formatSuffix(Format f);
};

//! Everything the plugin saves (CAL-I-08).
struct UserSettings
{
	//! The target panel starts with the values of the Visual preset, switched off.
	UserSettings();

	ChartSettings chart;

	//! CAL-L-09: use Stellarium's current location (if it is on Earth). If false,
	//! or if Stellarium's observer is not on Earth (CAL-I-09), use chart.location.
	bool followStellariumLocation = true;
	//! True when chart.location holds a saved Earth location (CAL-I-09).
	bool hasSavedLocation = false;
	//! CAL-L-06: use the calendar month of Stellarium's current date. If false,
	//! use chart.firstEvening and chart.lastEvening.
	bool currentMonth = true;

	ChartStyle style;               //!< user-configurable colours and line weights
	ExportOptions exportOptions;

	//! Read the group [Calenduh]. Missing or invalid values keep their defaults.
	void load(QSettings& conf);
	//! Write the group [Calenduh].
	void save(QSettings& conf) const;
};

} // namespace calenduh

#endif // CALENDUH_USERSETTINGS_HPP
