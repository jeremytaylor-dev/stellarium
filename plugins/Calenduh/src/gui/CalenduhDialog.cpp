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

#include "ChartExporter.hpp"

#include "StelApp.hpp"
#include "StelMainView.hpp"
#include "StelModuleMgr.hpp"
#include "StelTranslator.hpp"

#include <QFileDialog>
#include <QStandardPaths>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <memory>

namespace {

//! True if the two settings give the same chart model (style and export aside).
bool sameChartInputs(const calenduh::UserSettings& a, const calenduh::UserSettings& b)
{
	const calenduh::ChartSettings& x = a.chart;
	const calenduh::ChartSettings& y = b.chart;
	if (a.followStellariumLocation != b.followStellariumLocation || a.currentMonth != b.currentMonth
	    || x.autoWindow != y.autoWindow || x.stars != y.stars || x.panel.toJson() != y.panel.toJson())
		return false;
	if (!a.followStellariumLocation
	    && (x.location.name != y.location.name || x.location.latitude != y.location.latitude
		|| x.location.longitude != y.location.longitude || x.location.elevation != y.location.elevation
		|| x.location.timeZone != y.location.timeZone))
		return false;
	if (!a.currentMonth && (x.firstEvening != y.firstEvening || x.lastEvening != y.lastEvening))
		return false;
	if (!x.autoWindow && (x.windowStartClock != y.windowStartClock || x.windowEndClock != y.windowEndClock))
		return false;
	return true;
}

} // namespace

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
	if (!visible)
		saveSettingsNow();
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
	saveTimer = new QTimer(this);
	saveTimer->setSingleShot(true);
	saveTimer->setInterval(500);
	connect(saveTimer, &QTimer::timeout, this, &CalenduhDialog::saveSettingsNow);

	connect(ui->pushButtonGenerate, &QPushButton::clicked, this, &CalenduhDialog::generate);
	connect(ui->pushButtonCancel, &QPushButton::clicked, job, &calenduh::ChartJob::cancel);
	connect(ui->pushButtonExport, &QPushButton::clicked, this, &CalenduhDialog::exportChart);
	setRunning(false);
}

void CalenduhDialog::generate()
{
	if (!dialog)
		return;
	saveSettingsNow();
	changedDuringJob = false;
	Calenduh* plugin = GETSTELMODULE(Calenduh);
	calenduh::ChartSettings settings;
	QString error;
	if (!plugin->resolveChartSettings(settings, &error))
	{
		jobFailed(error);
		return;
	}
	// resolveChartSettings() can save the last Earth location; show it in the editor,
	// so that a later save of the editor's copy keeps it.
	editor->setSettings(plugin->getUserSettings());
	if (!job->start(settings, &error))
	{
		jobFailed(error);
		return;
	}
	jobSettings = plugin->getUserSettings();
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
				const QFileInfo info(pngPath);
				const QString pagePath = info.dir().filePath(QString("%1-page%2.png").arg(info.completeBaseName()).arg(i));
				dialog->grab().save(pagePath);
			}
			if (dialog)
				ui->listWidgetPages->setCurrentRow(0);
			const QFileInfo info(pngPath);
			const calenduh::ExportOptions options = GETSTELMODULE(Calenduh)->getUserSettings().exportOptions;
			using Format = calenduh::ExportOptions::Format;
			for (Format f : { Format::Pdf, Format::Svg, Format::Png })
			{
				const QString path = info.dir().filePath(QString("%1-export.%2").arg(info.completeBaseName(),
										     calenduh::ExportOptions::formatSuffix(f)));
				qInfo().noquote() << QString("[Calenduh] dialog check export: %1 %2")
						     .arg(writeChart(path, f, options) ? "written" : "NOT written", path);
			}
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
	ui->pushButtonExport->setEnabled(true);
	// CAL-S-07, CAL-G-12: names that Stellarium cannot find are left out and reported.
	shownUnresolved = unresolvedNames;
	const bool outOfDate = changedDuringJob;
	setStatus([unresolvedNames, outOfDate]() {
		QStringList lines;
		if (outOfDate)
			lines.append(q_("Settings changed during the calculation. Generate the chart again to apply them."));
		if (!unresolvedNames.isEmpty())
			lines.append(q_("Not found, left out: %1").arg(unresolvedNames.join(", ")));
		return lines.join(" ");
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
	ui->pushButtonExport->setEnabled(false);
	shownUnresolved.clear();
	// The message is translated by its sender; it is not built again after a language change.
	setStatus([message]() { return message; });
}

void CalenduhDialog::settingsChanged(const calenduh::UserSettings& settings)
{
	savePending = true;
	saveTimer->start();
	if (job->isRunning())
	{
		changedDuringJob = true;
		return;
	}
	if (!ui->preview->hasChart())
		return;
	// Colours, line widths and the page need no new calculation.
	ui->preview->setAppearance(settings.style, settings.exportOptions.pagePoints());
	if (!sameChartInputs(settings, jobSettings))
		setStatus([]() { return q_("Settings changed. Generate the chart again to apply them."); });
}

bool CalenduhDialog::writeChart(const QString& path, calenduh::ExportOptions::Format format,
				const calenduh::ExportOptions& options) const
{
	const calenduh::ChartModel* model = ui->preview->chartModel();
	if (!model)
		return false;
	const calenduh::ChartStyle& style = ui->preview->chartStyle();
	switch (format)
	{
		case calenduh::ExportOptions::Format::Svg:
			return calenduh::ChartExporter::writeSvg(*model, style, path, options.pagePoints());
		case calenduh::ExportOptions::Format::Png:
			return calenduh::ChartExporter::writePng(*model, style, path, options.pagePoints(), options.pngDpi);
		case calenduh::ExportOptions::Format::Pdf:
			break;
	}
	return calenduh::ChartExporter::writePdf(*model, style, path, QPageSize(options.pageSize), options.orientation);
}

void CalenduhDialog::exportChart()
{
	const calenduh::ChartModel* model = ui->preview->chartModel();
	if (!model || model->rows.isEmpty())
	{
		setStatus([]() { return q_("Generate a chart first."); });
		return;
	}
	saveSettingsNow();
	calenduh::UserSettings settings = GETSTELMODULE(Calenduh)->getUserSettings();
	calenduh::ExportOptions& options = settings.exportOptions;

	using Format = calenduh::ExportOptions::Format;
	const QString filters[3] = { q_("PDF document (*.pdf)"), q_("SVG image (*.svg)"), q_("PNG image (*.png)") };
	const Format formats[3] = { Format::Pdf, Format::Svg, Format::Png };
	QString selected = filters[static_cast<int>(options.format)];
	QString directory = options.directory;
	if (directory.isEmpty() || !QDir(directory).exists())
		directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString name = QString("calenduh-%1-%2.%3").arg(model->rows.first().eveningDate.toString(Qt::ISODate),
							     model->rows.last().eveningDate.toString(Qt::ISODate),
							     calenduh::ExportOptions::formatSuffix(options.format));
	QString path = QFileDialog::getSaveFileName(&StelMainView::getInstance(), q_("Export chart"),
						    QDir(directory).filePath(name),
						    QStringList(std::begin(filters), std::end(filters)).join(";;"), &selected);
	if (path.isEmpty())
		return;

	// The suffix selects the format; without a known suffix, the selected filter does.
	Format format = formats[std::max<qsizetype>(0, QStringList(std::begin(filters), std::end(filters)).indexOf(selected))];
	const QString suffix = QFileInfo(path).suffix().toLower();
	bool known = false;
	for (Format f : formats)
		if (suffix == calenduh::ExportOptions::formatSuffix(f))
		{
			format = f;
			known = true;
		}
	if (!known)
		path += "." + calenduh::ExportOptions::formatSuffix(format);

	if (!writeChart(path, format, options))
	{
		setStatus([path]() { return q_("The chart could not be written to %1.").arg(QDir::toNativeSeparators(path)); });
		return;
	}
	setStatus([path]() { return q_("Chart written to %1.").arg(QDir::toNativeSeparators(path)); });

	// Remember the format and the directory (CAL-I-08).
	options.format = format;
	options.directory = QFileInfo(path).absolutePath();
	GETSTELMODULE(Calenduh)->setUserSettings(settings);
	editor->setSettings(settings);
}

void CalenduhDialog::saveSettingsNow()
{
	if (!savePending)
		return;
	savePending = false;
	saveTimer->stop();
	GETSTELMODULE(Calenduh)->setUserSettings(editor->settings());
}

void CalenduhDialog::setStatus(std::function<QString()> text)
{
	statusText = std::move(text);
	ui->labelStatus->setText(statusText ? statusText() : QString());
}

void CalenduhDialog::setRunning(bool running)
{
	ui->pushButtonGenerate->setEnabled(!running);
	ui->pushButtonExport->setEnabled(!running && ui->preview->hasChart());
	ui->pushButtonCancel->setEnabled(running);
	ui->progressBar->setVisible(running);
}
