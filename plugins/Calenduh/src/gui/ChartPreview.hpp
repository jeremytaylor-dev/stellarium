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

#ifndef CALENDUH_CHARTPREVIEW_HPP
#define CALENDUH_CHARTPREVIEW_HPP

#include "ChartModel.hpp"
#include "ChartStyle.hpp"

#include <QImage>
#include <QSizeF>
#include <QTimer>
#include <QWidget>
#include <optional>

//! Shows the chart page scaled to the widget (CAL-I-04). The page is drawn by
//! the same renderer as the exports, through an image at the widget resolution.
//! No Stellarium dependency.
class ChartPreview : public QWidget
{
	Q_OBJECT

public:
	explicit ChartPreview(QWidget* parent = nullptr);

	//! Show a chart on a page of @p pagePoints (points).
	void setChart(const calenduh::ChartModel& model, const calenduh::ChartStyle& style, const QSizeF& pagePoints);
	//! Draw the shown chart again with another style or page (no new calculation).
	void setAppearance(const calenduh::ChartStyle& style, const QSizeF& pagePoints);
	//! The chart shown, or nullptr.
	const calenduh::ChartModel* chartModel() const { return model ? &*model : nullptr; }
	const calenduh::ChartStyle& chartStyle() const { return style; }
	QSizeF pagePoints() const { return page; }

	//! Remove the chart and show @p message instead.
	void clearChart(const QString& message = QString());
	bool hasChart() const { return model.has_value(); }

	QSize minimumSizeHint() const override { return QSize(320, 240); }
	QSize sizeHint() const override { return QSize(842, 595); }

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private:
	std::optional<calenduh::ChartModel> model;
	calenduh::ChartStyle style;
	QSizeF page;
	QString message;
	QImage cache;           //!< rendered page at the current widget size
	QSize cacheKey;         //!< device-pixel size the cache was made for
	QTimer settleTimer;     //!< renders again when a resize has stopped
	QRectF pageRect() const;
};

#endif // CALENDUH_CHARTPREVIEW_HPP
