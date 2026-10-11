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

#include "ChartSettings.hpp"

#include <QJsonArray>
#include <QTimeZone>
#include <cmath>

namespace calenduh {

namespace {

const char* const eventKeys[SunEventCount] = {
	"sunset", "civilDusk", "nauticalDusk", "astronomicalDusk",
	"astronomicalDawn", "nauticalDawn", "civilDawn", "sunrise" };

const char* const eventNames[SunEventCount] = {
	"sunset", "civil dusk", "nautical dusk", "astronomical dusk",
	"astronomical dawn", "nautical dawn", "civil dawn", "sunrise" };

QString hoursAndMinutes(double hours)
{
	const int minutes = static_cast<int>(std::lround(std::fabs(hours) * 60.));
	return QString("%1:%2").arg(minutes / 60).arg(minutes % 60, 2, 10, QChar('0'));
}

QString clockText(int minutes)
{
	return QString("%1:%2").arg(minutes / 60, 2, 10, QChar('0')).arg(minutes % 60, 2, 10, QChar('0'));
}

std::optional<int> clockFromText(const QString& text)
{
	const QStringList parts = text.split(':');
	bool okH = false, okM = false;
	if (parts.size() != 2)
		return std::nullopt;
	const int h = parts.at(0).toInt(&okH);
	const int m = parts.at(1).toInt(&okM);
	if (!okH || !okM || h < 0 || h > 23 || m < 0 || m > 59)
		return std::nullopt;
	return h * 60 + m;
}

} // namespace

QString sunEventKey(SunEvent e)
{
	return QString::fromLatin1(eventKeys[static_cast<int>(e)]);
}

std::optional<SunEvent> sunEventFromKey(const QString& key)
{
	for (int i = 0; i < SunEventCount; ++i)
		if (key == QLatin1String(eventKeys[i]))
			return static_cast<SunEvent>(i);
	return std::nullopt;
}

QString sunEventName(SunEvent e)
{
	return QString::fromLatin1(eventNames[static_cast<int>(e)]);
}

std::optional<double> sunEventTime(const NightRow& row, SunEvent e)
{
	const int i = static_cast<int>(e);
	// Dusk events are thresholds 0..3 in order; dawn events are thresholds 3..0.
	return i < 4 ? row.dusk[i] : row.dawn[7 - i];
}

PeriodEdge PeriodEdge::atClock(int minutes)
{
	PeriodEdge p;
	p.kind = Kind::Clock;
	p.clockMinutes = minutes;
	return p;
}

PeriodEdge PeriodEdge::atEvent(SunEvent e, double offsetHours)
{
	PeriodEdge p;
	p.kind = Kind::Event;
	p.event = e;
	p.offsetHours = offsetHours;
	return p;
}

PeriodEdge PeriodEdge::afterStart(double hours)
{
	PeriodEdge p;
	p.kind = Kind::Duration;
	p.durationHours = hours;
	return p;
}

QString PeriodEdge::describe() const
{
	switch (kind)
	{
		case Kind::Clock:
			return clockText(clockMinutes);
		case Kind::Duration:
			return QString("start + %1").arg(hoursAndMinutes(durationHours));
		case Kind::Event:
			break;
	}
	QString text = sunEventName(event);
	if (offsetHours != 0.)
		text += QString(" %1 %2").arg(offsetHours > 0. ? "+" : "-", hoursAndMinutes(offsetHours));
	return text;
}

QJsonObject PeriodEdge::toJson() const
{
	switch (kind)
	{
		case Kind::Clock:
			return QJsonObject{ { "clock", clockText(clockMinutes) } };
		case Kind::Duration:
			return QJsonObject{ { "duration", durationHours } };
		case Kind::Event:
			break;
	}
	return QJsonObject{ { "event", sunEventKey(event) }, { "offset", offsetHours } };
}

std::optional<PeriodEdge> PeriodEdge::fromJson(const QJsonObject& o)
{
	if (o.contains("clock"))
	{
		const std::optional<int> m = clockFromText(o.value("clock").toString());
		return m ? std::optional<PeriodEdge>(atClock(*m)) : std::nullopt;
	}
	if (o.contains("duration"))
		return afterStart(o.value("duration").toDouble());
	const std::optional<SunEvent> e = sunEventFromKey(o.value("event").toString());
	if (!e)
		return std::nullopt;
	return atEvent(*e, o.value("offset").toDouble());
}

PanelSettings PanelSettings::visualPreset()
{
	PanelSettings p;
	p.enabled = true;
	p.title = QStringLiteral("Visual");
	p.start = PeriodEdge::atEvent(SunEvent::NauticalDusk);
	p.end = PeriodEdge::afterStart(2.5);
	p.minAltitude = 20.;
	p.minDurationHours = 1.;
	return p;
}

PanelSettings PanelSettings::astrophotographyPreset()
{
	PanelSettings p;
	p.enabled = true;
	p.title = QStringLiteral("Astrophotography");
	p.start = PeriodEdge::atEvent(SunEvent::AstronomicalDusk);
	p.end = PeriodEdge::atEvent(SunEvent::AstronomicalDawn);
	p.minAltitude = 30.;
	p.minDurationHours = 3.;
	return p;
}

QString PanelSettings::summary() const
{
	return QString("%1 to %2, %3+ above %4°")
		.arg(start.describe(), end.describe(), hoursAndMinutes(minDurationHours))
		.arg(minAltitude, 0, 'g', 4);
}

std::optional<std::pair<double, double>> PanelSettings::period(const NightRow& row, const QTimeZone& timeZone) const
{
	auto resolve = [&](const PeriodEdge& edge) -> std::optional<double> {
		if (edge.kind == PeriodEdge::Kind::Clock)
			return nightClockToJd(row.eveningDate, edge.clockMinutes, timeZone);
		const std::optional<double> t = sunEventTime(row, edge.event);
		if (!t)
			return std::nullopt;
		return *t + edge.offsetHours / 24.;
	};

	if (start.kind == PeriodEdge::Kind::Duration)
		return std::nullopt;
	const std::optional<double> s = resolve(start);
	if (!s)
		return std::nullopt;
	const std::optional<double> e = (end.kind == PeriodEdge::Kind::Duration)
		? std::optional<double>(*s + end.durationHours / 24.) : resolve(end);
	if (!e || *e <= *s)
		return std::nullopt;
	return std::make_pair(*s, *e);
}

QJsonObject PanelSettings::toJson() const
{
	return QJsonObject{
		{ "enabled", enabled },
		{ "title", title },
		{ "start", start.toJson() },
		{ "end", end.toJson() },
		{ "minAltitude", minAltitude },
		{ "minDuration", minDurationHours },
		{ "targets", QJsonArray::fromStringList(targets) } };
}

std::optional<PanelSettings> PanelSettings::fromJson(const QJsonObject& o)
{
	PanelSettings p;
	const std::optional<PeriodEdge> s = PeriodEdge::fromJson(o.value("start").toObject());
	const std::optional<PeriodEdge> e = PeriodEdge::fromJson(o.value("end").toObject());
	if (!s || !e)
		return std::nullopt;
	p.enabled = o.value("enabled").toBool(true);
	p.title = o.value("title").toString();
	p.start = *s;
	p.end = *e;
	p.minAltitude = o.value("minAltitude").toDouble();
	p.minDurationHours = o.value("minDuration").toDouble();
	for (const QJsonValue& t : o.value("targets").toArray())
		p.targets.append(t.toString());
	return p;
}

QStringList ChartSettings::defaultStars()
{
	return { "Diphda", "Hamal", "Menkar", "Aldebaran", "Rigel", "Betelgeuse", "Procyon", "Alphard",
		 "Regulus", "Denebola", "Spica", "Arcturus", "Zubenelgenubi", "Antares", "Rasalhague",
		 "Altair", "Enif", "Sadalmelik" };
}

} // namespace calenduh
