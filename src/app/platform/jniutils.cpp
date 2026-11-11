// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "jniutils.h"

#include <QCoreApplication>
#include <QJniEnvironment>

namespace PicSync {
namespace Jni {

// Returns the Android context (the activity in the app, the service in the sync process).
QJniObject context()
{
    return QNativeInterface::QAndroidApplication::context();
}

// Returns the current activity, or an invalid object in the service process.
QJniObject activity()
{
    return QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative",
                                              "activity", "()Landroid/app/Activity;");
}

// Returns the Android package name of PicSync.
QString packageName()
{
    return context().callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
}

// Returns a system service such as "notification" or "jobscheduler".
QJniObject systemService(const char *name)
{
    return context().callObjectMethod("getSystemService",
                                      "(Ljava/lang/String;)Ljava/lang/Object;",
                                      string(QString::fromLatin1(name)).object<jstring>());
}

// Creates a Java string.
QJniObject string(const QString &value)
{
    return QJniObject::fromString(value);
}

// Creates a Java String[] from a list of strings.
QJniObject stringArray(const QStringList &values)
{
    QJniEnvironment env;
    jclass stringClass = env.findClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(jsize(values.size()), stringClass, nullptr);
    for (int i = 0; i < values.size(); ++i) {
        const QJniObject item = string(values.at(i));
        env->SetObjectArrayElement(array, jsize(i), item.object());
    }
    return QJniObject::fromLocalRef(array);
}

// Creates a Java byte[] from bytes.
QJniObject byteArray(const QByteArray &data)
{
    QJniEnvironment env;
    jbyteArray array = env->NewByteArray(jsize(data.size()));
    env->SetByteArrayRegion(array, 0, jsize(data.size()),
                            reinterpret_cast<const jbyte *>(data.constData()));
    return QJniObject::fromLocalRef(array);
}

// Copies a Java byte[] into a QByteArray.
QByteArray toByteArray(const QJniObject &array)
{
    if (!array.isValid())
        return QByteArray();
    QJniEnvironment env;
    jbyteArray raw = array.object<jbyteArray>();
    const jsize length = env->GetArrayLength(raw);
    QByteArray result(length, Qt::Uninitialized);
    env->GetByteArrayRegion(raw, 0, length, reinterpret_cast<jbyte *>(result.data()));
    return result;
}

// Returns an Android resource id, e.g. ("ic_stat_picsync", "drawable").
int resourceId(const QString &name, const QString &type)
{
    const QJniObject resources = context().callObjectMethod("getResources",
                                                            "()Landroid/content/res/Resources;");
    return resources.callMethod<jint>("getIdentifier",
                                      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
                                      string(name).object<jstring>(),
                                      string(type).object<jstring>(),
                                      string(packageName()).object<jstring>());
}

} // namespace Jni
} // namespace PicSync
