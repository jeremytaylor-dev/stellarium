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

// Unit tests of UserSettings: storage in config.ini (CAL-I-08).

#include "UserSettings.hpp"

#include <QSettings>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

using namespace calenduh;

class TestCalenduhSettings : public QObject
{
	Q_OBJECT

private:
	QTemporaryDir dir;
	QString iniPath(const QString& name) const { return dir.filePath(name + ".ini"); }

private slots:
	void emptyFileGivesDefaults()
	{
		QSettings conf(iniPath("empty"), QSettings::IniFormat);
		UserSettings s;
		s.load(conf);
		const UserSettings d;
		QVERIFY(s.followStellariumLocation);
		QVERIFY(!s.hasSavedLocation);
		QVERIFY(s.currentMonth);
		QCOMPARE(s.chart.stars, d.chart.stars);
		QCOMPARE(s.chart.autoWindow, d.chart.autoWindow);
		QCOMPARE(s.style.starLineColor, d.style.starLineColor);
		QCOMPARE(s.style.eventColor, QColor("#e00000"));
		for (bool on : s.chart.events)
			QVERIFY(on); // CAL-E-01: all event types on by default
		QCOMPARE(s.exportOptions.pageSize, QPageSize::A4);
		QCOMPARE(s.exportOptions.orientation, QPageLayout::Landscape);
	}

	void panelDefaultsAreVisualPreset()
	{
		const UserSettings s;
		const PanelSettings visual = PanelSettings::visualPreset();
		QVERIFY(!s.chart.panel.enabled);
		QCOMPARE(s.chart.panel.title, visual.title);
		QCOMPARE(s.chart.panel.summary(), visual.summary());
	}

	void panelSummaryText()
	{
		// CAL-G-11: sums in brackets, and the minimum duration at the end.
		PanelSettings p = PanelSettings::visualPreset();
		QCOMPARE(p.summary(), QString::fromUtf8("nautical dusk to (start + 2:30), above 20° for minimum 1:00"));
		p.start = PeriodEdge::atEvent(SunEvent::Sunset, 0.5);
		p.end = PeriodEdge::atEvent(SunEvent::Sunrise, -1.);
		p.minDurationHours = 0.75;
		QCOMPARE(p.summary(), QString::fromUtf8("(sunset + 0:30) to (sunrise - 1:00), above 20° for minimum 0:45"));
		p.start = PeriodEdge::atEvent(SunEvent::Sunset, 0.004);
		p.end = PeriodEdge::atClock(23 * 60);
		p.minDurationHours = 0.004;
		QCOMPARE(p.summary(), QString::fromUtf8("sunset to 23:00, above 20° at any time"));
	}

	void roundTrip()
	{
		UserSettings s;
		s.followStellariumLocation = false;
		s.hasSavedLocation = true;
		s.chart.location = { QString::fromUtf8("Tromsø, Norway"), 69.6492, 18.9553, 10., "Europe/Oslo" };
		s.currentMonth = false;
		s.chart.firstEvening = QDate(2026, 11, 10);
		s.chart.lastEvening = QDate(2026, 12, 5);
		s.chart.autoWindow = false;
		s.chart.windowStartClock = 14 * 60;
		s.chart.windowEndClock = 10 * 60 + 30;
		s.chart.stars = { "Sirius" };
		s.chart.panel = PanelSettings::visualPreset();
		s.chart.panel.start = PeriodEdge::atEvent(SunEvent::CivilDusk, -0.5);
		s.chart.panel.targets = { "M31", "NGC 253, Sculptor Galaxy", "C/2025 A1 (Test)" };
		s.style.bandColor[2] = QColor("#123456");
		s.style.starLineColor = QColor("#00ff00");
		s.style.starLineWidth = 0.8;
		s.style.moonMinWidth = 0.7;
		s.style.eventColor = QColor("#ff00ff");
		s.chart.events = { true, false, false, true };
		s.exportOptions.format = ExportOptions::Format::Png;
		s.exportOptions.pageSize = QPageSize::A3;
		s.exportOptions.orientation = QPageLayout::Portrait;
		s.exportOptions.pngDpi = 300;
		s.exportOptions.directory = "C:/Charts";
		{
			QSettings conf(iniPath("roundtrip"), QSettings::IniFormat);
			s.save(conf);
		}

		QSettings conf(iniPath("roundtrip"), QSettings::IniFormat);
		UserSettings r;
		r.load(conf);
		QCOMPARE(r.followStellariumLocation, false);
		QVERIFY(r.hasSavedLocation);
		QCOMPARE(r.chart.location.name, s.chart.location.name);
		QCOMPARE(r.chart.location.latitude, s.chart.location.latitude);
		QCOMPARE(r.chart.location.longitude, s.chart.location.longitude);
		QCOMPARE(r.chart.location.elevation, s.chart.location.elevation);
		QCOMPARE(r.chart.location.timeZone, s.chart.location.timeZone);
		QCOMPARE(r.currentMonth, false);
		QCOMPARE(r.chart.firstEvening, s.chart.firstEvening);
		QCOMPARE(r.chart.lastEvening, s.chart.lastEvening);
		QCOMPARE(r.chart.autoWindow, false);
		QCOMPARE(r.chart.windowStartClock, s.chart.windowStartClock);
		QCOMPARE(r.chart.windowEndClock, s.chart.windowEndClock);
		QCOMPARE(r.chart.stars, s.chart.stars);
		QCOMPARE(r.chart.panel.enabled, true);
		QCOMPARE(r.chart.panel.title, QString("Visual"));
		QCOMPARE(r.chart.panel.summary(), s.chart.panel.summary());
		QCOMPARE(r.chart.panel.targets, s.chart.panel.targets);
		QCOMPARE(r.style.bandColor[2], s.style.bandColor[2]);
		QCOMPARE(r.style.starLineColor, s.style.starLineColor);
		QCOMPARE(r.style.starLineWidth, s.style.starLineWidth);
		QCOMPARE(r.style.moonMinWidth, s.style.moonMinWidth);
		QCOMPARE(r.style.eventColor, s.style.eventColor);
		QVERIFY(r.chart.events == s.chart.events);
		QCOMPARE(r.exportOptions.format, ExportOptions::Format::Png);
		QCOMPARE(r.exportOptions.pageSize, QPageSize::A3);
		QCOMPARE(r.exportOptions.orientation, QPageLayout::Portrait);
		QCOMPARE(r.exportOptions.pngDpi, 300);
		QCOMPARE(r.exportOptions.directory, QString("C:/Charts"));
	}

	void emptyListsSurvive()
	{
		UserSettings s;
		s.chart.stars.clear();
		s.chart.panel.targets.clear();
		{
			QSettings conf(iniPath("emptylists"), QSettings::IniFormat);
			s.save(conf);
		}
		QSettings conf(iniPath("emptylists"), QSettings::IniFormat);
		UserSettings r;
		r.load(conf);
		QVERIFY(r.chart.stars.isEmpty());
		QVERIFY(r.chart.panel.targets.isEmpty());
	}

	void invalidValuesKeepDefaults()
	{
		{
			QSettings conf(iniPath("invalid"), QSettings::IniFormat);
			conf.beginGroup("Calenduh");
			conf.setValue("current_month", false);
			conf.setValue("first_evening", "2026-13-01");
			conf.setValue("window_start", "25:99");
			conf.setValue("panel_start", "{\"duration\":2}");
			conf.setValue("panel_end", "not json");
			conf.setValue("color_star_line", "notacolour");
			conf.setValue("star_line_width", -1);
			conf.setValue("export_page_size", "NoSuchPage");
			conf.setValue("export_png_dpi", 5);
			conf.setValue("location_latitude", "abc");
			conf.setValue("location_longitude", 10.);
			conf.setValue("location_time_zone", "Europe/Oslo");
			conf.setValue("panel_min_duration", "foo");
			conf.endGroup();
		}
		QSettings conf(iniPath("invalid"), QSettings::IniFormat);
		UserSettings r;
		r.load(conf);
		const UserSettings d;
		QVERIFY(r.currentMonth);  // no valid saved dates
		QCOMPARE(r.chart.windowStartClock, d.chart.windowStartClock);
		QCOMPARE(r.chart.panel.start.kind, PeriodEdge::Kind::Event);  // a duration cannot start a period
		QCOMPARE(r.chart.panel.end.kind, d.chart.panel.end.kind);
		QCOMPARE(r.style.starLineColor, d.style.starLineColor);
		QCOMPARE(r.style.starLineWidth, d.style.starLineWidth);
		QCOMPARE(r.exportOptions.pageSize, QPageSize::A4);
		QCOMPARE(r.exportOptions.pngDpi, d.exportOptions.pngDpi);
		QVERIFY(!r.hasSavedLocation);
		QCOMPARE(r.chart.panel.minDurationHours, d.chart.panel.minDurationHours);
	}

	void unknownTimeZoneIsKept()
	{
		{
			QSettings conf(iniPath("badzone"), QSettings::IniFormat);
			conf.beginGroup("Calenduh");
			conf.setValue("location_latitude", 10.);
			conf.setValue("location_longitude", 10.);
			conf.setValue("location_time_zone", "Foo/Bar");
			conf.setValue("panel_end", "{\"duration\":-2}");
			conf.endGroup();
		}
		QSettings conf(iniPath("badzone"), QSettings::IniFormat);
		UserSettings r;
		r.load(conf);
		QVERIFY(r.hasSavedLocation);
		QCOMPARE(r.chart.location.timeZone, QString("Foo/Bar"));
		QVERIFY(!isValidTimeZone(r.chart.location.timeZone));
		QCOMPARE(r.chart.panel.end.kind, UserSettings().chart.panel.end.kind);
	}

	void emptyTimeZoneIsNotSaved()
	{
		{
			QSettings conf(iniPath("emptyzone"), QSettings::IniFormat);
			conf.beginGroup("Calenduh");
			conf.setValue("location_latitude", 10.);
			conf.setValue("location_longitude", 10.);
			conf.setValue("location_time_zone", "");
			conf.endGroup();
		}
		QSettings conf(iniPath("emptyzone"), QSettings::IniFormat);
		UserSettings r;
		r.load(conf);
		QVERIFY(!r.hasSavedLocation);
		QVERIFY(!isValidTimeZone(""));
		QVERIFY(!isValidTimeZone("  "));
		QVERIFY(isValidTimeZone("Pacific/Auckland"));
	}

	void clockWraps()
	{
		UserSettings s;
		s.chart.autoWindow = false;
		s.chart.windowEndClock = 24 * 60;
		{
			QSettings conf(iniPath("clock"), QSettings::IniFormat);
			s.save(conf);
		}
		QSettings conf(iniPath("clock"), QSettings::IniFormat);
		UserSettings r;
		r.load(conf);
		QCOMPARE(r.chart.windowEndClock, 0);
	}

	void fixedOffsetZones()
	{
		QCOMPARE(fixedOffsetTimeZone(12 * 60 + 45), QString("UTC+12:45"));
		QCOMPARE(fixedOffsetTimeZone(-(9 * 60 + 30)), QString("UTC-09:30"));
		for (int minutes : { 0, 699, -37, 12 * 60 + 45, -(9 * 60 + 30) })
		{
			const QTimeZone tz = chartTimeZone(fixedOffsetTimeZone(minutes));
			QVERIFY2(tz.isValid(), qPrintable(fixedOffsetTimeZone(minutes)));
			QCOMPARE(tz.offsetFromUtc(QDateTime::currentDateTimeUtc()), minutes * 60);
		}
		QVERIFY(chartTimeZone("Pacific/Auckland").isValid());
	}

	void pagePoints()
	{
		ExportOptions e;
		QCOMPARE(e.pagePoints().toSize(), QSize(842, 595));
		e.orientation = QPageLayout::Portrait;
		QCOMPARE(e.pagePoints().toSize(), QSize(595, 842));
	}

	// CAL-I-11, LANG-2: the title's date range in the chart language.
	void dateRanges()
	{
		const QDate aug1(2026, 8, 1), aug31(2026, 8, 31), sep30(2026, 9, 30), jan2(2027, 1, 2), dec30(2026, 12, 30);
		const ChartText english;
		QCOMPARE(english.dateRange(aug1, aug31), QString::fromUtf8("1–31 August 2026"));
		QCOMPARE(english.dateRange(aug1, sep30), QString::fromUtf8("1 August – 30 September 2026"));
		QCOMPARE(english.dateRange(dec30, jan2), QString::fromUtf8("30 December 2026 – 2 January 2027"));
		// LANG-4: US English has the month first.
		const ChartText us = inLocale("en_US");
		QCOMPARE(us.dateRange(aug1, aug31), QString::fromUtf8("August 1–31, 2026"));
		QCOMPARE(us.dateRange(aug1, sep30), QString::fromUtf8("August 1 – September 30, 2026"));
		QCOMPARE(us.dateRange(dec30, jan2), QString::fromUtf8("December 30, 2026 – January 2, 2027"));
		QCOMPARE(inLocale("en_NZ").dateRange(aug1, aug31), QString::fromUtf8("1–31 August 2026"));
		// The locale's day format: two digits in South African English.
		QCOMPARE(inLocale("en_ZA").dateRange(aug1, aug31), QString::fromUtf8("01–31 August 2026"));
		QCOMPARE(inLocale("fil_PH").dateRange(aug1, aug31), QString::fromUtf8("Agosto 1–31, 2026"));
		// A mark after the day stays with both days; a comma or hyphen with the last only.
		QCOMPARE(inLocale("smn_FI").dateRange(aug1, sep30).count(QLatin1Char('.')), 2);
		QVERIFY(!inLocale("uz_Latn_UZ").dateRange(aug1, aug31).contains(QString::fromUtf8("-–")));

		const ChartText german = inLocale("de_DE");
		QCOMPARE(german.dateRange(aug1, aug31), QString::fromUtf8("1.–31. August 2026"));
		QCOMPARE(german.dateRange(aug1, sep30), QString::fromUtf8("1. August – 30. September 2026"));
		QCOMPARE(german.dateRange(dec30, jan2), QString::fromUtf8("30. Dezember 2026 – 2. Januar 2027"));
		QCOMPARE(inLocale("fr_FR").dateRange(aug1, aug31), QString::fromUtf8("1–31 août 2026"));
		// A locale whose date does not start with the day: both dates complete, no weekday.
		const QString japanese = inLocale("ja_JP").dateRange(aug1, aug31);
		QVERIFY2(japanese.contains(QString::fromUtf8("2026年8月1日")) && japanese.contains(QString::fromUtf8("2026年8月31日")),
			 qPrintable(japanese));
		QVERIFY2(!japanese.contains(QString::fromUtf8("曜日")), qPrintable(japanese));
		// The words and separators next to the weekday go with it.
		QCOMPARE(inLocale("da_DK").dateRange(aug1, aug31), QString::fromUtf8("1.–31. august 2026"));
		const QString arabic = inLocale("ar_EG").dateRange(aug1, aug31);
		QVERIFY2(!arabic.contains(QChar(0x060C)), arabic.toUtf8().constData());
		QCOMPARE(inLocale("th_TH").dateRange(aug1, aug31), QString::fromUtf8("1–31 สิงหาคม 2026"));
		const QString thai = inLocale("th_TH").dateRange(aug1, sep30);
		QVERIFY2(!thai.contains(QString::fromUtf8("ที่")) && !thai.contains(QString::fromUtf8("วัน")), thai.toUtf8().constData());
		for (const char* name : { "es_ES", "ru_RU", "pl_PL", "nl_NL", "cs_CZ", "pt_BR", "bg_BG" })
		{
			const QString range = inLocale(name).dateRange(aug1, aug31);
			QVERIFY2(range.startsWith(QLatin1Char('1')) && range.contains(QLatin1String("31")), qPrintable(range));
		}
	}

	// CAL-I-11, LANG-2: coordinates with the locale's decimal separator and translated letters.
	void coordinates()
	{
		const ChartText english;
		QCOMPARE(english.latitude(51.514), QString::fromUtf8("51.51° N"));
		QCOMPARE(english.longitude(-0.126), QString::fromUtf8("0.13° W"));
		const ChartText german = ChartText().translated(
			[](const QString& s) { return s == "E" ? QString("O") : s; }, QLocale("de_DE"));
		QCOMPARE(german.latitude(-41.11), QString::fromUtf8("41,11° S"));
		QCOMPARE(german.longitude(173.0), QString::fromUtf8("173,00° O"));
	}

	// CAL-E-08, CAL-I-11, LANG-1: event labels from kind, planet and detail.
	void eventLabels()
	{
		ChartText text;
		QCOMPARE(text.eventLabel({ EventKind::SolarEclipse, 0., QString(), EventDetail::Annular }), QString("Annular solar eclipse"));
		QCOMPARE(text.eventLabel({ EventKind::SolarEclipse, 0., QString(), EventDetail::Partial }), QString("Partial solar eclipse"));
		QCOMPARE(text.eventLabel({ EventKind::LunarEclipse, 0., QString(), EventDetail::Total }), QString("Total lunar eclipse"));
		QCOMPARE(text.eventLabel({ EventKind::LunarEclipse, 0., QString(), EventDetail::Penumbral }), QString("Penumbral lunar eclipse"));
		QCOMPARE(text.eventLabel({ EventKind::Opposition, 0., "Jupiter", EventDetail::None }), QString("Jupiter opposition"));
		QCOMPARE(text.eventLabel({ EventKind::GreatestElongation, 0., "Venus", EventDetail::West }), QString("Venus greatest elongation W"));
		text.setBodyNames({ { "Jupiter", QString::fromUtf8("Júpiter") } });
		QCOMPARE(text.eventLabel({ EventKind::Opposition, 0., "Jupiter", EventDetail::None }), QString::fromUtf8("Júpiter opposition"));
		QCOMPARE(text.eventLabel({ EventKind::Opposition, 0., "Mars", EventDetail::None }), QString("Mars opposition"));
	}

	// CAL-G-11, CAL-I-11: the panel summary in the chart language.
	void translatedPanelSummary()
	{
		PanelSettings p = PanelSettings::visualPreset();
		p.minAltitude = 22.5;
		const ChartText text = ChartText().translated(
			[](const QString& s) {
				if (s == "nautical dusk")
					return QString("nautische Dämmerung");
				if (s == "%1 to %2, above %3° %4")
					return QString("%1 bis %2, über %3° %4");
				return s;
			},
			QLocale("de_DE"));
		QCOMPARE(p.summary(text), QString::fromUtf8("nautische Dämmerung bis (start + 2:30), über 22,5° for minimum 1:00"));
		QCOMPARE(p.summary(), QString::fromUtf8("nautical dusk to (start + 2:30), above 22.5° for minimum 1:00"));
	}

	// The model file keeps planet and detail; the label is English (fixture comparison).
	void notableEventJson()
	{
		ChartModel m;
		m.location.timeZone = "UTC";
		NightRow row;
		row.eveningDate = QDate(2026, 8, 1);
		row.notableEvents.append({ EventKind::GreatestElongation, 2461254.25, "Mercury", EventDetail::West });
		row.notableEvents.append({ EventKind::LunarEclipse, 2461280.75, QString(), EventDetail::Partial });
		m.rows.append(row);
		const QByteArray json = m.toJson();
		QVERIFY(json.contains("\"label\": \"Mercury greatest elongation W\""));
		ChartModel r;
		QString error;
		QVERIFY2(r.fromJson(json, &error), qPrintable(error));
		QCOMPARE(r.rows.size(), 1);
		const QVector<NotableEvent>& e = r.rows.first().notableEvents;
		QCOMPARE(e.size(), 2);
		QCOMPARE(e.at(0).body, QString("Mercury"));
		QVERIFY(e.at(0).detail == EventDetail::West);
		QVERIFY(e.at(1).kind == EventKind::LunarEclipse);
		QVERIFY(e.at(1).detail == EventDetail::Partial);
		QVERIFY(e.at(1).body.isEmpty());

		// A model file from before body and detail: both come from the English label.
		const QByteArray old = R"({"schema": 1, "location": {"timeZone": "UTC"}, "rows": [{"eveningDate": "2026-08-01",
			"notableEvents": [{"kind": "lunarEclipse", "jd": 1.0, "label": "Total lunar eclipse"},
					  {"kind": "opposition", "jd": 2.0, "label": "Jupiter opposition"},
					  {"kind": "greatestElongation", "jd": 3.0, "label": "Venus greatest elongation W"}]}]})";
		QVERIFY2(r.fromJson(old, &error), qPrintable(error));
		const ChartText text;
		const QVector<NotableEvent>& o = r.rows.first().notableEvents;
		QCOMPARE(o.size(), 3);
		QCOMPARE(text.eventLabel(o.at(0)), QString("Total lunar eclipse"));
		QCOMPARE(text.eventLabel(o.at(1)), QString("Jupiter opposition"));
		QCOMPARE(text.eventLabel(o.at(2)), QString("Venus greatest elongation W"));
	}

private:
	static ChartText inLocale(const char* name)
	{
		return ChartText().translated([](const QString& s) { return s; }, QLocale(QString::fromLatin1(name)));
	}
};

QTEST_GUILESS_MAIN(TestCalenduhSettings)
#include "testCalenduhSettings.moc"
