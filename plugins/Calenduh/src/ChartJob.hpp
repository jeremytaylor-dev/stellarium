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

#ifndef CALENDUH_CHARTJOB_HPP
#define CALENDUH_CHARTJOB_HPP

#include "ChartModel.hpp"
#include "ChartSettings.hpp"

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace calenduh {

class EphemerisSampler;

//! Computes a chart model without blocking the user interface (CAL-I-07,
//! architecture section 5.6): one step per event-loop turn (the Sun events of a
//! night, then the rest of a night), with progress and cancel. A reload of the
//! Solar System data stops the job, because the sampler holds pointers into it.
class ChartJob : public QObject
{
	Q_OBJECT

public:
	explicit ChartJob(QObject* parent = nullptr);
	~ChartJob() override;

	//! Start a job. A running job is replaced; it emits no signal. Returns false
	//! and sets @p error if the settings cannot be used.
	bool start(const ChartSettings& settings, QString* error = nullptr);
	//! Stop a running job. Emits cancelled().
	void cancel();
	bool isRunning() const { return sampler != nullptr; }

signals:
	void progress(int stepsDone, int steps);
	void finished(const calenduh::ChartModel& model, const QStringList& unresolvedNames);
	void cancelled();
	void failed(const QString& message);

private slots:
	void step();
	void solarSystemReloaded();

private:
	std::unique_ptr<EphemerisSampler> sampler;
	int next = 0;           //!< next step: 0..n-1 Sun events, n..2n-1 nights
	QTimer timer;
	quint64 generation = 0; //!< counts start() calls
};

} // namespace calenduh

#endif // CALENDUH_CHARTJOB_HPP
