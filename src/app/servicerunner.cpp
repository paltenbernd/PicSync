// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "servicerunner.h"
#include "platform/platform.h"

#include <QDateTime>
#include <QFile>

namespace PicSync {

// Creates an idle runner.
ServiceRunner::ServiceRunner()
{
    connect(&m_pollTimer, &QTimer::timeout, this, &ServiceRunner::poll);
    connect(&m_controller, &SyncController::started, this, &ServiceRunner::onStarted);
    connect(&m_controller, &SyncController::progress, this, &ServiceRunner::onProgress);
    connect(&m_controller, &SyncController::phaseChanged, this, &ServiceRunner::onPhase);
    connect(&m_controller, &SyncController::finished, this, &ServiceRunner::onFinished);
}

// Returns how many synchronisations this runner has started (for tests).
int ServiceRunner::runsStarted() const
{
    return m_runs;
}

// Returns true while a synchronisation is running.
bool ServiceRunner::isRunning() const
{
    return m_controller.isRunning();
}

// Starts the first synchronisation (manual if requested, else scheduled).
void ServiceRunner::start()
{
    SyncProgress::take(SyncProgress::stopRequestPath());   // stale stop request
    runOnce(SyncProgress::take(SyncProgress::manualRequestPath()));
    m_pollTimer.start(500);
}

// Starts one synchronisation; manual runs put the service in the foreground.
void ServiceRunner::runOnce(bool manual)
{
    if (manual && !m_foreground) {
        Platform::enterForeground();
        m_foreground = true;
    }
    m_info = SyncProgressInfo();
    m_info.running = true;
    m_info.manual = manual;
    ++m_runs;
    if (manual)
        m_controller.start();
    else
        m_controller.startScheduled();
}

// Looks for new manual or stop requests from the app.
void ServiceRunner::poll()
{
    if (SyncProgress::take(SyncProgress::stopRequestPath()))
        m_controller.stop();
    // Android expects startForeground() soon after the app started the service.
    if (!m_foreground && QFile::exists(SyncProgress::manualRequestPath())) {
        Platform::enterForeground();
        m_foreground = true;
    }
    if (m_controller.isRunning())
        publish(false);   // keeps the progress file fresh during long transfers
}

// Records that a synchronisation has started.
void ServiceRunner::onStarted()
{
    publish(true);
}

// Records the progress of the running synchronisation.
void ServiceRunner::onProgress(int done, int total, const QString &currentFile)
{
    m_info.done = done;
    m_info.total = total;
    m_info.currentFile = currentFile;
    publish(false);
}

// Records the phase of the running synchronisation.
void ServiceRunner::onPhase(const QString &phase)
{
    m_info.phase = phase;
    publish(true);
}

// Starts the next requested run or finishes the service.
void ServiceRunner::onFinished(const PicSync::SyncResult &result)
{
    Q_UNUSED(result)
    if (SyncProgress::take(SyncProgress::manualRequestPath())) {
        runOnce(true);   // the user asked for a sync while another one was running
        return;
    }
    m_info.running = false;
    publish(true);
    m_pollTimer.stop();
    // The JobService unbinds when it receives this; a manually started service
    // stops itself. Android then ends the ":sync" process.
    Platform::signalServiceFinished();
    if (m_foreground) {
        Platform::leaveForeground();
        m_foreground = false;
    }
    Q_EMIT allDone();
}

// Writes the progress file (at most every 300 ms unless forced).
void ServiceRunner::publish(bool force)
{
    if (!force && m_sinceWrite.isValid() && m_sinceWrite.elapsed() < 300)
        return;
    m_info.updatedMs = QDateTime::currentMSecsSinceEpoch();
    SyncProgress::write(m_info);
    m_sinceWrite.restart();
}

} // namespace PicSync
