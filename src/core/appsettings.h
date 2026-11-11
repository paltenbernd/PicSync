// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QList>
#include <QString>

class QSettings;

namespace PicSync {

/// User settings of PicSync, persisted with QSettings in the app's private
/// configuration directory (the password is kept separately, see CredentialStore).
struct AppSettings
{
    QString serverUrl;
    QString user;
    QString remoteFolder;        ///< Relative to the user's files root, e.g. "PicSync/FP6".
    int intervalMinutes = 60;
    bool wifiOnly = true;
    bool chargingOnly = false;
    bool notifications = true;

    // Returns the selectable sync intervals in minutes.
    static QList<int> intervalChoices();

    // Reads the settings; missing values get defaults (folder derived from the device name).
    static AppSettings load(QSettings &settings, const QString &deviceName);

    // Writes the settings.
    void save(QSettings &settings) const;

    // Returns true if server, user and folder are filled in.
    bool isComplete() const;
};

} // namespace PicSync
