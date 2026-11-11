// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "fakeremotestore.h"
#include "pathutils.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

using namespace PicSync;

// Creates a fake server whose device folder is the given directory.
FakeRemoteStore::FakeRemoteStore(const QString &rootPath)
    : m_root(rootPath)
{
}

// Returns the absolute path of a file on the "server".
QString FakeRemoteStore::path(const QString &relativePath) const
{
    return m_root + QLatin1Char('/') + PathUtils::normalizeRelative(relativePath);
}

// Describes a file on the "server" like a PROPFIND would.
RemoteFile FakeRemoteStore::describe(const QString &relativePath) const
{
    RemoteFile f;
    const QFileInfo info(path(relativePath));
    f.relativePath = PathUtils::normalizeRelative(relativePath);
    f.size = info.size();
    f.mtime = info.lastModified().toSecsSinceEpoch();
    f.etag = QString::fromLatin1(
        QCryptographicHash::hash(content(relativePath), QCryptographicHash::Md5).toHex());
    return f;
}

// Lists all media files below the folder.
RemoteStore::ListStatus FakeRemoteStore::list(RemoteFileMap &files, QString &error)
{
    files.clear();
    if (m_offline) {
        error = QStringLiteral("offline");
        return ListStatus::Error;
    }
    QDir root(m_root);
    if (!root.exists())
        return ListStatus::RootMissing;
    QDirIterator it(m_root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = root.relativeFilePath(it.filePath());
        if (!PathUtils::isIgnored(rel) && PathUtils::isMediaFile(rel))
            files.insert(rel, describe(rel));
    }
    return ListStatus::Ok;
}

// Creates the folder.
bool FakeRemoteStore::createRoot(QString &error)
{
    Q_UNUSED(error)
    return QDir().mkpath(m_root);
}

// Copies a local file into the folder.
bool FakeRemoteStore::upload(const QString &relativePath, const QString &localPath,
                             qint64 mtime, RemoteFile &result, QString &error)
{
    Q_UNUSED(mtime)
    if (m_offline) {
        error = QStringLiteral("offline");
        return false;
    }
    QFile source(localPath);
    if (!source.open(QIODevice::ReadOnly)) {
        error = source.errorString();
        return false;
    }
    put(relativePath, source.readAll());
    ++uploads;
    result = describe(relativePath);
    return true;
}

// Copies a file from the folder to a local path.
bool FakeRemoteStore::download(const QString &relativePath, const QString &localPath,
                               RemoteFile &file, QString &error)
{
    if (m_offline || !exists(relativePath)) {
        error = QStringLiteral("not available");
        return false;
    }
    QFile target(localPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        error = target.errorString();
        return false;
    }
    target.write(content(relativePath));
    target.close();
    ++downloads;
    file = describe(relativePath);
    return true;
}

// Deletes a file from the folder.
bool FakeRemoteStore::remove(const QString &relativePath, QString &error)
{
    if (m_offline) {
        error = QStringLiteral("offline");
        return false;
    }
    erase(relativePath);
    return true;
}

// Reports a fixed quota.
bool FakeRemoteStore::quota(QuotaInfo &info, QString &error)
{
    Q_UNUSED(error)
    info.valid = true;
    info.usedBytes = 1000;
    info.availableBytes = 9000;
    return !m_offline;
}

// Always succeeds unless failures are simulated.
bool FakeRemoteStore::testConnection(QString &error)
{
    if (m_offline)
        error = QStringLiteral("offline");
    return !m_offline;
}

// Writes a file directly on the "server" (simulates another client).
void FakeRemoteStore::put(const QString &relativePath, const QByteArray &data)
{
    const QString target = path(relativePath);
    QDir().mkpath(QFileInfo(target).absolutePath());
    QFile file(target);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(data);
}

// Deletes a file directly on the "server" (simulates the web interface).
void FakeRemoteStore::erase(const QString &relativePath)
{
    QFile::remove(path(relativePath));
}

// Returns the content of a file on the "server" (empty if missing).
QByteArray FakeRemoteStore::content(const QString &relativePath) const
{
    QFile file(path(relativePath));
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

// Returns true if the file exists on the "server".
bool FakeRemoteStore::exists(const QString &relativePath) const
{
    return QFileInfo::exists(path(relativePath));
}

// Makes every following request fail (simulates a network outage).
void FakeRemoteStore::setOffline(bool offline)
{
    m_offline = offline;
}
