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
#include "StelTranslator.hpp"
#include "Calenduh.hpp"
#ifndef NO_GUI
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "CalenduhDialog.hpp"
#endif

#include <QPixmap>

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
