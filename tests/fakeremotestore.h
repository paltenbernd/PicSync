// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "interfaces.h"

#include <QSet>

/// RemoteStore for tests that keeps the "server" in a local directory.
/// ETags are content hashes, just like a real server changes them on edits.
class FakeRemoteStore : public PicSync::RemoteStore
{
public:
    // Creates a fake server whose device folder is the given directory.
    explicit FakeRemoteStore(const QString &rootPath);

    // Lists all media files below the folder.
    ListStatus list(PicSync::RemoteFileMap &files, QString &error) override;

    // Creates the folder.
    bool createRoot(QString &error) override;

    // Copies a local file into the folder.
    bool upload(const QString &relativePath, const QString &localPath, qint64 mtime,
                PicSync::RemoteFile &result, QString &error) override;

    // Copies a file from the folder to a local path.
    bool download(const QString &relativePath, const QString &localPath,
                  PicSync::RemoteFile &file, QString &error) override;

    // Deletes a file from the folder.
    bool remove(const QString &relativePath, QString &error) override;

    // Reports a fixed quota.
    bool quota(PicSync::QuotaInfo &info, QString &error) override;

    // Always succeeds unless failures are simulated.
    bool testConnection(QString &error) override;

    // Writes a file directly on the "server" (simulates another client).
    void put(const QString &relativePath, const QByteArray &content);

    // Deletes a file directly on the "server" (simulates the web interface).
    void erase(const QString &relativePath);

    // Returns the content of a file on the "server" (empty if missing).
    QByteArray content(const QString &relativePath) const;

    // Returns true if the file exists on the "server".
    bool exists(const QString &relativePath) const;

    // Makes every following request fail (simulates a network outage).
    void setOffline(bool offline);

    // Number of uploads performed so far.
    int uploads = 0;

    // Number of downloads performed so far.
    int downloads = 0;

private:
    // Returns the absolute path of a file on the "server".
    QString path(const QString &relativePath) const;

    // Describes a file on the "server" like a PROPFIND would.
    PicSync::RemoteFile describe(const QString &relativePath) const;

    QString m_root;
    bool m_offline = false;
};
