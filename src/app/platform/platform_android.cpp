// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.
//
// Android implementation of the platform layer, written in C++ on top of JNI.
// Only the two framework entry points Android insists on (the JobService and
// the Qt service class) are written in Java, see android/src.

#include "platform.h"
#include "androidcredentialstore.h"
#include "androidmediastore.h"
#include "jniutils.h"
#include "syncprogress.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJniObject>
#include <QSettings>
#include <QStandardPaths>

namespace PicSync {
namespace Platform {

namespace {

constexpr jint kJobId = 4711;
constexpr jint kProgressNotificationId = 1;
constexpr jint kResultNotificationId = 2;
constexpr jint kNetworkAny = 1;          // JobInfo.NETWORK_TYPE_ANY
constexpr jint kNetworkUnmetered = 2;    // JobInfo.NETWORK_TYPE_UNMETERED
constexpr jint kImportanceLow = 2;       // NotificationManager.IMPORTANCE_LOW
constexpr jint kFlagImmutable = 0x04000000;   // PendingIntent.FLAG_IMMUTABLE
constexpr char kChannelId[] = "picsync_sync";
constexpr char kJobServiceClass[] = "de.altenbernd.picsync.PicSyncJobService";
constexpr char kFinishedAction[] = "de.altenbernd.picsync.SYNC_FINISHED";
constexpr char kSyncServiceClass[] = "de.altenbernd.picsync.PicSyncService";
constexpr jint kForegroundTypeDataSync = 1;   // ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC
constexpr jint kStopForegroundRemove = 1;     // Service.STOP_FOREGROUND_REMOVE

// Returns the notification manager, creating the PicSync channel on first use.
QJniObject notificationManager()
{
    QJniObject manager = Jni::systemService("notification");
    if (!manager.isValid())
        return manager;
    const QJniObject channel(
        "android/app/NotificationChannel", "(Ljava/lang/String;Ljava/lang/CharSequence;I)V",
        Jni::string(QString::fromLatin1(kChannelId)).object<jstring>(),
        Jni::string(QCoreApplication::translate("Platform", "Synchronisation"))
            .object<jstring>(),
        kImportanceLow);
    manager.callMethod<void>("createNotificationChannel",
                             "(Landroid/app/NotificationChannel;)V", channel.object());
    return manager;
}

// Creates a notification builder with icon, title and tap action already set.
QJniObject notificationBuilder(const QString &title, const QString &text)
{
    const QJniObject context = Jni::context();
    QJniObject builder("android/app/Notification$Builder",
                       "(Landroid/content/Context;Ljava/lang/String;)V", context.object(),
                       Jni::string(QString::fromLatin1(kChannelId)).object<jstring>());
    int icon = Jni::resourceId(QStringLiteral("ic_stat_picsync"), QStringLiteral("drawable"));
    if (icon == 0) {
        const QJniObject info = context.callObjectMethod(
            "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
        icon = info.getField<jint>("icon");
    }
    constexpr char self[] = "Landroid/app/Notification$Builder;";
    builder.callObjectMethod("setSmallIcon", QByteArray("(I)").append(self).constData(),
                             jint(icon));
    builder.callObjectMethod("setContentTitle",
                             QByteArray("(Ljava/lang/CharSequence;)").append(self).constData(),
                             Jni::string(title).object<jstring>());
    builder.callObjectMethod("setContentText",
                             QByteArray("(Ljava/lang/CharSequence;)").append(self).constData(),
                             Jni::string(text).object<jstring>());
    builder.callObjectMethod("setOnlyAlertOnce", QByteArray("(Z)").append(self).constData(),
                             jboolean(true));

    const QJniObject packageManager = context.callObjectMethod(
        "getPackageManager", "()Landroid/content/pm/PackageManager;");
    const QJniObject launch = packageManager.callObjectMethod(
        "getLaunchIntentForPackage", "(Ljava/lang/String;)Landroid/content/Intent;",
        Jni::string(Jni::packageName()).object<jstring>());
    if (launch.isValid()) {
        const QJniObject pending = QJniObject::callStaticObjectMethod(
            "android/app/PendingIntent", "getActivity",
            "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;",
            context.object(), jint(0), launch.object(), kFlagImmutable);
        builder.callObjectMethod("setContentIntent",
                                 QByteArray("(Landroid/app/PendingIntent;)").append(self).constData(),
                                 pending.object());
    }
    return builder;
}

// Posts a notification built from the given builder.
void post(jint id, const QJniObject &builder)
{
    QJniObject manager = notificationManager();
    const QJniObject notification = builder.callObjectMethod("build",
                                                             "()Landroid/app/Notification;");
    if (manager.isValid() && notification.isValid())
        manager.callMethod<void>("notify", "(ILandroid/app/Notification;)V", id,
                                 notification.object());
}

// Returns true if the user wants notifications and Android allows them.
bool notificationsWanted()
{
    QSettings settings;
    return settings.value(QStringLiteral("sync/notifications"), true).toBool()
            && hasNotificationPermission();
}

} // namespace

// Returns the user-visible name of the phone (Settings > About phone > Device name).
QString deviceName()
{
    const QJniObject resolver = Jni::context().callObjectMethod(
        "getContentResolver", "()Landroid/content/ContentResolver;");
    QString name = QJniObject::callStaticObjectMethod(
                       "android/provider/Settings$Global", "getString",
                       "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
                       resolver.object(),
                       Jni::string(QStringLiteral("device_name")).object<jstring>())
                       .toString();
    if (name.trimmed().isEmpty())
        name = QJniObject::getStaticObjectField("android/os/Build", "MODEL",
                                                "Ljava/lang/String;").toString();
    return name.trimmed();
}

// Returns the private directory for state, history and lock files.
QString dataDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

// Creates the access object for the gallery media.
std::unique_ptr<LocalMediaStore> createMediaStore()
{
    return std::make_unique<AndroidMediaStore>();
}

// Creates the protected storage for the Nextcloud app password.
std::unique_ptr<CredentialStore> createCredentialStore()
{
    return std::make_unique<AndroidCredentialStore>();
}

// Returns true if PicSync may read, write and delete all gallery files.
bool hasStorageAccess()
{
    return QJniObject::callStaticMethod<jboolean>("android/os/Environment",
                                                  "isExternalStorageManager", "()Z");
}

// Opens the system page on which the user grants "All files access".
void requestStorageAccess()
{
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        Jni::string(QStringLiteral("package:") + Jni::packageName()).object<jstring>());
    const QJniObject intent(
        "android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
        Jni::string(QStringLiteral("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION"))
            .object<jstring>(),
        uri.object());
    QJniObject activity = Jni::activity();
    if (activity.isValid())
        activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V",
                                  intent.object());
}

// Returns true if PicSync may show notifications.
bool hasNotificationPermission()
{
    const jint granted = Jni::context().callMethod<jint>(
        "checkSelfPermission", "(Ljava/lang/String;)I",
        Jni::string(QStringLiteral("android.permission.POST_NOTIFICATIONS")).object<jstring>());
    return granted == 0;   // PackageManager.PERMISSION_GRANTED
}

// Asks the user for permission to show notifications. Android shows its
// dialog at most twice; after that PicSync opens the app's notification settings.
void requestNotificationPermission()
{
    QJniObject activity = Jni::activity();
    if (!activity.isValid())
        return;
    QSettings settings;
    const QString askedKey = QStringLiteral("permissions/notificationRequests");
    const int asked = settings.value(askedKey, 0).toInt();
    if (asked < 2) {
        settings.setValue(askedKey, asked + 1);
        const QJniObject permissions =
            Jni::stringArray({ QStringLiteral("android.permission.POST_NOTIFICATIONS") });
        activity.callMethod<void>("requestPermissions", "([Ljava/lang/String;I)V",
                                  permissions.object(), jint(42));
        return;
    }
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      Jni::string(QStringLiteral("android.settings.APP_NOTIFICATION_SETTINGS"))
                          .object<jstring>());
    intent.callObjectMethod("putExtra",
                            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                            Jni::string(QStringLiteral("android.provider.extra.APP_PACKAGE"))
                                .object<jstring>(),
                            Jni::string(Jni::packageName()).object<jstring>());
    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object());
}

// Registers (or replaces) the periodic background synchronisation.
void scheduleSync(const AppSettings &settings)
{
    QJniObject scheduler = Jni::systemService("jobscheduler");
    if (!scheduler.isValid())
        return;
    const QJniObject component(
        "android/content/ComponentName", "(Landroid/content/Context;Ljava/lang/String;)V",
        Jni::context().object(),
        Jni::string(QString::fromLatin1(kJobServiceClass)).object<jstring>());
    QJniObject builder("android/app/job/JobInfo$Builder", "(ILandroid/content/ComponentName;)V",
                       kJobId, component.object());
    constexpr char self[] = "Landroid/app/job/JobInfo$Builder;";
    builder.callObjectMethod("setPeriodic", QByteArray("(J)").append(self).constData(),
                             jlong(settings.intervalMinutes) * 60 * 1000);
    builder.callObjectMethod("setRequiredNetworkType", QByteArray("(I)").append(self).constData(),
                             settings.wifiOnly ? kNetworkUnmetered : kNetworkAny);
    builder.callObjectMethod("setRequiresCharging", QByteArray("(Z)").append(self).constData(),
                             jboolean(settings.chargingOnly));
    builder.callObjectMethod("setPersisted", QByteArray("(Z)").append(self).constData(),
                             jboolean(true));
    const QJniObject job = builder.callObjectMethod("build", "()Landroid/app/job/JobInfo;");
    if (job.isValid())
        scheduler.callMethod<jint>("schedule", "(Landroid/app/job/JobInfo;)I", job.object());
}

// Removes the periodic background synchronisation.
void cancelScheduledSync()
{
    QJniObject scheduler = Jni::systemService("jobscheduler");
    if (scheduler.isValid())
        scheduler.callMethod<void>("cancel", "(I)V", kJobId);
}

// Shows or updates the progress notification of a running sync.
void showProgress(int done, int total, const QString &file)
{
    if (!notificationsWanted())
        return;
    const QString title = QCoreApplication::translate("Platform", "Synchronising %1 of %2")
                              .arg(qMin(done + 1, total)).arg(total);
    QJniObject builder = notificationBuilder(title, file.section(QLatin1Char('/'), -1));
    builder.callObjectMethod("setProgress", "(IIZ)Landroid/app/Notification$Builder;",
                             jint(total), jint(done), jboolean(total == 0));
    builder.callObjectMethod("setOngoing", "(Z)Landroid/app/Notification$Builder;",
                             jboolean(true));
    post(kProgressNotificationId, builder);
}

// Shows the result of a finished sync (errors always, successes only if files changed).
void showResult(const SyncResult &result)
{
    clearProgress();
    if (!notificationsWanted())
        return;
    const int changes = result.uploaded + result.downloaded + result.deletedLocal
            + result.deletedRemote + result.conflicts;
    if (result.success && changes == 0)
        return;
    QString title;
    QString text;
    if (result.success) {
        title = QCoreApplication::translate("Platform", "Synchronisation finished");
        text = QCoreApplication::translate("Platform", "%1 up, %2 down, %3 deleted, %4 conflicts")
                   .arg(result.uploaded).arg(result.downloaded)
                   .arg(result.deletedLocal + result.deletedRemote).arg(result.conflicts);
    } else {
        title = QCoreApplication::translate("Platform", "Synchronisation failed");
        text = result.errorMessage;
    }
    QJniObject builder = notificationBuilder(title, text);
    builder.callObjectMethod("setAutoCancel", "(Z)Landroid/app/Notification$Builder;",
                             jboolean(true));
    post(kResultNotificationId, builder);
}

// Removes the progress notification.
void clearProgress()
{
    QJniObject manager = Jni::systemService("notification");
    if (manager.isValid())
        manager.callMethod<void>("cancel", "(I)V", kProgressNotificationId);
}

// Tells the background job that the sync service has finished its work.
void signalServiceFinished()
{
    QJniObject intent("android/content/Intent", "(Ljava/lang/String;)V",
                      Jni::string(QString::fromLatin1(kFinishedAction)).object<jstring>());
    intent.callObjectMethod("setPackage", "(Ljava/lang/String;)Landroid/content/Intent;",
                            Jni::string(Jni::packageName()).object<jstring>());
    Jni::context().callMethod<void>("sendBroadcast", "(Landroid/content/Intent;)V",
                                    intent.object());
}

// Asks the sync service to run a manual synchronisation in the background.
bool requestManualSync()
{
    if (!SyncProgress::touch(SyncProgress::manualRequestPath()))
        return false;
    QJniObject intent("android/content/Intent", "()V");
    intent.callObjectMethod("setClassName",
                            "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                            Jni::string(Jni::packageName()).object<jstring>(),
                            Jni::string(QString::fromLatin1(kSyncServiceClass)).object<jstring>());
    const QJniObject started = Jni::context().callObjectMethod(
        "startForegroundService", "(Landroid/content/Intent;)Landroid/content/ComponentName;",
        intent.object());
    if (!started.isValid()) {
        qWarning("PicSync: startForegroundService failed, syncing in the app");
        SyncProgress::take(SyncProgress::manualRequestPath());
        return false;
    }
    qInfo("PicSync: sync service requested");
    return true;
}

// Asks the running synchronisation (in the app or in the service) to stop.
void requestStopSync()
{
    SyncProgress::touch(SyncProgress::stopRequestPath());
}

// Turns the running sync service into a foreground service (manual runs).
void enterForeground()
{
    notificationManager();   // makes sure the channel exists
    QJniObject builder = notificationBuilder(
        QCoreApplication::translate("Platform", "Synchronising"),
        QCoreApplication::translate("Platform", "PicSync is synchronising your gallery."));
    builder.callObjectMethod("setOngoing", "(Z)Landroid/app/Notification$Builder;",
                             jboolean(true));
    const QJniObject notification = builder.callObjectMethod("build",
                                                             "()Landroid/app/Notification;");
    // Jni::context() is the Service object in the ":sync" process.
    Jni::context().callMethod<void>("startForeground", "(ILandroid/app/Notification;I)V",
                                    kProgressNotificationId, notification.object(),
                                    kForegroundTypeDataSync);
    qInfo("PicSync: sync service in the foreground (notification %s)",
          notification.isValid() ? "ok" : "invalid");
}

// Ends the foreground state and stops the manually started sync service.
void leaveForeground()
{
    QJniObject service = Jni::context();
    service.callMethod<void>("stopForeground", "(I)V", kStopForegroundRemove);
    service.callMethod<void>("stopSelf", "()V");
}

// Returns true if this process was started as the background sync service.
bool isServiceProcess(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "-service") == 0)
            return true;
    }
    return false;
}

} // namespace Platform
} // namespace PicSync
