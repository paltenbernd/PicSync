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

private Q_SLOTS:
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
};

} // namespace PicSync
