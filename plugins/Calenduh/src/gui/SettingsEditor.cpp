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

#include "SettingsEditor.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocationMgr.hpp"
#include "StelMainView.hpp"
#include "StelModuleMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimeEdit>
#include <QTimeZone>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace calenduh;

namespace {

constexpr int CUSTOM_PRESET = 2;
constexpr int MANY_TARGETS = 20;   // CAL-G-14
const QColor NOT_FOUND_COLOR(220, 60, 60);

int minutesOf(const QTime& t) { return t.hour() * 60 + t.minute(); }
QTime timeOf(int minutes) { minutes = ((minutes % 1440) + 1440) % 1440; return QTime(minutes / 60, minutes % 60); }
double hoursOf(const QTime& t) { return t.hour() + t.minute() / 60.; }
QTime timeOfHours(double hours) { return timeOf(static_cast<int>(std::lround(hours * 60.))); }

QStringList listItems(const QListWidget* list)
{
	QStringList items;
	for (int i = 0; i < list->count(); ++i)
		items.append(list->item(i)->text());
	return items;
}

void setListItems(QListWidget* list, const QStringList& items)
{
	list->clear();
	list->addItems(items);
}

//! The page widget inside a scroll area, so that a small window still shows all fields.
QWidget* scrolled(QWidget* page)
{
	auto* area = new QScrollArea();
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	area->setWidget(page);
	return area;
}

QLabel* statusLabel()
{
	auto* l = new QLabel();
	l->setWordWrap(true);
	return l;
}

} // namespace

SettingsEditor::SettingsEditor(QObject* parent)
	: QObject(parent)
{
}

template <class W> W* SettingsEditor::text(W* widget, const char* source)
{
	texts.append([widget, source]() { widget->setText(q_(source)); });
	widget->setText(q_(source));
	return widget;
}

QLabel* SettingsEditor::label(const char* source)
{
	return text(new QLabel(), source);
}

QGroupBox* SettingsEditor::group(const char* title)
{
	auto* box = new QGroupBox();
	texts.append([box, title]() { box->setTitle(q_(title)); });
	box->setTitle(q_(title));
	return box;
}

QDate SettingsEditor::stellariumDate()
{
	const StelCore* core = StelApp::getInstance().getCore();
	const double jd = core->getJD();
	int year, month, day;
	StelUtils::getDateFromJulianDay(jd + core->getUTCOffset(jd) / 24., &year, &month, &day);
	return QDate(year, month, day);
}

QList<QPair<QString, QWidget*>> SettingsEditor::pages()
{
	if (pageList.isEmpty())
	{
		pageTitles = { N_("Location and dates"), N_("Stars"), N_("Target panel"), N_("Appearance") };
		// No change handling while the widgets are made: store() needs all of them.
		loading = true;
		const QList<QWidget*> widgets = { buildLocationPage(), buildStarsPage(), buildPanelPage(), buildAppearancePage() };
		for (int i = 0; i < widgets.size(); ++i)
			pageList.append({ q_(pageTitles.at(i)), scrolled(widgets.at(i)) });
		load();
	}
	return pageList;
}

void SettingsEditor::retranslate()
{
	for (const auto& set : std::as_const(texts))
		set();
	const bool wasLoading = loading;
	loading = true;
	fillKindCombos();
	fillEventCombos();
	fillPresetCombo();
	loading = wasLoading;
	for (int i = 0; i < pageList.size(); ++i)
		pageList[i].first = q_(pageTitles.at(i));
	if (!pageList.isEmpty())
		load();
}

void SettingsEditor::setSettings(const UserSettings& settings)
{
	current = settings;
	if (!pageList.isEmpty())
		load();
}

// ---------------------------------------------------------------- pages

QWidget* SettingsEditor::buildLocationPage()
{
	auto* page = new QWidget();
	auto* layout = new QVBoxLayout(page);

	// CAL-L-09, CAL-I-09
	auto* locationBox = group(N_("Location"));
	auto* locationForm = new QFormLayout(locationBox);
	followLocation = text(new QCheckBox(), N_("Use Stellarium's location"));
	locationForm->addRow(followLocation);
	locationSearch = new QLineEdit();
	texts.append([this]() { locationSearch->setPlaceholderText(q_("Type a place name from the Stellarium list")); });
	texts.last()();
	{
		const LocationMap all = StelApp::getInstance().getLocationMgr().getAllMap();
		QStringList ids;
		for (auto it = all.cbegin(); it != all.cend(); ++it)
			if (it.value().planetName == "Earth")
				ids.append(it.key());
		auto* completer = new QCompleter(ids, locationSearch);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		completer->setFilterMode(Qt::MatchContains);
		locationSearch->setCompleter(completer);
		connect(completer, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString& id) {
			const StelLocation loc = StelApp::getInstance().getLocationMgr().locationForString(id);
			if (!loc.isValid())
				return;
			loading = true;
			locationName->setText(loc.name);
			latitude->setValue(loc.getLatitude(true));
			longitude->setValue(loc.getLongitude(true));
			elevation->setValue(loc.altitude);
			timeZone->setCurrentText(loc.ianaTimeZone);
			followLocation->setChecked(false);
			loading = false;
			store();
		});
	}
	locationForm->addRow(label(N_("Find")), locationSearch);
	locationName = new QLineEdit();
	locationForm->addRow(label(N_("Name")), locationName);
	latitude = new QDoubleSpinBox();
	latitude->setRange(-90., 90.);
	latitude->setDecimals(4);
	latitude->setSuffix("°");
	locationForm->addRow(label(N_("Latitude (north positive)")), latitude);
	longitude = new QDoubleSpinBox();
	longitude->setRange(-180., 180.);
	longitude->setDecimals(4);
	longitude->setSuffix("°");
	locationForm->addRow(label(N_("Longitude (east positive)")), longitude);
	elevation = new QDoubleSpinBox();
	elevation->setRange(-500., 9000.);
	elevation->setDecimals(0);
	elevation->setSuffix(" m");
	locationForm->addRow(label(N_("Elevation")), elevation);
	timeZone = new QComboBox();
	timeZone->setEditable(true);
	timeZone->setInsertPolicy(QComboBox::NoInsert);
	{
		QStringList zones;
		for (const QByteArray& id : QTimeZone::availableTimeZoneIds())
			zones.append(QString::fromUtf8(id));
		timeZone->addItems(zones);
	}
	locationForm->addRow(label(N_("Time zone")), timeZone);
	locationStatus = statusLabel();
	locationForm->addRow(locationStatus);
	layout->addWidget(locationBox);

	// CAL-L-06
	auto* datesBox = group(N_("Dates"));
	auto* datesForm = new QFormLayout(datesBox);
	currentMonth = text(new QCheckBox(), N_("The month of Stellarium's current date"));
	datesForm->addRow(currentMonth);
	firstEvening = new QDateEdit();
	firstEvening->setCalendarPopup(true);
	firstEvening->setDisplayFormat("yyyy-MM-dd");
	datesForm->addRow(label(N_("First evening")), firstEvening);
	lastEvening = new QDateEdit();
	lastEvening->setCalendarPopup(true);
	lastEvening->setDisplayFormat("yyyy-MM-dd");
	datesForm->addRow(label(N_("Last evening")), lastEvening);
	dateStatus = statusLabel();
	datesForm->addRow(dateStatus);
	layout->addWidget(datesBox);

	// CAL-L-07
	auto* windowBox = group(N_("Time window"));
	auto* windowForm = new QFormLayout(windowBox);
	autoWindow = text(new QCheckBox(), N_("Automatic: from the hour before the earliest sunset to the hour after the latest sunrise"));
	windowForm->addRow(autoWindow);
	windowStart = new QTimeEdit();
	windowStart->setDisplayFormat("HH:mm");
	windowForm->addRow(label(N_("Start (local time)")), windowStart);
	windowEnd = new QTimeEdit();
	windowEnd->setDisplayFormat("HH:mm");
	windowForm->addRow(label(N_("End (local time)")), windowEnd);
	layout->addWidget(windowBox);
	layout->addStretch(1);

	connect(followLocation, &QCheckBox::toggled, this, &SettingsEditor::onChanged);
	connect(locationName, &QLineEdit::textEdited, this, &SettingsEditor::onChanged);
	connect(latitude, &QDoubleSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(longitude, &QDoubleSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(elevation, &QDoubleSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(timeZone, &QComboBox::currentTextChanged, this, &SettingsEditor::onChanged);
	connect(currentMonth, &QCheckBox::toggled, this, &SettingsEditor::onChanged);
	connect(firstEvening, &QDateEdit::dateChanged, this, &SettingsEditor::onChanged);
	connect(lastEvening, &QDateEdit::dateChanged, this, &SettingsEditor::onChanged);
	connect(autoWindow, &QCheckBox::toggled, this, &SettingsEditor::onChanged);
	connect(windowStart, &QTimeEdit::timeChanged, this, &SettingsEditor::onChanged);
	connect(windowEnd, &QTimeEdit::timeChanged, this, &SettingsEditor::onChanged);
	return page;
}

void SettingsEditor::buildListEditor(ListEditor& editor, QBoxLayout* layout, bool upDown)
{
	editor.list = new QListWidget();
	editor.list->setDragDropMode(QAbstractItemView::InternalMove);
	editor.list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	layout->addWidget(editor.list, 1);
	editor.status = statusLabel();
	layout->addWidget(editor.status);

	auto* row = new QHBoxLayout();
	editor.add = new QLineEdit();
	row->addWidget(editor.add, 1);
	auto* add = text(new QPushButton(), N_("Add"));
	auto* remove = text(new QPushButton(), N_("Remove"));
	row->addWidget(add);
	row->addWidget(remove);
	QListWidget* list = editor.list;
	QLineEdit* addEdit = editor.add;
	if (upDown)
	{
		auto* up = text(new QPushButton(), N_("Up"));
		auto* down = text(new QPushButton(), N_("Down"));
		row->addWidget(up);
		row->addWidget(down);
		auto move = [this, list](int step) {
			const int from = list->currentRow();
			const int to = from + step;
			if (from < 0 || to < 0 || to >= list->count())
				return;
			list->insertItem(to, list->takeItem(from));
			list->setCurrentRow(to);
			store();
		};
		connect(up, &QPushButton::clicked, this, [move]() { move(-1); });
		connect(down, &QPushButton::clicked, this, [move]() { move(1); });
	}
	layout->addLayout(row);

	auto addName = [this, list, addEdit]() {
		const QString name = addEdit->text().trimmed();
		if (name.isEmpty())
			return;
		if (listItems(list).contains(name, Qt::CaseInsensitive))
		{
			addEdit->clear();
			return;
		}
		list->addItem(name);
		addEdit->clear();
		store();
	};
	connect(add, &QPushButton::clicked, this, addName);
	connect(addEdit, &QLineEdit::returnPressed, this, addName);
	connect(remove, &QPushButton::clicked, this, [this, list]() {
		qDeleteAll(list->selectedItems());
		store();
	});
	connect(list->model(), &QAbstractItemModel::rowsMoved, this, &SettingsEditor::onChanged);
}

QWidget* SettingsEditor::buildStarsPage()
{
	// CAL-S-07
	auto* page = new QWidget();
	auto* layout = new QVBoxLayout(page);
	layout->addWidget(label(N_("Stars whose meridian transits are drawn. Any name that Stellarium finds is accepted.")));
	buildListEditor(stars, layout, false);
	auto* reset = text(new QPushButton(), N_("Reset to default"));
	auto* row = new QHBoxLayout();
	row->addStretch(1);
	row->addWidget(reset);
	layout->addLayout(row);
	connect(reset, &QPushButton::clicked, this, [this]() {
		setListItems(stars.list, ChartSettings::defaultStars());
		store();
	});
	return page;
}

void SettingsEditor::buildEdge(EdgeWidgets& edge, bool isEnd, QFormLayout* form, const char* source)
{
	auto* row = new QWidget();
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	edge.kind = new QComboBox();
	edge.clock = new QTimeEdit();
	edge.clock->setDisplayFormat("HH:mm");
	edge.event = new QComboBox();
	edge.offset = new QSpinBox();
	edge.offset->setRange(-720, 720);
	edge.offset->setSingleStep(15);
	edge.offset->setSuffix(" min");
	layout->addWidget(edge.kind);
	layout->addWidget(edge.clock);
	layout->addWidget(edge.event);
	layout->addWidget(edge.offset);
	if (isEnd)
	{
		edge.duration = new QTimeEdit();
		edge.duration->setDisplayFormat("H:mm");
		edge.duration->setMinimumTime(QTime(0, 5));
		layout->addWidget(edge.duration);
	}
	layout->addStretch(1);
	form->addRow(label(source), row);

	connect(edge.kind, &QComboBox::currentIndexChanged, this, &SettingsEditor::onChanged);
	connect(edge.clock, &QTimeEdit::timeChanged, this, &SettingsEditor::onChanged);
	connect(edge.event, &QComboBox::currentIndexChanged, this, &SettingsEditor::onChanged);
	connect(edge.offset, &QSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	if (edge.duration)
		connect(edge.duration, &QTimeEdit::timeChanged, this, &SettingsEditor::onChanged);
}

QWidget* SettingsEditor::buildPanelPage()
{
	// Requirements section 8
	auto* page = new QWidget();
	auto* layout = new QVBoxLayout(page);
	panelEnabled = text(new QCheckBox(), N_("Show the target panel"));
	layout->addWidget(panelEnabled);

	auto* form = new QFormLayout();
	panelPreset = new QComboBox();
	form->addRow(label(N_("Preset")), panelPreset);
	panelTitle = new QLineEdit();
	form->addRow(label(N_("Title")), panelTitle);
	buildEdge(periodStart, false, form, N_("Period start"));
	buildEdge(periodEnd, true, form, N_("Period end"));
	minAltitude = new QSpinBox();
	minAltitude->setRange(0, 89);
	minAltitude->setSuffix("°");
	form->addRow(label(N_("Minimum altitude")), minAltitude);
	minDuration = new QTimeEdit();
	minDuration->setDisplayFormat("H:mm");
	form->addRow(label(N_("Minimum duration (0:00: at any time)")), minDuration);
	panelSummary = statusLabel();
	form->addRow(label(N_("Heading")), panelSummary);
	layout->addLayout(form);

	layout->addWidget(label(N_("Targets, in column order (drag to reorder):")));
	targets.warnAboveTwenty = true;
	buildListEditor(targets, layout, true);

	fillKindCombos();
	fillEventCombos();
	fillPresetCombo();

	connect(panelEnabled, &QCheckBox::toggled, this, &SettingsEditor::onChanged);
	connect(panelPreset, &QComboBox::activated, this, &SettingsEditor::applyPreset);
	connect(panelTitle, &QLineEdit::textEdited, this, &SettingsEditor::onChanged);
	connect(minAltitude, &QSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(minDuration, &QTimeEdit::timeChanged, this, &SettingsEditor::onChanged);
	return page;
}

QWidget* SettingsEditor::buildAppearancePage()
{
	auto* page = new QWidget();
	auto* layout = new QVBoxLayout(page);
	auto* form = new QFormLayout();
	auto colorButton = [this]() {
		auto* b = new QToolButton();
		b->setIconSize(QSize(40, 16));
		connect(b, &QToolButton::clicked, this, [this, b]() { chooseColor(b); });
		return b;
	};
	// CAL-T-08
	const char* bandLabels[4] = { N_("Sunset to civil dusk"), N_("Civil to nautical dusk"),
				      N_("Nautical to astronomical dusk"), N_("Astronomical night") };
	for (int k = 0; k < 4; ++k)
	{
		bandColor[k] = colorButton();
		form->addRow(label(bandLabels[k]), bandColor[k]);
	}
	// CAL-S-05
	starColor = colorButton();
	form->addRow(label(N_("Star line colour")), starColor);
	starWidth = new QDoubleSpinBox();
	starWidth->setRange(0.1, 3.);
	starWidth->setSingleStep(0.1);
	starWidth->setSuffix(" pt");
	form->addRow(label(N_("Star line width")), starWidth);
	// CAL-M-02, CAL-M-03, CAL-M-10
	moonNightColor = colorButton();
	form->addRow(label(N_("Moon curve at night")), moonNightColor);
	moonDayColor = colorButton();
	form->addRow(label(N_("Moon curve, Sun above -6°")), moonDayColor);
	moonMinWidth = new QDoubleSpinBox();
	moonMinWidth->setRange(0.1, 5.);
	moonMinWidth->setSingleStep(0.1);
	moonMinWidth->setSuffix(" pt");
	form->addRow(label(N_("Moon curve width, thinnest crescent")), moonMinWidth);
	moonMaxWidth = new QDoubleSpinBox();
	moonMaxWidth->setRange(0.1, 1.);
	moonMaxWidth->setSingleStep(0.05);
	form->addRow(label(N_("Moon curve width, full Moon (fraction of row height)")), moonMaxWidth);
	layout->addLayout(form);

	auto* reset = text(new QPushButton(), N_("Reset to defaults"));
	auto* row = new QHBoxLayout();
	row->addWidget(reset);
	row->addStretch(1);
	layout->addLayout(row);
	layout->addStretch(1);

	connect(starWidth, &QDoubleSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(moonMinWidth, &QDoubleSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(moonMaxWidth, &QDoubleSpinBox::valueChanged, this, &SettingsEditor::onChanged);
	connect(reset, &QPushButton::clicked, this, [this]() {
		const ChartStyle defaults;
		ChartStyle& s = current.style;
		std::copy(std::begin(defaults.bandColor), std::end(defaults.bandColor), std::begin(s.bandColor));
		s.starLineColor = defaults.starLineColor;
		s.starLineWidth = defaults.starLineWidth;
		s.moonNightColor = defaults.moonNightColor;
		s.moonDayColor = defaults.moonDayColor;
		s.moonMinWidth = defaults.moonMinWidth;
		s.moonMaxWidthFactor = defaults.moonMaxWidthFactor;
		load();
		store();
	});
	return page;
}

// ---------------------------------------------------------------- combos

void SettingsEditor::fillKindCombos()
{
	for (EdgeWidgets* edge : { &periodStart, &periodEnd })
	{
		if (!edge->kind)
			continue;
		const int index = edge->kind->currentIndex();
		edge->kind->clear();
		edge->kind->addItem(q_("Clock time"));
		edge->kind->addItem(q_("Sun event"));
		if (edge == &periodEnd)
			edge->kind->addItem(q_("Duration after start"));
		edge->kind->setCurrentIndex(std::max(index, 0));
	}
}

void SettingsEditor::fillEventCombos()
{
	for (EdgeWidgets* edge : { &periodStart, &periodEnd })
	{
		if (!edge->event)
			continue;
		const int index = edge->event->currentIndex();
		edge->event->clear();
		for (int e = 0; e < SunEventCount; ++e)
			edge->event->addItem(q_(sunEventName(static_cast<SunEvent>(e))));
		edge->event->setCurrentIndex(std::max(index, 0));
	}
}

void SettingsEditor::fillPresetCombo()
{
	if (!panelPreset)
		return;
	const int index = panelPreset->currentIndex();
	panelPreset->clear();
	panelPreset->addItem(q_("Visual"));
	panelPreset->addItem(q_("Astrophotography"));
	panelPreset->addItem(q_("Custom"));
	panelPreset->setCurrentIndex(std::max(index, 0));
}

// ---------------------------------------------------------------- load and store

void SettingsEditor::setEdge(EdgeWidgets& edge, const PeriodEdge& value)
{
	edge.kind->setCurrentIndex(static_cast<int>(value.kind));
	edge.clock->setTime(timeOf(value.clockMinutes));
	edge.event->setCurrentIndex(static_cast<int>(value.event));
	edge.offset->setValue(static_cast<int>(std::lround(value.offsetHours * 60.)));
	if (edge.duration)
		edge.duration->setTime(timeOfHours(value.durationHours));
}

PeriodEdge SettingsEditor::readEdge(const EdgeWidgets& edge) const
{
	switch (static_cast<PeriodEdge::Kind>(edge.kind->currentIndex()))
	{
		case PeriodEdge::Kind::Clock:
			return PeriodEdge::atClock(minutesOf(edge.clock->time()));
		case PeriodEdge::Kind::Duration:
			return PeriodEdge::afterStart(hoursOf(edge.duration->time()));
		case PeriodEdge::Kind::Event:
			break;
	}
	return PeriodEdge::atEvent(static_cast<SunEvent>(edge.event->currentIndex()), edge.offset->value() / 60.);
}

void SettingsEditor::load()
{
	loading = true;
	const ChartSettings& c = current.chart;

	followLocation->setChecked(current.followStellariumLocation);
	locationName->setText(c.location.name);
	latitude->setValue(c.location.latitude);
	longitude->setValue(c.location.longitude);
	elevation->setValue(c.location.elevation);
	timeZone->setCurrentText(c.location.timeZone);
	currentMonth->setChecked(current.currentMonth);
	// Without saved dates, show the month of Stellarium's date (CAL-L-06).
	const QDate today = stellariumDate();
	const QDate monthStart(today.year(), today.month(), 1);
	firstEvening->setDate(c.firstEvening.isValid() ? c.firstEvening : monthStart);
	lastEvening->setDate(c.lastEvening.isValid() ? c.lastEvening : monthStart.addMonths(1).addDays(-1));
	autoWindow->setChecked(c.autoWindow);
	windowStart->setTime(timeOf(c.windowStartClock));
	windowEnd->setTime(timeOf(c.windowEndClock));

	setListItems(stars.list, c.stars);

	const PanelSettings& p = c.panel;
	panelEnabled->setChecked(p.enabled);
	panelTitle->setText(p.title);
	setEdge(periodStart, p.start);
	setEdge(periodEnd, p.end);
	minAltitude->setValue(static_cast<int>(std::lround(p.minAltitude)));
	minDuration->setTime(timeOfHours(p.minDurationHours));
	setListItems(targets.list, p.targets);

	for (int k = 0; k < 4; ++k)
		setColorButton(bandColor[k], current.style.bandColor[k]);
	setColorButton(starColor, current.style.starLineColor);
	starWidth->setValue(current.style.starLineWidth);
	setColorButton(moonNightColor, current.style.moonNightColor);
	setColorButton(moonDayColor, current.style.moonDayColor);
	moonMinWidth->setValue(current.style.moonMinWidth);
	moonMaxWidth->setValue(current.style.moonMaxWidthFactor);

	// The lists were refilled: check all names again.
	stars.checked.clear();
	targets.checked.clear();
	refreshDerived();
	loading = false;
}

void SettingsEditor::store()
{
	ChartSettings& c = current.chart;

	// A custom location is kept even while its time zone is not valid; the page says so.
	current.followStellariumLocation = followLocation->isChecked();
	if (!current.followStellariumLocation)
	{
		c.location = { locationName->text().trimmed(), latitude->value(), longitude->value(),
			       elevation->value(), timeZone->currentText().trimmed() };
		current.hasSavedLocation = true;
	}
	// The dates are stored as entered; the page reports an invalid range.
	current.currentMonth = currentMonth->isChecked();
	c.firstEvening = firstEvening->date();
	c.lastEvening = lastEvening->date();
	c.autoWindow = autoWindow->isChecked();
	c.windowStartClock = minutesOf(windowStart->time());
	c.windowEndClock = minutesOf(windowEnd->time());

	c.stars = listItems(stars.list);

	PanelSettings& p = c.panel;
	p.enabled = panelEnabled->isChecked();
	p.title = panelTitle->text().trimmed();
	p.start = readEdge(periodStart);
	p.end = readEdge(periodEnd);
	p.minAltitude = minAltitude->value();
	p.minDurationHours = hoursOf(minDuration->time());
	p.targets = listItems(targets.list);

	for (int k = 0; k < 4; ++k)
		current.style.bandColor[k] = colorOf(bandColor[k]);
	current.style.starLineColor = colorOf(starColor);
	current.style.starLineWidth = starWidth->value();
	current.style.moonNightColor = colorOf(moonNightColor);
	current.style.moonDayColor = colorOf(moonDayColor);
	current.style.moonMinWidth = moonMinWidth->value();
	current.style.moonMaxWidthFactor = moonMaxWidth->value();

	// The widgets keep their state (lists are not refilled, which could disturb a drag).
	refreshDerived();
	emit changed(current);
}

void SettingsEditor::refreshDerived()
{
	const bool wasLoading = loading;
	loading = true;
	const ChartSettings& c = current.chart;
	const PanelSettings& p = c.panel;
	panelPreset->setCurrentIndex(presetIndex());
	// CAL-G-11: the heading as the chart shows it.
	panelSummary->setText(p.title.isEmpty() ? p.summary() : QString("%1 — %2").arg(p.title, p.summary()));

	locationStatus->setText(!current.followStellariumLocation && !isValidTimeZone(c.location.timeZone)
		? q_("Unknown time zone. Select one from the list.") : QString());
	QString dates;
	if (!current.currentMonth)
	{
		const qint64 nights = c.firstEvening.daysTo(c.lastEvening) + 1;
		if (nights < 1)
			dates = q_("The last evening is before the first evening.");
		else if (nights > ChartSettings::MaxNights)
			dates = q_("The date range has more than %1 nights.").arg(ChartSettings::MaxNights);
	}
	dateStatus->setText(dates);

	updateEnabled();
	checkNames(stars);
	checkNames(targets);
	loading = wasLoading;
}

void SettingsEditor::updateEnabled()
{
	const bool custom = !followLocation->isChecked();
	for (QWidget* w : std::initializer_list<QWidget*>{ locationSearch, locationName, latitude, longitude, elevation, timeZone })
		w->setEnabled(custom);
	firstEvening->setEnabled(!currentMonth->isChecked());
	lastEvening->setEnabled(!currentMonth->isChecked());
	windowStart->setEnabled(!autoWindow->isChecked());
	windowEnd->setEnabled(!autoWindow->isChecked());

	for (EdgeWidgets* edge : { &periodStart, &periodEnd })
	{
		const auto kind = static_cast<PeriodEdge::Kind>(edge->kind->currentIndex());
		edge->clock->setVisible(kind == PeriodEdge::Kind::Clock);
		edge->event->setVisible(kind == PeriodEdge::Kind::Event);
		edge->offset->setVisible(kind == PeriodEdge::Kind::Event);
		if (edge->duration)
			edge->duration->setVisible(kind == PeriodEdge::Kind::Duration);
	}
}

// ---------------------------------------------------------------- names

void SettingsEditor::checkNames(ListEditor& editor)
{
	// CAL-S-07, CAL-G-12: report names that Stellarium cannot find. Only when the list changed.
	const QStringList names = listItems(editor.list);
	if (names == editor.checked)
		return;
	editor.checked = names;
	const StelObjectMgr* objectMgr = GETSTELMODULE(StelObjectMgr);
	QStringList missing;
	for (int i = 0; i < editor.list->count(); ++i)
	{
		QListWidgetItem* item = editor.list->item(i);
		const bool found = !objectMgr || objectMgr->searchByName(item->text());
		item->setForeground(found ? editor.list->palette().color(QPalette::Text) : NOT_FOUND_COLOR);
		item->setToolTip(found ? QString() : q_("Stellarium cannot find this name. It is left out of the chart."));
		if (!found)
			missing.append(item->text());
	}
	QStringList lines;
	if (!missing.isEmpty())
		lines.append(q_("Not found, left out: %1").arg(missing.join(", ")));
	// CAL-G-14
	if (editor.warnAboveTwenty && editor.list->count() > MANY_TARGETS)
		lines.append(q_("More than %1 targets: the chart may become too narrow.").arg(MANY_TARGETS));
	editor.status->setText(lines.join("\n"));
}

// ---------------------------------------------------------------- presets

int SettingsEditor::presetIndex() const
{
	const PanelSettings& p = current.chart.panel;
	const PanelSettings presets[2] = { PanelSettings::visualPreset(), PanelSettings::astrophotographyPreset() };
	for (int i = 0; i < 2; ++i)
	{
		const PanelSettings& q = presets[i];
		if (p.start.toJson() == q.start.toJson() && p.end.toJson() == q.end.toJson()
		    && p.minAltitude == q.minAltitude && p.minDurationHours == q.minDurationHours)
			return i;
	}
	return CUSTOM_PRESET;
}

void SettingsEditor::applyPreset(int index)
{
	if (index == CUSTOM_PRESET || loading)
		return;
	// The preset gives the period, altitude and duration. The panel switch, the
	// targets and a title that the user entered are kept (CAL-G-11).
	const PanelSettings old = current.chart.panel;
	PanelSettings p = (index == 0) ? PanelSettings::visualPreset() : PanelSettings::astrophotographyPreset();
	const bool presetTitle = old.title.isEmpty() || old.title == PanelSettings::visualPreset().title
				 || old.title == PanelSettings::astrophotographyPreset().title;
	if (!presetTitle)
		p.title = old.title;
	p.enabled = old.enabled;
	p.targets = old.targets;
	current.chart.panel = p;
	load();
	emit changed(current);
}

// ---------------------------------------------------------------- colours

void SettingsEditor::setColorButton(QToolButton* button, const QColor& color)
{
	button->setProperty("color", color);
	QPixmap swatch(button->iconSize());
	swatch.fill(color);
	QPainter painter(&swatch);
	painter.setPen(Qt::gray);
	painter.drawRect(swatch.rect().adjusted(0, 0, -1, -1));
	button->setIcon(QIcon(swatch));
	button->setToolTip(color.name());
}

QColor SettingsEditor::colorOf(const QToolButton* button) const
{
	return button->property("color").value<QColor>();
}

void SettingsEditor::chooseColor(QToolButton* button)
{
	const QColor color = QColorDialog::getColor(colorOf(button), &StelMainView::getInstance());
	if (!color.isValid())
		return;
	setColorButton(button, color);
	store();
}
