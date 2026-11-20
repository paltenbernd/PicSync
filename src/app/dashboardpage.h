// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QFrame>
#include <QPoint>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <vector>

namespace PicSync {

/// Main screen ("dashboard" layout): cards for connection, storage usage,
/// folders and sync conditions, plus the big "Sync now" button.
class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    // Builds the page.
    DashboardPage();

    // Destroys the page.
    ~DashboardPage() override;

    // Re-reads settings, state and history and updates all cards.
    void refresh();

    // Switches between the idle and the running look.
    void setRunning(bool running);

    // Returns the short status line shown below the connection (e.g. "1 deletion waiting").
    QString statusLine() const;

    // Returns the explanation shown when the user taps the connection or folders card.
    QString statusDetails() const;

public Q_SLOTS:
    // Shows the progress of a running synchronisation.
    void setProgress(int done, int total, const QString &currentFile);

    // Shows the current phase of a running synchronisation.
    void setPhase(const QString &phase);

Q_SIGNALS:
    // The user wants to synchronise now.
    void syncRequested();

    // The user wants to stop the running synchronisation.
    void stopRequested();

protected:
    // Opens the status explanation when the connection or folders card is tapped.
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    // Shows the status explanation in a dialog.
    void showStatusDetails();

    // Starts or stops a synchronisation, depending on the current state.
    void onSyncButton();

    // Opens the system page for the storage permission.
    void onGrantStorage();

private:
    // Creates an empty card with icon and title in its first row.
    static void setupCard(QFrame &card, QGridLayout &layout, QLabel &icon, QLabel &title,
                          const QString &iconName, const QString &text);

    // Updates the folder list of the folders card.
    void refreshFolders();

    bool m_running = false;
    QPoint m_pressPosition;   ///< Where a tap on a card started (to tell taps from swipes).

    // Declared parents-first, so children are destroyed before their parents.
    std::unique_ptr<QVBoxLayout> m_layout;
    std::unique_ptr<QScrollArea> m_scroll;
    std::unique_ptr<QWidget> m_content;
    std::unique_ptr<QVBoxLayout> m_contentLayout;

    std::unique_ptr<QFrame> m_permCard;
    std::unique_ptr<QHBoxLayout> m_permLayout;
    std::unique_ptr<QLabel> m_permIcon;
    std::unique_ptr<QLabel> m_permText;
    std::unique_ptr<QPushButton> m_permButton;

    std::unique_ptr<QFrame> m_serverCard;
    std::unique_ptr<QGridLayout> m_serverLayout;
    std::unique_ptr<QLabel> m_serverIcon;
    std::unique_ptr<QLabel> m_serverTitle;
    std::unique_ptr<QLabel> m_serverStatus;
    std::unique_ptr<QLabel> m_serverDetail;
    std::unique_ptr<QLabel> m_serverState;

    std::unique_ptr<QFrame> m_storageCard;
    std::unique_ptr<QGridLayout> m_storageLayout;
    std::unique_ptr<QLabel> m_storageIcon;
    std::unique_ptr<QLabel> m_storageTitle;
    std::unique_ptr<QLabel> m_serverSpaceTitle;
    std::unique_ptr<QProgressBar> m_storageBar;
    std::unique_ptr<QLabel> m_storageDetail;
    std::unique_ptr<QLabel> m_phoneSpaceTitle;
    std::unique_ptr<QProgressBar> m_phoneBar;
    std::unique_ptr<QLabel> m_phoneDetail;
    std::unique_ptr<QLabel> m_syncedDetail;

    std::unique_ptr<QFrame> m_foldersCard;
    std::unique_ptr<QGridLayout> m_foldersLayout;
    std::unique_ptr<QLabel> m_foldersIcon;
    std::unique_ptr<QLabel> m_foldersTitle;
    std::unique_ptr<QLabel> m_foldersCount;
    std::vector<std::unique_ptr<QLabel>> m_folderLabels;

    std::unique_ptr<QFrame> m_conditionsCard;
    std::unique_ptr<QGridLayout> m_conditionsLayout;
    std::unique_ptr<QLabel> m_conditionsIcon;
    std::unique_ptr<QLabel> m_conditionsTitle;
    std::unique_ptr<QLabel> m_conditionsDetail;
    std::unique_ptr<QLabel> m_lastSync;

    std::unique_ptr<QLabel> m_phase;
    std::unique_ptr<QProgressBar> m_progress;
    std::unique_ptr<QPushButton> m_syncButton;
};

} // namespace PicSync
