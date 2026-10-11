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

#ifndef CALENDUH_SETTINGSEDITOR_HPP
#define CALENDUH_SETTINGSEDITOR_HPP

#include "UserSettings.hpp"

#include <QDate>
#include <QList>
#include <QObject>
#include <QPair>
#include <QStringList>
#include <functional>

class QBoxLayout;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTimeEdit;
class QToolButton;
class QWidget;

//! The settings pages of the Calenduh window: location and dates, stars,
//! target panel, appearance. Edits UserSettings; every change emits changed().
//! Names of stars and targets are checked with Stellarium's object search
//! (CAL-S-07, CAL-G-12).
class SettingsEditor : public QObject
{
	Q_OBJECT

public:
	explicit SettingsEditor(QObject* parent = nullptr);

	//! The pages, with their titles. Created on the first call.
	QList<QPair<QString, QWidget*>> pages();

	void setSettings(const calenduh::UserSettings& settings);
	//! Keep the format and directory of the last export (no widgets show them).
	void setExportTarget(calenduh::ExportOptions::Format format, const QString& directory)
	{
		current.exportOptions.format = format;
		current.exportOptions.directory = directory;
	}
	const calenduh::UserSettings& settings() const { return current; }

	//! Set the texts again after a language change.
	void retranslate();

signals:
	void changed(const calenduh::UserSettings& settings);

private:
	//! A list of names with Add, Remove and optional Up and Down buttons.
	struct ListEditor
	{
		QListWidget* list = nullptr;
		QLineEdit* add = nullptr;
		QLabel* status = nullptr;
		QStringList checked;        //!< the names of the last check
		bool warnAboveTwenty = false;
	};

	calenduh::UserSettings current;
	bool loading = false;
	QList<std::function<void()>> texts;   //!< set the translated texts
	QList<QPair<QString, QWidget*>> pageList;
	QList<const char*> pageTitles;

	// Location and dates
	QCheckBox* followLocation = nullptr;
	QLineEdit* locationSearch = nullptr;
	QLineEdit* locationName = nullptr;
	QDoubleSpinBox* latitude = nullptr;
	QDoubleSpinBox* longitude = nullptr;
	QDoubleSpinBox* elevation = nullptr;
	QComboBox* timeZone = nullptr;
	QLabel* locationStatus = nullptr;
	QCheckBox* currentMonth = nullptr;
	QDateEdit* firstEvening = nullptr;
	QDateEdit* lastEvening = nullptr;
	QLabel* dateStatus = nullptr;
	QCheckBox* autoWindow = nullptr;
	QTimeEdit* windowStart = nullptr;
	QTimeEdit* windowEnd = nullptr;

	// Stars
	ListEditor stars;

	// Target panel
	QCheckBox* panelEnabled = nullptr;
	QComboBox* panelPreset = nullptr;
	QLineEdit* panelTitle = nullptr;
	struct EdgeWidgets
	{
		QComboBox* kind = nullptr;
		QTimeEdit* clock = nullptr;
		QComboBox* event = nullptr;
		QSpinBox* offset = nullptr;     //!< minutes
		QTimeEdit* duration = nullptr;  //!< end only
	};
	EdgeWidgets periodStart, periodEnd;
	QSpinBox* minAltitude = nullptr;
	QTimeEdit* minDuration = nullptr;
	QLabel* panelSummary = nullptr;
	ListEditor targets;

	// Appearance
	QToolButton* bandColor[4] = {};
	QToolButton* starColor = nullptr;
	QDoubleSpinBox* starWidth = nullptr;
	QToolButton* moonNightColor = nullptr;
	QToolButton* moonDayColor = nullptr;
	QDoubleSpinBox* moonMinWidth = nullptr;
	QDoubleSpinBox* moonMaxWidth = nullptr;

	// Page and export (CAL-I-05)
	QComboBox* pageSize = nullptr;
	QComboBox* orientation = nullptr;
	QSpinBox* pngDpi = nullptr;

	QWidget* buildLocationPage();
	QWidget* buildStarsPage();
	QWidget* buildPanelPage();
	QWidget* buildAppearancePage();
	QWidget* buildExportPage();
	void buildEdge(EdgeWidgets& edge, bool isEnd, QFormLayout* form, const char* label);
	void buildListEditor(ListEditor& editor, QBoxLayout* layout, bool upDown);

	//! Register a translated text for a widget with setText() and set it.
	template <class W> W* text(W* widget, const char* source);
	QLabel* label(const char* source);
	QGroupBox* group(const char* title);
	void fillEventCombos();
	void fillKindCombos();
	void fillPresetCombo();
	void fillOrientationCombo();

	void onChanged() { if (!loading) store(); }
	void load();            //!< settings -> widgets
	void store();           //!< widgets -> settings, then changed()
	void refreshDerived();  //!< preset, heading, enabled fields, checks
	void updateEnabled();
	void checkNames(ListEditor& editor);
	void setColorButton(QToolButton* button, const QColor& color);
	QColor colorOf(const QToolButton* button) const;
	void chooseColor(QToolButton* button);
	void applyPreset(int index);
	int presetIndex() const;
	void setEdge(EdgeWidgets& edge, const calenduh::PeriodEdge& value);
	calenduh::PeriodEdge readEdge(const EdgeWidgets& edge) const;
	//! Stellarium's current local date (CAL-L-06).
	static QDate stellariumDate();
};

#endif // CALENDUH_SETTINGSEDITOR_HPP
