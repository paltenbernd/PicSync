// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "directorymediastore.h"
#include "pathutils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace PicSync {

// Creates a store rooted at the given directory.
DirectoryMediaStore::DirectoryMediaStore(const QString &rootPath)
    : m_root(QDir(rootPath).absolutePath())
{
}

// Lists all media files below the root directory.
bool DirectoryMediaStore::scan(LocalFileMap &files, QString &error)
{
    files.clear();
    QDir root(m_root);
    if (!root.exists()) {
        error = QStringLiteral("Folder %1 does not exist").arg(m_root);
        return false;
    }
    QDirIterator it(m_root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = root.relativeFilePath(it.filePath());
        if (PathUtils::isIgnored(rel) || !PathUtils::isMediaFile(rel))
            continue;
        LocalFile file;
        if (stat(rel, file))
            files.insert(rel, file);
    }
    return true;
}

// Returns the absolute path of a relative media path.
QString DirectoryMediaStore::absolutePath(const QString &relativePath) const
{
    return m_root + QLatin1Char('/') + PathUtils::normalizeRelative(relativePath);
}

// Creates the parent folders of the given file.
bool DirectoryMediaStore::prepareWrite(const QString &relativePath, QString &error)
{
    const QString dir = QFileInfo(absolutePath(relativePath)).absolutePath();
    if (QDir().mkpath(dir))
        return true;
    error = QStringLiteral("Cannot create folder %1").arg(dir);
    return false;
}

// Does nothing: a plain directory has no media index.
void DirectoryMediaStore::fileWritten(const QString &relativePath)
{
    Q_UNUSED(relativePath)
}

// Deletes the file.
bool DirectoryMediaStore::remove(const QString &relativePath, QString &error)
{
    QFile file(absolutePath(relativePath));
    if (!file.exists() || file.remove())
        return true;
    error = file.errorString();
    return false;
}

// Renames the file, creating the target folder if needed.
bool DirectoryMediaStore::rename(const QString &fromRelative, const QString &toRelative,
                                 QString &error)
{
    if (!prepareWrite(toRelative, error))
        return false;
    QFile file(absolutePath(fromRelative));
    if (file.rename(absolutePath(toRelative)))
        return true;
    error = file.errorString();
    return false;
}

// Reads size and modification time of one file.
bool DirectoryMediaStore::stat(const QString &relativePath, LocalFile &file) const
{
    const QFileInfo info(absolutePath(relativePath));
    if (!info.exists() || !info.isFile())
        return false;
    file.relativePath = PathUtils::normalizeRelative(relativePath);
    file.size = info.size();
    file.mtime = info.lastModified().toSecsSinceEpoch();
    return true;
}

} // namespace PicSync
