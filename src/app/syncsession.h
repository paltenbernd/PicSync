// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncengine.h"
#include "syncmodel.h"

#include <QMutex>
#include <QObject>

#include <atomic>
#include <memory>

namespace PicSync {

/// One complete synchronisation as the app and the background service run it:
/// reads settings and login, takes the inter-process lock, runs the engine,
/// writes the history and shows notifications.
class SyncSession : public QObject
{
    Q_OBJECT
public:
    // Creates a session; nothing happens until run() is called.
    SyncSession();

    // Destroys the session.
    ~SyncSession() override;

    // Runs the synchronisation in the calling thread and returns its result.
    SyncResult run(bool manual);

    // Asks a running synchronisation to stop after the current file.
    void requestStop();

    // Returns the file that holds the sync state.
    static QString stateFilePath();

    // Returns the file that holds the sync history.
    static QString historyFilePath();

public Q_SLOTS:
    // Runs a synchronisation started by the user and emits finished().
    void runManual();

    // Runs a background synchronisation and emits finished().
    void runScheduled();

Q_SIGNALS:
    // Progress of the running synchronisation.
    void progress(int done, int total, const QString &currentFile);

    // Current phase of the running synchronisation.
    void phaseChanged(const QString &phase);

    // Emitted by runManual() and runScheduled() when the run is over.
    void finished(const PicSync::SyncResult &result);

private Q_SLOTS:
    // Forwards engine progress to the notification and to listeners.
    void onProgress(int done, int total, const QString &currentFile);

private:
    QMutex m_mutex;
    std::unique_ptr<SyncEngine> m_engine;
    std::atomic_bool m_stopRequested { false };
};

} // namespace PicSync

Q_DECLARE_METATYPE(PicSync::SyncResult)
