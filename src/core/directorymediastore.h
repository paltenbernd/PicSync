// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "interfaces.h"

namespace PicSync {

/// LocalMediaStore working on a plain directory tree.
///
/// Used on desktop builds and by the unit tests. The Android build uses the
/// MediaStore based implementation instead.
class DirectoryMediaStore : public LocalMediaStore
{
public:
    // Creates a store rooted at the given directory.
    explicit DirectoryMediaStore(const QString &rootPath);

    // Lists all media files below the root directory.
    bool scan(LocalFileMap &files, QString &error) override;

    // Returns the absolute path of a relative media path.
    QString absolutePath(const QString &relativePath) const override;

    // Creates the parent folders of the given file.
    bool prepareWrite(const QString &relativePath, QString &error) override;

    // Does nothing: a plain directory has no media index.
    void fileWritten(const QString &relativePath) override;

    // Deletes the file.
    bool remove(const QString &relativePath, QString &error) override;

    // Renames the file, creating the target folder if needed.
    bool rename(const QString &fromRelative, const QString &toRelative,
                QString &error) override;

    // Reads size and modification time of one file.
    bool stat(const QString &relativePath, LocalFile &file) const override;

private:
    QString m_root;
};

} // namespace PicSync
