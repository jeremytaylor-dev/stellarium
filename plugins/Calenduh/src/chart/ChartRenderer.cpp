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

#include "ChartRenderer.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>
#include <QPolygonF>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace calenduh {

namespace {

//! Transit times of a star move earlier by (1 solar day - 1 sidereal day) per day.
constexpr double STAR_DRIFT_MINUTES_PER_DAY = -3.9333;
constexpr double SIDEREAL_DAY = 0.99726957; //!< days

enum class RowKind { Crossing, NeverBelow, NeverAbove };

int clockMinutes(const QDateTime& local)
{
	return local.time().hour() * 60 + local.time().minute();
}

QString formatLatitude(double deg)
{
	return QString("%1° %2").arg(std::fabs(deg), 0, 'f', 2).arg(deg < 0. ? "S" : "N");
}

QString formatLongitude(double deg)
{
	return QString("%1° %2").arg(std::fabs(deg), 0, 'f', 2).arg(deg < 0. ? "W" : "E");
}

} // namespace

ChartRenderer::ChartRenderer(const ChartModel& model, const ChartStyle& style)
	: model(model)
	, style(style)
	, timeZone(model.location.timeZone.toUtf8())
{
}

void ChartRenderer::render(QPainter& painter, const QRectF& page)
{
	if (model.rows.isEmpty())
		return;
	layout(page);

	painter.save();
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);

	QPainterPath bands[ThresholdCount];
	for (int t = 0; t < ThresholdCount; ++t)
		bands[t] = belowThresholdRegion(static_cast<Threshold>(t));

	// Paint order: fills behind everything (CAL-T-10), Moon above the star lines (CAL-M-11).
	drawBands(painter, bands);
	drawGrid(painter);
	const QVector<StarLabel> starLabels = drawStarLines(painter);
	drawMoonCurves(painter, bands[static_cast<int>(Threshold::Civil)]);
	drawStarLabels(painter, starLabels);
	drawPhaseGraphics(painter);
	drawLabels(painter);
	if (model.panel)
		drawPanel(painter);

	painter.restore();
}

void ChartRenderer::layout(const QRectF& page)
{
	const int rowCount = model.rows.size();
	const double margin = 0.03 * std::min(page.width(), page.height());
	em = 0.016 * page.height();
	const double titleHeight = 2.4 * em;
	const double hourLabelHeight = 1.6 * em;

	rowHeight = (page.height() - 2. * margin - titleHeight - 2. * hourLabelHeight) / rowCount;
	const double leftReserve = 2.6 * em + 1.2 * rowHeight;
	const double rightReserve = 2.6 * em;
	// CAL-G-17: the panel is directly right of the chart; the chart's morning dates sit between them.
	columnWidth = style.panelColumnFactor * em;
	const double panelWidth = model.panel ? model.panel->targets.size() * columnWidth + em : 0.;
	chart = QRectF(page.left() + margin + leftReserve,
		       page.top() + margin + titleHeight + hourLabelHeight,
		       page.width() - 2. * margin - leftReserve - rightReserve - panelWidth,
		       rowCount * rowHeight);
	panelArea = model.panel ? QRectF(chart.right() + rightReserve, chart.top(), panelWidth, chart.height()) : QRectF();

	const NightRow& first = model.rows.first();
	windowStartClock = clockMinutes(jdToDateTime(first.windowStart).toTimeZone(timeZone));
	const int endClock = clockMinutes(jdToDateTime(first.windowEnd).toTimeZone(timeZone));
	windowMinutes = (endClock - windowStartClock + 1440) % 1440;
	if (windowMinutes == 0)
		windowMinutes = 1440;
}

int ChartRenderer::utcOffsetSeconds(double jd) const
{
	return timeZone.offsetFromUtc(jdToDateTime(jd));
}

double ChartRenderer::clockMinutesFromWindowStart(int row, double jd) const
{
	const double start = model.rows.at(row).windowStart;
	return (jd - start) * 1440. + (utcOffsetSeconds(jd) - utcOffsetSeconds(start)) / 60.;
}

double ChartRenderer::xOf(int row, double jd) const
{
	return chart.left() + clockMinutesFromWindowStart(row, jd) * pointsPerMinute();
}

QVector<QPointF> ChartRenderer::chain(const QVector<RowPoint>& points, double extendStart, double extendEnd,
				      double fallbackMinutesPerDay) const
{
	QVector<QPointF> out;
	if (points.isEmpty())
		return out;

	// Row-to-row step without the DST jump, for the extensions.
	auto stepBetween = [this](const RowPoint& a, const RowPoint& b) {
		const double jump = (utcOffsetSeconds(b.jd) - utcOffsetSeconds(a.jd)) / 60. * pointsPerMinute();
		return QPointF(xOf(b.row, b.jd) - jump - xOf(a.row, a.jd), rowHeight);
	};
	const QPointF fallbackStep(fallbackMinutesPerDay * pointsPerMinute(), rowHeight);

	for (int i = 0; i < points.size(); ++i)
	{
		const RowPoint& p = points.at(i);
		const QPointF here(xOf(p.row, p.jd), rowCentre(p.row));
		out.append(here);
		if (i + 1 < points.size())
		{
			const RowPoint& q = points.at(i + 1);
			const int jumpSeconds = utcOffsetSeconds(q.jd) - utcOffsetSeconds(p.jd);
			if (jumpSeconds != 0)
			{
				// CAL-L-08: close the line across the clock shift with a horizontal
				// section on the row edge between the two rows.
				const QPointF edgeOld = here + 0.5 * stepBetween(p, q);
				out.append(edgeOld);
				out.append(edgeOld + QPointF(jumpSeconds / 60. * pointsPerMinute(), 0.));
			}
		}
	}

	if (extendStart > 0.)
	{
		const QPointF step = points.size() > 1 ? stepBetween(points.at(0), points.at(1)) : fallbackStep;
		out.prepend(out.first() - extendStart * step);
	}
	if (extendEnd > 0.)
	{
		const int n = points.size();
		const QPointF step = n > 1 ? stepBetween(points.at(n - 2), points.at(n - 1)) : fallbackStep;
		out.append(out.last() + extendEnd * step);
	}
	return out;
}

QPainterPath ChartRenderer::belowThresholdRegion(Threshold t) const
{
	const int k = static_cast<int>(t);
	const double level = thresholdAltitude(t);
	const int rowCount = model.rows.size();
	const double far = 4. * chart.width();

	QVector<RowKind> kind(rowCount);
	for (int i = 0; i < rowCount; ++i)
	{
		const NightRow& r = model.rows.at(i);
		if (r.dusk[k] && r.dawn[k])
			kind[i] = RowKind::Crossing;
		else if (r.sunLowest.altitude >= level)
			kind[i] = RowKind::NeverBelow;
		else
			kind[i] = RowKind::NeverAbove;
	}

	// Vertical position between a crossing row c and its neighbour n where the
	// interpolated Sun extreme equals the threshold (CAL-T-09a, CAL-T-09b).
	auto edgeY = [&](int c, int n) {
		const NightRow& rc = model.rows.at(c);
		const NightRow& rn = model.rows.at(n);
		const bool lowest = (kind[n] == RowKind::NeverBelow);
		const double ac = lowest ? rc.sunLowest.altitude : rc.sunHighest.altitude;
		const double an = lowest ? rn.sunLowest.altitude : rn.sunHighest.altitude;
		const double f = (an != ac) ? std::clamp((level - ac) / (an - ac), 0., 1.) : 0.5;
		return rowCentre(c) + (n > c ? 1. : -1.) * f * rowHeight;
	};

	QPainterPath region;
	region.setFillRule(Qt::WindingFill);
	int i = 0;
	while (i < rowCount)
	{
		int j = i;
		while (j < rowCount && kind[j] == kind[i])
			++j;

		if (kind[i] == RowKind::Crossing)
		{
			QVector<RowPoint> dusks, dawns;
			for (int r = i; r < j; ++r)
			{
				dusks.append({ r, *model.rows.at(r).dusk[k] });
				dawns.append({ r, *model.rows.at(r).dawn[k] });
			}
			const double extTop = (i == 0) ? 0.5 : 0.;
			const double extBottom = (j == rowCount) ? 0.5 : 0.;
			const QVector<QPointF> left = chain(dusks, extTop, extBottom);
			const QVector<QPointF> right = chain(dawns, extTop, extBottom);

			QPolygonF poly;
			if (i > 0)
			{
				const double y = edgeY(i, i - 1);
				if (kind[i - 1] == RowKind::NeverBelow)
					poly << QPointF(xOf(i, model.rows.at(i).sunLowest.jd), y);
				else
					poly << QPointF(chart.right() + far, y) << QPointF(chart.left() - far, y);
			}
			for (const QPointF& p : left)
				poly << p;
			if (j < rowCount)
			{
				const double y = edgeY(j - 1, j);
				if (kind[j] == RowKind::NeverBelow)
					poly << QPointF(xOf(j - 1, model.rows.at(j - 1).sunLowest.jd), y);
				else
					poly << QPointF(chart.left() - far, y) << QPointF(chart.right() + far, y);
			}
			for (auto it = right.crbegin(); it != right.crend(); ++it)
				poly << *it;
			region.addPolygon(poly);
			region.closeSubpath();
		}
		else if (kind[i] == RowKind::NeverAbove)
		{
			const double top = (i == 0) ? chart.top() - far
				: (kind[i - 1] == RowKind::Crossing ? edgeY(i - 1, i) : rowTop(i));
			const double bottom = (j == rowCount) ? chart.bottom() + far
				: (kind[j] == RowKind::Crossing ? edgeY(j, j - 1) : rowTop(j));
			region.addRect(QRectF(QPointF(chart.left() - far, top), QPointF(chart.right() + far, bottom)));
		}
		i = j;
	}
	return region;
}

void ChartRenderer::drawBands(QPainter& painter, const QPainterPath bands[ThresholdCount])
{
	painter.save();
	painter.fillRect(chart, style.daylight);
	painter.setClipRect(chart);
	painter.setPen(Qt::NoPen);
	for (int t = 0; t < ThresholdCount; ++t)
		painter.fillPath(bands[t], style.bandColor[t]);
	painter.restore();
}

void ChartRenderer::drawGrid(QPainter& painter)
{
	painter.save();
	painter.setPen(QPen(style.gridColor, style.gridWidth));
	const int firstHour = (windowStartClock + 59) / 60 * 60;
	for (int m = firstHour; m <= windowStartClock + windowMinutes; m += 60)
	{
		const double x = chart.left() + (m - windowStartClock) * pointsPerMinute();
		painter.drawLine(QPointF(x, chart.top()), QPointF(x, chart.bottom()));
	}
	for (int r = 0; r <= model.rows.size(); ++r)
		painter.drawLine(QPointF(chart.left(), rowTop(r)), QPointF(chart.right(), rowTop(r)));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(chart);
	painter.restore();
}

QVector<ChartRenderer::StarLabel> ChartRenderer::drawStarLines(QPainter& painter)
{
	QVector<StarLabel> labels;
	painter.save();
	painter.setClipRect(chart);
	painter.setPen(QPen(style.starLineColor, style.starLineWidth));

	const int rowCount = model.rows.size();
	for (const QString& star : model.stars)
	{
		int r = 0;
		while (r < rowCount)
		{
			if (!model.rows.at(r).transits.contains(star))
			{
				++r;
				continue;
			}
			// A run ends where the star has no transit, or where its transit jumps to
			// the other end of a window of about 24 hours (one sidereal day apart).
			QVector<RowPoint> points;
			while (r < rowCount && model.rows.at(r).transits.contains(star))
			{
				const double jd = model.rows.at(r).transits.value(star);
				if (!points.isEmpty() && std::abs(jd - points.last().jd - SIDEREAL_DAY) > 0.25)
					break;
				points.append({ r, jd });
				++r;
			}
			// Extend half a row to the chart edges (CAL-S-04); one row at a break,
			// so that the line reaches the window edge before clipping.
			const double extStart = (points.first().row == 0) ? 0.5 : 1.;
			const double extEnd = (points.last().row == rowCount - 1) ? 0.5 : 1.;
			const QVector<QPointF> line = chain(points, extStart, extEnd, STAR_DRIFT_MINUTES_PER_DAY);
			painter.drawPolyline(line.constData(), line.size());
			labels.append({ star, line });
		}
	}
	painter.restore();
	return labels;
}

void ChartRenderer::drawStarLabels(QPainter& painter, const QVector<StarLabel>& labels)
{
	painter.save();
	painter.setClipRect(chart);
	painter.setPen(style.starLineColor);
	QFont font = painter.font();
	font.setPointSizeF(std::min(0.8 * em, 0.9 * rowHeight));
	font.setItalic(true);
	painter.setFont(font);
	const QFontMetricsF fm(font);

	// CAL-S-06: one label per segment, near its top, on the left side of the line.
	// The text reads upwards (first letter lowest), with its baseline along the line.
	for (const StarLabel& label : labels)
	{
		const QPointF top = label.line.at(1);
		const QPointF next = label.line.size() > 2 ? label.line.at(2) : top + QPointF(0., rowHeight);
		QPointF down = next - top;
		down /= std::hypot(down.x(), down.y());
		const QPointF anchor = top + (0.3 * rowHeight + fm.horizontalAdvance(label.name)) * down;
		painter.save();
		painter.translate(anchor);
		painter.rotate(qRadiansToDegrees(std::atan2(-down.y(), -down.x())));
		painter.drawText(QPointF(0., -0.25 * em), label.name);
		painter.restore();
	}
	painter.restore();
}

void ChartRenderer::drawMoonCurves(QPainter& painter, const QPainterPath& civilRegion)
{
	const double maxAltitude = model.maxMoonAltitude > 0. ? model.maxMoonAltitude : 90.;

	struct Curve
	{
		QPainterPath path;
		double width;
	};
	QVector<Curve> curves;

	for (int row = 0; row < model.rows.size(); ++row)
	{
		const NightRow& r = model.rows.at(row);
		// CAL-M-03: width linear from the minimum width to the full-Moon width.
		const double maxWidth = style.moonMaxWidthFactor * rowHeight;
		const double w = style.moonMinWidth + (maxWidth - style.moonMinWidth) * r.moonIllumination;
		const double bottom = rowTop(row) + rowHeight;
		// CAL-M-05: the stroke touches the row bottom at 0 degrees and the row top at the maximum.
		auto yOf = [&](double altitude) {
			return bottom - 0.5 * w - altitude / maxAltitude * (rowHeight - w);
		};

		for (const MoonSegment& seg : r.moonSegments)
		{
			if (seg.samples.size() < 2)
				continue;
			QPainterPath path;
			path.moveTo(xOf(row, seg.samples.first().jd), yOf(seg.samples.first().altitude));
			for (int i = 1; i < seg.samples.size(); ++i)
			{
				const MoonSample& a = seg.samples.at(i - 1);
				const MoonSample& b = seg.samples.at(i);
				const int jumpSeconds = utcOffsetSeconds(b.jd) - utcOffsetSeconds(a.jd);
				if (jumpSeconds != 0)
				{
					// CAL-L-08a: horizontal section at the Moon altitude at the moment of the change.
					const QTimeZone::OffsetData change = timeZone.nextTransition(jdToDateTime(a.jd));
					const double jdChange = dateTimeToJd(change.atUtc);
					const double f = (jdChange - a.jd) / (b.jd - a.jd);
					const double y = yOf(a.altitude + f * (b.altitude - a.altitude));
					const double xNew = xOf(row, jdChange);
					path.lineTo(xNew - jumpSeconds / 60. * pointsPerMinute(), y);
					path.lineTo(xNew, y);
				}
				path.lineTo(xOf(row, b.jd), yOf(b.altitude));
			}
			curves.append({ path, w });
		}
	}

	// CAL-M-10: darker yellow where the Sun is above -6 degrees, pale yellow elsewhere.
	QPainterPath chartPath;
	chartPath.addRect(chart);
	const QPainterPath nightClip = chartPath.intersected(civilRegion);
	const QPainterPath dayClip = chartPath.subtracted(civilRegion);

	painter.save();
	painter.setBrush(Qt::NoBrush);
	for (int pass = 0; pass < 2; ++pass)
	{
		painter.setClipPath(pass == 0 ? dayClip : nightClip);
		const QColor color = (pass == 0) ? style.moonDayColor : style.moonNightColor;
		for (const Curve& c : curves)
		{
			painter.setPen(QPen(color, c.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			painter.drawPath(c.path);
		}
	}
	painter.restore();
}

void ChartRenderer::drawPhaseGraphics(QPainter& painter)
{
	painter.save();
	const double radius = 0.4 * rowHeight;
	const bool northern = model.location.latitude >= 0.;
	for (int row = 0; row < model.rows.size(); ++row)
	{
		const NightRow& r = model.rows.at(row);
		const QPointF c(chart.left() - 0.6 * rowHeight, rowCentre(row));
		const QRectF disc(c.x() - radius, c.y() - radius, 2. * radius, 2. * radius);
		// CAL-P-05: hemisphere view.
		const bool litRight = northern ? !r.moonWaning : r.moonWaning;
		const double k = std::clamp(r.moonIllumination, 0., 1.);

		painter.setPen(Qt::NoPen);
		painter.setBrush(Qt::black);
		painter.drawEllipse(disc);

		QPainterPath half;
		half.moveTo(c);
		half.arcTo(disc, litRight ? -90. : 90., 180.);
		half.closeSubpath();
		painter.setBrush(Qt::white);
		painter.drawPath(half);

		// CAL-P-03: elliptical terminator; the white area equals the illuminated fraction.
		const double a = std::fabs(1. - 2. * k) * radius;
		painter.setBrush(k > 0.5 ? Qt::white : Qt::black);
		painter.drawEllipse(c, a, radius);

		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(style.phaseOutline, 0.4));
		painter.drawEllipse(disc);
	}
	painter.restore();
}

void ChartRenderer::drawLabels(QPainter& painter)
{
	painter.save();
	painter.setPen(style.textColor);

	QFont titleFont = painter.font();
	titleFont.setPointSizeF(1.3 * em);
	titleFont.setBold(true);
	painter.setFont(titleFont);
	const QRectF titleRect(chart.left(), chart.top() - 1.6 * em - 2.4 * em, chart.width(), 2.4 * em);
	painter.drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter, titleText());

	QFont font = painter.font();
	font.setBold(false);
	font.setPointSizeF(0.85 * em);
	painter.setFont(font);
	const QFontMetricsF fm(font);

	// CAL-L-03: hour labels at the top and the bottom.
	const int firstHour = (windowStartClock + 59) / 60 * 60;
	for (int m = firstHour; m <= windowStartClock + windowMinutes; m += 60)
	{
		const double x = chart.left() + (m - windowStartClock) * pointsPerMinute();
		const QString text = QString("%1").arg((m / 60) % 24, 2, 10, QChar('0'));
		const double half = 0.5 * fm.horizontalAdvance(text);
		painter.drawText(QPointF(x - half, chart.top() - 0.45 * em), text);
		painter.drawText(QPointF(x - half, chart.bottom() + 0.45 * em + fm.ascent()), text);
	}

	// CAL-L-04: evening date at the left, morning date at the right.
	QFont dateFont = font;
	dateFont.setPointSizeF(std::min(0.85 * em, 0.75 * rowHeight));
	painter.setFont(dateFont);
	const QFontMetricsF dfm(dateFont);
	for (int row = 0; row < model.rows.size(); ++row)
	{
		const QDate evening = model.rows.at(row).eveningDate;
		const double baseline = rowCentre(row) + 0.5 * (dfm.ascent() - dfm.descent());
		const QString left = QString::number(evening.day());
		painter.drawText(QPointF(chart.left() - 1.2 * rowHeight - 0.3 * em - dfm.horizontalAdvance(left), baseline), left);
		painter.drawText(QPointF(chart.right() + 0.4 * em, baseline), QString::number(evening.addDays(1).day()));
	}
	painter.restore();
}

void ChartRenderer::drawPanel(QPainter& painter)
{
	const TargetPanel& panel = *model.panel;
	const int rowCount = model.rows.size();
	painter.save();

	// Rows: separators, and shading where the observing period does not exist (CAL-G-16).
	bool anyShaded = false;
	for (int r = 0; r < rowCount; ++r)
	{
		if (!model.rows.at(r).hasPeriod())
		{
			painter.fillRect(QRectF(panelArea.left(), rowTop(r), panelArea.width(), rowHeight), style.panelShadeColor);
			anyShaded = true;
		}
	}
	painter.setPen(QPen(style.panelRowColor, style.gridWidth));
	for (int r = 1; r < rowCount; ++r)
		painter.drawLine(QPointF(panelArea.left(), rowTop(r)), QPointF(panelArea.right(), rowTop(r)));
	painter.setPen(QPen(style.gridColor, style.gridWidth));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(panelArea);

	// One grey line per span of qualifying nights (CAL-G-05, CAL-G-06).
	struct Span
	{
		int target, first, last;
	};
	QVector<Span> spans;
	for (int k = 0; k < panel.targets.size(); ++k)
	{
		int r = 0;
		auto qualifies = [&](int row) {
			const QVector<bool>& q = model.rows.at(row).targetQualifies;
			return k < q.size() && q.at(k);
		};
		while (r < rowCount)
		{
			if (!qualifies(r))
			{
				++r;
				continue;
			}
			const int first = r;
			while (r < rowCount && qualifies(r))
				++r;
			spans.append({ k, first, r - 1 });
		}
	}
	auto columnX = [&](int k) { return panelArea.left() + 0.5 * em + (k + 0.5) * columnWidth; };
	painter.setPen(QPen(style.panelLineColor, style.panelLineWidth, Qt::SolidLine, Qt::FlatCap));
	for (const Span& s : spans)
	{
		const double x = columnX(s.target);
		painter.drawLine(QPointF(x, rowTop(s.first) + 0.15 * rowHeight),
				 QPointF(x, rowTop(s.last + 1) - 0.15 * rowHeight));
	}

	// Labels: black, overlaid on the line, reading upwards, near the top of each span,
	// not cut off at the end of a short span (CAL-G-13).
	QFont font = painter.font();
	font.setPointSizeF(0.8 * em);
	painter.setFont(font);
	const QFontMetricsF fm(font);
	painter.setPen(style.panelTextColor);
	for (const Span& s : spans)
	{
		const QString& name = panel.targets.at(s.target);
		const QPointF anchor(columnX(s.target), rowTop(s.first) + 0.15 * rowHeight + 0.2 * em + fm.horizontalAdvance(name));
		painter.save();
		painter.translate(anchor);
		painter.rotate(-90.);
		painter.drawText(QPointF(0., 0.5 * (fm.ascent() - fm.descent())), name);
		painter.restore();
	}

	// Heading level with the chart title (CAL-G-11, CAL-G-17).
	QFont titleFont = font;
	titleFont.setPointSizeF(1.0 * em);
	titleFont.setBold(true);
	painter.setFont(titleFont);
	painter.setPen(style.textColor);
	const double headingTop = chart.top() - 1.6 * em - 2.4 * em;
	painter.drawText(QPointF(panelArea.left(), headingTop + 1.1 * em), panel.title);
	QFont summaryFont = font;
	summaryFont.setPointSizeF(0.7 * em);
	painter.setFont(summaryFont);
	painter.drawText(QPointF(panelArea.left(), headingTop + 2.1 * em), panel.summary);

	if (anyShaded)
		painter.drawText(QPointF(panelArea.left(), chart.bottom() + 0.45 * em + QFontMetricsF(summaryFont).ascent()),
				 style.panelShadeNote);
	painter.restore();
}

QString ChartRenderer::titleText() const
{
	// CAL-L-05: date range, location name, latitude and longitude.
	const QLocale english(QLocale::English);
	const QDate first = model.rows.first().eveningDate;
	const QDate last = model.rows.last().eveningDate;
	QString range;
	if (first.year() == last.year() && first.month() == last.month())
		range = QString("%1–%2 %3").arg(first.day()).arg(last.day()).arg(english.toString(last, "MMMM yyyy"));
	else if (first.year() == last.year())
		range = QString("%1 – %2").arg(english.toString(first, "d MMMM"), english.toString(last, "d MMMM yyyy"));
	else
		range = QString("%1 – %2").arg(english.toString(first, "d MMMM yyyy"), english.toString(last, "d MMMM yyyy"));

	const QString coordinates = QString("%1, %2").arg(formatLatitude(model.location.latitude),
							  formatLongitude(model.location.longitude));
	// A location without a name (for example a position from the network) shows the coordinates only.
	if (model.location.name.trimmed().isEmpty())
		return QString("%1, %2, %3").arg(style.title, range, coordinates);
	return QString("%1, %2, %3 (%4)").arg(style.title, range, model.location.name, coordinates);
}

} // namespace calenduh
