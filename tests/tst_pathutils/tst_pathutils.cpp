// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "pathutils.h"

#include <QTest>

using namespace PicSync;

/// Tests of the URL and file name helpers.
class TestPathUtils : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Checks which file names count as gallery media.
    void mediaFiles();
    // Checks that hidden and temporary files are ignored.
    void ignoredFiles();
    // Checks the default folder derived from the device name.
    void defaultFolder();
    // Checks normalisation and joining of relative paths.
    void joinAndNormalize();
    // Checks building of WebDAV URLs including special characters.
    void davUrls();
    // Checks that user input for the server address is cleaned up.
    void serverUrl();
    // Checks the "keep both" conflict file name.
    void conflictNames();
    // Checks ETag normalisation.
    void etags();
    // Folders are grouped into albums for the folder overview.
    void galleryGroups_data();
    void galleryGroups();
    // Names Android's shared storage rejects are recognised.
    void storableOnPhone();
};

// Checks which file names count as gallery media.
void TestPathUtils::mediaFiles()
{
    QVERIFY(PathUtils::isMediaFile(QStringLiteral("IMG_0001.JPG")));
    QVERIFY(PathUtils::isMediaFile(QStringLiteral("DCIM/Camera/VID_1.mp4")));
    QVERIFY(PathUtils::isMediaFile(QStringLiteral("a.heic")));
    QVERIFY(!PathUtils::isMediaFile(QStringLiteral("notes.txt")));
    QVERIFY(!PathUtils::isMediaFile(QStringLiteral("noextension")));
}

// Checks that hidden and temporary files are ignored.
void TestPathUtils::ignoredFiles()
{
    QVERIFY(PathUtils::isIgnored(QStringLiteral("DCIM/.thumbnails/x.jpg")));
    QVERIFY(PathUtils::isIgnored(QStringLiteral("Pictures/.trashed-1700000000-a.jpg")));
    QVERIFY(PathUtils::isIgnored(QStringLiteral("DCIM/Camera/a.jpg.picsync-part")));
    QVERIFY(!PathUtils::isIgnored(QStringLiteral("DCIM/Camera/a.jpg")));
}

// Checks the default folder derived from the device name.
void TestPathUtils::defaultFolder()
{
    QCOMPARE(PathUtils::defaultRemoteFolder(QStringLiteral("FP6 von Peter")),
             QStringLiteral("PicSync/FP6 von Peter"));
    QCOMPARE(PathUtils::defaultRemoteFolder(QStringLiteral("a/b:c")),
             QStringLiteral("PicSync/a_b_c"));
    QCOMPARE(PathUtils::defaultRemoteFolder(QStringLiteral("  ")),
             QStringLiteral("PicSync/Fairphone"));
}

// Checks normalisation and joining of relative paths.
void TestPathUtils::joinAndNormalize()
{
    QCOMPARE(PathUtils::normalizeRelative(QStringLiteral("//a///b/")), QStringLiteral("a/b"));
    QCOMPARE(PathUtils::join(QStringLiteral("a/"), QStringLiteral("/b")), QStringLiteral("a/b"));
    QCOMPARE(PathUtils::join(QString(), QStringLiteral("b")), QStringLiteral("b"));
    QCOMPARE(PathUtils::parentPath(QStringLiteral("DCIM/Camera/a.jpg")),
             QStringLiteral("DCIM/Camera"));
    QCOMPARE(PathUtils::parentPath(QStringLiteral("a.jpg")), QString());
}

// Checks building of WebDAV URLs including special characters.
void TestPathUtils::davUrls()
{
    const QUrl root = PathUtils::davFilesRoot(QStringLiteral("cloud.example.org"),
                                              QStringLiteral("peter a"));
    QCOMPARE(root.toString(QUrl::FullyEncoded),
             QStringLiteral("https://cloud.example.org/remote.php/dav/files/peter%20a/"));
    const QUrl file = PathUtils::davUrl(root, QStringLiteral("PicSync/FP6/Bild #1 ä.jpg"));
    QCOMPARE(file.toString(QUrl::FullyEncoded),
             QStringLiteral("https://cloud.example.org/remote.php/dav/files/peter%20a/"
                            "PicSync/FP6/Bild%20%231%20%C3%A4.jpg"));
    const QUrl legacy = PathUtils::davLegacyRoot(
        QStringLiteral("https://magentacloud.de/remote.php/webdav"));
    QCOMPARE(legacy.toString(QUrl::FullyEncoded),
             QStringLiteral("https://magentacloud.de/remote.php/webdav/"));
    QCOMPARE(PathUtils::davUrl(legacy, QStringLiteral("PicSync/a b.jpg")).toString(QUrl::FullyEncoded),
             QStringLiteral("https://magentacloud.de/remote.php/webdav/PicSync/a%20b.jpg"));
}

// Checks that user input for the server address is cleaned up.
void TestPathUtils::serverUrl()
{
    QCOMPARE(PathUtils::normalizeServerUrl(QStringLiteral(" cloud.example.org/ ")),
             QStringLiteral("https://cloud.example.org"));
    QCOMPARE(PathUtils::normalizeServerUrl(
                 QStringLiteral("https://x.org/nc/remote.php/dav/files/peter/")),
             QStringLiteral("https://x.org/nc"));
    QCOMPARE(PathUtils::normalizeServerUrl(QStringLiteral("http://x.org/index.php/apps/files")),
             QStringLiteral("http://x.org"));
}

// Checks the "keep both" conflict file name.
void TestPathUtils::conflictNames()
{
    const QDateTime when(QDate(2026, 11, 11), QTime(21, 40, 5));
    QCOMPARE(PathUtils::conflictName(QStringLiteral("DCIM/Camera/a.jpg"), when),
             QStringLiteral("DCIM/Camera/a (conflicted copy 2026-11-11 214005).jpg"));
    QCOMPARE(PathUtils::conflictName(QStringLiteral("noext"), when),
             QStringLiteral("noext (conflicted copy 2026-11-11 214005)"));
}

// Checks ETag normalisation.
void TestPathUtils::etags()
{
    QCOMPARE(PathUtils::normalizeEtag(QStringLiteral("\"abc\"")), QStringLiteral("abc"));
    QCOMPARE(PathUtils::normalizeEtag(QStringLiteral("W/\"abc\"")), QStringLiteral("abc"));
    QCOMPARE(PathUtils::normalizeEtag(QStringLiteral("abc")), QStringLiteral("abc"));
}

// Folders are grouped into albums for the folder overview (test data).
void TestPathUtils::galleryGroups_data()
{
    QTest::addColumn<QString>("folder");
    QTest::addColumn<QString>("group");
    QTest::newRow("camera") << "DCIM/Camera" << "DCIM/Camera";
    QTest::newRow("dcim root") << "DCIM" << "DCIM";
    QTest::newRow("dated") << "apple/2026/09" << "apple";
    QTest::newRow("top level") << "apple" << "apple";
    QTest::newRow("pictures sub") << "Pictures/Signal/2024/02" << "Pictures/Signal";
    QTest::newRow("screenshots") << "Pictures/Screenshots" << "Pictures/Screenshots";
    QTest::newRow("lower case") << "pictures/x/y" << "pictures/x";
    QTest::newRow("whatsapp") << "Android/media/com.whatsapp/WhatsApp/Media/WhatsApp Images"
                              << "Android/media/com.whatsapp/WhatsApp";
    QTest::newRow("main folder") << "" << "";
    QTest::newRow("trailing slash") << "Movies/Holiday/" << "Movies/Holiday";
}

// Folders are grouped into albums for the folder overview.
void TestPathUtils::galleryGroups()
{
    QFETCH(QString, folder);
    QFETCH(QString, group);
    QCOMPARE(PathUtils::galleryGroup(folder), group);
}

// Names Android's shared storage rejects are recognised.
void TestPathUtils::storableOnPhone()
{
    QVERIFY(PathUtils::isStorableOnPhone(QStringLiteral("DCIM/Camera/IMG_1.jpg")));
    QVERIFY(PathUtils::isStorableOnPhone(QStringLiteral("apple/2025/04/Ä ö + [1].jpg")));
    QVERIFY(!PathUtils::isStorableOnPhone(
        QStringLiteral("apple/2025/04/SharePic 2025-04-14 15:52:53 +0000.jpg")));
    QVERIFY(!PathUtils::isStorableOnPhone(QStringLiteral("a/what?.jpg")));
    QVERIFY(!PathUtils::isStorableOnPhone(QStringLiteral("a\"b/c.jpg")));
    QVERIFY(!PathUtils::isStorableOnPhone(QStringLiteral("a/b|c.jpg")));
    QVERIFY(!PathUtils::isStorableOnPhone(QString(250, QLatin1Char('x')) + QStringLiteral(".jpg")));
    QVERIFY(!PathUtils::isStorableOnPhone(QString()));

    // Making names storable.
    QCOMPARE(PathUtils::phoneSafePath(QStringLiteral("a/SharePic 15:52:53 +0000.jpg")),
             QStringLiteral("a/SharePic 15-52-53 +0000.jpg"));
    QCOMPARE(PathUtils::phoneSafePath(QStringLiteral("a:b/w?h*a<t>\"|.png")),
             QStringLiteral("a-b/w_h_a_t___.png"));
    QCOMPARE(PathUtils::phoneSafePath(QStringLiteral("DCIM/Camera/IMG_1.jpg")),
             QStringLiteral("DCIM/Camera/IMG_1.jpg"));
    const QString longName = PathUtils::phoneSafePath(QString(300, QLatin1Char('x')) + QStringLiteral(".jpeg"));
    QVERIFY(longName.endsWith(QStringLiteral(".jpeg")));
    QVERIFY(PathUtils::isStorableOnPhone(longName));
}

QTEST_GUILESS_MAIN(TestPathUtils)
#include "tst_pathutils.moc"
