// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace PicSync {

/// A media file as seen on the phone (gallery / MediaStore).
struct LocalFile
{
    QString relativePath;   ///< Path relative to the shared storage root, e.g. "DCIM/Camera/a.jpg".
    qint64 size = 0;        ///< File size in bytes.
    qint64 mtime = 0;       ///< Modification time in seconds since the epoch.
};

/// A media file as seen in the Nextcloud folder of this device.
struct RemoteFile
{
    QString relativePath;   ///< Path relative to the device folder on the server.
    qint64 size = 0;        ///< File size in bytes.
    qint64 mtime = 0;       ///< Modification time in seconds since the epoch.
    QString etag;           ///< Server ETag, changes whenever the content changes.
};

/// What both sides looked like after the last successful transfer of a file.
struct StateEntry
{
    QString relativePath;
    qint64 localSize = 0;
    qint64 localMtime = 0;
    qint64 remoteSize = 0;
    QString remoteEtag;
};

/// Side of the synchronisation.
enum class Side { Local, Remote };

/// A deletion that was detected but is deliberately delayed until the next run.
/// While it exists, the file on the other side is locked.
struct PendingDeletion
{
    QString relativePath;
    Side deletedOn = Side::Local;   ///< The side on which the user deleted the file.
    qint64 detectedAt = 0;          ///< Seconds since the epoch.
};

/// Kinds of steps the planner can ask the engine to perform.
enum class ActionType {
    Upload,               ///< Copy the local file to the server.
    Download,             ///< Copy the remote file to the phone.
    DeleteLocal,          ///< Delete the file on the phone (deletion was pending).
    DeleteRemote,         ///< Delete the file on the server (deletion was pending).
    MarkLocalDeleted,     ///< Remember a local deletion and lock the remote copy.
    MarkRemoteDeleted,    ///< Remember a remote deletion and lock the local copy.
    CancelPending,        ///< Forget a pending deletion (file came back or changed).
    Conflict,             ///< Both sides changed: keep both versions.
    Adopt,                ///< Both sides already equal: just record the state.
    Forget                ///< File is gone on both sides: drop the state entry.
};

/// One step of a synchronisation plan.
struct SyncAction
{
    ActionType type = ActionType::Adopt;
    QString relativePath;

    /// Compares two actions, used by the unit tests.
    bool operator==(const SyncAction &other) const
    {
        return type == other.type && relativePath == other.relativePath;
    }
};

using LocalFileMap = QHash<QString, LocalFile>;
using RemoteFileMap = QHash<QString, RemoteFile>;
using StateMap = QHash<QString, StateEntry>;
using PendingMap = QHash<QString, PendingDeletion>;
using SyncPlan = QList<SyncAction>;

/// Storage usage of the Nextcloud account.
struct QuotaInfo
{
    bool valid = false;
    qint64 usedBytes = 0;
    qint64 availableBytes = -1;   ///< -1 if the server reports "unlimited".
};

/// Summary of one synchronisation run.
struct SyncResult
{
    bool success = false;
    QString errorMessage;
    qint64 startedAt = 0;
    qint64 finishedAt = 0;
    bool manual = false;
    int uploaded = 0;
    int downloaded = 0;
    int deletedLocal = 0;
    int deletedRemote = 0;
    int conflicts = 0;
    int pendingDeletions = 0;
    int failedFiles = 0;
    QStringList changedFiles;   ///< Human readable list, capped in length.
    QuotaInfo quota;            ///< Account usage measured at the end of the run.
};

} // namespace PicSync
