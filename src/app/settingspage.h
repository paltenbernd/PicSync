// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace PicSync {

/// Settings screen: Nextcloud login, sync folder, interval and conditions,
/// permissions, maintenance and information about the app.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    // Builds the page.
    SettingsPage();

    // Destroys the page.
    ~SettingsPage() override;

    // Fills all fields from the stored settings.
    void load();

    // Updates the permission status lines (after returning from the system settings).
    void refreshPermissions();

Q_SIGNALS:
    // Emitted after the settings were stored successfully.
    void saved();

    // Emitted after the synchronisation state or history was reset.
    void resetDone();

private Q_SLOTS:
    // Validates and stores all settings and reschedules the background sync.
    void onSave();

    // Checks server address and login with the values currently entered.
    void onTestConnection();

    // Opens the system page for "All files access".
    void onGrantStorage();

    // Asks for permission to show notifications.
    void onGrantNotifications();

    // Forgets the sync state so the next run starts from scratch.
    void onResetSync();

    // Deletes the stored login and stops the background sync.
    void onSignOut();

    // Scrolls the newly focused input field into view.
    void onFocusChanged(QWidget *previous, QWidget *current);

    // Keeps the focused input field visible when the keyboard opens or closes.
    void ensureFocusVisible();

    // Puts the default folder (derived from the device name) into the folder field.
    void onDefaultFolder();

private:
    // Creates a titled card that holds a form.
    static void setupSection(QFrame &card, QVBoxLayout &layout, QLabel &title,
                             const QString &text);

    // Shows a status line below the buttons (success or error).
    void showMessage(const QString &text, bool error);

    // Declared parents-first, so children are destroyed before their parents.
    std::unique_ptr<QVBoxLayout> m_layout;
    std::unique_ptr<QScrollArea> m_scroll;
    std::unique_ptr<QWidget> m_content;
    std::unique_ptr<QVBoxLayout> m_contentLayout;

    std::unique_ptr<QFrame> m_serverCard;
    std::unique_ptr<QVBoxLayout> m_serverLayout;
    std::unique_ptr<QLabel> m_serverTitle;
    std::unique_ptr<QFormLayout> m_serverForm;
    std::unique_ptr<QLineEdit> m_url;
    std::unique_ptr<QLineEdit> m_user;
    std::unique_ptr<QLineEdit> m_password;
    std::unique_ptr<QLabel> m_passwordHint;
    std::unique_ptr<QHBoxLayout> m_folderRow;
    std::unique_ptr<QLineEdit> m_folder;
    std::unique_ptr<QPushButton> m_defaultFolder;
    std::unique_ptr<QPushButton> m_test;

    std::unique_ptr<QFrame> m_syncCard;
    std::unique_ptr<QVBoxLayout> m_syncLayout;
    std::unique_ptr<QLabel> m_syncTitle;
    std::unique_ptr<QFormLayout> m_syncForm;
    std::unique_ptr<QComboBox> m_interval;
    std::unique_ptr<QCheckBox> m_wifiOnly;
    std::unique_ptr<QCheckBox> m_chargingOnly;
    std::unique_ptr<QCheckBox> m_notifications;
    std::unique_ptr<QLabel> m_deleteHint;

    std::unique_ptr<QFrame> m_permCard;
    std::unique_ptr<QVBoxLayout> m_permLayout;
    std::unique_ptr<QLabel> m_permTitle;
    std::unique_ptr<QFormLayout> m_permForm;
    std::unique_ptr<QPushButton> m_storageButton;
    std::unique_ptr<QPushButton> m_notifyButton;

    std::unique_ptr<QFrame> m_maintCard;
    std::unique_ptr<QVBoxLayout> m_maintLayout;
    std::unique_ptr<QLabel> m_maintTitle;
    std::unique_ptr<QPushButton> m_reset;
    std::unique_ptr<QPushButton> m_signOut;

    std::unique_ptr<QFrame> m_aboutCard;
    std::unique_ptr<QVBoxLayout> m_aboutLayout;
    std::unique_ptr<QLabel> m_aboutTitle;
    std::unique_ptr<QLabel> m_about;

    std::unique_ptr<QLabel> m_message;
    std::unique_ptr<QPushButton> m_save;
};

} // namespace PicSync
