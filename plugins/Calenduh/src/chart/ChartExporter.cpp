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

#include "ChartExporter.hpp"
#include "ChartRenderer.hpp"

#include <QPainter>
#include <QPdfWriter>
#include <QSvgGenerator>

namespace Calenduh {

namespace {
constexpr int DOTS_PER_METRE_72DPI = 2835; // ChartRenderer works at 72 dpi (1 unit = 1 point)
}

QImage ChartExporter::renderImage(const ChartModel& model, const ChartStyle& style,
				  const QSizeF& pagePoints, double dpi)
{
	const double scale = dpi / 72.;
	QImage image(qRound(pagePoints.width() * scale), qRound(pagePoints.height() * scale),
		     QImage::Format_ARGB32_Premultiplied);
	image.setDotsPerMeterX(DOTS_PER_METRE_72DPI);
	image.setDotsPerMeterY(DOTS_PER_METRE_72DPI);
	image.fill(Qt::white);

	QPainter painter(&image);
	painter.scale(scale, scale);
	ChartRenderer(model, style).render(painter, QRectF(QPointF(0., 0.), pagePoints));
	painter.end();
	return image;
}

bool ChartExporter::writePng(const ChartModel& model, const ChartStyle& style, const QString& path,
			     const QSizeF& pagePoints, double dpi)
{
	QImage image = renderImage(model, style, pagePoints, dpi);
	const int dpm = qRound(dpi / 0.0254);
	image.setDotsPerMeterX(dpm);
	image.setDotsPerMeterY(dpm);
	return image.save(path, "PNG");
}

bool ChartExporter::writeSvg(const ChartModel& model, const ChartStyle& style, const QString& path,
			     const QSizeF& pagePoints)
{
	QSvgGenerator generator;
	generator.setFileName(path);
	generator.setResolution(72);
	generator.setSize(pagePoints.toSize());
	generator.setViewBox(QRectF(QPointF(0., 0.), pagePoints));
	generator.setTitle(style.title);
	generator.setDescription(QStringLiteral("Calenduh observation planning calendar"));

	QPainter painter;
	if (!painter.begin(&generator))
		return false;
	ChartRenderer(model, style).render(painter, QRectF(QPointF(0., 0.), pagePoints));
	return painter.end();
}

bool ChartExporter::writePdf(const ChartModel& model, const ChartStyle& style, const QString& path,
			     const QPageSize& pageSize, QPageLayout::Orientation orientation)
{
	QPdfWriter writer(path);
	writer.setResolution(72);
	writer.setPageLayout(QPageLayout(pageSize, orientation, QMarginsF(0., 0., 0., 0.), QPageLayout::Point));
	writer.setTitle(style.title);
	writer.setCreator(QStringLiteral("Calenduh plug-in for Stellarium"));

	QPainter painter;
	if (!painter.begin(&writer))
		return false;
	ChartRenderer(model, style).render(painter, QRectF(0., 0., writer.width(), writer.height()));
	return painter.end();
}

} // namespace Calenduh
