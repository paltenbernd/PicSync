// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "webdavparser.h"
#include "pathutils.h"

#include <QDateTime>
#include <QLocale>
#include <QTimeZone>
#include <QUrl>
#include <QXmlStreamReader>

namespace PicSync {

namespace {

const QString kDav = QStringLiteral("DAV:");

// Collects the properties of one <d:propstat> block before they are applied.
struct PropBlock
{
    bool ok = false;
    bool hasCollection = false;
    QString etag;
    QString length;
    QString modified;
    QString quotaUsed;
    QString quotaAvailable;
};

// Reads the text content of the current element (including nested text).
QString readText(QXmlStreamReader &xml)
{
    return xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
}

// Parses one <d:prop> element into the given block.
void readProp(QXmlStreamReader &xml, PropBlock &block)
{
    while (xml.readNextStartElement()) {
        const QStringView name = xml.name();
        if (name == QLatin1String("getetag")) {
            block.etag = readText(xml);
        } else if (name == QLatin1String("getcontentlength")) {
            block.length = readText(xml);
        } else if (name == QLatin1String("getlastmodified")) {
            block.modified = readText(xml);
        } else if (name == QLatin1String("quota-used-bytes")) {
            block.quotaUsed = readText(xml);
        } else if (name == QLatin1String("quota-available-bytes")) {
            block.quotaAvailable = readText(xml);
        } else if (name == QLatin1String("resourcetype")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("collection"))
                    block.hasCollection = true;
                xml.skipCurrentElement();
            }
        } else {
            xml.skipCurrentElement();
        }
    }
}

// Parses one <d:propstat> element.
PropBlock readPropstat(QXmlStreamReader &xml)
{
    PropBlock block;
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("prop"))
            readProp(xml, block);
        else if (xml.name() == QLatin1String("status"))
            block.ok = readText(xml).contains(QLatin1String(" 200 "));
        else
            xml.skipCurrentElement();
    }
    return block;
}

// Applies the properties of a successful propstat block to an entry.
void apply(const PropBlock &block, DavEntry &entry)
{
    if (!block.ok)
        return;
    if (block.hasCollection)
        entry.isCollection = true;
    if (!block.etag.isEmpty())
        entry.etag = PathUtils::normalizeEtag(block.etag);
    if (!block.length.isEmpty())
        entry.size = block.length.toLongLong();
    if (!block.modified.isEmpty())
        entry.mtime = WebDavParser::parseHttpDate(block.modified);
    if (!block.quotaUsed.isEmpty())
        entry.quotaUsed = block.quotaUsed.toLongLong();
    if (!block.quotaAvailable.isEmpty())
        entry.quotaAvailable = block.quotaAvailable.toLongLong();
}

// Turns an href (path or absolute URL, percent-encoded) into a decoded path.
QString decodeHref(const QString &href)
{
    QString path = href;
    if (path.startsWith(QLatin1String("http://")) || path.startsWith(QLatin1String("https://")))
        path = QUrl(path).path(QUrl::FullyEncoded);
    return QUrl::fromPercentEncoding(path.toUtf8());
}

} // namespace

// Parses a PROPFIND multistatus body. Returns false if the XML is malformed.
bool WebDavParser::parseMultiStatus(const QByteArray &body, QList<DavEntry> &entries,
                                    QString &errorMessage)
{
    entries.clear();
    QXmlStreamReader xml(body);
    xml.setNamespaceProcessing(true);

    if (!xml.readNextStartElement() || xml.name() != QLatin1String("multistatus")
            || xml.namespaceUri() != kDav) {
        errorMessage = QStringLiteral("Response is not a WebDAV multistatus document");
        return false;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("response")) {
            xml.skipCurrentElement();
            continue;
        }
        DavEntry entry;
        bool anyOk = false;
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("href")) {
                entry.path = decodeHref(readText(xml));
            } else if (xml.name() == QLatin1String("propstat")) {
                const PropBlock block = readPropstat(xml);
                anyOk = anyOk || block.ok;
                apply(block, entry);
            } else {
                xml.skipCurrentElement();
            }
        }
        entry.found = anyOk;
        if (entry.path.endsWith(QLatin1Char('/')))
            entry.isCollection = true;
        if (!entry.path.isEmpty())
            entries.append(entry);
    }

    if (xml.hasError()) {
        errorMessage = xml.errorString();
        return false;
    }
    return true;
}

// Parses an HTTP date such as "Wed, 11 Nov 2026 20:15:00 GMT" to epoch seconds.
qint64 WebDavParser::parseHttpDate(const QString &value)
{
    const QString v = value.trimmed();
    QDateTime dt = QLocale::c().toDateTime(v, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
    if (dt.isValid()) {
        dt.setTimeZone(QTimeZone::utc());
        return dt.toSecsSinceEpoch();
    }
    dt = QDateTime::fromString(v, Qt::RFC2822Date);
    if (!dt.isValid())
        dt = QDateTime::fromString(v, Qt::ISODate);
    return dt.isValid() ? dt.toSecsSinceEpoch() : 0;
}

// Returns the request body used to list a folder.
QByteArray WebDavParser::propfindBody()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\"><d:prop>"
        "<d:resourcetype/><d:getetag/><d:getcontentlength/><d:getlastmodified/>"
        "</d:prop></d:propfind>");
}

// Returns the request body used to query the account quota.
QByteArray WebDavParser::quotaBody()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\"><d:prop>"
        "<d:quota-used-bytes/><d:quota-available-bytes/>"
        "</d:prop></d:propfind>");
}

} // namespace PicSync
