// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "statestore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace PicSync;

/// Tests of the persistent sync state.
class TestStateStore : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Saves and reloads entries, pending deletions and the fingerprint.
    void roundTrip();
    // A missing file is an empty, valid state.
    void missingFile();
    // A corrupt file is reported and leaves an empty state.
    void corruptFile();
};

// Saves and reloads entries, pending deletions and the fingerprint.
void TestStateStore::roundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("sub/state.json"));

    StateStore store(path);
    StateEntry e;
    e.relativePath = QStringLiteral("DCIM/Camera/ä b.jpg");
    e.localSize = 123456789012;
    e.localMtime = 1762891200;
    e.remoteSize = 123456789012;
    e.remoteEtag = QStringLiteral("etag1");
    store.setEntry(e);
    PendingDeletion p;
    p.relativePath = QStringLiteral("Pictures/x.png");
    p.deletedOn = Side::Remote;
    p.detectedAt = 1762891300;
    store.setPending(p);
    store.setFingerprint(QStringLiteral("fp"));
    QVERIFY(store.save());

    StateStore loaded(path);
    QVERIFY(loaded.load());
    QCOMPARE(loaded.fingerprint(), QStringLiteral("fp"));
    QCOMPARE(loaded.entries().size(), 1);
    const StateEntry r = loaded.entries().value(e.relativePath);
    QCOMPARE(r.localSize, e.localSize);
    QCOMPARE(r.localMtime, e.localMtime);
    QCOMPARE(r.remoteEtag, e.remoteEtag);
    QCOMPARE(loaded.pending().size(), 1);
    QCOMPARE(loaded.pending().value(p.relativePath).deletedOn, Side::Remote);
    QCOMPARE(loaded.pending().value(p.relativePath).detectedAt, p.detectedAt);

    loaded.removeEntry(e.relativePath);
    loaded.removePending(p.relativePath);
    QVERIFY(loaded.entries().isEmpty());
    QVERIFY(loaded.pending().isEmpty());
}

// A missing file is an empty, valid state.
void TestStateStore::missingFile()
{
    QTemporaryDir dir;
    StateStore store(dir.filePath(QStringLiteral("none.json")));
    QVERIFY(store.load());
    QVERIFY(store.entries().isEmpty());
}

// A corrupt file is reported and leaves an empty state.
void TestStateStore::corruptFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("state.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ not json");
    file.close();
    StateStore store(path);
    QVERIFY(!store.load());
    QVERIFY(store.entries().isEmpty());
}

QTEST_GUILESS_MAIN(TestStateStore)
#include "tst_statestore.moc"
