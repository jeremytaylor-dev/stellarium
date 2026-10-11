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

#include "CalenduhDialog.hpp"
#include "ui_calenduhDialog.h"
#include "Calenduh.hpp"
#include "ChartJob.hpp"
#include "SettingsEditor.hpp"

#include "StelApp.hpp"
#include "StelModuleMgr.hpp"
#include "StelTranslator.hpp"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QTimer>
#include <memory>

CalenduhDialog::CalenduhDialog()
	: StelDialog("Calenduh")
	, job(new calenduh::ChartJob(this))
{
	ui = new Ui_calenduhDialogForm();
	connect(job, &calenduh::ChartJob::progress, this, &CalenduhDialog::jobProgress);
	connect(job, &calenduh::ChartJob::finished, this, &CalenduhDialog::jobFinished);
	connect(job, &calenduh::ChartJob::cancelled, this, &CalenduhDialog::jobCancelled);
	connect(job, &calenduh::ChartJob::failed, this, &CalenduhDialog::jobFailed);
}

CalenduhDialog::~CalenduhDialog()
{
	delete ui;
}

void CalenduhDialog::retranslate()
{
	if (dialog)
	{
		ui->retranslateUi(dialog);
		editor->retranslate();
		const auto pages = editor->pages();
		for (int i = 0; i < pages.size(); ++i)
			ui->listWidgetPages->item(i + 1)->setText(pages.at(i).first);
		setStatus(statusText);
	}
}

void CalenduhDialog::setVisible(bool visible)
{
	StelDialog::setVisible(visible);
	if (visible && dialog && !generatedOnce)
	{
		generatedOnce = true;
		generate();
	}
}

void CalenduhDialog::createDialogContent()
{
	ui->setupUi(dialog);

	connect(&StelApp::getInstance(), SIGNAL(languageChanged()), this, SLOT(retranslate()));
	connect(ui->titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
	connect(ui->titleBar, SIGNAL(movedTo(QPoint)), this, SLOT(handleMovedTo(QPoint)));

	// Settings pages after the chart page.
	editor = new SettingsEditor(this);
	editor->setSettings(GETSTELMODULE(Calenduh)->getUserSettings());
	for (const auto& page : editor->pages())
	{
		ui->listWidgetPages->addItem(page.first);
		ui->stackedWidget->addWidget(page.second);
	}
	connect(ui->listWidgetPages, &QListWidget::currentRowChanged, ui->stackedWidget, &QStackedWidget::setCurrentIndex);
	ui->listWidgetPages->setCurrentRow(0);
	connect(editor, &SettingsEditor::changed, this, &CalenduhDialog::settingsChanged);

	connect(ui->pushButtonGenerate, &QPushButton::clicked, this, &CalenduhDialog::generate);
	connect(ui->pushButtonCancel, &QPushButton::clicked, job, &calenduh::ChartJob::cancel);
	setRunning(false);
}

void CalenduhDialog::generate()
{
	if (!dialog)
		return;
	Calenduh* plugin = GETSTELMODULE(Calenduh);
	calenduh::ChartSettings settings;
	QString error;
	if (!plugin->resolveChartSettings(settings, &error) || !job->start(settings, &error))
	{
		jobFailed(error);
		return;
	}
	// resolveChartSettings() can save the last Earth location; show it in the editor.
	editor->setSettings(plugin->getUserSettings());
	jobStyle = plugin->getUserSettings().style;
	jobPage = plugin->getUserSettings().exportOptions.pagePoints();
	setStatus([]() { return q_("Calculating..."); });
	setRunning(true);
}

void CalenduhDialog::runDialogCheck(const QString& pngPath, bool quit)
{
	// Connections that end at the first finish or failure (no Qt::SingleShotConnection: Qt 5).
	auto connections = std::make_shared<QList<QMetaObject::Connection>>();
	auto save = [this, pngPath, quit, connections]() {
		for (const QMetaObject::Connection& c : std::as_const(*connections))
			disconnect(c);
		connections->clear();
		// Let the preview paint before the grab.
		QTimer::singleShot(500, this, [this, pngPath, quit]() {
			const bool ok = dialog && dialog->grab().save(pngPath);
			qInfo().noquote() << QString("[Calenduh] dialog check: %1 %2; status: %3")
					     .arg(ok ? "written" : "NOT written", pngPath, ui->labelStatus->text());
			// Also an image of each settings page: <name>-page<N>.png.
			for (int i = 1; dialog && i < ui->listWidgetPages->count(); ++i)
			{
				ui->listWidgetPages->setCurrentRow(i);
				QCoreApplication::processEvents();
				QString pagePath = pngPath;
				pagePath.replace(QRegularExpression("\\.png$"), QString("-page%1.png").arg(i));
				dialog->grab().save(pagePath);
			}
			if (dialog)
				ui->listWidgetPages->setCurrentRow(0);
			if (quit)
				QTimer::singleShot(0, qApp, &QCoreApplication::quit);
		});
	};
	connections->append(connect(job, &calenduh::ChartJob::finished, this, save));
	connections->append(connect(job, &calenduh::ChartJob::failed, this, save));
	setVisible(true);
	if (!job->isRunning())
		save();
}

void CalenduhDialog::jobProgress(int stepsDone, int steps)
{
	ui->progressBar->setMaximum(steps);
	ui->progressBar->setValue(stepsDone);
}

void CalenduhDialog::jobFinished(const calenduh::ChartModel& model, const QStringList& unresolvedNames)
{
	setRunning(false);
	ui->preview->setChart(model, jobStyle, jobPage);
	// CAL-S-07, CAL-G-12: names that Stellarium cannot find are left out and reported.
	shownUnresolved = unresolvedNames;
	setStatus([unresolvedNames]() {
		return unresolvedNames.isEmpty() ? QString()
			: q_("Not found, left out: %1").arg(unresolvedNames.join(", "));
	});
}

void CalenduhDialog::jobCancelled()
{
	setRunning(false);
	if (!ui->preview->hasChart())
	{
		setStatus([]() { return q_("Calculation cancelled."); });
		return;
	}
	const QStringList unresolved = shownUnresolved;
	setStatus([unresolved]() {
		QString text = q_("Calculation cancelled. The chart shown is the previous one.");
		if (!unresolved.isEmpty())
			text += " " + q_("Not found, left out: %1").arg(unresolved.join(", "));
		return text;
	});
}

void CalenduhDialog::jobFailed(const QString& message)
{
	setRunning(false);
	ui->preview->clearChart();
	shownUnresolved.clear();
	// The message is translated by its sender; it is not built again after a language change.
	setStatus([message]() { return message; });
}

void CalenduhDialog::settingsChanged(const calenduh::UserSettings& settings)
{
	GETSTELMODULE(Calenduh)->setUserSettings(settings);
	if (ui->preview->hasChart() && !job->isRunning())
		setStatus([]() { return q_("Settings changed. Generate the chart again to apply them."); });
}

void CalenduhDialog::setStatus(std::function<QString()> text)
{
	statusText = std::move(text);
	ui->labelStatus->setText(statusText ? statusText() : QString());
}

void CalenduhDialog::setRunning(bool running)
{
	ui->pushButtonGenerate->setEnabled(!running);
	ui->pushButtonCancel->setEnabled(running);
	ui->progressBar->setVisible(running);
}
