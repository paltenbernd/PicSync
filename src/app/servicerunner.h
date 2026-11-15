// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "synccontroller.h"
#include "syncprogress.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace PicSync {

/// Runs synchronisations inside the Android sync service process (":sync").
///
/// The service is started in two ways:
///  - by the JobScheduler (periodic background sync): one scheduled run;
///  - by the app when the user taps "Sync now": the app leaves a request file
///    and starts the service as a foreground service, so Android does not
///    freeze it when the user switches to another app.
/// The runner reports progress through SyncProgress, so the app can show it,
/// and reacts to stop requests from the app.
class ServiceRunner : public QObject
{
    Q_OBJECT
public:
    // Creates an idle runner.
    ServiceRunner();

    // Returns how many synchronisations this runner has started (for tests).
    int runsStarted() const;

    // Returns true while a synchronisation is running.
    bool isRunning() const;

public Q_SLOTS:
    // Starts the first synchronisation (manual if requested, else scheduled).
    void start();

Q_SIGNALS:
    // Emitted when no synchronisation is left to do.
    void allDone();

private Q_SLOTS:
    // Looks for new manual or stop requests from the app.
    void poll();

    // Records that a synchronisation has started.
    void onStarted();

    // Records the progress of the running synchronisation.
    void onProgress(int done, int total, const QString &currentFile);

    // Records the phase of the running synchronisation.
    void onPhase(const QString &phase);

    // Starts the next requested run or finishes the service.
    void onFinished(const PicSync::SyncResult &result);

private:
    // Starts one synchronisation; manual runs put the service in the foreground.
    void runOnce(bool manual);

    // Writes the progress file (at most every 300 ms unless forced).
    void publish(bool force);

    SyncController m_controller;
    QTimer m_pollTimer;
    QElapsedTimer m_sinceWrite;
    SyncProgressInfo m_info;
    bool m_foreground = false;
    int m_runs = 0;
};

} // namespace PicSync
