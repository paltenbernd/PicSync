// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include "appsettings.h"
#include "interfaces.h"
#include "syncmodel.h"

#include <QString>

#include <memory>

namespace PicSync {

/// Everything that differs between Android and desktop builds.
///
/// The Android implementation (platform_android.cpp) talks to the Android
/// framework through JNI; the desktop implementation (platform_desktop.cpp)
/// provides simple stand-ins so the app can be developed and tested on a PC.
namespace Platform {

// Returns the user-visible name of the phone (Settings > About phone > Device name).
QString deviceName();

// Returns the private directory for state, history and lock files.
QString dataDirectory();

// Creates the access object for the gallery media.
std::unique_ptr<LocalMediaStore> createMediaStore();

// Creates the protected storage for the Nextcloud app password.
std::unique_ptr<CredentialStore> createCredentialStore();

// Returns true if PicSync may read, write and delete all gallery files.
bool hasStorageAccess();

// Opens the system page on which the user grants "All files access".
void requestStorageAccess();

// Returns true if PicSync may show notifications.
bool hasNotificationPermission();

// Asks the user for permission to show notifications.
void requestNotificationPermission();

// Registers (or replaces) the periodic background synchronisation.
void scheduleSync(const AppSettings &settings);

// Removes the periodic background synchronisation.
void cancelScheduledSync();

// Shows or updates the progress notification of a running sync.
void showProgress(int done, int total, const QString &file);

// Shows the result of a finished sync (errors always, successes only if files changed).
void showResult(const SyncResult &result);

// Removes the progress notification.
void clearProgress();

// Tells the background job that the sync service has finished its work.
void signalServiceFinished();

// Returns true if this process was started as the background sync service.
bool isServiceProcess(int argc, char **argv);

} // namespace Platform
} // namespace PicSync
