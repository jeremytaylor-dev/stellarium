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

#include "ChartModel.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <cmath>

namespace calenduh {

namespace {

constexpr double JD_UNIX_EPOCH = 2440587.5;
constexpr double MS_PER_DAY = 86400000.;

const char* const duskKeys[ThresholdCount] = { "sunset", "civilDusk", "nauticalDusk", "astronomicalDusk" };
const char* const dawnKeys[ThresholdCount] = { "sunrise", "civilDawn", "nauticalDawn", "astronomicalDawn" };

std::optional<double> optionalJd(const QJsonValue& v)
{
	if (v.isDouble())
		return v.toDouble();
	return std::nullopt;
}

QJsonValue jsonValue(const std::optional<double>& v)
{
	return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}

SunExtreme readExtreme(const QJsonObject& o)
{
	SunExtreme e;
	e.jd = o.value("jd").toDouble();
	e.altitude = o.value("alt").toDouble();
	return e;
}

QJsonObject writeExtreme(const SunExtreme& e)
{
	return QJsonObject{ { "jd", e.jd }, { "alt", e.altitude } };
}

} // namespace

double thresholdAltitude(Threshold t)
{
	switch (t)
	{
		case Threshold::Sunset:       return -0.8333;
		case Threshold::Civil:        return -6.;
		case Threshold::Nautical:     return -12.;
		case Threshold::Astronomical: return -18.;
	}
	return 0.;
}

QDateTime jdToDateTime(double jd)
{
	const qint64 ms = static_cast<qint64>(std::llround((jd - JD_UNIX_EPOCH) * MS_PER_DAY));
	return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc());
}

double dateTimeToJd(const QDateTime& dateTime)
{
	return JD_UNIX_EPOCH + static_cast<double>(dateTime.toMSecsSinceEpoch()) / MS_PER_DAY;
}

QTimeZone chartTimeZone(const QString& id)
{
	static const QRegularExpression fixedOffset(QStringLiteral("^UTC([+-])(\\d{2}):(\\d{2})$"));
	const QRegularExpressionMatch m = fixedOffset.match(id);
	if (m.hasMatch())
	{
		const int seconds = m.captured(2).toInt() * 3600 + m.captured(3).toInt() * 60;
		return QTimeZone(m.captured(1) == QLatin1String("-") ? -seconds : seconds);
	}
	return QTimeZone(id.toUtf8());
}

double localClockToJd(const QDate& date, int minutes, const QTimeZone& timeZone)
{
	// Local wall-clock time expressed as if it were UTC, then corrected by the zone offset.
	// The second pass handles a guess that falls on the other side of a transition.
	const qint64 wall = QDateTime(date, QTime(0, 0), QTimeZone::utc()).toMSecsSinceEpoch() + qint64(minutes) * 60000;
	const int guess = timeZone.offsetFromUtc(QDateTime::fromMSecsSinceEpoch(wall, QTimeZone::utc()));
	qint64 utc = wall - qint64(guess) * 1000;
	const int actual = timeZone.offsetFromUtc(QDateTime::fromMSecsSinceEpoch(utc, QTimeZone::utc()));
	if (actual != guess)
		utc = wall - qint64(actual) * 1000;
	return JD_UNIX_EPOCH + static_cast<double>(utc) / MS_PER_DAY;
}

double nightClockToJd(const QDate& evening, int minutes, const QTimeZone& timeZone)
{
	return localClockToJd(minutes < 12 * 60 ? evening.addDays(1) : evening, minutes, timeZone);
}

double localClockMinutes(double jd, const QTimeZone& timeZone)
{
	const QTime t = jdToDateTime(jd).toTimeZone(timeZone).time();
	return t.hour() * 60. + t.minute() + (t.second() + t.msec() / 1000.) / 60.;
}

bool ChartModel::fromJson(const QByteArray& json, QString* errorMessage)
{
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
	if (doc.isNull())
	{
		if (errorMessage)
			*errorMessage = parseError.errorString();
		return false;
	}
	const QJsonObject root = doc.object();
	if (root.value("schema").toInt() != 1)
	{
		if (errorMessage)
			*errorMessage = QStringLiteral("unsupported schema version");
		return false;
	}

	const QJsonObject loc = root.value("location").toObject();
	location.name = loc.value("name").toString();
	location.latitude = loc.value("latitude").toDouble();
	location.longitude = loc.value("longitude").toDouble();
	location.elevation = loc.value("elevation").toDouble();
	location.timeZone = loc.value("timeZone").toString();

	maxMoonAltitude = root.value("maxMoonAltitude").toDouble();
	stars.clear();
	for (const QJsonValue& s : root.value("stars").toArray())
		stars.append(s.toString());

	panel.reset();
	if (root.contains("panel"))
	{
		const QJsonObject p = root.value("panel").toObject();
		TargetPanel tp;
		tp.title = p.value("title").toString();
		tp.summary = p.value("summary").toString();
		for (const QJsonValue& t : p.value("targets").toArray())
			tp.targets.append(t.toString());
		panel = tp;
	}

	rows.clear();
	for (const QJsonValue& rv : root.value("rows").toArray())
	{
		const QJsonObject r = rv.toObject();
		NightRow row;
		row.eveningDate = QDate::fromString(r.value("eveningDate").toString(), Qt::ISODate);
		row.windowStart = r.value("windowStart").toDouble();
		row.windowEnd = r.value("windowEnd").toDouble();

		const QJsonObject events = r.value("events").toObject();
		for (int i = 0; i < ThresholdCount; ++i)
		{
			row.dusk[i] = optionalJd(events.value(duskKeys[i]));
			row.dawn[i] = optionalJd(events.value(dawnKeys[i]));
		}
		row.sunLowest = readExtreme(r.value("sunLowest").toObject());
		row.sunHighest = readExtreme(r.value("sunHighest").toObject());

		const QJsonObject moon = r.value("moon").toObject();
		row.moonIllumination = moon.value("illumination").toDouble();
		row.moonWaning = moon.value("waning").toBool();
		for (const QJsonValue& sv : moon.value("segments").toArray())
		{
			const QJsonObject s = sv.toObject();
			MoonSegment seg;
			seg.rise = optionalJd(s.value("rise"));
			seg.set = optionalJd(s.value("set"));
			for (const QJsonValue& pv : s.value("samples").toArray())
			{
				const QJsonArray p = pv.toArray();
				seg.samples.append(MoonSample{ p.at(0).toDouble(), p.at(1).toDouble() });
			}
			row.moonSegments.append(seg);
		}

		const QJsonObject transits = r.value("transits").toObject();
		for (auto it = transits.constBegin(); it != transits.constEnd(); ++it)
			row.transits.insert(it.key(), it.value().toDouble());

		const QJsonObject panelRow = r.value("panel").toObject();
		row.periodStart = optionalJd(panelRow.value("periodStart"));
		row.periodEnd = optionalJd(panelRow.value("periodEnd"));
		for (const QJsonValue& q : panelRow.value("qualifies").toArray())
			row.targetQualifies.append(q.toBool());

		rows.append(row);
	}

	if (!chartTimeZone(location.timeZone).isValid())
	{
		if (errorMessage)
			*errorMessage = QStringLiteral("unknown time zone: %1").arg(location.timeZone);
		return false;
	}
	return true;
}

QByteArray ChartModel::toJson() const
{
	QJsonObject root;
	root.insert("schema", 1);
	root.insert("location", QJsonObject{
		{ "name", location.name },
		{ "latitude", location.latitude },
		{ "longitude", location.longitude },
		{ "elevation", location.elevation },
		{ "timeZone", location.timeZone } });
	root.insert("maxMoonAltitude", maxMoonAltitude);
	root.insert("stars", QJsonArray::fromStringList(stars));
	if (panel)
		root.insert("panel", QJsonObject{
			{ "title", panel->title },
			{ "summary", panel->summary },
			{ "targets", QJsonArray::fromStringList(panel->targets) } });

	QJsonArray rowArray;
	for (const NightRow& row : rows)
	{
		QJsonObject events;
		for (int i = 0; i < ThresholdCount; ++i)
		{
			events.insert(duskKeys[i], jsonValue(row.dusk[i]));
			events.insert(dawnKeys[i], jsonValue(row.dawn[i]));
		}

		QJsonArray segments;
		for (const MoonSegment& seg : row.moonSegments)
		{
			QJsonArray samples;
			for (const MoonSample& s : seg.samples)
				samples.append(QJsonArray{ s.jd, s.altitude });
			segments.append(QJsonObject{
				{ "rise", jsonValue(seg.rise) },
				{ "set", jsonValue(seg.set) },
				{ "samples", samples } });
		}

		QJsonObject transits;
		for (auto it = row.transits.constBegin(); it != row.transits.constEnd(); ++it)
			transits.insert(it.key(), it.value());

		QJsonArray qualifies;
		for (bool q : row.targetQualifies)
			qualifies.append(q);

		rowArray.append(QJsonObject{
			{ "eveningDate", row.eveningDate.toString(Qt::ISODate) },
			{ "windowStart", row.windowStart },
			{ "windowEnd", row.windowEnd },
			{ "events", events },
			{ "sunLowest", writeExtreme(row.sunLowest) },
			{ "sunHighest", writeExtreme(row.sunHighest) },
			{ "moon", QJsonObject{
				{ "illumination", row.moonIllumination },
				{ "waning", row.moonWaning },
				{ "segments", segments } } },
			{ "transits", transits },
			{ "panel", QJsonObject{
				{ "periodStart", jsonValue(row.periodStart) },
				{ "periodEnd", jsonValue(row.periodEnd) },
				{ "qualifies", qualifies } } } });
	}
	root.insert("rows", rowArray);
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace calenduh
