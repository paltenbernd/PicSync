// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "interfaces.h"

namespace PicSync {

/// LocalMediaStore backed by Android's MediaStore, i.e. exactly the folders the
/// /e/OS Gallery app shows. File contents are accessed through their paths,
/// which requires the "All files access" permission.
class AndroidMediaStore : public LocalMediaStore
{
public:
    // Creates the store and determines the shared storage root.
    AndroidMediaStore();

    // Lists all images and videos known to the MediaStore (trashed files excluded).
    bool scan(LocalFileMap &files, QString &error) override;

    // Returns the absolute path below the shared storage root.
    QString absolutePath(const QString &relativePath) const override;

    // Creates the parent folders of a file that is about to be downloaded.
    bool prepareWrite(const QString &relativePath, QString &error) override;

    // Asks the media scanner to index a new or changed file.
    void fileWritten(const QString &relativePath) override;

    // Moves a file to the gallery's trash (or deletes it if that is not possible).
    bool remove(const QString &relativePath, QString &error) override;

    // Renames a file and updates the media index.
    bool rename(const QString &fromRelative, const QString &toRelative,
                QString &error) override;

    // Reads size and modification time of one file.
    bool stat(const QString &relativePath, LocalFile &file) const override;

private:
    // Asks the media scanner to (re-)index the given absolute paths.
    static void scanPaths(const QStringList &absolutePaths);

    // Tries to move a file into the MediaStore trash; returns true on success.
    bool moveToTrash(const QString &relativePath) const;

    QString m_root;
};

} // namespace PicSync
