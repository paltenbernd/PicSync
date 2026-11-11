// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "syncplanner.h"

#include <QSet>

#include <algorithm>

namespace PicSync {

namespace {

// Returns the value stored under key as an optional (empty if missing).
template<typename Map>
std::optional<typename Map::mapped_type> lookup(const Map &map, const QString &key)
{
    const auto it = map.constFind(key);
    if (it == map.constEnd())
        return std::nullopt;
    return it.value();
}

// Appends one action to the plan.
void add(SyncPlan &plan, ActionType type, const QString &path)
{
    SyncAction action;
    action.type = type;
    action.relativePath = path;
    plan.append(action);
}

} // namespace

// Computes the actions that bring both sides into agreement.
SyncPlan SyncPlanner::plan(const LocalFileMap &local,
                           const RemoteFileMap &remote,
                           const StateMap &state,
                           const PendingMap &pending)
{
    QSet<QString> paths;
    for (auto it = local.constBegin(); it != local.constEnd(); ++it)
        paths.insert(it.key());
    for (auto it = remote.constBegin(); it != remote.constEnd(); ++it)
        paths.insert(it.key());
    for (auto it = state.constBegin(); it != state.constEnd(); ++it)
        paths.insert(it.key());
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it)
        paths.insert(it.key());

    QStringList sorted(paths.cbegin(), paths.cend());
    std::sort(sorted.begin(), sorted.end());

    SyncPlan result;
    for (const QString &path : std::as_const(sorted)) {
        const std::optional<LocalFile> l = lookup(local, path);
        const std::optional<RemoteFile> r = lookup(remote, path);
        const std::optional<StateEntry> s = lookup(state, path);
        const std::optional<PendingDeletion> p = lookup(pending, path);

        if (!s) {
            // A pending deletion without state is stale; drop it first.
            if (p)
                add(result, ActionType::CancelPending, path);
            planUnknown(path, l, r, result);
        } else {
            planKnown(path, l, r, *s, p, result);
        }
    }
    return result;
}

// Returns true if the local file differs from what was recorded in the state.
bool SyncPlanner::localChanged(const LocalFile &file, const StateEntry &entry)
{
    return file.size != entry.localSize || file.mtime != entry.localMtime;
}

// Returns true if the remote file differs from what was recorded in the state.
bool SyncPlanner::remoteChanged(const RemoteFile &file, const StateEntry &entry)
{
    if (!entry.remoteEtag.isEmpty() && !file.etag.isEmpty())
        return file.etag != entry.remoteEtag;
    return file.size != entry.remoteSize;
}

// Decides what to do with a path that has no recorded state yet.
void SyncPlanner::planUnknown(const QString &path,
                              const std::optional<LocalFile> &local,
                              const std::optional<RemoteFile> &remote,
                              SyncPlan &plan)
{
    if (local && !remote) {
        add(plan, ActionType::Upload, path);
    } else if (!local && remote) {
        add(plan, ActionType::Download, path);
    } else if (local && remote) {
        // Same size on both sides is treated as "already identical" (typical
        // for the very first run after an earlier manual copy).
        if (local->size == remote->size)
            add(plan, ActionType::Adopt, path);
        else
            add(plan, ActionType::Conflict, path);
    }
}

// Decides what to do with a path that was synchronised before.
void SyncPlanner::planKnown(const QString &path,
                            const std::optional<LocalFile> &local,
                            const std::optional<RemoteFile> &remote,
                            const StateEntry &entry,
                            const std::optional<PendingDeletion> &pending,
                            SyncPlan &plan)
{
    const bool lChanged = local && localChanged(*local, entry);
    const bool rChanged = remote && remoteChanged(*remote, entry);

    if (local && remote) {
        // The file exists on both sides (again): a pending deletion is obsolete.
        if (pending)
            add(plan, ActionType::CancelPending, path);
        if (lChanged && rChanged)
            add(plan, ActionType::Conflict, path);
        else if (lChanged)
            add(plan, ActionType::Upload, path);
        else if (rChanged)
            add(plan, ActionType::Download, path);
        return;
    }

    if (!local && remote) {
        const bool deletedHereBefore = pending && pending->deletedOn == Side::Local;
        if (pending && !deletedHereBefore)
            add(plan, ActionType::CancelPending, path);

        if (rChanged) {
            // Changed on the server while deleted on the phone: keep the data.
            if (deletedHereBefore)
                add(plan, ActionType::CancelPending, path);
            add(plan, ActionType::Download, path);
        } else if (deletedHereBefore) {
            add(plan, ActionType::DeleteRemote, path);
        } else {
            add(plan, ActionType::MarkLocalDeleted, path);
        }
        return;
    }

    if (local && !remote) {
        const bool deletedThereBefore = pending && pending->deletedOn == Side::Remote;
        if (pending && !deletedThereBefore)
            add(plan, ActionType::CancelPending, path);

        if (lChanged) {
            // Changed on the phone while deleted on the server: keep the data.
            if (deletedThereBefore)
                add(plan, ActionType::CancelPending, path);
            add(plan, ActionType::Upload, path);
        } else if (deletedThereBefore) {
            add(plan, ActionType::DeleteLocal, path);
        } else {
            add(plan, ActionType::MarkRemoteDeleted, path);
        }
        return;
    }

    // Gone on both sides.
    add(plan, ActionType::Forget, path);
}

} // namespace PicSync
