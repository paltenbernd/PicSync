// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "dashboardpage.h"
#include "historypage.h"
#include "settingspage.h"
#include "synccontroller.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QElapsedTimer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>

namespace PicSync {

/// Top-level window: blue app bar plus the dashboard, settings and history pages.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // Builds the window and shows the settings first if PicSync is not set up yet.
    MainWindow();

    // Stops a running synchronisation and destroys the window.
    ~MainWindow() override;

public Q_SLOTS:
    // Starts a synchronisation: in the background service on Android, else here.
    void startSync();

    // Asks the running synchronisation to stop.
    void stopSync();

private Q_SLOTS:
    // Shows the progress of a synchronisation running in the sync service.
    void pollServiceProgress();

    // Shows the dashboard.
    void showDashboard();

    // Shows the settings page.
    void showSettings();

    // Shows the history page.
    void showHistory();

    // Updates the UI when a synchronisation starts.
    void onSyncStarted();

    // Updates the UI when a synchronisation has finished.
    void onSyncFinished(const PicSync::SyncResult &result);

    // Refreshes the visible page (e.g. after a background sync).
    void refreshVisible();

    // Reacts to the app coming back to the foreground.
    void onApplicationStateChanged(Qt::ApplicationState state);

    // Returns to the dashboard after the settings were saved.
    void onSettingsSaved();


private:
    // Switches the stack to a page and adapts the app bar.
    void showPage(QWidget &page, const QString &title, bool isHome);

    // Handles the Android back key (returns to the dashboard first).
    void keyPressEvent(QKeyEvent *event) override;

    // Shows every repaint on the whole screen (works around a Qt bug on Android).
    bool event(QEvent *event) override;

    // Declared parents-first, so children are destroyed before their parents.
    std::unique_ptr<QWidget> m_central;
    std::unique_ptr<QVBoxLayout> m_centralLayout;
    std::unique_ptr<QFrame> m_header;
    std::unique_ptr<QHBoxLayout> m_headerLayout;
    std::unique_ptr<QToolButton> m_back;
    std::unique_ptr<QLabel> m_logo;
    std::unique_ptr<QLabel> m_title;
    std::unique_ptr<QToolButton> m_historyButton;
    std::unique_ptr<QToolButton> m_settingsButton;
    std::unique_ptr<QStackedWidget> m_stack;
    std::unique_ptr<DashboardPage> m_dashboard;
    std::unique_ptr<SettingsPage> m_settings;
    std::unique_ptr<HistoryPage> m_history;

    SyncController m_controller;
    QTimer m_refreshTimer;
    bool m_fullFlush = false;
    QTimer m_progressTimer;
    QElapsedTimer m_requestedAt;     ///< When "Sync now" handed work to the service.
    bool m_serviceSync = false;      ///< The dashboard shows a service run.
    bool m_serviceSeenRunning = false;
};

} // namespace PicSync
