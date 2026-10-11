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

#include "ChartJob.hpp"
#include "EphemerisSampler.hpp"

#include "StelApp.hpp"
#include "StelModuleMgr.hpp"
#include "StelTranslator.hpp"
#include "SolarSystem.hpp"

namespace calenduh {

ChartJob::ChartJob(QObject* parent)
	: QObject(parent)
{
	timer.setSingleShot(true);
	timer.setInterval(0);
	connect(&timer, &QTimer::timeout, this, &ChartJob::step);
}

ChartJob::~ChartJob() = default;

bool ChartJob::start(const ChartSettings& settings, QString* error)
{
	// Replace a running job without a cancelled() signal, so that no receiver
	// can start another job inside this call.
	timer.stop();
	sampler.reset();
	++generation;
	// Connected here, not in the constructor: the plugin may be created before SolarSystem.
	if (SolarSystem* solarSystem = GETSTELMODULE(SolarSystem))
		connect(solarSystem, &SolarSystem::solarSystemDataReloaded, this, &ChartJob::solarSystemReloaded, Qt::UniqueConnection);
	auto s = std::make_unique<EphemerisSampler>(settings);
	if (!s->begin(error))
		return false;
	sampler = std::move(s);
	next = 0;
	emit progress(0, 2 * sampler->nightCount());
	timer.start();
	return true;
}

void ChartJob::cancel()
{
	if (!isRunning())
		return;
	timer.stop();
	sampler.reset();
	emit cancelled();
}

void ChartJob::step()
{
	if (!isRunning())
		return;
	const int nights = sampler->nightCount();
	if (next < nights)
	{
		sampler->computeSunNight(next);
		if (next == nights - 1)
			sampler->chooseTimeWindow();
	}
	else
		sampler->computeNight(next - nights);
	++next;
	if (next < 2 * nights)
	{
		const quint64 current = generation;
		emit progress(next, 2 * nights);
		// A receiver can have cancelled this job or started another one.
		if (isRunning() && generation == current)
			timer.start();
		return;
	}
	// Release the sampler before the signals, so that a receiver can start a new job.
	const std::unique_ptr<EphemerisSampler> done = std::move(sampler);
	emit progress(next, 2 * nights);
	emit finished(done->model(), done->unresolvedNames());
}

void ChartJob::solarSystemReloaded()
{
	if (!isRunning())
		return;
	timer.stop();
	sampler.reset();
	emit failed(q_("The Solar System data was reloaded during the calculation. Generate the chart again."));
}

} // namespace calenduh
