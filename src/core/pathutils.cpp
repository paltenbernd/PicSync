// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "pathutils.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace PicSync {
namespace PathUtils {

namespace {

// Returns the set of lower-case extensions treated as gallery media.
const QSet<QString> &mediaExtensions()
{
    static const QSet<QString> extensions = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("heic"),
        QStringLiteral("heif"), QStringLiteral("avif"), QStringLiteral("bmp"),
        QStringLiteral("dng"), QStringLiteral("raw"), QStringLiteral("mp4"),
        QStringLiteral("m4v"), QStringLiteral("mov"), QStringLiteral("3gp"),
        QStringLiteral("3g2"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("avi")
    };
    return extensions;
}

} // namespace

// Returns true if the file name has an image or video extension the gallery shows.
bool isMediaFile(const QString &fileName)
{
    return mediaExtensions().contains(QFileInfo(fileName).suffix().toLower());
}

// Returns true for files PicSync must never synchronise (hidden, temporary, ...).
bool isIgnored(const QString &relativePath)
{
    const QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.startsWith(QLatin1Char('.')))
            return true;   // hidden files, ".trashed-…", ".pending-…", ".thumbnails"
    }
    return relativePath.endsWith(QLatin1String(".picsync-part"));
}

// Turns a device name into a string that is safe as a single folder name.
QString sanitizeFolderName(const QString &name)
{
    QString result = name;
    static const QRegularExpression forbidden(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]"));
    result.replace(forbidden, QStringLiteral("_"));
    result = result.trimmed();
    while (result.startsWith(QLatin1Char('.')))
        result.remove(0, 1);
    if (result.isEmpty())
        result = QStringLiteral("Fairphone");
    return result;
}

// Returns the default remote folder for a device, e.g. "PicSync/FP6 Peter".
QString defaultRemoteFolder(const QString &deviceName)
{
    return join(QStringLiteral("PicSync"), sanitizeFolderName(deviceName));
}

// Removes duplicate, leading and trailing slashes from a relative path.
QString normalizeRelative(const QString &path)
{
    QString p = path;
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QStringList parts = p.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.join(QLatin1Char('/'));
}

// Joins two relative path parts with exactly one slash.
QString join(const QString &a, const QString &b)
{
    const QString na = normalizeRelative(a);
    const QString nb = normalizeRelative(b);
    if (na.isEmpty())
        return nb;
    if (nb.isEmpty())
        return na;
    return na + QLatin1Char('/') + nb;
}

// Returns the parent folder of a relative path ("" for top-level entries).
QString parentPath(const QString &relativePath)
{
    const QString p = normalizeRelative(relativePath);
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : p.left(slash);
}

// Percent-encodes every segment of a relative path for use in a URL.
QString encodePath(const QString &relativePath)
{
    const QStringList parts = normalizeRelative(relativePath).split(QLatin1Char('/'),
                                                                    Qt::SkipEmptyParts);
    QStringList encoded;
    for (const QString &part : parts)
        encoded << QString::fromLatin1(QUrl::toPercentEncoding(part));
    return encoded.join(QLatin1Char('/'));
}

// Returns the WebDAV files root of a user, e.g. https://host/remote.php/dav/files/user/.
QUrl davFilesRoot(const QString &serverUrl, const QString &user)
{
    const QString base = normalizeServerUrl(serverUrl);
    return QUrl(base + QStringLiteral("/remote.php/dav/files/")
                + QString::fromLatin1(QUrl::toPercentEncoding(user)) + QLatin1Char('/'),
                QUrl::StrictMode);
}

// Returns the legacy WebDAV root https://host/remote.php/webdav/ (used e.g. by MagentaCLOUD).
QUrl davLegacyRoot(const QString &serverUrl)
{
    return QUrl(normalizeServerUrl(serverUrl) + QStringLiteral("/remote.php/webdav/"),
                QUrl::StrictMode);
}

// Returns the URL of a path below the WebDAV files root.
QUrl davUrl(const QUrl &filesRoot, const QString &relativePath)
{
    QString root = filesRoot.toString(QUrl::FullyEncoded);
    if (!root.endsWith(QLatin1Char('/')))
        root += QLatin1Char('/');
    return QUrl(root + encodePath(relativePath), QUrl::StrictMode);
}

// Adds https:// if the user typed only a host name and strips trailing slashes.
QString normalizeServerUrl(const QString &input)
{
    QString url = input.trimmed();
    if (url.isEmpty())
        return url;
    if (!url.contains(QStringLiteral("://")))
        url.prepend(QStringLiteral("https://"));
    while (url.endsWith(QLatin1Char('/')))
        url.chop(1);
    // Users sometimes paste the full WebDAV address; reduce it to the server base.
    const int dav = url.indexOf(QStringLiteral("/remote.php/"));
    if (dav > 0)
        url.truncate(dav);
    const int index = url.indexOf(QStringLiteral("/index.php"));
    if (index > 0)
        url.truncate(index);
    return url;
}

// Builds the "keep both" file name for a conflict, next to the original.
QString conflictName(const QString &relativePath, const QDateTime &when)
{
    const QString parent = parentPath(relativePath);
    const QString fileName = normalizeRelative(relativePath).section(QLatin1Char('/'), -1);
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    const QString stem = dot > 0 ? fileName.left(dot) : fileName;
    const QString ext = dot > 0 ? fileName.mid(dot) : QString();
    const QString stamp = when.toString(QStringLiteral("yyyy-MM-dd HHmmss"));
    return join(parent, stem + QStringLiteral(" (conflicted copy ") + stamp
                + QLatin1Char(')') + ext);
}

// Strips surrounding quotes and weak-validator prefixes from an ETag.
QString normalizeEtag(const QString &etag)
{
    QString e = etag.trimmed();
    if (e.startsWith(QLatin1String("W/")))
        e.remove(0, 2);
    if (e.size() >= 2 && e.startsWith(QLatin1Char('"')) && e.endsWith(QLatin1Char('"')))
        e = e.mid(1, e.size() - 2);
    return e;
}

// Returns a fingerprint identifying server, user and folder (for state resets).
QString configFingerprint(const QString &serverUrl, const QString &user,
                          const QString &remoteFolder)
{
    return normalizeServerUrl(serverUrl).toLower() + QLatin1Char('|') + user
            + QLatin1Char('|') + normalizeRelative(remoteFolder);
}

} // namespace PathUtils
} // namespace PicSync
