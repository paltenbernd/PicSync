// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QString>
#include <QtGlobal>

namespace PicSync {

/// Progress of a synchronisation, shared between the background sync service
/// (which writes it) and the app's user interface (which shows it).
struct SyncProgressInfo
{
    bool running = false;
    bool manual = false;
    int done = 0;
    int total = 0;
    QString currentFile;
    QString phase;
    qint64 updatedMs = 0;   ///< Milliseconds since the epoch of the last update.
};

/// Small files in the app's data directory used to talk to the sync service.
namespace SyncProgress {

// Returns the path of the progress file.
QString progressPath();

// Returns the path of the file that requests a manual synchronisation.
QString manualRequestPath();

// Returns the path of the file that asks a running synchronisation to stop.
QString stopRequestPath();

// Writes the progress atomically. Returns false on I/O errors.
bool write(const SyncProgressInfo &info);

// Reads the progress; a missing or broken file means "not running".
SyncProgressInfo read();

// Returns true if the progress says "running" and was updated recently.
bool isActive(const SyncProgressInfo &info, qint64 nowMs, qint64 maxAgeMs = 90000);

// Creates a request/stop file. Returns false on I/O errors.
bool touch(const QString &path);

// Removes a request/stop file; returns true if it existed.
bool take(const QString &path);

} // namespace SyncProgress
} // namespace PicSync
