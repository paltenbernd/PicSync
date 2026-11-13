// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "dashboardpage.h"
#include "statestore.h"
#include "synchistory.h"
#include "syncsession.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace PicSync;

namespace {
const int kPhoneWidth = 400;   // logical width of a phone screen
}

/// Widget tests of the dashboard (run offscreen on the build machine).
class TestDashboardPage : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Starts every test with empty state and history.
    void init();

    // A long file name during the sync must not make the page wider than the phone.
    void longFileNameKeepsWidth();
    // Long texts in the cards (paths, errors) must not widen the page either.
    void longTextsKeepWidth();
    // Every folder is listed with number of files and size; no "+ n more".
    void listsAllFoldersWithSizes();
    // Two folders with the same last name are shown with their full path.
    void duplicateFolderNamesAreDisambiguated();

private:
    // Writes a sync state with the given number of files per folder (1 MB each).
    static void writeState(const QList<std::pair<QString, int>> &folders);
    // Returns the texts of all labels on the page.
    static QStringList labelTexts(const QWidget &page);
};

// Starts every test with empty state and history.
void TestDashboardPage::init()
{
    QFile::remove(SyncSession::stateFilePath());
    QFile::remove(SyncSession::historyFilePath());
    QSettings settings;
    settings.clear();
}

// Writes a sync state with the given number of files per folder (1 MB each).
void TestDashboardPage::writeState(const QList<std::pair<QString, int>> &folders)
{
    StateStore state(SyncSession::stateFilePath());
    for (const std::pair<QString, int> &folder : folders) {
        for (int i = 0; i < folder.second; ++i) {
            StateEntry e;
            e.relativePath = folder.first + QStringLiteral("/IMG_%1.jpg").arg(i);
            e.localSize = 1000000;
            state.setEntry(e);
        }
    }
    QVERIFY(state.save());
}

// Returns the texts of all labels on the page.
QStringList TestDashboardPage::labelTexts(const QWidget &page)
{
    QStringList texts;
    const QList<QLabel *> labels = page.findChildren<QLabel *>();
    for (int i = 0; i < labels.size(); ++i) {
        const QPointer<QLabel> label(labels.at(i));
        if (label->isVisibleTo(&page))
            texts << label->text();
    }
    return texts;
}

// A long file name during the sync must not make the page wider than the phone.
void TestDashboardPage::longFileNameKeepsWidth()
{
    DashboardPage page;
    page.resize(kPhoneWidth, 800);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    page.setRunning(true);
    page.setProgress(0, 83, QStringLiteral("DCIM/Camera/") + QString(160, QLatin1Char('x'))
                     + QStringLiteral(".jpg"));
    QVERIFY2(page.minimumSizeHint().width() <= kPhoneWidth,
             qPrintable(QString::number(page.minimumSizeHint().width())));
    QVERIFY(page.sizeHint().width() <= kPhoneWidth + 100);
}

// Long texts in the cards (paths, errors) must not widen the page either.
void TestDashboardPage::longTextsKeepWidth()
{
    QSettings settings;
    settings.setValue(QStringLiteral("server/url"), QStringLiteral("https://magentacloud.de"));
    settings.setValue(QStringLiteral("server/user"), QStringLiteral("120049010000000000179901"));
    settings.setValue(QStringLiteral("server/folder"),
                      QStringLiteral("PicSync/") + QString(80, QLatin1Char('F')));
    SyncHistory history(SyncSession::historyFilePath());
    SyncResult failed;
    failed.finishedAt = 1;
    failed.errorMessage = QStringLiteral("Cannot read /storage/emulated/0/")
            + QString(150, QLatin1Char('y'));
    QVERIFY(history.append(failed));
    writeState({ { QStringLiteral("Pictures/") + QString(90, QLatin1Char('z')), 3 } });

    DashboardPage page;
    page.refresh();
    QVERIFY2(page.minimumSizeHint().width() <= kPhoneWidth,
             qPrintable(QString::number(page.minimumSizeHint().width())));
}

// Every folder is listed with number of files and size; no "+ n more".
void TestDashboardPage::listsAllFoldersWithSizes()
{
    QList<std::pair<QString, int>> folders;
    for (int i = 1; i <= 12; ++i)
        folders.append({ QStringLiteral("Pictures/Folder%1").arg(i, 2, 10, QLatin1Char('0')), i });
    writeState(folders);

    DashboardPage page;
    page.refresh();
    const QStringList texts = labelTexts(page);
    for (int i = 1; i <= 12; ++i)
        QVERIFY2(texts.contains(QStringLiteral("Folder%1").arg(i, 2, 10, QLatin1Char('0'))),
                 qPrintable(QString::number(i)));
    // Untranslated plural forms read "file(s)" in this test.
    QVERIFY2(texts.contains(QStringLiteral("12 file(s) · 12.0 MB")),
             qPrintable(texts.join(QLatin1Char('|'))));
    QVERIFY(texts.contains(QStringLiteral("1 file(s) · 1.0 MB")));
    for (const QString &text : texts)
        QVERIFY2(!text.contains(QLatin1String("more")), qPrintable(text));
}

// Two folders with the same last name are shown with their full path.
void TestDashboardPage::duplicateFolderNamesAreDisambiguated()
{
    writeState({ { QStringLiteral("Pictures/Signal"), 2 },
                 { QStringLiteral("Movies/Signal"), 1 },
                 { QStringLiteral("DCIM/Camera"), 3 } });
    DashboardPage page;
    page.refresh();
    const QStringList texts = labelTexts(page);
    QVERIFY(texts.contains(QStringLiteral("Pictures/Signal")));
    QVERIFY(texts.contains(QStringLiteral("Movies/Signal")));
    QVERIFY(texts.contains(QStringLiteral("Camera")));
}

// Runs the tests offscreen with private, temporary settings and data.
int main(int argc, char *argv[])
{
    const QTemporaryDir home;
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QApplication app(argc, argv);
    QFile style(QStringLiteral(":/style.qss"));
    if (style.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(style.readAll()));
    QCoreApplication::setOrganizationName(QStringLiteral("PicSyncTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PicSyncTest"));
    TestDashboardPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dashboardpage.moc"
