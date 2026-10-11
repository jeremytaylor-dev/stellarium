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

#ifndef CALENDUH_HPP
#define CALENDUH_HPP

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"
#include "UserSettings.hpp"

#include <QObject>

class StelButton;
class CalenduhDialog;

/*! @defgroup calenduh Calenduh plug-in
@{
The %Calenduh plugin draws an observation planning calendar: one row per night,
with twilight bands, the Moon's phase and altitude, and the meridian transits
of selected bright stars.

<b>Configuration</b>

The plug-in's configuration data is stored in Stellarium's main configuration
file (section [Calenduh]).

@}
*/

//! @class Calenduh
//! @ingroup calenduh
//! Main class of the %Calenduh plugin.
class Calenduh : public StelModule
{
	Q_OBJECT

public:
	Calenduh();
	~Calenduh() override;

	void init() override;
	bool configureGui(bool show) override;

	//! The saved user settings (CAL-I-08).
	const calenduh::UserSettings& getUserSettings() const { return userSettings; }
	//! Replace the user settings and save them in config.ini.
	void setUserSettings(const calenduh::UserSettings& settings);

	//! The inputs of the next chart: the user settings with the location and the
	//! dates resolved (CAL-L-06, CAL-L-09, CAL-I-09). Returns false and sets
	//! @p error if no Earth location is available; the user must then select one.
	bool resolveChartSettings(calenduh::ChartSettings& settings, QString* error = nullptr) const;

	//! Stellarium's current location as a chart location. Returns false if the
	//! observer is not on Earth or the time zone cannot be used.
	static bool stellariumLocation(calenduh::ChartLocation& location);

private slots:
	//! Development diagnostic: time StelCore::setJD() + update(0) over many steps
	//! and write the statistics to the log. Runs at start-up only if the
	//! environment variable CALENDUH_BENCHMARK=1. Restores the simulation time.
	void runUpdateBenchmark();

	//! Development diagnostic: run the EphemerisSampler with the settings of a
	//! chart-model fixture and write the result as JSON, for comparison with the
	//! fixture (tools/compare-models.py). Runs at start-up only if the environment
	//! variable CALENDUH_SAMPLE="<fixture.json>;<output.json>" is set; quits
	//! Stellarium afterwards if CALENDUH_SAMPLE_QUIT=1.
	void runSamplerCheck();

private:
	calenduh::UserSettings userSettings;

#ifndef NO_GUI
	CalenduhDialog* dialog;
	StelButton* toolbarButton;
#endif
};


//! This class is used by Qt to manage a plug-in interface
class CalenduhStelPluginInterface : public QObject, public StelPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
	Q_INTERFACES(StelPluginInterface)
public:
	StelModule* getStelModule() const override;
	StelPluginInfo getPluginInfo() const override;
};

#endif /* CALENDUH_HPP */
