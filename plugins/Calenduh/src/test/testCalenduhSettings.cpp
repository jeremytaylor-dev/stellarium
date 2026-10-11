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
		QCOMPARE(s.exportOptions.pageSize, QPageSize::A4);
		QCOMPARE(s.exportOptions.orientation, QPageLayout::Landscape);
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

	void invalidTimeZoneIsNotSaved()
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
		QVERIFY(!r.hasSavedLocation);
		QCOMPARE(r.chart.panel.end.kind, UserSettings().chart.panel.end.kind);
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
			const QTimeZone tz(fixedOffsetTimeZone(minutes).toUtf8());
			QVERIFY2(tz.isValid(), qPrintable(fixedOffsetTimeZone(minutes)));
			QCOMPARE(tz.offsetFromUtc(QDateTime::currentDateTimeUtc()), minutes * 60);
		}
	}

	void pagePoints()
	{
		ExportOptions e;
		QCOMPARE(e.pagePoints().toSize(), QSize(842, 595));
		e.orientation = QPageLayout::Portrait;
		QCOMPARE(e.pagePoints().toSize(), QSize(595, 842));
	}
};

QTEST_GUILESS_MAIN(TestCalenduhSettings)
#include "testCalenduhSettings.moc"
