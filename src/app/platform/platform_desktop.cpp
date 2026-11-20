// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.
//
// Desktop stand-ins for the Android platform layer. They let the app run on a
// PC for development: the "gallery" is the user's Pictures folder and the
// password is kept in the settings file.

#include "platform.h"
#include "directorymediastore.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>

namespace PicSync {

namespace {

/// Development-only credential store. Not secure: the password is only obfuscated.
class SettingsCredentialStore : public CredentialStore
{
public:
    // Stores the app password for a user.
    bool save(const QString &user, const QString &password) override
    {
        QSettings settings;
        settings.setValue(QStringLiteral("credentials/user"), user);
        settings.setValue(QStringLiteral("credentials/data"),
                          QString::fromLatin1(password.toUtf8().toBase64()));
        settings.sync();
        return settings.status() == QSettings::NoError;
    }

    // Loads the stored app password of a user.
    bool load(const QString &user, QString &password) override
    {
        QSettings settings;
        if (settings.value(QStringLiteral("credentials/user")).toString() != user)
            return false;
        const QByteArray data = settings.value(QStringLiteral("credentials/data")).toByteArray();
        if (data.isEmpty())
            return false;
        password = QString::fromUtf8(QByteArray::fromBase64(data));
        return true;
    }

    // Deletes the stored credentials.
    void clear() override
    {
        QSettings settings;
        settings.remove(QStringLiteral("credentials"));
    }
};

} // namespace

namespace Platform {

// Returns the user-visible name of the computer.
QString deviceName()
{
    return QSysInfo::machineHostName();
}

// Returns the root of the phone's shared storage (the home folder on a PC).
QString sharedStorageRoot()
{
    return QDir::homePath();
}

// Returns the private directory for state, history and lock files.
QString dataDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

// Creates the access object for the "gallery" (the Pictures folder).
std::unique_ptr<LocalMediaStore> createMediaStore()
{
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QDir().mkpath(pictures);
    return std::make_unique<DirectoryMediaStore>(pictures);
}

// Creates the development credential store.
std::unique_ptr<CredentialStore> createCredentialStore()
{
    return std::make_unique<SettingsCredentialStore>();
}

// Desktop builds always have access to the Pictures folder.
bool hasStorageAccess()
{
    return true;
}

// Nothing to request on desktop.
void requestStorageAccess()
{
}

// Desktop builds do not show notifications.
bool hasNotificationPermission()
{
    return true;
}

// Nothing to request on desktop.
void requestNotificationPermission()
{
}

// Background synchronisation is not available on desktop.
void scheduleSync(const AppSettings &settings)
{
    qInfo() << "Background sync every" << settings.intervalMinutes
            << "minutes (only scheduled on Android)";
}

// Background synchronisation is not available on desktop.
void cancelScheduledSync()
{
}

// Logs the progress instead of showing a notification.
void showProgress(int done, int total, const QString &file)
{
    qInfo().noquote() << QStringLiteral("[%1/%2] %3").arg(done).arg(total).arg(file);
}

// Logs the result instead of showing a notification.
void showResult(const SyncResult &result)
{
    qInfo().noquote() << (result.success ? QStringLiteral("Sync finished")
                                         : QStringLiteral("Sync failed: ") + result.errorMessage);
}

// Nothing to clear on desktop.
void clearProgress()
{
}

// There is no background job on desktop.
void signalServiceFinished()
{
}

// There is no sync service on desktop: the app synchronises itself.
bool requestManualSync()
{
    return false;
}

// Leaves a stop request (only used by the service, harmless on desktop).
void requestStopSync()
{
}

// No foreground services on desktop.
void enterForeground()
{
}

// No foreground services on desktop.
void leaveForeground()
{
}

// Desktop builds never run as a service.
bool isServiceProcess(int argc, char **argv)
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    return false;
}

} // namespace Platform
} // namespace PicSync
