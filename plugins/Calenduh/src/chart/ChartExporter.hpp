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

#ifndef CALENDUH_CHARTEXPORTER_HPP
#define CALENDUH_CHARTEXPORTER_HPP

#include "ChartModel.hpp"
#include "ChartStyle.hpp"

#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QSizeF>

namespace calenduh {

//! Writes the chart to PDF, SVG and PNG (CAL-I-05). Sizes are in points.
class ChartExporter
{
public:
	//! A4 landscape in points (the default page, CAL-I-05).
	static QSizeF defaultPageSize() { return QSizeF(842., 595.); }

	static QImage renderImage(const ChartModel& model, const ChartStyle& style,
				  const QSizeF& pagePoints, double dpi);

	static bool writePng(const ChartModel& model, const ChartStyle& style, const QString& path,
			     const QSizeF& pagePoints, double dpi);
	//! True if one image of the page needs more than about 400 MB; writePng() refuses it.
	static bool pngTooLarge(const QSizeF& pagePoints, double dpi);
	static bool writeSvg(const ChartModel& model, const ChartStyle& style, const QString& path,
			     const QSizeF& pagePoints);
	static bool writePdf(const ChartModel& model, const ChartStyle& style, const QString& path,
			     const QPageSize& pageSize = QPageSize(QPageSize::A4),
			     QPageLayout::Orientation orientation = QPageLayout::Landscape);
};

} // namespace calenduh

#endif // CALENDUH_CHARTEXPORTER_HPP
