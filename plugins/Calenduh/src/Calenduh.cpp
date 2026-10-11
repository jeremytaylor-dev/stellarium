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

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocation.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"
#include "Calenduh.hpp"
#ifndef NO_GUI
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "CalenduhDialog.hpp"
#endif

#include "EphemerisSampler.hpp"
#include "ChartSettings.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QSettings>
#include <QTimeZone>
#include <QTimer>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <numeric>

StelModule* CalenduhStelPluginInterface::getStelModule() const
{
	return new Calenduh();
}

StelPluginInfo CalenduhStelPluginInterface::getPluginInfo() const
{
	// Allow to load the resources when used as a static plugin
	Q_INIT_RESOURCE(Calenduh);

	StelPluginInfo info;
	info.id = "Calenduh";
	info.displayedName = N_("Calenduh");
	info.authors = "Jeremy Taylor";
	info.contact = STELLARIUM_URL;
	info.description = N_("Observation planning calendar: one row per night, showing twilight, the Moon's phase and altitude, and the meridian transits of selected bright stars.");
	info.version = CALENDUH_PLUGIN_VERSION;
	info.license = CALENDUH_PLUGIN_LICENSE;
	return info;
}

Calenduh::Calenduh()
#ifndef NO_GUI
	: dialog(new CalenduhDialog())
	, toolbarButton(nullptr)
#endif
{
	setObjectName("Calenduh");
}

Calenduh::~Calenduh()
{
#ifndef NO_GUI
	delete dialog;
#endif
}

void Calenduh::init()
{
	if (QSettings* conf = StelApp::getInstance().getSettings())
		userSettings.load(*conf);

#ifndef NO_GUI
	addAction("actionShow_Calenduh_dialog", N_("Calenduh"), N_("Show observation planning calendar"), dialog, "visible");

	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if (gui)
	{
		toolbarButton = new StelButton(nullptr,
					       QPixmap(":/Calenduh/bt_Calenduh_On.png"),
					       QPixmap(":/Calenduh/bt_Calenduh_Off.png"),
					       QPixmap(":/graphicGui/miscGlow32x32.png"),
					       "actionShow_Calenduh_dialog",
					       false);
		gui->getButtonBar()->addButton(toolbarButton, "065-pluginsGroup");
	}
#endif

	if (qEnvironmentVariableIntValue("CALENDUH_BENCHMARK") == 1)
		QTimer::singleShot(5000, this, &Calenduh::runUpdateBenchmark);
	if (qEnvironmentVariableIsSet("CALENDUH_SAMPLE"))
		QTimer::singleShot(3000, this, &Calenduh::runSamplerCheck);
}

void Calenduh::setUserSettings(const calenduh::UserSettings& settings)
{
	userSettings = settings;
	if (QSettings* conf = StelApp::getInstance().getSettings())
	{
		userSettings.save(*conf);
		conf->sync();
	}
}

bool Calenduh::stellariumLocation(calenduh::ChartLocation& location)
{
	const StelCore* core = StelApp::getInstance().getCore();
	const StelLocation& loc = core->getCurrentLocation();
	if (loc.planetName != "Earth")
		return false;

	// Stellarium's time zone may differ from the location's (user setting).
	QString tz = core->getCurrentTimeZone();
	if (tz.isEmpty())
		tz = loc.ianaTimeZone;
	if (tz == "system_default")
		tz = QString::fromUtf8(QTimeZone::systemTimeZoneId());
	else if (tz == "LMST" || tz == "LTST")
	{
		// Local mean solar time: a fixed offset from the longitude, to the minute.
		const int minutes = static_cast<int>(std::lround(loc.getLongitude(true) * 4.));
		tz = QString("UTC%1%2:%3").arg(minutes < 0 ? '-' : '+')
			.arg(std::abs(minutes) / 60, 2, 10, QChar('0')).arg(std::abs(minutes) % 60, 2, 10, QChar('0'));
	}
	if (!QTimeZone(tz.toUtf8()).isValid())
		return false;

	location.name = loc.name;
	location.latitude = loc.getLatitude(true);
	location.longitude = loc.getLongitude(true);
	location.elevation = loc.altitude;
	location.timeZone = tz;
	return true;
}

bool Calenduh::resolveChartSettings(calenduh::ChartSettings& settings, QString* error) const
{
	settings = userSettings.chart;

	// CAL-L-09, CAL-I-09
	calenduh::ChartLocation current;
	const bool onEarth = stellariumLocation(current);
	if (userSettings.followStellariumLocation && onEarth)
		settings.location = current;
	else if (!userSettings.hasSavedLocation)
	{
		if (error)
			*error = q_("Stellarium's observer is not on Earth. Select a chart location.");
		return false;
	}

	// CAL-L-06: the calendar month of Stellarium's current date (local time).
	if (userSettings.currentMonth || !settings.firstEvening.isValid() || !settings.lastEvening.isValid())
	{
		const StelCore* core = StelApp::getInstance().getCore();
		const double jd = core->getJD();
		int year, month, day;
		StelUtils::getDateFromJulianDay(jd + core->getUTCOffset(jd) / 24., &year, &month, &day);
		settings.firstEvening = QDate(year, month, 1);
		settings.lastEvening = settings.firstEvening.addMonths(1).addDays(-1);
	}
	return true;
}

void Calenduh::runSamplerCheck()
{
	const QStringList paths = qEnvironmentVariable("CALENDUH_SAMPLE").split(';');
	if (paths.size() != 2)
	{
		qWarning() << "[Calenduh] CALENDUH_SAMPLE must be <fixture.json>;<output.json>";
		return;
	}
	QFile in(paths.at(0));
	if (!in.open(QIODevice::ReadOnly))
	{
		qWarning() << "[Calenduh] cannot read" << paths.at(0);
		return;
	}
	const QByteArray json = in.readAll();
	calenduh::ChartModel fixture;
	QString error;
	if (!fixture.fromJson(json, &error) || fixture.rows.isEmpty())
	{
		qWarning() << "[Calenduh] invalid fixture:" << error;
		return;
	}
	const QJsonObject root = QJsonDocument::fromJson(json).object();

	calenduh::ChartSettings settings;
	settings.location = fixture.location;
	settings.firstEvening = fixture.rows.first().eveningDate;
	settings.lastEvening = fixture.rows.last().eveningDate;
	settings.stars = fixture.stars;
	const QTime start = QTime::fromString(root.value("windowStart").toString(), "HH:mm");
	const QTime end = QTime::fromString(root.value("windowEnd").toString(), "HH:mm");
	settings.autoWindow = !(start.isValid() && end.isValid());
	settings.windowStartClock = start.hour() * 60 + start.minute();
	settings.windowEndClock = end.hour() * 60 + end.minute();
	const QJsonObject panelSettings = root.value("panel").toObject().value("settings").toObject();
	if (!panelSettings.isEmpty())
		settings.panel = calenduh::PanelSettings::fromJson(panelSettings).value_or(calenduh::PanelSettings());

	QElapsedTimer timer;
	timer.start();
	calenduh::EphemerisSampler sampler(settings);
	if (!sampler.prepare(&error))
	{
		qWarning() << "[Calenduh] sampler failed:" << error;
		return;
	}
	for (int i = 0; i < sampler.nightCount(); ++i)
		sampler.computeNight(i);
	const qint64 elapsed = timer.elapsed();

	// Also report the automatic window (CAL-L-07) for comparison with the fixture's window.
	calenduh::ChartSettings autoSettings = settings;
	autoSettings.autoWindow = true;
	autoSettings.panel.enabled = false;
	calenduh::EphemerisSampler autoSampler(autoSettings);
	QString autoWindow;
	if (autoSampler.prepare())
	{
		const QTimeZone tz(settings.location.timeZone.toUtf8());
		const auto& r = autoSampler.model().rows.first();
		autoWindow = calenduh::jdToDateTime(r.windowStart).toTimeZone(tz).toString("HH:mm") + "-"
			     + calenduh::jdToDateTime(r.windowEnd).toTimeZone(tz).toString("HH:mm");
	}

	QFile out(paths.at(1));
	if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
		out.write(sampler.model().toJson());
	qInfo().noquote() << QString("[Calenduh] sampler check: %1 nights in %2 ms; automatic window %3; unresolved: %4; written %5")
			     .arg(sampler.nightCount()).arg(elapsed).arg(autoWindow, sampler.unresolvedNames().join(", "), paths.at(1));

	if (qEnvironmentVariableIntValue("CALENDUH_SAMPLE_QUIT") == 1)
		QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void Calenduh::runUpdateBenchmark()
{
	StelCore* core = StelApp::getInstance().getCore();
	// Restore as Planet::getRTSTime() does: the JD and wall-clock time of the last
	// time sync, so that a running clock continues as if it had not been touched.
	const double jd0 = core->getJDOfLastJDUpdate();
	const qint64 millis0 = core->getMilliSecondsOfLastJDUpdate();

	const double step = 5. / 1440.; // 5 minutes, as for the Moon sampling
	const int warmUp = 20;
	const int n = 2000;
	QVector<double> ms;
	ms.reserve(n);
	QElapsedTimer timer;
	for (int i = -warmUp; i < n; ++i)
	{
		timer.start();
		core->setJD(jd0 + (i + warmUp) * step);
		core->update(0.);
		if (i >= 0)
			ms.append(static_cast<double>(timer.nsecsElapsed()) * 1e-6);
	}

	core->setJD(jd0);
	core->setMilliSecondsOfLastJDUpdate(millis0);
	core->update(0.);

	std::sort(ms.begin(), ms.end());
	const double mean = std::accumulate(ms.cbegin(), ms.cend(), 0.) / n;
	const double perMonth = mean * 350. * 31. / 1000.; // architecture §5.6: ~350 updates per night
	qInfo().noquote() << QString("[Calenduh] update benchmark: n=%1 mean=%2 ms median=%3 ms p95=%4 ms min=%5 ms max=%6 ms; estimate for 31 nights x 350 updates: %7 s")
			     .arg(n).arg(mean, 0, 'f', 3).arg(ms.at(n / 2), 0, 'f', 3).arg(ms.at(n * 95 / 100), 0, 'f', 3)
			     .arg(ms.first(), 0, 'f', 3).arg(ms.last(), 0, 'f', 3).arg(perMonth, 0, 'f', 1);
}

bool Calenduh::configureGui(bool show)
{
#ifdef NO_GUI
	Q_UNUSED(show)
	return false;
#else
	if (show)
		dialog->setVisible(true);
	return true;
#endif
}
