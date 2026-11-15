// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "servicerunner.h"
#include "syncprogress.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace PicSync;

/// Tests of the sync service logic: manual requests, stop requests, progress file.
/// PicSync is not configured here, so every run ends quickly with an error.
class TestServiceRunner : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Removes request and progress files before each test.
    void init();
    // Without a request, the service performs one scheduled run and reports the end.
    void scheduledRun();
    // A manual request is consumed and marks the run as manual.
    void manualRequest();
    // "Sync now" while a scheduled run is going on turns it into the manual run.
    void requestDuringRun();
    // A request that arrives just as a run ends leads to a second (manual) run.
    void requestAtEndOfRun();
    // An old stop request does not cancel a new run.
    void staleStopRequestIsIgnored();
};

// Removes request and progress files before each test.
void TestServiceRunner::init()
{
    QFile::remove(SyncProgress::progressPath());
    QFile::remove(SyncProgress::manualRequestPath());
    QFile::remove(SyncProgress::stopRequestPath());
}

// Without a request, the service performs one scheduled run and reports the end.
void TestServiceRunner::scheduledRun()
{
    ServiceRunner runner;
    QSignalSpy done(&runner, &ServiceRunner::allDone);
    runner.start();
    QVERIFY(runner.isRunning());
    QVERIFY(done.wait(10000));
    QCOMPARE(runner.runsStarted(), 1);
    const SyncProgressInfo info = SyncProgress::read();
    QVERIFY(!info.running);
    QVERIFY(!info.manual);
    QVERIFY(info.updatedMs > 0);
}

// A manual request is consumed and marks the run as manual.
void TestServiceRunner::manualRequest()
{
    QVERIFY(SyncProgress::touch(SyncProgress::manualRequestPath()));
    ServiceRunner runner;
    QSignalSpy done(&runner, &ServiceRunner::allDone);
    runner.start();
    QVERIFY(!QFile::exists(SyncProgress::manualRequestPath()));
    QVERIFY(done.wait(10000));
    QCOMPARE(runner.runsStarted(), 1);
    QVERIFY(SyncProgress::read().manual);
}

// "Sync now" while a scheduled run is going on turns it into the manual run.
void TestServiceRunner::requestDuringRun()
{
    ServiceRunner runner;
    QSignalSpy done(&runner, &ServiceRunner::allDone);
    runner.start();
    QVERIFY(runner.isRunning());
    QVERIFY(SyncProgress::touch(SyncProgress::manualRequestPath()));
    runner.poll();
    QVERIFY(done.wait(10000));
    QCOMPARE(runner.runsStarted(), 1);   // the running run became the manual one
    QVERIFY(!QFile::exists(SyncProgress::manualRequestPath()));
    QVERIFY(SyncProgress::read().manual);
    QCOMPARE(done.count(), 1);
}

// A request that arrives just as a run ends leads to a second (manual) run.
void TestServiceRunner::requestAtEndOfRun()
{
    ServiceRunner runner;
    QSignalSpy done(&runner, &ServiceRunner::allDone);
    runner.start();
    QVERIFY(SyncProgress::touch(SyncProgress::manualRequestPath()));
    QVERIFY(done.wait(10000));
    QCOMPARE(runner.runsStarted(), 2);
    QVERIFY(SyncProgress::read().manual);
    QCOMPARE(done.count(), 1);
}

// An old stop request does not cancel a new run.
void TestServiceRunner::staleStopRequestIsIgnored()
{
    QVERIFY(SyncProgress::touch(SyncProgress::stopRequestPath()));
    ServiceRunner runner;
    runner.start();
    QVERIFY(!QFile::exists(SyncProgress::stopRequestPath()));
    QSignalSpy done(&runner, &ServiceRunner::allDone);
    QVERIFY(done.wait(10000));
}

// Runs the tests with private, temporary settings and data.
int main(int argc, char *argv[])
{
    const QTemporaryDir home;
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PicSyncTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PicSyncTest"));
    qRegisterMetaType<PicSync::SyncResult>();
    TestServiceRunner test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_servicerunner.moc"
