// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "dashboardpage.h"
#include "syncmodel.h"
#include "statestore.h"
#include "synchistory.h"
#include "syncsession.h"
#include "uihelpers.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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
    // The warning sign comes with a status line and a tap explanation listing the files.
    void waitingDeletionsAreExplained();
    // A failed sync shows the error in the explanation.
    void failedSyncIsExplained();

private:
    // Folders are sorted by storage space, not by number of files.
    void foldersSortedBySize();
    // Messages appear inside the main window (no separate window) and react to taps.
    void dialogIsOverlayInWindow();
    // The back key closes a message like "No".
    void dialogClosesWithBackKey();

public Q_SLOTS:
    // Taps the "yes"/"OK" button of the open message (called by a timer).
    void tapPrimary();
    // Presses the back key in the open message (called by a timer).
    void pressBack();

private:
    // Writes a sync state with the given number of files per folder.
    static void writeState(const QList<std::pair<QString, int>> &folders,
                           qint64 fileSize = 1000000);
    // Returns the open message overlay, if there is one.
    static QWidget *openOverlay();
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

// Writes a sync state with the given number of files per folder.
void TestDashboardPage::writeState(const QList<std::pair<QString, int>> &folders,
                                   qint64 fileSize)
{
    StateStore state(SyncSession::stateFilePath());
    for (const std::pair<QString, int> &folder : folders) {
        for (int i = 0; i < folder.second; ++i) {
            StateEntry e;
            e.relativePath = folder.first + QStringLiteral("/IMG_%1.jpg").arg(i);
            e.localSize = fileSize;
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

// Folders are sorted by storage space, not by number of files.
void TestDashboardPage::foldersSortedBySize()
{
    writeState({ { QStringLiteral("Pictures/ManySmall"), 20 } }, 1000);
    StateStore state(SyncSession::stateFilePath());
    QVERIFY(state.load());
    for (int i = 0; i < 2; ++i) {
        StateEntry e;
        e.relativePath = QStringLiteral("Movies/FewBig/VID_%1.mp4").arg(i);
        e.localSize = 50000000;
        state.setEntry(e);
    }
    QVERIFY(state.save());

    DashboardPage page;
    page.refresh();
    const QStringList texts = labelTexts(page);
    const int big = texts.indexOf(QStringLiteral("FewBig"));
    const int small = texts.indexOf(QStringLiteral("ManySmall"));
    QVERIFY(big >= 0 && small >= 0);
    QVERIFY2(big < small, qPrintable(texts.join(QLatin1Char('|'))));
}

// Returns the open message overlay, if there is one.
QWidget *TestDashboardPage::openOverlay()
{
    const QList<QWidget *> windows = QApplication::topLevelWidgets();
    for (int i = 0; i < windows.size(); ++i) {
        const QPointer<QWidget> window(windows.at(i));
        const QPointer<QWidget> overlay(
            window->findChild<QWidget *>(QStringLiteral("dialogOverlay")));
        if (overlay && overlay->isVisible())
            return overlay.data();
    }
    return nullptr;
}

// Taps the "yes"/"OK" button of the open message (called by a timer).
void TestDashboardPage::tapPrimary()
{
    const QPointer<QWidget> overlay(openOverlay());
    QVERIFY(overlay);
    QVERIFY(!overlay->isWindow());
    const QPointer<QPushButton> button(
        overlay->findChild<QPushButton *>(QStringLiteral("primaryButton")));
    QVERIFY(button);
    QTest::mouseClick(button.data(), Qt::LeftButton);
}

// Presses the back key in the open message (called by a timer).
void TestDashboardPage::pressBack()
{
    const QPointer<QWidget> overlay(openOverlay());
    QVERIFY(overlay);
    QTest::keyClick(overlay.data(), Qt::Key_Back);
}

// Messages appear inside the main window (no separate window) and react to taps.
void TestDashboardPage::dialogIsOverlayInWindow()
{
    DashboardPage page;
    page.resize(kPhoneWidth, 800);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    const int windows = QApplication::topLevelWidgets().size();
    QTimer::singleShot(200, this, &TestDashboardPage::tapPrimary);
    QVERIFY(Ui::confirm(page, QStringLiteral("Title"), QStringLiteral("Question?")));
    QCOMPARE(QApplication::topLevelWidgets().size(), windows);
    QVERIFY(!openOverlay());
}

// The back key closes a message like "No".
void TestDashboardPage::dialogClosesWithBackKey()
{
    DashboardPage page;
    page.resize(kPhoneWidth, 800);
    page.show();
    QVERIFY(QTest::qWaitForWindowExposed(&page));
    QTimer::singleShot(200, this, &TestDashboardPage::pressBack);
    QVERIFY(!Ui::confirm(page, QStringLiteral("Title"), QStringLiteral("Question?")));
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

// The warning sign comes with a status line and a tap explanation listing the files.
void TestDashboardPage::waitingDeletionsAreExplained()
{
    writeState({ { QStringLiteral("Pictures/eBay"), 3 } });
    StateStore state(SyncSession::stateFilePath());
    QVERIFY(state.load());
    PendingDeletion p;
    p.relativePath = QStringLiteral("Pictures/eBay/IMG_1.jpg");
    p.deletedOn = Side::Local;
    state.setPending(p);
    QVERIFY(state.save());
    SyncHistory history(SyncSession::historyFilePath());
    SyncResult ok;
    ok.success = true;
    ok.pendingDeletions = 1;
    QVERIFY(history.append(ok));

    DashboardPage page;
    page.refresh();
    QVERIFY2(page.statusLine().contains(QLatin1String("waiting for the next sync")),
             qPrintable(page.statusLine()));
    QVERIFY(labelTexts(page).contains(page.statusLine()));
    const QString details = page.statusDetails();
    QVERIFY(details.contains(QLatin1String("Pictures/eBay/IMG_1.jpg")));
    QVERIFY(details.contains(QLatin1String("deleted on the phone, will be deleted in Nextcloud")));
    QVERIFY(details.contains(QLatin1String("restore")));
}

// A failed sync shows the error in the explanation.
void TestDashboardPage::failedSyncIsExplained()
{
    SyncHistory history(SyncSession::historyFilePath());
    SyncResult failed;
    failed.errorMessage = QStringLiteral("Server not found.");
    QVERIFY(history.append(failed));
    DashboardPage page;
    page.refresh();
    QVERIFY(page.statusLine().contains(QLatin1String("failed")));
    QVERIFY(page.statusDetails().contains(QLatin1String("Server not found.")));
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
