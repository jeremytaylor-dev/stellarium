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
#include "StelTranslator.hpp"
#include "Calenduh.hpp"
#ifndef NO_GUI
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "CalenduhDialog.hpp"
#endif

#include <QPixmap>
#include <QTimer>
#include <QElapsedTimer>
#include <algorithm>
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
}

void Calenduh::runUpdateBenchmark()
{
	StelCore* core = StelApp::getInstance().getCore();
	const double jd0 = core->getJD();
	const double rate0 = core->getTimeRate();
	const qint64 millis0 = core->getMilliSecondsOfLastJDUpdate();
	core->setTimeRate(0.);

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
	core->setTimeRate(rate0);
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
