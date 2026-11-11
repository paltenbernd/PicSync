// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "interfaces.h"
#include "statestore.h"
#include "syncmodel.h"

#include <QObject>

#include <atomic>

namespace PicSync {

/// Runs one complete two-way synchronisation.
///
/// The engine scans both sides, lets the SyncPlanner decide, performs the
/// resulting actions and keeps the StateStore up to date after every step,
/// so an interrupted run can be resumed safely.
class SyncEngine : public QObject
{
    Q_OBJECT
public:
    // Creates an engine working on the given stores (which must outlive it).
    SyncEngine(LocalMediaStore &local, RemoteStore &remote, StateStore &state);

    // Performs one synchronisation run and returns its summary.
    SyncResult run(bool manual, const QString &configFingerprint);

    // Asks a running synchronisation to stop after the current file.
    void requestStop();

    // Maximum number of entries kept in SyncResult::changedFiles.
    static constexpr int kMaxChangedFiles = 200;

Q_SIGNALS:
    // Reports progress: number of finished steps, total steps and current file.
    void progress(int done, int total, const QString &currentFile);

    // Reports the current phase in a human readable form.
    void phaseChanged(const QString &phase);

private:
    // Executes a single planned action; returns false if the file failed.
    bool execute(const SyncAction &action, const LocalFileMap &local,
                 const RemoteFileMap &remote, SyncResult &result, QString &error);

    // Uploads one local file and records the new state.
    bool doUpload(const QString &path, QString &error);

    // Downloads one remote file and records the new state.
    bool doDownload(const QString &path, QString &error);

    // Keeps both versions of a file that changed on both sides.
    bool doConflict(const QString &path, QString &error);

    // Records the state of a file that is identical on both sides.
    void recordState(const LocalFile &local, const RemoteFile &remote);

    // Adds a line to the list of changed files (respecting the cap).
    static void note(SyncResult &result, const QString &line);

    // Returns true if the action moves or deletes data (used for progress).
    static bool isWork(ActionType type);

    LocalMediaStore &m_local;
    RemoteStore &m_remote;
    StateStore &m_state;
    std::atomic_bool m_stop { false };
};

} // namespace PicSync
