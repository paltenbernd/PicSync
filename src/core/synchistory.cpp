// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "synchistory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace PicSync {

namespace {

// Converts a result to a JSON object.
QJsonObject toObject(const SyncResult &r)
{
    QJsonObject o;
    o.insert(QStringLiteral("success"), r.success);
    o.insert(QStringLiteral("error"), r.errorMessage);
    o.insert(QStringLiteral("started"), double(r.startedAt));
    o.insert(QStringLiteral("finished"), double(r.finishedAt));
    o.insert(QStringLiteral("manual"), r.manual);
    o.insert(QStringLiteral("up"), r.uploaded);
    o.insert(QStringLiteral("down"), r.downloaded);
    o.insert(QStringLiteral("dell"), r.deletedLocal);
    o.insert(QStringLiteral("delr"), r.deletedRemote);
    o.insert(QStringLiteral("conflicts"), r.conflicts);
    o.insert(QStringLiteral("pending"), r.pendingDeletions);
    o.insert(QStringLiteral("failed"), r.failedFiles);
    o.insert(QStringLiteral("files"), QJsonArray::fromStringList(r.changedFiles));
    o.insert(QStringLiteral("qvalid"), r.quota.valid);
    o.insert(QStringLiteral("qused"), double(r.quota.usedBytes));
    o.insert(QStringLiteral("qavail"), double(r.quota.availableBytes));
    return o;
}

// Reads a result from a JSON object.
SyncResult fromObject(const QJsonObject &o)
{
    SyncResult r;
    r.success = o.value(QStringLiteral("success")).toBool();
    r.errorMessage = o.value(QStringLiteral("error")).toString();
    r.startedAt = qint64(o.value(QStringLiteral("started")).toDouble());
    r.finishedAt = qint64(o.value(QStringLiteral("finished")).toDouble());
    r.manual = o.value(QStringLiteral("manual")).toBool();
    r.uploaded = o.value(QStringLiteral("up")).toInt();
    r.downloaded = o.value(QStringLiteral("down")).toInt();
    r.deletedLocal = o.value(QStringLiteral("dell")).toInt();
    r.deletedRemote = o.value(QStringLiteral("delr")).toInt();
    r.conflicts = o.value(QStringLiteral("conflicts")).toInt();
    r.pendingDeletions = o.value(QStringLiteral("pending")).toInt();
    r.failedFiles = o.value(QStringLiteral("failed")).toInt();
    const QJsonArray files = o.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &v : files)
        r.changedFiles.append(v.toString());
    r.quota.valid = o.value(QStringLiteral("qvalid")).toBool();
    r.quota.usedBytes = qint64(o.value(QStringLiteral("qused")).toDouble());
    r.quota.availableBytes = qint64(o.value(QStringLiteral("qavail")).toDouble(-1));
    return r;
}

} // namespace

// Creates a history backed by the given JSON file.
SyncHistory::SyncHistory(const QString &filePath, int maxEntries)
    : m_filePath(filePath)
    , m_maxEntries(maxEntries)
{
}

// Loads the stored runs (newest first). A missing file yields an empty list.
bool SyncHistory::load()
{
    m_entries.clear();
    QFile file(m_filePath);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return false;
    const QJsonArray array = doc.array();
    for (const QJsonValue &v : array)
        m_entries.append(fromObject(v.toObject()));
    return true;
}

// Prepends a run and writes the file, dropping the oldest entries.
bool SyncHistory::append(const SyncResult &result)
{
    m_entries.prepend(result);
    while (m_entries.size() > m_maxEntries)
        m_entries.removeLast();
    return save();
}

// Returns the stored runs, newest first.
const QList<SyncResult> &SyncHistory::entries() const
{
    return m_entries;
}

// Removes all stored runs.
bool SyncHistory::clear()
{
    m_entries.clear();
    return save();
}

// Writes all entries atomically.
bool SyncHistory::save() const
{
    QJsonArray array;
    for (const SyncResult &r : m_entries)
        array.append(toObject(r));
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    return file.commit();
}

// Converts a result to JSON bytes (one entry).
QByteArray SyncHistory::toJson(const SyncResult &result)
{
    return QJsonDocument(toObject(result)).toJson(QJsonDocument::Compact);
}

// Reads a result from JSON bytes (one entry).
SyncResult SyncHistory::fromJson(const QByteArray &json)
{
    return fromObject(QJsonDocument::fromJson(json).object());
}

} // namespace PicSync
