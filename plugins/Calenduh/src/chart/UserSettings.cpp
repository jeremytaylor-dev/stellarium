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

#include "UserSettings.hpp"

#include <QJsonDocument>
#include <QSettings>
#include <QTime>
#include <algorithm>

namespace calenduh {

namespace {

const QString GROUP = QStringLiteral("Calenduh");
const char* const BAND_KEYS[4] = { "color_band_sunset", "color_band_civil", "color_band_nautical", "color_band_astronomical" };

QString clockText(int minutes)
{
	return QTime(minutes / 60, minutes % 60).toString("HH:mm");
}

void readClock(const QSettings& conf, const QString& key, int& minutes)
{
	const QTime t = QTime::fromString(conf.value(key).toString(), "HH:mm");
	if (t.isValid())
		minutes = t.hour() * 60 + t.minute();
}

void readColor(const QSettings& conf, const QString& key, QColor& color)
{
	const QColor c = QColor::fromString(conf.value(key).toString());
	if (c.isValid())
		color = c;
}

void readPositive(const QSettings& conf, const QString& key, double& value)
{
	bool ok = false;
	const double v = conf.value(key).toDouble(&ok);
	if (ok && v > 0.)
		value = v;
}

void readEdge(const QSettings& conf, const QString& key, PeriodEdge& edge)
{
	const QJsonDocument doc = QJsonDocument::fromJson(conf.value(key).toString().toUtf8());
	if (const std::optional<PeriodEdge> e = PeriodEdge::fromJson(doc.object()))
		edge = *e;
}

QString edgeText(const PeriodEdge& edge)
{
	return QString::fromUtf8(QJsonDocument(edge.toJson()).toJson(QJsonDocument::Compact));
}

} // namespace

QSizeF ExportOptions::pagePoints() const
{
	const QSizeF size = QPageSize(pageSize).size(QPageSize::Point);
	const QSizeF landscape(std::max(size.width(), size.height()), std::min(size.width(), size.height()));
	return orientation == QPageLayout::Landscape ? landscape : landscape.transposed();
}

QString ExportOptions::formatSuffix(Format f)
{
	switch (f)
	{
		case Format::Svg: return QStringLiteral("svg");
		case Format::Png: return QStringLiteral("png");
		case Format::Pdf: break;
	}
	return QStringLiteral("pdf");
}

void UserSettings::load(QSettings& conf)
{
	conf.beginGroup(GROUP);

	// Location (CAL-L-09, CAL-I-09)
	followStellariumLocation = conf.value("follow_stellarium_location", followStellariumLocation).toBool();
	const QString tz = conf.value("location_time_zone").toString();
	if (conf.contains("location_latitude") && conf.contains("location_longitude") && !tz.isEmpty())
	{
		ChartLocation& l = chart.location;
		l.name = conf.value("location_name").toString();
		l.latitude = std::clamp(conf.value("location_latitude").toDouble(), -90., 90.);
		l.longitude = std::clamp(conf.value("location_longitude").toDouble(), -180., 180.);
		l.elevation = conf.value("location_elevation").toDouble();
		l.timeZone = tz;
		hasSavedLocation = true;
	}

	// Dates (CAL-L-06)
	currentMonth = conf.value("current_month", currentMonth).toBool();
	const QDate first = QDate::fromString(conf.value("first_evening").toString(), Qt::ISODate);
	const QDate last = QDate::fromString(conf.value("last_evening").toString(), Qt::ISODate);
	if (first.isValid() && last.isValid() && first <= last)
	{
		chart.firstEvening = first;
		chart.lastEvening = last;
	}
	else
		currentMonth = true;

	// Time window (CAL-L-07)
	chart.autoWindow = conf.value("window_automatic", chart.autoWindow).toBool();
	readClock(conf, "window_start", chart.windowStartClock);
	readClock(conf, "window_end", chart.windowEndClock);

	// Stars (CAL-S-07)
	if (conf.contains("stars"))
		chart.stars = conf.value("stars").toStringList();

	// Target panel (requirements section 8)
	PanelSettings& p = chart.panel;
	p.enabled = conf.value("panel_enabled", p.enabled).toBool();
	p.title = conf.value("panel_title", p.title).toString();
	readEdge(conf, "panel_start", p.start);
	readEdge(conf, "panel_end", p.end);
	if (p.start.kind == PeriodEdge::Kind::Duration)
		p.start = PeriodEdge::atEvent(SunEvent::NauticalDusk);
	p.minAltitude = std::clamp(conf.value("panel_min_altitude", p.minAltitude).toDouble(), -90., 90.);
	p.minDurationHours = std::max(0., conf.value("panel_min_duration", p.minDurationHours).toDouble());
	if (conf.contains("panel_targets"))
		p.targets = conf.value("panel_targets").toStringList();

	// Colours and line weights (CAL-T-08, CAL-S-05, CAL-M-02, CAL-M-03, CAL-M-10)
	for (int k = 0; k < 4; ++k)
		readColor(conf, BAND_KEYS[k], style.bandColor[k]);
	readColor(conf, "color_star_line", style.starLineColor);
	readPositive(conf, "star_line_width", style.starLineWidth);
	readColor(conf, "color_moon_night", style.moonNightColor);
	readColor(conf, "color_moon_day", style.moonDayColor);
	readPositive(conf, "moon_min_width", style.moonMinWidth);
	readPositive(conf, "moon_max_width_factor", style.moonMaxWidthFactor);
	style.moonMaxWidthFactor = std::min(style.moonMaxWidthFactor, 1.);

	// Export (CAL-I-05)
	const QString format = conf.value("export_format").toString();
	for (ExportOptions::Format f : { ExportOptions::Format::Pdf, ExportOptions::Format::Svg, ExportOptions::Format::Png })
		if (format == ExportOptions::formatSuffix(f))
			exportOptions.format = f;
	const QString page = conf.value("export_page_size").toString();
	for (int id = 0; !page.isEmpty() && id <= QPageSize::LastPageSize; ++id)
	{
		const auto pageId = static_cast<QPageSize::PageSizeId>(id);
		if (pageId != QPageSize::Custom && QPageSize::key(pageId) == page)
			exportOptions.pageSize = pageId;
	}
	const QString orientation = conf.value("export_orientation").toString();
	if (orientation == "portrait")
		exportOptions.orientation = QPageLayout::Portrait;
	else if (orientation == "landscape")
		exportOptions.orientation = QPageLayout::Landscape;
	exportOptions.pngDpi = std::clamp(conf.value("export_png_dpi", exportOptions.pngDpi).toInt(), 36, 1200);
	exportOptions.directory = conf.value("export_directory", exportOptions.directory).toString();

	conf.endGroup();
}

void UserSettings::save(QSettings& conf) const
{
	conf.beginGroup(GROUP);

	conf.setValue("follow_stellarium_location", followStellariumLocation);
	if (hasSavedLocation)
	{
		conf.setValue("location_name", chart.location.name);
		conf.setValue("location_latitude", chart.location.latitude);
		conf.setValue("location_longitude", chart.location.longitude);
		conf.setValue("location_elevation", chart.location.elevation);
		conf.setValue("location_time_zone", chart.location.timeZone);
	}

	conf.setValue("current_month", currentMonth);
	if (chart.firstEvening.isValid() && chart.lastEvening.isValid())
	{
		conf.setValue("first_evening", chart.firstEvening.toString(Qt::ISODate));
		conf.setValue("last_evening", chart.lastEvening.toString(Qt::ISODate));
	}

	conf.setValue("window_automatic", chart.autoWindow);
	conf.setValue("window_start", clockText(chart.windowStartClock));
	conf.setValue("window_end", clockText(chart.windowEndClock));

	conf.setValue("stars", chart.stars);

	const PanelSettings& p = chart.panel;
	conf.setValue("panel_enabled", p.enabled);
	conf.setValue("panel_title", p.title);
	conf.setValue("panel_start", edgeText(p.start));
	conf.setValue("panel_end", edgeText(p.end));
	conf.setValue("panel_min_altitude", p.minAltitude);
	conf.setValue("panel_min_duration", p.minDurationHours);
	conf.setValue("panel_targets", p.targets);

	for (int k = 0; k < 4; ++k)
		conf.setValue(BAND_KEYS[k], style.bandColor[k].name());
	conf.setValue("color_star_line", style.starLineColor.name());
	conf.setValue("star_line_width", style.starLineWidth);
	conf.setValue("color_moon_night", style.moonNightColor.name());
	conf.setValue("color_moon_day", style.moonDayColor.name());
	conf.setValue("moon_min_width", style.moonMinWidth);
	conf.setValue("moon_max_width_factor", style.moonMaxWidthFactor);

	conf.setValue("export_format", ExportOptions::formatSuffix(exportOptions.format));
	conf.setValue("export_page_size", QPageSize::key(exportOptions.pageSize));
	conf.setValue("export_orientation", exportOptions.orientation == QPageLayout::Portrait ? "portrait" : "landscape");
	conf.setValue("export_png_dpi", exportOptions.pngDpi);
	conf.setValue("export_directory", exportOptions.directory);

	conf.endGroup();
}

} // namespace calenduh
