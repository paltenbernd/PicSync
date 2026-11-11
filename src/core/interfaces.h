// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncmodel.h"

#include <QString>

namespace PicSync {

/// Access to the phone's gallery media. Implemented on Android via MediaStore
/// and on desktop (and in tests) via a plain directory tree.
class LocalMediaStore
{
public:
    // Destroys the store.
    virtual ~LocalMediaStore() = default;

    // Lists all gallery media files. Returns false (and sets error) on failure.
    virtual bool scan(LocalFileMap &files, QString &error) = 0;

    // Returns the absolute file system path of a relative media path.
    virtual QString absolutePath(const QString &relativePath) const = 0;

    // Creates the parent folders needed to write the given file.
    virtual bool prepareWrite(const QString &relativePath, QString &error) = 0;

    // Informs the gallery that a file was created or changed.
    virtual void fileWritten(const QString &relativePath) = 0;

    // Deletes a media file (moving it to the gallery's trash where possible).
    virtual bool remove(const QString &relativePath, QString &error) = 0;

    // Renames a media file within the gallery.
    virtual bool rename(const QString &fromRelative, const QString &toRelative,
                        QString &error) = 0;

    // Reads size and modification time of one file as the scan would report them.
    virtual bool stat(const QString &relativePath, LocalFile &file) const = 0;
};

/// Access to the Nextcloud folder of this device.
class RemoteStore
{
public:
    /// Outcome of listing the device folder.
    enum class ListStatus { Ok, RootMissing, Error };

    // Destroys the store.
    virtual ~RemoteStore() = default;

    // Lists all media files below the device folder (recursively).
    virtual ListStatus list(RemoteFileMap &files, QString &error) = 0;

    // Creates the device folder (and all parents) on the server.
    virtual bool createRoot(QString &error) = 0;

    // Uploads a local file; on success "result" describes the new remote file.
    virtual bool upload(const QString &relativePath, const QString &localPath,
                        qint64 mtime, RemoteFile &result, QString &error) = 0;

    // Downloads a remote file to the given local path; "file" describes what was fetched.
    virtual bool download(const QString &relativePath, const QString &localPath,
                          RemoteFile &file, QString &error) = 0;

    // Deletes a remote file (it goes to the Nextcloud trash bin).
    virtual bool remove(const QString &relativePath, QString &error) = 0;

    // Queries the storage quota of the account.
    virtual bool quota(QuotaInfo &info, QString &error) = 0;

    // Checks that server address and credentials are valid.
    virtual bool testConnection(QString &error) = 0;
};

/// Persistent, protected storage of the Nextcloud login.
class CredentialStore
{
public:
    // Destroys the store.
    virtual ~CredentialStore() = default;

    // Stores the app password for a user. Returns false on failure.
    virtual bool save(const QString &user, const QString &password) = 0;

    // Loads the stored app password of a user. Returns false if none is stored.
    virtual bool load(const QString &user, QString &password) = 0;

    // Deletes the stored credentials.
    virtual void clear() = 0;
};

} // namespace PicSync
