// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "appsettings.h"
#include "synchistory.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace PicSync;

/// Tests of the sync history and of the settings defaults.
class TestSyncHistory : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Appends runs, keeps the newest first and caps the length.
    void appendAndCap();
    // Converts a result to JSON and back.
    void jsonRoundTrip();
    // Uses the device name for the default folder and validates the interval.
    void settingsDefaults();
};

// Appends runs, keeps the newest first and caps the length.
void TestSyncHistory::appendAndCap()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("history.json"));
    SyncHistory history(path, 3);
    QVERIFY(history.load());
    for (int i = 0; i < 5; ++i) {
        SyncResult r;
        r.uploaded = i;
        QVERIFY(history.append(r));
    }
    SyncHistory loaded(path, 3);
    QVERIFY(loaded.load());
    QCOMPARE(loaded.entries().size(), 3);
    QCOMPARE(loaded.entries().first().uploaded, 4);
    QCOMPARE(loaded.entries().last().uploaded, 2);
    QVERIFY(loaded.clear());
    QVERIFY(loaded.entries().isEmpty());
}

// Converts a result to JSON and back.
void TestSyncHistory::jsonRoundTrip()
{
    SyncResult r;
    r.success = true;
    r.manual = true;
    r.startedAt = 10;
    r.finishedAt = 20;
    r.downloaded = 3;
    r.deletedRemote = 1;
    r.changedFiles = { QStringLiteral("↑ a.jpg"), QStringLiteral("✕ b.jpg") };
    r.quota.valid = true;
    r.quota.usedBytes = 5000000000;
    r.quota.availableBytes = -1;
    const SyncResult back = SyncHistory::fromJson(SyncHistory::toJson(r));
    QCOMPARE(back.success, true);
    QCOMPARE(back.manual, true);
    QCOMPARE(back.finishedAt, qint64(20));
    QCOMPARE(back.downloaded, 3);
    QCOMPARE(back.deletedRemote, 1);
    QCOMPARE(back.changedFiles, r.changedFiles);
    QCOMPARE(back.quota.usedBytes, r.quota.usedBytes);
    QCOMPARE(back.quota.availableBytes, qint64(-1));
}

// Uses the device name for the default folder and validates the interval.
void TestSyncHistory::settingsDefaults()
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);
    AppSettings s = AppSettings::load(settings, QStringLiteral("FP6 Peter"));
    QCOMPARE(s.remoteFolder, QStringLiteral("PicSync/FP6 Peter"));
    QCOMPARE(s.intervalMinutes, 60);
    QVERIFY(s.wifiOnly);
    QVERIFY(!s.isComplete());

    s.serverUrl = QStringLiteral("cloud.example.org/");
    s.user = QStringLiteral(" peter ");
    s.intervalMinutes = 15;
    s.save(settings);
    const AppSettings loaded = AppSettings::load(settings, QStringLiteral("other"));
    QCOMPARE(loaded.serverUrl, QStringLiteral("https://cloud.example.org"));
    QCOMPARE(loaded.user, QStringLiteral("peter"));
    QCOMPARE(loaded.remoteFolder, QStringLiteral("PicSync/FP6 Peter"));
    QCOMPARE(loaded.intervalMinutes, 15);
    QVERIFY(loaded.isComplete());

    settings.setValue(QStringLiteral("sync/interval"), 7);
    QCOMPARE(AppSettings::load(settings, QString()).intervalMinutes, 60);
}

QTEST_GUILESS_MAIN(TestSyncHistory)
#include "tst_synchistory.moc"
