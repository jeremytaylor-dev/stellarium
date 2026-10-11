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

// Stand-alone test program: renders a chart-model JSON file without Stellarium.
// Usage: calenduh-render <model.json> <output.png|svg|pdf> [--dpi N]

#include "ChartExporter.hpp"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTextStream>

int main(int argc, char* argv[])
{
	QGuiApplication app(argc, argv);
	QTextStream err(stderr);

	const QStringList args = app.arguments();
	if (args.size() < 3)
	{
		err << "Usage: calenduh-render <model.json> <output.png|svg|pdf> [--dpi N]\n";
		return 2;
	}
	double dpi = 150.;
	const int dpiIndex = args.indexOf("--dpi");
	if (dpiIndex > 0 && dpiIndex + 1 < args.size())
		dpi = args.at(dpiIndex + 1).toDouble();

	QFile file(args.at(1));
	if (!file.open(QIODevice::ReadOnly))
	{
		err << "Cannot read " << args.at(1) << "\n";
		return 1;
	}
	Calenduh::ChartModel model;
	QString message;
	if (!model.fromJson(file.readAll(), &message))
	{
		err << "Invalid model: " << message << "\n";
		return 1;
	}

	const Calenduh::ChartStyle style;
	const QString out = args.at(2);
	const QString suffix = QFileInfo(out).suffix().toLower();
	const QSizeF page = Calenduh::ChartExporter::defaultPageSize();
	bool ok = false;
	if (suffix == "png")
		ok = Calenduh::ChartExporter::writePng(model, style, out, page, dpi);
	else if (suffix == "svg")
		ok = Calenduh::ChartExporter::writeSvg(model, style, out, page);
	else if (suffix == "pdf")
		ok = Calenduh::ChartExporter::writePdf(model, style, out);
	else
		err << "Unknown output type: " << suffix << "\n";

	if (!ok)
	{
		err << "Failed to write " << out << "\n";
		return 1;
	}
	return 0;
}
