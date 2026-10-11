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

#ifndef CALENDUHDIALOG_HPP
#define CALENDUHDIALOG_HPP

#include "StelDialog.hpp"
#include "ChartModel.hpp"
#include "ChartStyle.hpp"

#include <QSizeF>
#include <functional>

class Ui_calenduhDialogForm;
class SettingsEditor;
namespace calenduh { class ChartJob; struct UserSettings; }

//! Main window of the Calenduh plug-in: chart generation and preview.
//! @ingroup calenduh
class CalenduhDialog : public StelDialog
{
	Q_OBJECT

public:
	CalenduhDialog();
	~CalenduhDialog() override;

public slots:
	void retranslate() override;
	//! Shows the window; generates the chart the first time (CAL-I-03).
	void setVisible(bool visible) override;
	//! Calculate the chart for the current settings.
	void generate();
	//! Development check: show the window, wait for the chart, save an image of
	//! the window to @p pngPath, and quit Stellarium if @p quit.
	void runDialogCheck(const QString& pngPath, bool quit);

protected:
	void createDialogContent() override;

private slots:
	void jobProgress(int stepsDone, int steps);
	void jobFinished(const calenduh::ChartModel& model, const QStringList& unresolvedNames);
	void jobCancelled();
	void jobFailed(const QString& message);
	void settingsChanged(const calenduh::UserSettings& settings);

private:
	Ui_calenduhDialogForm* ui;
	calenduh::ChartJob* job;
	SettingsEditor* editor = nullptr;
	bool generatedOnce = false;       //!< the first opening has started a chart
	calenduh::ChartStyle jobStyle;    //!< style and page in force when the job started
	QSizeF jobPage;
	std::function<QString()> statusText;  //!< status line, built again after a language change
	QStringList shownUnresolved;      //!< names left out of the chart shown (CAL-S-07, CAL-G-12)

	void setRunning(bool running);
	void setStatus(std::function<QString()> text);
};

#endif /* CALENDUHDIALOG_HPP */
