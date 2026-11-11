// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QByteArray>
#include <QJniObject>
#include <QString>
#include <QStringList>

namespace PicSync {

/// Small helpers that keep the JNI code of the Android platform layer readable.
namespace Jni {

// Returns the Android context (the activity in the app, the service in the sync process).
QJniObject context();

// Returns the current activity, or an invalid object in the service process.
QJniObject activity();

// Returns the Android package name of PicSync.
QString packageName();

// Returns a system service such as "notification" or "jobscheduler".
QJniObject systemService(const char *name);

// Creates a Java string.
QJniObject string(const QString &value);

// Creates a Java String[] from a list of strings.
QJniObject stringArray(const QStringList &values);

// Creates a Java byte[] from bytes.
QJniObject byteArray(const QByteArray &data);

// Copies a Java byte[] into a QByteArray.
QByteArray toByteArray(const QJniObject &array);

// Returns an Android resource id, e.g. ("ic_stat_picsync", "drawable").
int resourceId(const QString &name, const QString &type);

} // namespace Jni
} // namespace PicSync
