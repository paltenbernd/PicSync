// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QDateTime>
#include <QString>
#include <QUrl>

namespace PicSync {

/// Helpers for building Nextcloud URLs and file names.
namespace PathUtils {

// Returns true if the file name has an image or video extension the gallery shows.
bool isMediaFile(const QString &fileName);

// Returns true for files PicSync must never synchronise (hidden, temporary, ...).
bool isIgnored(const QString &relativePath);

// Turns a device name into a string that is safe as a single folder name.
QString sanitizeFolderName(const QString &name);

// Returns the default remote folder for a device, e.g. "PicSync/FP6 Peter".
QString defaultRemoteFolder(const QString &deviceName);

// Removes duplicate, leading and trailing slashes from a relative path.
QString normalizeRelative(const QString &path);

// Joins two relative path parts with exactly one slash.
QString join(const QString &a, const QString &b);

// Returns the parent folder of a relative path ("" for top-level entries).
QString parentPath(const QString &relativePath);

// Returns the gallery album a folder belongs to, for the folder overview:
// the first folder below the standard Android folders (DCIM, Pictures, ...),
// otherwise the top-level folder. "DCIM/Camera" -> "DCIM/Camera",
// "apple/2026/09" -> "apple", "Pictures/Signal/2024" -> "Pictures/Signal",
// "Android/media/com.whatsapp/WhatsApp/Media/..." -> ".../com.whatsapp/WhatsApp".
QString galleryGroup(const QString &relativeFolder);

// Percent-encodes every segment of a relative path for use in a URL.
QString encodePath(const QString &relativePath);

// Returns the WebDAV files root of a user, e.g. https://host/remote.php/dav/files/user/.
QUrl davFilesRoot(const QString &serverUrl, const QString &user);

// Returns the legacy WebDAV root https://host/remote.php/webdav/ (used e.g. by MagentaCLOUD).
QUrl davLegacyRoot(const QString &serverUrl);

// Returns the URL of a path below the WebDAV files root.
QUrl davUrl(const QUrl &filesRoot, const QString &relativePath);

// Adds https:// if the user typed only a host name and strips trailing slashes.
QString normalizeServerUrl(const QString &input);

// Builds the "keep both" file name for a conflict, next to the original.
QString conflictName(const QString &relativePath, const QDateTime &when);

// Strips surrounding quotes and weak-validator prefixes from an ETag.
QString normalizeEtag(const QString &etag);

// Returns a fingerprint identifying server, user and folder (for state resets).
QString configFingerprint(const QString &serverUrl, const QString &user,
                          const QString &remoteFolder);

} // namespace PathUtils
} // namespace PicSync
