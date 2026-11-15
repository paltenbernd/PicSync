// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "syncprogress.h"
#include "platform/platform.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace PicSync {
namespace SyncProgress {

// Returns the path of the progress file.
QString progressPath()
{
    return Platform::dataDirectory() + QStringLiteral("/progress.json");
}

// Returns the path of the file that requests a manual synchronisation.
QString manualRequestPath()
{
    return Platform::dataDirectory() + QStringLiteral("/manual-sync.request");
}

// Returns the path of the file that asks a running synchronisation to stop.
QString stopRequestPath()
{
    return Platform::dataDirectory() + QStringLiteral("/stop-sync.request");
}

// Writes the progress atomically. Returns false on I/O errors.
bool write(const SyncProgressInfo &info)
{
    QJsonObject o;
    o.insert(QStringLiteral("running"), info.running);
    o.insert(QStringLiteral("manual"), info.manual);
    o.insert(QStringLiteral("done"), info.done);
    o.insert(QStringLiteral("total"), info.total);
    o.insert(QStringLiteral("file"), info.currentFile);
    o.insert(QStringLiteral("phase"), info.phase);
    o.insert(QStringLiteral("updated"), double(info.updatedMs));
    QSaveFile file(progressPath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return file.commit();
}

// Reads the progress; a missing or broken file means "not running".
SyncProgressInfo read()
{
    SyncProgressInfo info;
    QFile file(progressPath());
    if (!file.open(QIODevice::ReadOnly))
        return info;
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    info.running = o.value(QStringLiteral("running")).toBool();
    info.manual = o.value(QStringLiteral("manual")).toBool();
    info.done = o.value(QStringLiteral("done")).toInt();
    info.total = o.value(QStringLiteral("total")).toInt();
    info.currentFile = o.value(QStringLiteral("file")).toString();
    info.phase = o.value(QStringLiteral("phase")).toString();
    info.updatedMs = qint64(o.value(QStringLiteral("updated")).toDouble());
    return info;
}

// Returns true if the progress says "running" and was updated recently.
bool isActive(const SyncProgressInfo &info, qint64 nowMs, qint64 maxAgeMs)
{
    return info.running && nowMs - info.updatedMs <= maxAgeMs;
}

// Creates a request/stop file. Returns false on I/O errors.
bool touch(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

// Removes a request/stop file; returns true if it existed.
bool take(const QString &path)
{
    return QFile::exists(path) && QFile::remove(path);
}

} // namespace SyncProgress
} // namespace PicSync
