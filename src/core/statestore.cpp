// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "statestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace PicSync {

namespace {
const int kFormatVersion = 1;
}

// Creates a store backed by the given JSON file (nothing is read yet).
StateStore::StateStore(const QString &filePath)
    : m_filePath(filePath)
{
}

// Loads the file; a missing file yields an empty state. Returns false on corruption.
bool StateStore::load()
{
    clear();
    QFile file(m_filePath);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return fromJson(file.readAll());
}

// Writes the current content atomically. Returns false on I/O errors.
bool StateStore::save() const
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(toJson());
    return file.commit();
}

// Removes all entries (used when the server or folder configuration changes).
void StateStore::clear()
{
    m_entries.clear();
    m_pending.clear();
    m_fingerprint.clear();
}

// Returns the path of the backing file.
QString StateStore::filePath() const
{
    return m_filePath;
}

// Returns all recorded file states.
const StateMap &StateStore::entries() const
{
    return m_entries;
}

// Returns all pending (delayed) deletions.
const PendingMap &StateStore::pending() const
{
    return m_pending;
}

// Inserts or replaces the recorded state of one file.
void StateStore::setEntry(const StateEntry &entry)
{
    m_entries.insert(entry.relativePath, entry);
}

// Removes the recorded state of one file.
void StateStore::removeEntry(const QString &relativePath)
{
    m_entries.remove(relativePath);
}

// Records a delayed deletion, which locks the file on the other side.
void StateStore::setPending(const PendingDeletion &deletion)
{
    m_pending.insert(deletion.relativePath, deletion);
}

// Removes a delayed deletion.
void StateStore::removePending(const QString &relativePath)
{
    m_pending.remove(relativePath);
}

// Returns the configuration fingerprint the state belongs to.
QString StateStore::fingerprint() const
{
    return m_fingerprint;
}

// Sets the configuration fingerprint (server, user and folder).
void StateStore::setFingerprint(const QString &fingerprint)
{
    m_fingerprint = fingerprint;
}

// Serialises the content to JSON bytes.
QByteArray StateStore::toJson() const
{
    QJsonArray files;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const StateEntry &e = it.value();
        QJsonObject o;
        o.insert(QStringLiteral("path"), e.relativePath);
        o.insert(QStringLiteral("lsize"), double(e.localSize));
        o.insert(QStringLiteral("lmtime"), double(e.localMtime));
        o.insert(QStringLiteral("rsize"), double(e.remoteSize));
        o.insert(QStringLiteral("etag"), e.remoteEtag);
        files.append(o);
    }
    QJsonArray pending;
    for (auto it = m_pending.constBegin(); it != m_pending.constEnd(); ++it) {
        const PendingDeletion &p = it.value();
        QJsonObject o;
        o.insert(QStringLiteral("path"), p.relativePath);
        o.insert(QStringLiteral("side"),
                 p.deletedOn == Side::Local ? QStringLiteral("local") : QStringLiteral("remote"));
        o.insert(QStringLiteral("at"), double(p.detectedAt));
        pending.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), kFormatVersion);
    root.insert(QStringLiteral("fingerprint"), m_fingerprint);
    root.insert(QStringLiteral("files"), files);
    root.insert(QStringLiteral("pending"), pending);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

// Replaces the content from JSON bytes. Returns false if the data is invalid.
bool StateStore::fromJson(const QByteArray &json)
{
    m_entries.clear();
    m_pending.clear();
    m_fingerprint.clear();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kFormatVersion)
        return false;

    m_fingerprint = root.value(QStringLiteral("fingerprint")).toString();

    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &v : files) {
        const QJsonObject o = v.toObject();
        StateEntry e;
        e.relativePath = o.value(QStringLiteral("path")).toString();
        e.localSize = qint64(o.value(QStringLiteral("lsize")).toDouble());
        e.localMtime = qint64(o.value(QStringLiteral("lmtime")).toDouble());
        e.remoteSize = qint64(o.value(QStringLiteral("rsize")).toDouble());
        e.remoteEtag = o.value(QStringLiteral("etag")).toString();
        if (!e.relativePath.isEmpty())
            m_entries.insert(e.relativePath, e);
    }

    const QJsonArray pending = root.value(QStringLiteral("pending")).toArray();
    for (const QJsonValue &v : pending) {
        const QJsonObject o = v.toObject();
        PendingDeletion p;
        p.relativePath = o.value(QStringLiteral("path")).toString();
        p.deletedOn = o.value(QStringLiteral("side")).toString() == QLatin1String("remote")
                ? Side::Remote : Side::Local;
        p.detectedAt = qint64(o.value(QStringLiteral("at")).toDouble());
        if (!p.relativePath.isEmpty())
            m_pending.insert(p.relativePath, p);
    }
    return true;
}

} // namespace PicSync
