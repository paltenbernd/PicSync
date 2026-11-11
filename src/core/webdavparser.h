// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "syncmodel.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace PicSync {

/// One &lt;d:response&gt; element of a WebDAV multistatus answer.
struct DavEntry
{
    QString path;          ///< Decoded URL path, e.g. "/remote.php/dav/files/u/PicSync/a.jpg".
    bool isCollection = false;
    qint64 size = 0;
    qint64 mtime = 0;      ///< Seconds since the epoch.
    QString etag;          ///< Without quotes.
    qint64 quotaUsed = -1;
    qint64 quotaAvailable = -1;
    bool found = true;     ///< False if the server answered 404 for the properties.
};

/// Parser for the XML answers of Nextcloud's WebDAV interface.
class WebDavParser
{
public:
    // Parses a PROPFIND multistatus body. Returns false if the XML is malformed.
    static bool parseMultiStatus(const QByteArray &xml, QList<DavEntry> &entries,
                                 QString &errorMessage);

    // Parses an HTTP date such as "Wed, 11 Nov 2026 20:15:00 GMT" to epoch seconds.
    static qint64 parseHttpDate(const QString &value);

    // Returns the request body used to list a folder.
    static QByteArray propfindBody();

    // Returns the request body used to query the account quota.
    static QByteArray quotaBody();
};

} // namespace PicSync
