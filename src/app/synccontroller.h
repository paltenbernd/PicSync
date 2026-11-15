// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncsession.h"

#include <QObject>
#include <QThread>

#include <memory>

namespace PicSync {

/// Runs manual synchronisations in a worker thread, so the UI stays responsive.
class SyncController : public QObject
{
    Q_OBJECT
public:
    // Creates an idle controller.
    SyncController();

    // Stops a running synchronisation and waits for the worker thread.
    ~SyncController() override;

    // Returns true while a synchronisation is running.
    bool isRunning() const;

public Q_SLOTS:
    // Starts a manual synchronisation unless one is already running.
    void start();

    // Starts a background (scheduled) synchronisation unless one is already running.
    void startScheduled();

    // Asks the running synchronisation to stop after the current file.
    void stop();

Q_SIGNALS:
    // Emitted when a synchronisation starts.
    void started();

    // Progress of the running synchronisation.
    void progress(int done, int total, const QString &currentFile);

    // Current phase of the running synchronisation.
    void phaseChanged(const QString &phase);

    // Emitted with the result when the synchronisation is over.
    void finished(const PicSync::SyncResult &result);

private Q_SLOTS:
    // Cleans up the worker thread and forwards the result.
    void onSessionFinished(const PicSync::SyncResult &result);

private:
    // Starts a synchronisation in the worker thread.
    void begin(bool manual);

    // Ends the worker thread and releases the session.
    void shutdown();

    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<SyncSession> m_session;
};

} // namespace PicSync
