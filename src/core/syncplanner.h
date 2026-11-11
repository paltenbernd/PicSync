// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncmodel.h"

#include <optional>

namespace PicSync {

/// Pure decision logic of the two-way synchronisation.
///
/// The planner compares the current local listing, the current remote listing
/// and the state recorded after the previous run. It never touches files; it
/// only returns the list of actions the engine has to perform.
///
/// Deletions are delayed by one run: the run that detects a deletion only
/// records it (and thereby locks the file on the other side); the following
/// run executes it, unless the file came back or was changed in the meantime.
class SyncPlanner
{
public:
    // Computes the actions that bring both sides into agreement.
    static SyncPlan plan(const LocalFileMap &local,
                         const RemoteFileMap &remote,
                         const StateMap &state,
                         const PendingMap &pending);

    // Returns true if the local file differs from what was recorded in the state.
    static bool localChanged(const LocalFile &file, const StateEntry &entry);

    // Returns true if the remote file differs from what was recorded in the state.
    static bool remoteChanged(const RemoteFile &file, const StateEntry &entry);

private:
    // Decides what to do with a path that has no recorded state yet.
    static void planUnknown(const QString &path,
                            const std::optional<LocalFile> &local,
                            const std::optional<RemoteFile> &remote,
                            SyncPlan &plan);

    // Decides what to do with a path that was synchronised before.
    static void planKnown(const QString &path,
                          const std::optional<LocalFile> &local,
                          const std::optional<RemoteFile> &remote,
                          const StateEntry &entry,
                          const std::optional<PendingDeletion> &pending,
                          SyncPlan &plan);
};

} // namespace PicSync
