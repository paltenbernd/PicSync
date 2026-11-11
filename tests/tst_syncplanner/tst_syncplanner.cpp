// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "syncplanner.h"

#include <QTest>

using namespace PicSync;

Q_DECLARE_METATYPE(PicSync::SyncPlan)

namespace {

const QString kPath = QStringLiteral("DCIM/Camera/a.jpg");

// Builds a local file description.
LocalFile local(qint64 size = 100, qint64 mtime = 1000)
{
    LocalFile f;
    f.relativePath = kPath;
    f.size = size;
    f.mtime = mtime;
    return f;
}

// Builds a remote file description.
RemoteFile remote(qint64 size = 100, const QString &etag = QStringLiteral("e1"))
{
    RemoteFile f;
    f.relativePath = kPath;
    f.size = size;
    f.etag = etag;
    return f;
}

// Builds the state recorded after an earlier sync of the default files.
StateEntry known()
{
    StateEntry s;
    s.relativePath = kPath;
    s.localSize = 100;
    s.localMtime = 1000;
    s.remoteSize = 100;
    s.remoteEtag = QStringLiteral("e1");
    return s;
}

// Builds a pending deletion of the default file.
PendingDeletion pending(Side side)
{
    PendingDeletion p;
    p.relativePath = kPath;
    p.deletedOn = side;
    p.detectedAt = 5;
    return p;
}

// Builds a plan with one or two actions on the default file.
SyncPlan plan(ActionType a)
{
    return { SyncAction { a, kPath } };
}

// Builds a plan with two actions on the default file.
SyncPlan plan(ActionType a, ActionType b)
{
    return { SyncAction { a, kPath }, SyncAction { b, kPath } };
}

} // namespace

/// Tests of the decision table of the two-way synchronisation.
class TestSyncPlanner : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Provides one row per situation of the decision table.
    void decisions_data();
    // Checks that every situation leads to the expected actions.
    void decisions();
    // Checks that nothing at all happens if both sides are unchanged.
    void unchanged();
    // Checks that plans are sorted by path, so runs are reproducible.
    void deterministicOrder();
};

// Provides one row per situation of the decision table.
void TestSyncPlanner::decisions_data()
{
    QTest::addColumn<bool>("hasLocal");
    QTest::addColumn<qint64>("localSize");
    QTest::addColumn<bool>("hasRemote");
    QTest::addColumn<QString>("remoteEtag");
    QTest::addColumn<bool>("hasState");
    QTest::addColumn<int>("pendingSide");   // -1 none, 0 local, 1 remote
    QTest::addColumn<SyncPlan>("expected");

    const QString e1 = QStringLiteral("e1");
    const QString e2 = QStringLiteral("e2");

    // First contact with a file.
    QTest::newRow("new local -> upload")
        << true << qint64(100) << false << e1 << false << -1 << plan(ActionType::Upload);
    QTest::newRow("new remote -> download")
        << false << qint64(100) << true << e1 << false << -1 << plan(ActionType::Download);
    QTest::newRow("both, same size -> adopt")
        << true << qint64(100) << true << e1 << false << -1 << plan(ActionType::Adopt);
    QTest::newRow("both, different size -> conflict")
        << true << qint64(200) << true << e1 << false << -1 << plan(ActionType::Conflict);

    // Changes after a successful sync.
    QTest::newRow("local changed -> upload")
        << true << qint64(200) << true << e1 << true << -1 << plan(ActionType::Upload);
    QTest::newRow("remote changed -> download")
        << true << qint64(100) << true << e2 << true << -1 << plan(ActionType::Download);
    QTest::newRow("both changed -> conflict")
        << true << qint64(200) << true << e2 << true << -1 << plan(ActionType::Conflict);
    QTest::newRow("gone on both sides -> forget")
        << false << qint64(0) << false << e1 << true << -1 << plan(ActionType::Forget);

    // Deletions are delayed: first detection only marks them (lock) ...
    QTest::newRow("deleted locally -> mark")
        << false << qint64(0) << true << e1 << true << -1 << plan(ActionType::MarkLocalDeleted);
    QTest::newRow("deleted remotely -> mark")
        << true << qint64(100) << false << e1 << true << -1 << plan(ActionType::MarkRemoteDeleted);

    // ... and the next run executes them.
    QTest::newRow("pending local deletion -> delete remote")
        << false << qint64(0) << true << e1 << true << 0 << plan(ActionType::DeleteRemote);
    QTest::newRow("pending remote deletion -> delete local")
        << true << qint64(100) << false << e1 << true << 1 << plan(ActionType::DeleteLocal);

    // The other side changed while the deletion was pending: keep the data.
    QTest::newRow("pending local deletion, remote changed -> restore")
        << false << qint64(0) << true << e2 << true << 0
        << plan(ActionType::CancelPending, ActionType::Download);
    QTest::newRow("pending remote deletion, local changed -> restore")
        << true << qint64(300) << false << e1 << true << 1
        << plan(ActionType::CancelPending, ActionType::Upload);

    // The user restored the file while the deletion was pending.
    QTest::newRow("pending deletion, file back on both sides -> cancel")
        << true << qint64(100) << true << e1 << true << 0 << plan(ActionType::CancelPending);
    QTest::newRow("pending deletion, gone on both sides -> forget")
        << false << qint64(0) << false << e1 << true << 1 << plan(ActionType::Forget);

    // A pending deletion without state is stale.
    QTest::newRow("stale pending -> cancel and upload")
        << true << qint64(100) << false << e1 << false << 0
        << plan(ActionType::CancelPending, ActionType::Upload);
}

// Checks that every situation leads to the expected actions.
void TestSyncPlanner::decisions()
{
    QFETCH(bool, hasLocal);
    QFETCH(qint64, localSize);
    QFETCH(bool, hasRemote);
    QFETCH(QString, remoteEtag);
    QFETCH(bool, hasState);
    QFETCH(int, pendingSide);
    QFETCH(SyncPlan, expected);

    LocalFileMap l;
    RemoteFileMap r;
    StateMap s;
    PendingMap p;
    if (hasLocal)
        l.insert(kPath, local(localSize));
    if (hasRemote)
        r.insert(kPath, remote(100, remoteEtag));
    if (hasState)
        s.insert(kPath, known());
    if (pendingSide >= 0)
        p.insert(kPath, pending(pendingSide == 0 ? Side::Local : Side::Remote));

    const SyncPlan actual = SyncPlanner::plan(l, r, s, p);
    QCOMPARE(actual.size(), expected.size());
    for (int i = 0; i < actual.size(); ++i) {
        QCOMPARE(int(actual[i].type), int(expected[i].type));
        QCOMPARE(actual[i].relativePath, expected[i].relativePath);
    }
}

// Checks that nothing at all happens if both sides are unchanged.
void TestSyncPlanner::unchanged()
{
    LocalFileMap l { { kPath, local() } };
    RemoteFileMap r { { kPath, remote() } };
    StateMap s { { kPath, known() } };
    QVERIFY(SyncPlanner::plan(l, r, s, PendingMap()).isEmpty());

    // A changed mtime alone counts as a local change.
    l[kPath].mtime = 2000;
    QCOMPARE(SyncPlanner::plan(l, r, s, PendingMap()).size(), 1);
}

// Checks that plans are sorted by path, so runs are reproducible.
void TestSyncPlanner::deterministicOrder()
{
    LocalFileMap l;
    for (const char *name : { "c.jpg", "a.jpg", "b.jpg" }) {
        LocalFile f;
        f.relativePath = QString::fromLatin1(name);
        l.insert(f.relativePath, f);
    }
    const SyncPlan actual = SyncPlanner::plan(l, RemoteFileMap(), StateMap(), PendingMap());
    QCOMPARE(actual.size(), 3);
    QCOMPARE(actual[0].relativePath, QStringLiteral("a.jpg"));
    QCOMPARE(actual[2].relativePath, QStringLiteral("c.jpg"));
}

QTEST_GUILESS_MAIN(TestSyncPlanner)
#include "tst_syncplanner.moc"
