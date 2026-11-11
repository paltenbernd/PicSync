// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "directorymediastore.h"
#include "fakeremotestore.h"
#include "statestore.h"
#include "syncengine.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace PicSync;

namespace {
const QString kFingerprint = QStringLiteral("https://cloud|peter|PicSync/FP6");
}

/// End-to-end tests of the sync engine with a directory as phone and a fake server.
class TestSyncEngine : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Creates fresh phone, server and state for every test.
    void init();
    // Releases the test fixture.
    void cleanup();

    // New files on either side end up on both sides.
    void initialSync();
    // A deletion on the phone reaches the server only in the following run.
    void localDeletionIsDelayed();
    // A deletion on the server reaches the phone only in the following run.
    void remoteDeletionIsDelayed();
    // Restoring a file before the next run cancels the pending deletion.
    void restoreCancelsDeletion();
    // A file changed on the server while deleted on the phone comes back.
    void remoteEditBeatsLocalDeletion();
    // Changes on both sides keep both versions everywhere.
    void conflictKeepsBoth();
    // Edits are transferred in the right direction.
    void editsPropagate();
    // An empty gallery after a sync is treated as an error, not as mass deletion.
    void emptyGalleryIsRefused();
    // A vanished server folder is an error, not a reason to delete everything.
    void missingRemoteFolderIsRefused();
    // Changing the configuration starts over without deleting anything.
    void fingerprintChangeResetsState();
    // Network failures abort the run and keep the state consistent.
    void offlineServer();
    // The engine reports progress for each transfer.
    void progressSignals();
    // State survives a restart of the app.
    void statePersists();
    // A file missing from the media index but present on disk is not "deleted".
    void unindexedFileIsKept();

private:
    // Writes a file into the phone's gallery.
    void writeLocal(const QString &rel, const QByteArray &data);
    // Returns true if the phone has the file.
    bool localExists(const QString &rel) const;
    // Returns the content of a file on the phone.
    QByteArray localContent(const QString &rel) const;
    // Runs one sync and returns its result.
    SyncResult sync();
    // Lists all files on the phone.
    QStringList localFiles() const;

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<DirectoryMediaStore> m_local;
    std::unique_ptr<FakeRemoteStore> m_remote;
    std::unique_ptr<StateStore> m_state;
};

// Creates fresh phone, server and state for every test.
void TestSyncEngine::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QDir().mkpath(m_dir->filePath(QStringLiteral("phone")));
    m_local = std::make_unique<DirectoryMediaStore>(m_dir->filePath(QStringLiteral("phone")));
    m_remote = std::make_unique<FakeRemoteStore>(m_dir->filePath(QStringLiteral("server/FP6")));
    m_state = std::make_unique<StateStore>(m_dir->filePath(QStringLiteral("state.json")));
}

// Releases the test fixture.
void TestSyncEngine::cleanup()
{
    m_state.reset();
    m_remote.reset();
    m_local.reset();
    m_dir.reset();
}

// Writes a file into the phone's gallery.
void TestSyncEngine::writeLocal(const QString &rel, const QByteArray &data)
{
    QString error;
    QVERIFY(m_local->prepareWrite(rel, error));
    QFile file(m_local->absolutePath(rel));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(data);
}

// Returns true if the phone has the file.
bool TestSyncEngine::localExists(const QString &rel) const
{
    return QFileInfo::exists(m_local->absolutePath(rel));
}

// Returns the content of a file on the phone.
QByteArray TestSyncEngine::localContent(const QString &rel) const
{
    QFile file(m_local->absolutePath(rel));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Lists all files on the phone.
QStringList TestSyncEngine::localFiles() const
{
    QStringList result;
    const QString root = m_dir->filePath(QStringLiteral("phone"));
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        result << QDir(root).relativeFilePath(it.next());
    result.sort();
    return result;
}

// Runs one sync and returns its result.
SyncResult TestSyncEngine::sync()
{
    SyncEngine engine(*m_local, *m_remote, *m_state);
    return engine.run(false, kFingerprint);
}

// New files on either side end up on both sides.
void TestSyncEngine::initialSync()
{
    writeLocal(QStringLiteral("DCIM/Camera/a.jpg"), "aaa");
    writeLocal(QStringLiteral("Pictures/Screenshots/b.png"), "bbbb");
    writeLocal(QStringLiteral("Documents/ignored.txt"), "not media");
    m_remote->put(QStringLiteral("DCIM/Camera/c.mp4"), "cc");

    const SyncResult r = sync();
    QVERIFY2(r.success, qPrintable(r.errorMessage));
    QCOMPARE(r.uploaded, 2);
    QCOMPARE(r.downloaded, 1);
    QVERIFY(m_remote->exists(QStringLiteral("DCIM/Camera/a.jpg")));
    QVERIFY(m_remote->exists(QStringLiteral("Pictures/Screenshots/b.png")));
    QVERIFY(!m_remote->exists(QStringLiteral("Documents/ignored.txt")));
    QCOMPARE(localContent(QStringLiteral("DCIM/Camera/c.mp4")), QByteArray("cc"));
    QCOMPARE(m_state->entries().size(), 3);
    QVERIFY(r.quota.valid);

    // A second run has nothing to do.
    const SyncResult again = sync();
    QVERIFY(again.success);
    QCOMPARE(again.uploaded + again.downloaded, 0);
}

// A deletion on the phone reaches the server only in the following run.
void TestSyncEngine::localDeletionIsDelayed()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    writeLocal(QStringLiteral("DCIM/keep.jpg"), "k");
    QVERIFY(sync().success);
    QFile::remove(m_local->absolutePath(QStringLiteral("DCIM/a.jpg")));

    SyncResult r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.pendingDeletions, 1);
    QCOMPARE(r.deletedRemote, 0);
    QVERIFY(m_remote->exists(QStringLiteral("DCIM/a.jpg")));   // locked, not re-downloaded
    QVERIFY(!localExists(QStringLiteral("DCIM/a.jpg")));

    r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.deletedRemote, 1);
    QCOMPARE(r.pendingDeletions, 0);
    QVERIFY(!m_remote->exists(QStringLiteral("DCIM/a.jpg")));
    QCOMPARE(m_state->entries().size(), 1);
}

// A deletion on the server reaches the phone only in the following run.
void TestSyncEngine::remoteDeletionIsDelayed()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    writeLocal(QStringLiteral("DCIM/keep.jpg"), "k");
    QVERIFY(sync().success);
    m_remote->erase(QStringLiteral("DCIM/a.jpg"));

    SyncResult r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.pendingDeletions, 1);
    QVERIFY(localExists(QStringLiteral("DCIM/a.jpg")));
    QVERIFY(!m_remote->exists(QStringLiteral("DCIM/a.jpg")));  // locked, not re-uploaded

    r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.deletedLocal, 1);
    QVERIFY(!localExists(QStringLiteral("DCIM/a.jpg")));
    QVERIFY(localExists(QStringLiteral("DCIM/keep.jpg")));
}

// Restoring a file before the next run cancels the pending deletion.
void TestSyncEngine::restoreCancelsDeletion()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    writeLocal(QStringLiteral("DCIM/keep.jpg"), "k");
    QVERIFY(sync().success);
    const QString abs = m_local->absolutePath(QStringLiteral("DCIM/a.jpg"));
    const QDateTime mtime = QFileInfo(abs).lastModified();
    QVERIFY(QFile::rename(abs, abs + QStringLiteral(".bak")));
    QCOMPARE(sync().pendingDeletions, 1);

    // The user restores the file from the gallery's trash.
    QVERIFY(QFile::rename(abs + QStringLiteral(".bak"), abs));
    QFile file(abs);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(mtime, QFileDevice::FileModificationTime));
    file.close();

    const SyncResult r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.pendingDeletions, 0);
    QCOMPARE(r.deletedRemote, 0);
    QVERIFY(m_remote->exists(QStringLiteral("DCIM/a.jpg")));
    QVERIFY(m_state->pending().isEmpty());
}

// A file changed on the server while deleted on the phone comes back.
void TestSyncEngine::remoteEditBeatsLocalDeletion()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    writeLocal(QStringLiteral("DCIM/keep.jpg"), "k");
    QVERIFY(sync().success);
    QFile::remove(m_local->absolutePath(QStringLiteral("DCIM/a.jpg")));
    QCOMPARE(sync().pendingDeletions, 1);

    m_remote->put(QStringLiteral("DCIM/a.jpg"), "edited on the server");
    const SyncResult r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.downloaded, 1);
    QCOMPARE(r.deletedRemote, 0);
    QCOMPARE(localContent(QStringLiteral("DCIM/a.jpg")), QByteArray("edited on the server"));
}

// Changes on both sides keep both versions everywhere.
void TestSyncEngine::conflictKeepsBoth()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "original");
    QVERIFY(sync().success);
    writeLocal(QStringLiteral("DCIM/a.jpg"), "phone version");
    m_remote->put(QStringLiteral("DCIM/a.jpg"), "server version!");

    const SyncResult r = sync();
    QVERIFY2(r.success, qPrintable(r.errorMessage));
    QCOMPARE(r.conflicts, 1);

    const QStringList files = localFiles();
    QCOMPARE(files.size(), 2);
    QCOMPARE(localContent(QStringLiteral("DCIM/a.jpg")), QByteArray("server version!"));
    const QString aside = files.first() == QLatin1String("DCIM/a.jpg") ? files.last()
                                                                     : files.first();
    QVERIFY(aside.contains(QLatin1String("conflicted copy")));
    QCOMPARE(localContent(aside), QByteArray("phone version"));
    QCOMPARE(m_remote->content(aside), QByteArray("phone version"));
    QCOMPARE(m_remote->content(QStringLiteral("DCIM/a.jpg")), QByteArray("server version!"));

    // Afterwards everything is in agreement.
    const SyncResult again = sync();
    QCOMPARE(again.uploaded + again.downloaded + again.conflicts, 0);
}

// Edits are transferred in the right direction.
void TestSyncEngine::editsPropagate()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "v1");
    writeLocal(QStringLiteral("DCIM/b.jpg"), "v1");
    QVERIFY(sync().success);

    writeLocal(QStringLiteral("DCIM/a.jpg"), "v2 from phone");
    m_remote->put(QStringLiteral("DCIM/b.jpg"), "v2 from server");
    const SyncResult r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.uploaded, 1);
    QCOMPARE(r.downloaded, 1);
    QCOMPARE(m_remote->content(QStringLiteral("DCIM/a.jpg")), QByteArray("v2 from phone"));
    QCOMPARE(localContent(QStringLiteral("DCIM/b.jpg")), QByteArray("v2 from server"));
}

// An empty gallery after a sync is treated as an error, not as mass deletion.
void TestSyncEngine::emptyGalleryIsRefused()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    QVERIFY(sync().success);
    QFile::remove(m_local->absolutePath(QStringLiteral("DCIM/a.jpg")));
    // With only one file this looks exactly like a lost permission.
    const SyncResult r = sync();
    QVERIFY(!r.success);
    QVERIFY(m_state->pending().isEmpty());
    QVERIFY(m_remote->exists(QStringLiteral("DCIM/a.jpg")));
}

// A vanished server folder is an error, not a reason to delete everything.
void TestSyncEngine::missingRemoteFolderIsRefused()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    QVERIFY(sync().success);
    QVERIFY(QDir(m_dir->filePath(QStringLiteral("server/FP6"))).removeRecursively());

    const SyncResult r = sync();
    QVERIFY(!r.success);
    QVERIFY(m_state->pending().isEmpty());
    QVERIFY(localExists(QStringLiteral("DCIM/a.jpg")));
}

// Changing the configuration starts over without deleting anything.
void TestSyncEngine::fingerprintChangeResetsState()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    QVERIFY(sync().success);
    QCOMPARE(m_state->entries().size(), 1);

    // New (empty) server folder with a different configuration.
    m_remote = std::make_unique<FakeRemoteStore>(m_dir->filePath(QStringLiteral("server2/X")));
    SyncEngine engine(*m_local, *m_remote, *m_state);
    const SyncResult r = engine.run(true, QStringLiteral("other"));
    QVERIFY(r.success);
    QCOMPARE(r.uploaded, 1);
    QCOMPARE(r.pendingDeletions, 0);
    QVERIFY(localExists(QStringLiteral("DCIM/a.jpg")));
    QCOMPARE(m_state->fingerprint(), QStringLiteral("other"));
}

// Network failures abort the run and keep the state consistent.
void TestSyncEngine::offlineServer()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    m_remote->setOffline(true);
    SyncResult r = sync();
    QVERIFY(!r.success);
    QVERIFY(!r.errorMessage.isEmpty());
    QVERIFY(m_state->entries().isEmpty());

    m_remote->setOffline(false);
    r = sync();
    QVERIFY(r.success);
    QCOMPARE(r.uploaded, 1);
}

// The engine reports progress for each transfer.
void TestSyncEngine::progressSignals()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "a");
    writeLocal(QStringLiteral("DCIM/b.jpg"), "b");
    SyncEngine engine(*m_local, *m_remote, *m_state);
    QSignalSpy progress(&engine, &SyncEngine::progress);
    QSignalSpy phase(&engine, &SyncEngine::phaseChanged);
    QVERIFY(engine.run(true, kFingerprint).success);
    QCOMPARE(progress.count(), 3);   // before each file, plus the final "done"
    QCOMPARE(progress.last().at(0).toInt(), 2);
    QCOMPARE(progress.last().at(1).toInt(), 2);
    QVERIFY(phase.count() >= 3);
}

// State survives a restart of the app.
void TestSyncEngine::statePersists()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    QVERIFY(sync().success);
    QFile::remove(m_local->absolutePath(QStringLiteral("DCIM/a.jpg")));
    writeLocal(QStringLiteral("DCIM/other.jpg"), "o");
    QCOMPARE(sync().pendingDeletions, 1);

    // Simulate an app restart: reload the state from disk.
    m_state = std::make_unique<StateStore>(m_dir->filePath(QStringLiteral("state.json")));
    QVERIFY(m_state->load());
    QCOMPARE(m_state->pending().size(), 1);
    const SyncResult r = sync();
    QCOMPARE(r.deletedRemote, 1);
}

/// Media store whose index "forgets" one file, like a pending media scan.
class ForgetfulMediaStore : public DirectoryMediaStore
{
public:
    // Creates the store; the given file is hidden from scans.
    ForgetfulMediaStore(const QString &root, const QString &hidden)
        : DirectoryMediaStore(root), m_hidden(hidden) {}

    // Lists all files except the hidden one.
    bool scan(LocalFileMap &files, QString &error) override
    {
        const bool ok = DirectoryMediaStore::scan(files, error);
        files.remove(m_hidden);
        return ok;
    }

private:
    QString m_hidden;
};

// A file missing from the media index but present on disk is not "deleted".
void TestSyncEngine::unindexedFileIsKept()
{
    writeLocal(QStringLiteral("DCIM/a.jpg"), "aaa");
    writeLocal(QStringLiteral("DCIM/b.jpg"), "bbb");
    QVERIFY(sync().success);

    ForgetfulMediaStore forgetful(m_dir->filePath(QStringLiteral("phone")),
                                  QStringLiteral("DCIM/a.jpg"));
    SyncEngine engine(forgetful, *m_remote, *m_state);
    for (int run = 0; run < 2; ++run) {
        const SyncResult r = engine.run(false, kFingerprint);
        QVERIFY(r.success);
        QCOMPARE(r.pendingDeletions, 0);
        QCOMPARE(r.deletedRemote, 0);
    }
    QVERIFY(m_remote->exists(QStringLiteral("DCIM/a.jpg")));
}

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "tst_syncengine.moc"
