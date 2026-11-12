// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "settingspage.h"
#include "appsettings.h"
#include "pathutils.h"
#include "platform/platform.h"
#include "synchistory.h"
#include "syncsession.h"
#include "uihelpers.h"
#include "webdavclient.h"

#include <QFile>
#include <QApplication>
#include <QGuiApplication>
#include <QInputMethod>
#include <QPointer>
#include <QSettings>
#include <QStyle>

namespace PicSync {

// Builds the page.
SettingsPage::SettingsPage()
    : m_layout(std::make_unique<QVBoxLayout>())
    , m_scroll(std::make_unique<QScrollArea>())
    , m_content(std::make_unique<QWidget>())
    , m_contentLayout(std::make_unique<QVBoxLayout>())
    , m_serverCard(std::make_unique<QFrame>())
    , m_serverLayout(std::make_unique<QVBoxLayout>())
    , m_serverTitle(std::make_unique<QLabel>())
    , m_serverForm(std::make_unique<QFormLayout>())
    , m_url(std::make_unique<QLineEdit>())
    , m_user(std::make_unique<QLineEdit>())
    , m_password(std::make_unique<QLineEdit>())
    , m_passwordHint(std::make_unique<QLabel>())
    , m_folderRow(std::make_unique<QHBoxLayout>())
    , m_folder(std::make_unique<QLineEdit>())
    , m_defaultFolder(std::make_unique<QPushButton>())
    , m_test(std::make_unique<QPushButton>())
    , m_syncCard(std::make_unique<QFrame>())
    , m_syncLayout(std::make_unique<QVBoxLayout>())
    , m_syncTitle(std::make_unique<QLabel>())
    , m_syncForm(std::make_unique<QFormLayout>())
    , m_interval(std::make_unique<QComboBox>())
    , m_wifiOnly(std::make_unique<QCheckBox>())
    , m_chargingOnly(std::make_unique<QCheckBox>())
    , m_notifications(std::make_unique<QCheckBox>())
    , m_deleteHint(std::make_unique<QLabel>())
    , m_permCard(std::make_unique<QFrame>())
    , m_permLayout(std::make_unique<QVBoxLayout>())
    , m_permTitle(std::make_unique<QLabel>())
    , m_permForm(std::make_unique<QFormLayout>())
    , m_storageButton(std::make_unique<QPushButton>())
    , m_notifyButton(std::make_unique<QPushButton>())
    , m_maintCard(std::make_unique<QFrame>())
    , m_maintLayout(std::make_unique<QVBoxLayout>())
    , m_maintTitle(std::make_unique<QLabel>())
    , m_reset(std::make_unique<QPushButton>())
    , m_signOut(std::make_unique<QPushButton>())
    , m_aboutCard(std::make_unique<QFrame>())
    , m_aboutLayout(std::make_unique<QVBoxLayout>())
    , m_aboutTitle(std::make_unique<QLabel>())
    , m_about(std::make_unique<QLabel>())
    , m_message(std::make_unique<QLabel>())
    , m_save(std::make_unique<QPushButton>())
{
    setLayout(m_layout.get());
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Ui::enableTouchScrolling(*m_scroll);
    m_content->setObjectName(QStringLiteral("page"));
    m_content->setLayout(m_contentLayout.get());
    m_contentLayout->setContentsMargins(14, 14, 14, 14);
    m_contentLayout->setSpacing(12);
    m_scroll->setWidget(m_content.get());
    m_layout->addWidget(m_scroll.get(), 1);

    // Nextcloud login.
    setupSection(*m_serverCard, *m_serverLayout, *m_serverTitle, tr("Nextcloud"));
    m_serverLayout->addLayout(m_serverForm.get());
    m_serverForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    m_url->setPlaceholderText(QStringLiteral("magentacloud.de"));
    // Without suggestions the Android keyboard commits every key immediately;
    // otherwise Qt Widgets show the typed text only after leaving the field.
    const Qt::InputMethodHints plain = Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;
    m_url->setObjectName(QStringLiteral("serverUrl"));
    m_url->setInputMethodHints(plain | Qt::ImhUrlCharactersOnly);
    m_user->setObjectName(QStringLiteral("userName"));
    m_user->setInputMethodHints(plain);
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setObjectName(QStringLiteral("appPassword"));
    m_password->setInputMethodHints(plain | Qt::ImhSensitiveData);
    m_passwordHint->setObjectName(QStringLiteral("detail"));
    m_passwordHint->setWordWrap(true);
    m_passwordHint->setText(tr("Create an app password in Nextcloud under Settings > "
                               "Security > Devices & sessions."));
    m_folder->setObjectName(QStringLiteral("remoteFolder"));
    m_folder->setInputMethodHints(plain);
    m_folderRow->addWidget(m_folder.get(), 1);
    m_defaultFolder->setText(tr("Default"));
    m_folderRow->addWidget(m_defaultFolder.get());
    m_serverForm->addRow(tr("Server address"), m_url.get());
    m_serverForm->addRow(tr("User name"), m_user.get());
    m_serverForm->addRow(tr("App password"), m_password.get());
    m_serverForm->addRow(m_passwordHint.get());
    m_serverForm->addRow(tr("Folder in Nextcloud"), m_folderRow.get());
    m_test->setText(tr("Test connection"));
    m_serverLayout->addWidget(m_test.get());
    m_contentLayout->addWidget(m_serverCard.get());
    connect(m_test.get(), &QPushButton::clicked, this, &SettingsPage::onTestConnection);
    connect(m_defaultFolder.get(), &QPushButton::clicked, this, &SettingsPage::onDefaultFolder);

    // Synchronisation.
    setupSection(*m_syncCard, *m_syncLayout, *m_syncTitle, tr("Synchronisation"));
    m_syncLayout->addLayout(m_syncForm.get());
    m_syncForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    const QList<int> choices = AppSettings::intervalChoices();
    for (int minutes : choices)
        m_interval->addItem(Ui::intervalText(minutes), minutes);
    m_syncForm->addRow(tr("Interval"), m_interval.get());
    m_wifiOnly->setText(tr("Only via Wi-Fi"));
    m_chargingOnly->setText(tr("Only while charging"));
    m_notifications->setText(tr("Show notifications"));
    m_syncForm->addRow(m_wifiOnly.get());
    m_syncForm->addRow(m_chargingOnly.get());
    m_syncForm->addRow(m_notifications.get());
    m_deleteHint->setObjectName(QStringLiteral("detail"));
    m_deleteHint->setWordWrap(true);
    m_deleteHint->setText(tr("Deletions are passed on one sync later. Until then the file is "
                             "locked on the other side, so you can still restore it."));
    m_syncLayout->addWidget(m_deleteHint.get());
    m_contentLayout->addWidget(m_syncCard.get());

    // Permissions.
    setupSection(*m_permCard, *m_permLayout, *m_permTitle, tr("Permissions"));
    m_permLayout->addLayout(m_permForm.get());
    m_permForm->setRowWrapPolicy(QFormLayout::DontWrapRows);
    m_permForm->addRow(tr("All files access"), m_storageButton.get());
    m_permForm->addRow(tr("Notifications"), m_notifyButton.get());
    m_contentLayout->addWidget(m_permCard.get());
    connect(m_storageButton.get(), &QPushButton::clicked, this, &SettingsPage::onGrantStorage);
    connect(m_notifyButton.get(), &QPushButton::clicked, this,
            &SettingsPage::onGrantNotifications);

    // Maintenance.
    setupSection(*m_maintCard, *m_maintLayout, *m_maintTitle, tr("Maintenance"));
    m_reset->setText(tr("Reset synchronisation"));
    m_signOut->setText(tr("Sign out"));
    m_signOut->setObjectName(QStringLiteral("dangerButton"));
    m_maintLayout->addWidget(m_reset.get());
    m_maintLayout->addWidget(m_signOut.get());
    m_contentLayout->addWidget(m_maintCard.get());
    connect(m_reset.get(), &QPushButton::clicked, this, &SettingsPage::onResetSync);
    connect(m_signOut.get(), &QPushButton::clicked, this, &SettingsPage::onSignOut);

    // About.
    setupSection(*m_aboutCard, *m_aboutLayout, *m_aboutTitle, tr("About PicSync"));
    m_about->setObjectName(QStringLiteral("detail"));
    m_about->setWordWrap(true);
    m_about->setTextFormat(Qt::RichText);
    m_about->setOpenExternalLinks(true);
    m_about->setText(tr("Version %1<br>© 2026 Peter Altenbernd<br>"
                        "Licence: GNU General Public License v3.0 or later<br>"
                        "Built with Qt %2.").arg(QStringLiteral(PICSYNC_VERSION),
                                                 QString::fromLatin1(qVersion())));
    m_aboutLayout->addWidget(m_about.get());
    m_contentLayout->addWidget(m_aboutCard.get());
    m_contentLayout->addStretch(1);

    // Bottom: status line and save button.
    m_message->setObjectName(QStringLiteral("phase"));
    m_message->setProperty("role", QStringLiteral("message"));
    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignCenter);
    m_message->hide();
    m_save->setObjectName(QStringLiteral("primaryButton"));
    m_save->setObjectName(QStringLiteral("saveButton"));
    m_save->setText(tr("Save"));
    m_save->setMinimumHeight(52);
    m_layout->addWidget(m_message.get());
    m_layout->addWidget(m_save.get());
    connect(m_save.get(), &QPushButton::clicked, this, &SettingsPage::onSave);

    connect(qApp, &QApplication::focusChanged, this, &SettingsPage::onFocusChanged);
    connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged, this,
            &SettingsPage::ensureFocusVisible);

    load();
}

// Destroys the page.
SettingsPage::~SettingsPage() = default;

// Creates a titled card that holds a form.
void SettingsPage::setupSection(QFrame &card, QVBoxLayout &layout, QLabel &title,
                                const QString &text)
{
    card.setObjectName(QStringLiteral("card"));
    card.setLayout(&layout);
    layout.setContentsMargins(14, 12, 14, 14);
    layout.setSpacing(8);
    title.setObjectName(QStringLiteral("cardTitle"));
    title.setText(text);
    layout.addWidget(&title);
}

// Fills all fields from the stored settings.
void SettingsPage::load()
{
    QSettings qsettings;
    const AppSettings s = AppSettings::load(qsettings, Platform::deviceName());
    m_url->setText(s.serverUrl);
    m_user->setText(s.user);
    m_folder->setText(s.remoteFolder);
    m_password->clear();
    QString stored;
    const std::unique_ptr<CredentialStore> credentials = Platform::createCredentialStore();
    const bool hasPassword = !s.user.isEmpty() && credentials->load(s.user, stored);
    m_password->setPlaceholderText(hasPassword ? tr("stored – leave empty to keep")
                                               : tr("app password"));
    m_interval->setCurrentIndex(qMax(0, m_interval->findData(s.intervalMinutes)));
    m_wifiOnly->setChecked(s.wifiOnly);
    m_chargingOnly->setChecked(s.chargingOnly);
    m_notifications->setChecked(s.notifications);
    m_message->hide();
    refreshPermissions();
}

// Updates the permission status lines (after returning from the system settings).
void SettingsPage::refreshPermissions()
{
    const bool storage = Platform::hasStorageAccess();
    m_storageButton->setText(storage ? tr("Granted") : tr("Grant"));
    m_storageButton->setEnabled(!storage);
    const bool notify = Platform::hasNotificationPermission();
    m_notifyButton->setText(notify ? tr("Granted") : tr("Allow"));
    m_notifyButton->setEnabled(!notify);
}

// Shows a status line below the buttons (success or error).
void SettingsPage::showMessage(const QString &text, bool error)
{
    m_message->setText(text);
    m_message->setProperty("error", error);
    m_message->style()->unpolish(m_message.get());
    m_message->style()->polish(m_message.get());
    m_message->show();
}

// Scrolls the newly focused input field into view.
void SettingsPage::onFocusChanged(QWidget *previous, QWidget *current)
{
    Q_UNUSED(previous)
    const QPointer<QWidget> focused(current);
    if (focused && m_content->isAncestorOf(focused.data()))
        ensureFocusVisible();
}

// Keeps the focused input field visible when the keyboard opens or closes.
void SettingsPage::ensureFocusVisible()
{
    const QPointer<QWidget> focused(QApplication::focusWidget());
    if (focused && m_content->isAncestorOf(focused.data()))
        m_scroll->ensureWidgetVisible(focused.data(), 20, 60);
    m_scroll->viewport()->update();
}

// Puts the default folder (derived from the device name) into the folder field.
void SettingsPage::onDefaultFolder()
{
    m_folder->setText(PathUtils::defaultRemoteFolder(Platform::deviceName()));
}

// Validates and stores all settings and reschedules the background sync.
void SettingsPage::onSave()
{
    AppSettings s;
    s.serverUrl = PathUtils::normalizeServerUrl(m_url->text());
    s.user = m_user->text().trimmed();
    s.remoteFolder = PathUtils::normalizeRelative(m_folder->text());
    s.intervalMinutes = m_interval->currentData().toInt();
    s.wifiOnly = m_wifiOnly->isChecked();
    s.chargingOnly = m_chargingOnly->isChecked();
    s.notifications = m_notifications->isChecked();

    if (!s.isComplete()) {
        showMessage(tr("Please fill in server address, user name and folder."), true);
        return;
    }
    const std::unique_ptr<CredentialStore> credentials = Platform::createCredentialStore();
    QString stored;
    if (!m_password->text().isEmpty()) {
        if (!credentials->save(s.user, m_password->text())) {
            showMessage(tr("The password could not be stored securely."), true);
            return;
        }
    } else if (!credentials->load(s.user, stored)) {
        showMessage(tr("Please enter the app password."), true);
        return;
    }

    QSettings qsettings;
    s.save(qsettings);
    Platform::scheduleSync(s);
    if (s.notifications && !Platform::hasNotificationPermission())
        Platform::requestNotificationPermission();
    load();
    showMessage(tr("Settings saved."), false);
    Q_EMIT saved();
}

// Checks server address and login with the values currently entered.
void SettingsPage::onTestConnection()
{
    const QString user = m_user->text().trimmed();
    QString password = m_password->text();
    if (password.isEmpty()) {
        const std::unique_ptr<CredentialStore> credentials = Platform::createCredentialStore();
        credentials->load(user, password);
    }
    if (m_url->text().trimmed().isEmpty() || user.isEmpty() || password.isEmpty()) {
        showMessage(tr("Please fill in server address, user name and app password."), true);
        return;
    }
    showMessage(tr("Connecting…"), false);
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    WebDavClient client(m_url->text(), user, password, m_folder->text());
    client.setTimeout(20000);
    QString error;
    const bool ok = client.testConnection(error);
    QuotaInfo quota;
    QString ignored;
    if (ok)
        client.quota(quota, ignored);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) {
        showMessage(error, true);
    } else if (quota.valid) {
        showMessage(tr("Connection works. %1 used on the server.")
                        .arg(Ui::sizeText(quota.usedBytes)), false);
    } else {
        showMessage(tr("Connection works."), false);
    }
}

// Opens the system page for "All files access".
void SettingsPage::onGrantStorage()
{
    Platform::requestStorageAccess();
}

// Asks for permission to show notifications.
void SettingsPage::onGrantNotifications()
{
    Platform::requestNotificationPermission();
}

// Forgets the sync state so the next run starts from scratch.
void SettingsPage::onResetSync()
{
    if (!Ui::confirm(*this, tr("Reset synchronisation"),
                     tr("PicSync forgets which files were synchronised. Nothing is deleted; "
                        "the next sync compares both sides again and copies what is missing. "
                        "Continue?")))
        return;
    QFile::remove(SyncSession::stateFilePath());
    SyncHistory history(SyncSession::historyFilePath());
    history.clear();
    showMessage(tr("Synchronisation was reset."), false);
    Q_EMIT resetDone();
}

// Deletes the stored login and stops the background sync.
void SettingsPage::onSignOut()
{
    if (!Ui::confirm(*this, tr("Sign out"),
                     tr("The app password is deleted from this phone and automatic "
                        "synchronisation stops. Your photos stay untouched. Continue?")))
        return;
    const std::unique_ptr<CredentialStore> credentials = Platform::createCredentialStore();
    credentials->clear();
    Platform::cancelScheduledSync();
    load();
    showMessage(tr("Signed out."), false);
    Q_EMIT saved();
}

} // namespace PicSync
