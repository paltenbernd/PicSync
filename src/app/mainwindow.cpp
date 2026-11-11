// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "mainwindow.h"
#include "appsettings.h"
#include "platform/platform.h"
#include "uihelpers.h"

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

    connect(m_dashboard.get(), &DashboardPage::syncRequested, &m_controller,
            &SyncController::start);
    connect(m_dashboard.get(), &DashboardPage::stopRequested, &m_controller,
            &SyncController::stop);
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
