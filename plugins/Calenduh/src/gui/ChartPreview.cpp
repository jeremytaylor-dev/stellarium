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

#include "ChartPreview.hpp"
#include "ChartExporter.hpp"

#include <QPainter>
#include <algorithm>

namespace {
constexpr double MARGIN = 8.;  // pixels around the page
}

ChartPreview::ChartPreview(QWidget* parent)
	: QWidget(parent)
	, page(calenduh::ChartExporter::defaultPageSize())
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ChartPreview::setChart(const calenduh::ChartModel& m, const calenduh::ChartStyle& s, const QSizeF& pagePoints)
{
	model = m;
	style = s;
	page = pagePoints;
	message.clear();
	cache = QImage();
	update();
}

void ChartPreview::clearChart(const QString& text)
{
	model.reset();
	message = text;
	cache = QImage();
	update();
}

QRectF ChartPreview::pageRect() const
{
	const QSizeF area = QSizeF(width(), height()) - QSizeF(2. * MARGIN, 2. * MARGIN);
	if (area.width() <= 0. || area.height() <= 0. || page.isEmpty())
		return QRectF();
	const double scale = std::min(area.width() / page.width(), area.height() / page.height());
	const QSizeF size = page * scale;
	return QRectF(QPointF((width() - size.width()) / 2., (height() - size.height()) / 2.), size);
}

void ChartPreview::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	const QRectF target = pageRect();
	if (!model || target.isEmpty())
	{
		painter.setPen(palette().color(QPalette::WindowText));
		painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, message);
		return;
	}

	// Render at device pixels; the image keeps the renderer's 72-dpi font metrics.
	const double ratio = devicePixelRatioF();
	const QSize pixels = (target.size() * ratio).toSize();
	if (cache.isNull() || cacheKey != pixels)
	{
		const double dpi = 72. * pixels.width() / page.width();
		cache = calenduh::ChartExporter::renderImage(*model, style, page, dpi);
		cache.setDevicePixelRatio(ratio);
		cacheKey = pixels;
	}
	painter.drawImage(target.topLeft(), cache);
}
