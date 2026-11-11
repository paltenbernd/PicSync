// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "webdavparser.h"

#include <QTest>
#include <QTimeZone>

using namespace PicSync;

/// Tests of the WebDAV multistatus parser with answers as Nextcloud sends them.
class TestWebDavParser : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Parses a folder listing with a sub folder and two files.
    void folderListing();
    // Parses a quota answer.
    void quota();
    // Ignores properties reported with status 404.
    void missingProperties();
    // Rejects documents that are not WebDAV multistatus answers.
    void malformed();
    // Parses HTTP dates.
    void httpDate();
};

// Parses a folder listing with a sub folder and two files.
void TestWebDavParser::folderListing()
{
    const QByteArray xml =
        "<?xml version=\"1.0\"?>\n"
        "<d:multistatus xmlns:d=\"DAV:\" xmlns:s=\"http://sabredav.org/ns\" "
        "xmlns:oc=\"http://owncloud.org/ns\" xmlns:nc=\"http://nextcloud.org/ns\">"
        "<d:response><d:href>/remote.php/dav/files/peter/PicSync/FP6/</d:href>"
        "<d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype>"
        "<d:getetag>&quot;6553f0d4a1b2c&quot;</d:getetag>"
        "<d:getlastmodified>Wed, 11 Nov 2026 20:15:00 GMT</d:getlastmodified></d:prop>"
        "<d:status>HTTP/1.1 200 OK</d:status></d:propstat>"
        "<d:propstat><d:prop><d:getcontentlength/></d:prop>"
        "<d:status>HTTP/1.1 404 Not Found</d:status></d:propstat></d:response>"
        "<d:response><d:href>/remote.php/dav/files/peter/PicSync/FP6/DCIM/</d:href>"
        "<d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype></d:prop>"
        "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
        "<d:response><d:href>/remote.php/dav/files/peter/PicSync/FP6/Bild%20%C3%A4.jpg</d:href>"
        "<d:propstat><d:prop><d:resourcetype/>"
        "<d:getetag>&quot;abc123&quot;</d:getetag>"
        "<d:getcontentlength>2048</d:getcontentlength>"
        "<d:getlastmodified>Wed, 11 Nov 2026 20:15:00 GMT</d:getlastmodified></d:prop>"
        "<d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
        "</d:multistatus>";

    QList<DavEntry> entries;
    QString error;
    QVERIFY2(WebDavParser::parseMultiStatus(xml, entries, error), qPrintable(error));
    QCOMPARE(entries.size(), 3);

    QVERIFY(entries[0].isCollection);
    QCOMPARE(entries[0].path, QStringLiteral("/remote.php/dav/files/peter/PicSync/FP6/"));
    QVERIFY(entries[1].isCollection);

    const DavEntry &file = entries[2];
    QVERIFY(!file.isCollection);
    QCOMPARE(file.path, QStringLiteral("/remote.php/dav/files/peter/PicSync/FP6/Bild ä.jpg"));
    QCOMPARE(file.size, qint64(2048));
    QCOMPARE(file.etag, QStringLiteral("abc123"));
    QCOMPARE(file.mtime, QDateTime(QDate(2026, 11, 11), QTime(20, 15), QTimeZone::utc())
             .toSecsSinceEpoch());
}

// Parses a quota answer.
void TestWebDavParser::quota()
{
    const QByteArray xml =
        "<d:multistatus xmlns:d=\"DAV:\"><d:response>"
        "<d:href>/remote.php/dav/files/peter/</d:href><d:propstat><d:prop>"
        "<d:quota-used-bytes>8160437862</d:quota-used-bytes>"
        "<d:quota-available-bytes>13314398618</d:quota-available-bytes>"
        "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>"
        "</d:multistatus>";
    QList<DavEntry> entries;
    QString error;
    QVERIFY(WebDavParser::parseMultiStatus(xml, entries, error));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].quotaUsed, Q_INT64_C(8160437862));
    QCOMPARE(entries[0].quotaAvailable, Q_INT64_C(13314398618));
}

// Ignores properties reported with status 404.
void TestWebDavParser::missingProperties()
{
    const QByteArray xml =
        "<d:multistatus xmlns:d=\"DAV:\"><d:response>"
        "<d:href>https://cloud.example.org/remote.php/dav/files/peter/a.jpg</d:href>"
        "<d:propstat><d:prop><d:getcontentlength>99</d:getcontentlength></d:prop>"
        "<d:status>HTTP/1.1 404 Not Found</d:status></d:propstat>"
        "</d:response></d:multistatus>";
    QList<DavEntry> entries;
    QString error;
    QVERIFY(WebDavParser::parseMultiStatus(xml, entries, error));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].path, QStringLiteral("/remote.php/dav/files/peter/a.jpg"));
    QCOMPARE(entries[0].size, qint64(0));
    QVERIFY(!entries[0].found);
}

// Rejects documents that are not WebDAV multistatus answers.
void TestWebDavParser::malformed()
{
    QList<DavEntry> entries;
    QString error;
    QVERIFY(!WebDavParser::parseMultiStatus("<html><body>Login</body></html>", entries, error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!WebDavParser::parseMultiStatus("<d:multistatus xmlns:d=\"DAV:\"><d:response>",
                                            entries, error));
}

// Parses HTTP dates.
void TestWebDavParser::httpDate()
{
    QCOMPARE(WebDavParser::parseHttpDate(QStringLiteral("Thu, 01 Jan 1970 00:01:00 GMT")),
             qint64(60));
    QCOMPARE(WebDavParser::parseHttpDate(QStringLiteral("garbage")), qint64(0));
}

QTEST_GUILESS_MAIN(TestWebDavParser)
#include "tst_webdavparser.moc"
