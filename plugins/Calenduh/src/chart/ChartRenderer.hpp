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

#ifndef CALENDUH_CHARTRENDERER_HPP
#define CALENDUH_CHARTRENDERER_HPP

#include "ChartModel.hpp"
#include "ChartStyle.hpp"

#include <QFont>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QTimeZone>
#include <QVector>

class QPainter;

namespace calenduh {

//! Draws a ChartModel with QPainter (architecture section 6). No Stellarium dependency.
//! Units: one painter unit is one point, and the paint device must have a logical
//! resolution of 72 dpi, so that font point sizes match (ChartExporter sets this up).
class ChartRenderer
{
public:
	ChartRenderer(const ChartModel& model, const ChartStyle& style);

	//! Draw the whole chart (title, labels, chart area) into @p page.
	void render(QPainter& painter, const QRectF& page);

private:
	//! A point of a row-to-row boundary line, before layout of the DST sections.
	struct RowPoint
	{
		int row;
		double jd;
	};

	const ChartModel& model;
	const ChartStyle& style;
	QTimeZone timeZone;

	// Layout, set by render().
	QRectF chart;         //!< the chart area (rows x time window)
	QRectF panelArea;     //!< the target-panel area (CAL-G-17); empty when the panel is off
	double columnWidth = 0.; //!< target-panel column width
	double rowHeight = 0.;
	double em = 0.;       //!< base font size [pt]
	int windowStartClock = 0;   //!< minutes after local midnight
	int windowMinutes = 0;      //!< clock length of the time window

	void layout(const QRectF& page, const QPainter& painter);
	QFont panelTitleFont(QFont base) const;    //!< target panel title, 1.0 em bold
	QFont panelSummaryFont(QFont base) const;  //!< panel summary and shade note, 0.7 em

	// Coordinates.
	int utcOffsetSeconds(double jd) const;
	double clockMinutesFromWindowStart(int row, double jd) const;
	double xOf(int row, double jd) const;
	double rowCentre(int row) const { return chart.top() + (row + 0.5) * rowHeight; }
	double rowTop(int row) const { return chart.top() + row * rowHeight; }
	double pointsPerMinute() const { return chart.width() / windowMinutes; }

	//! Polyline through the row-centre points of consecutive rows, with the DST
	//! horizontal sections (CAL-L-08) and optional straight extensions by
	//! @p extendStart / @p extendEnd rows at both ends.
	QVector<QPointF> chain(const QVector<RowPoint>& points, double extendStart, double extendEnd,
			       double fallbackMinutesPerDay = 0.) const;

	//! Region where the Sun is below threshold @p t (CAL-T-06, CAL-T-09).
	QPainterPath belowThresholdRegion(Threshold t) const;

	void drawBands(QPainter& painter, const QPainterPath bands[ThresholdCount]);
	void drawGrid(QPainter& painter);
	//! A star line segment and its name, for the label pass.
	struct StarLabel
	{
		QString name;
		QVector<QPointF> line;
	};

	QVector<StarLabel> drawStarLines(QPainter& painter);
	void drawStarLabels(QPainter& painter, const QVector<StarLabel>& labels);
	void drawMoonCurves(QPainter& painter, const QPainterPath& civilRegion);
	void drawPhaseGraphics(QPainter& painter);
	void drawLabels(QPainter& painter);
	void drawPanel(QPainter& painter);

	QString titleText() const;
};

} // namespace calenduh

#endif // CALENDUH_CHARTRENDERER_HPP
