// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "appsettings.h"
#include "pathutils.h"

#include <QSettings>

namespace PicSync {

// Returns the selectable sync intervals in minutes.
QList<int> AppSettings::intervalChoices()
{
    return { 15, 60, 360, 720, 1440 };
}

// Reads the settings; missing values get defaults (folder derived from the device name).
AppSettings AppSettings::load(QSettings &settings, const QString &deviceName)
{
    AppSettings s;
    s.serverUrl = settings.value(QStringLiteral("server/url")).toString();
    s.user = settings.value(QStringLiteral("server/user")).toString();
    s.remoteFolder = settings.value(QStringLiteral("server/folder"),
                                    PathUtils::defaultRemoteFolder(deviceName)).toString();
    s.intervalMinutes = settings.value(QStringLiteral("sync/interval"), 60).toInt();
    if (!intervalChoices().contains(s.intervalMinutes))
        s.intervalMinutes = 60;
    s.wifiOnly = settings.value(QStringLiteral("sync/wifiOnly"), true).toBool();
    s.chargingOnly = settings.value(QStringLiteral("sync/chargingOnly"), false).toBool();
    s.notifications = settings.value(QStringLiteral("sync/notifications"), true).toBool();
    return s;
}

// Writes the settings.
void AppSettings::save(QSettings &settings) const
{
    settings.setValue(QStringLiteral("server/url"), PathUtils::normalizeServerUrl(serverUrl));
    settings.setValue(QStringLiteral("server/user"), user.trimmed());
    settings.setValue(QStringLiteral("server/folder"), PathUtils::normalizeRelative(remoteFolder));
    settings.setValue(QStringLiteral("sync/interval"), intervalMinutes);
    settings.setValue(QStringLiteral("sync/wifiOnly"), wifiOnly);
    settings.setValue(QStringLiteral("sync/chargingOnly"), chargingOnly);
    settings.setValue(QStringLiteral("sync/notifications"), notifications);
    settings.sync();
}

// Returns true if server, user and folder are filled in.
bool AppSettings::isComplete() const
{
    return !serverUrl.trimmed().isEmpty() && !user.trimmed().isEmpty()
            && !PathUtils::normalizeRelative(remoteFolder).isEmpty();
}

} // namespace PicSync
