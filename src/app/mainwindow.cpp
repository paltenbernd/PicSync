// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "mainwindow.h"
#include "appsettings.h"
#include "platform/platform.h"
#include "syncprogress.h"
#include "uihelpers.h"

#include <QBackingStore>
#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QSettings>

namespace PicSync {

namespace {
const int kRefreshIntervalMs = 30 * 1000;
}

// Builds the window and shows the settings first if PicSync is not set up yet.
MainWindow::MainWindow()
    : m_central(std::make_unique<QWidget>())
    , m_centralLayout(std::make_unique<QVBoxLayout>())
    , m_header(std::make_unique<QFrame>())
    , m_headerLayout(std::make_unique<QHBoxLayout>())
    , m_back(std::make_unique<QToolButton>())
    , m_logo(std::make_unique<QLabel>())
    , m_title(std::make_unique<QLabel>())
    , m_historyButton(std::make_unique<QToolButton>())
    , m_settingsButton(std::make_unique<QToolButton>())
    , m_stack(std::make_unique<QStackedWidget>())
    , m_dashboard(std::make_unique<DashboardPage>())
    , m_settings(std::make_unique<SettingsPage>())
    , m_history(std::make_unique<HistoryPage>())
{
    setWindowTitle(QStringLiteral("PicSync"));
    setWindowIcon(QIcon(QStringLiteral(":/picsync.png")));
    setCentralWidget(m_central.get());
    m_central->setLayout(m_centralLayout.get());
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);

    // App bar.
    m_header->setObjectName(QStringLiteral("header"));
    m_header->setLayout(m_headerLayout.get());
    m_headerLayout->setContentsMargins(6, 6, 6, 6);
    m_back->setIcon(Ui::icon(QStringLiteral("back_white")));
    m_back->setIconSize(QSize(26, 26));
    m_back->setToolTip(tr("Back"));
    m_logo->setPixmap(Ui::pixmap(QStringLiteral("sync_white"), 26));
    m_title->setObjectName(QStringLiteral("headerTitle"));
    m_historyButton->setIcon(Ui::icon(QStringLiteral("history_white")));
    m_historyButton->setIconSize(QSize(26, 26));
    m_historyButton->setToolTip(tr("History"));
    m_settingsButton->setIcon(Ui::icon(QStringLiteral("settings_white")));
    m_settingsButton->setIconSize(QSize(26, 26));
    m_settingsButton->setToolTip(tr("Settings"));
    m_headerLayout->addWidget(m_back.get());
    m_headerLayout->addSpacing(4);
    m_headerLayout->addWidget(m_logo.get());
    m_headerLayout->addWidget(m_title.get(), 1);
    m_headerLayout->addWidget(m_historyButton.get());
    m_headerLayout->addWidget(m_settingsButton.get());
    m_centralLayout->addWidget(m_header.get());

    m_stack->addWidget(m_dashboard.get());
    m_stack->addWidget(m_settings.get());
    m_stack->addWidget(m_history.get());
    m_centralLayout->addWidget(m_stack.get(), 1);

    connect(m_back.get(), &QToolButton::clicked, this, &MainWindow::showDashboard);
    connect(m_historyButton.get(), &QToolButton::clicked, this, &MainWindow::showHistory);
    connect(m_settingsButton.get(), &QToolButton::clicked, this, &MainWindow::showSettings);

    connect(m_dashboard.get(), &DashboardPage::syncRequested, this, &MainWindow::startSync);
    connect(m_dashboard.get(), &DashboardPage::stopRequested, this, &MainWindow::stopSync);
    // Progress of runs in the background sync service (Android).
    connect(&m_progressTimer, &QTimer::timeout, this, &MainWindow::pollServiceProgress);
    m_progressTimer.start(700);
    connect(&m_controller, &SyncController::started, this, &MainWindow::onSyncStarted);
    connect(&m_controller, &SyncController::progress, m_dashboard.get(),
            &DashboardPage::setProgress);
    connect(&m_controller, &SyncController::phaseChanged, m_dashboard.get(),
            &DashboardPage::setPhase);
    connect(&m_controller, &SyncController::finished, this, &MainWindow::onSyncFinished);
    connect(m_settings.get(), &SettingsPage::saved, this, &MainWindow::onSettingsSaved);
    connect(m_settings.get(), &SettingsPage::resetDone, this, &MainWindow::refreshVisible);

    connect(&m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshVisible);
    m_refreshTimer.start(kRefreshIntervalMs);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
            &MainWindow::onApplicationStateChanged);

#ifdef Q_OS_ANDROID
    // Qt Widgets on Android show partial updates incompletely (QTBUG-135011):
    // typed text stayed invisible and scrolling left stale stripes. Re-sending
    // the complete window image after every update avoids that reliably.
    m_fullFlush = !QCoreApplication::arguments().contains(QStringLiteral("--partial-flush"));
#endif

    // Make sure the background job exists (e.g. after an app update).
    QSettings qsettings;
    const AppSettings settings = AppSettings::load(qsettings, Platform::deviceName());
    if (settings.isComplete()) {
        Platform::scheduleSync(settings);
        showDashboard();
    } else {
        showSettings();
    }
}

// Stops a running synchronisation and destroys the window.
MainWindow::~MainWindow() = default;

// Switches the stack to a page and adapts the app bar.
void MainWindow::showPage(QWidget &page, const QString &title, bool isHome)
{
    m_stack->setCurrentWidget(&page);
    m_title->setText(title);
    m_back->setVisible(!isHome);
    m_logo->setVisible(isHome);
    m_historyButton->setVisible(isHome);
    m_settingsButton->setVisible(isHome);
}

// Shows the dashboard.
void MainWindow::showDashboard()
{
    m_dashboard->refresh();
    showPage(*m_dashboard, QStringLiteral("PicSync"), true);
}

// Shows the settings page.
void MainWindow::showSettings()
{
    m_settings->load();
    showPage(*m_settings, tr("Settings"), false);
}

// Shows the history page.
void MainWindow::showHistory()
{
    m_history->refresh();
    showPage(*m_history, tr("History"), false);
}

// Starts a synchronisation: in the background service on Android, else here.
void MainWindow::startSync()
{
    if (m_controller.isRunning() || m_serviceSync)
        return;
    if (Platform::requestManualSync()) {
        // The service runs independently of this window, also when the app is
        // in the background; the progress file tells us what it is doing.
        m_serviceSync = true;
        m_serviceSeenRunning = false;
        m_requestedAt.start();
        m_dashboard->setRunning(true);
        return;
    }
    m_controller.start();
}

// Asks the running synchronisation to stop.
void MainWindow::stopSync()
{
    if (m_controller.isRunning())
        m_controller.stop();
    else
        Platform::requestStopSync();
}

// Shows the progress of a synchronisation running in the sync service.
void MainWindow::pollServiceProgress()
{
    if (m_controller.isRunning())
        return;   // the app is syncing itself (desktop)
    const SyncProgressInfo info = SyncProgress::read();
    const bool active = SyncProgress::isActive(info, QDateTime::currentMSecsSinceEpoch());
    if (active) {
        // Also shows background (scheduled) runs that started without us.
        if (!m_serviceSync) {
            m_serviceSync = true;
            m_dashboard->setRunning(true);
        }
        m_serviceSeenRunning = true;
        if (!info.phase.isEmpty())
            m_dashboard->setPhase(info.phase);
        if (info.total > 0)
            m_dashboard->setProgress(info.done, info.total, info.currentFile);
        return;
    }
    if (!m_serviceSync)
        return;
    // Finished, or the service never came up (give it 30 seconds to start).
    if (m_serviceSeenRunning || !m_requestedAt.isValid() || m_requestedAt.elapsed() > 30000) {
        m_serviceSync = false;
        m_requestedAt.invalidate();
        m_dashboard->setRunning(false);
        refreshVisible();
    }
}

// Updates the UI when a synchronisation starts.
void MainWindow::onSyncStarted()
{
    m_dashboard->setRunning(true);
}

// Updates the UI when a synchronisation has finished.
void MainWindow::onSyncFinished(const PicSync::SyncResult &result)
{
    Q_UNUSED(result)
    m_dashboard->setRunning(false);
    refreshVisible();
}

// Refreshes the visible page (e.g. after a background sync).
void MainWindow::refreshVisible()
{
    if (m_stack->currentWidget() == m_dashboard.get())
        m_dashboard->refresh();
    else if (m_stack->currentWidget() == m_history.get())
        m_history->refresh();
}

// Reacts to the app coming back to the foreground.
void MainWindow::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (state != Qt::ApplicationActive)
        return;
    m_settings->refreshPermissions();
    refreshVisible();
}

// Returns to the dashboard after the settings were saved.
void MainWindow::onSettingsSaved()
{
    showDashboard();
}

// Shows every repaint on the whole screen (works around a Qt bug on Android).
bool MainWindow::event(QEvent *event)
{
    const bool handled = QMainWindow::event(event);
    if (m_fullFlush && event->type() == QEvent::UpdateRequest && backingStore()
            && windowHandle())
        backingStore()->flush(QRegion(rect()), windowHandle());
    return handled;
}

// Handles the Android back key (returns to the dashboard first).
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Back && m_stack->currentWidget() != m_dashboard.get()) {
        event->accept();
        showDashboard();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

} // namespace PicSync
