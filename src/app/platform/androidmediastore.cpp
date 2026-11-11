// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "androidmediastore.h"
#include "jniutils.h"
#include "pathutils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace PicSync {

namespace {

// MediaStore.Files.FileColumns.MEDIA_TYPE_IMAGE / MEDIA_TYPE_VIDEO.
constexpr char kMediaSelection[] = "media_type IN (1,3)";

// Returns the content URI of the files on the internal shared storage ("external_primary").
QJniObject filesUri()
{
    return QJniObject::callStaticObjectMethod("android/provider/MediaStore$Files",
                                              "getContentUri",
                                              "(Ljava/lang/String;)Landroid/net/Uri;",
                                              Jni::string(QStringLiteral("external_primary"))
                                                  .object<jstring>());
}

// Returns the ContentResolver of the app.
QJniObject contentResolver()
{
    return Jni::context().callObjectMethod("getContentResolver",
                                           "()Landroid/content/ContentResolver;");
}

// Runs a MediaStore query and returns the cursor (invalid on failure).
QJniObject query(const QStringList &columns, const QString &selection,
                 const QStringList &arguments)
{
    const QJniObject projection = Jni::stringArray(columns);
    const QJniObject where = Jni::string(selection);
    const QJniObject args = arguments.isEmpty() ? QJniObject() : Jni::stringArray(arguments);
    return contentResolver().callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;"
        "Ljava/lang/String;)Landroid/database/Cursor;",
        filesUri().object(), projection.object(), where.object<jstring>(),
        args.isValid() ? args.object() : static_cast<jobject>(nullptr),
        static_cast<jobject>(nullptr));
}

// Returns the column index of a cursor column.
jint column(const QJniObject &cursor, const char *name)
{
    return cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I",
                                   Jni::string(QString::fromLatin1(name)).object<jstring>());
}

} // namespace

// Creates the store and determines the shared storage root.
AndroidMediaStore::AndroidMediaStore()
{
    const QJniObject dir = QJniObject::callStaticObjectMethod(
        "android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;");
    m_root = dir.isValid() ? dir.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;")
                                 .toString()
                           : QStringLiteral("/storage/emulated/0");
}

// Lists all images and videos known to the MediaStore (trashed files excluded).
bool AndroidMediaStore::scan(LocalFileMap &files, QString &error)
{
    files.clear();
    QJniObject cursor = query({ QStringLiteral("relative_path"),
                                QStringLiteral("_display_name") },
                              QString::fromLatin1(kMediaSelection), QStringList());
    if (!cursor.isValid()) {
        error = QCoreApplication::translate("AndroidMediaStore",
                                            "The gallery could not be read. "
                                            "Check the storage permission.");
        return false;
    }
    const jint pathColumn = column(cursor, "relative_path");
    const jint nameColumn = column(cursor, "_display_name");
    while (cursor.callMethod<jboolean>("moveToNext", "()Z")) {
        const QString folder = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;",
                                                       pathColumn).toString();
        const QString name = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;",
                                                     nameColumn).toString();
        const QString rel = PathUtils::join(folder, name);
        if (name.isEmpty() || PathUtils::isIgnored(rel) || !PathUtils::isMediaFile(rel))
            continue;
        LocalFile file;
        if (stat(rel, file))
            files.insert(file.relativePath, file);
    }
    cursor.callMethod<void>("close", "()V");
    return true;
}

// Returns the absolute path below the shared storage root.
QString AndroidMediaStore::absolutePath(const QString &relativePath) const
{
    return m_root + QLatin1Char('/') + PathUtils::normalizeRelative(relativePath);
}

// Creates the parent folders of a file that is about to be downloaded.
bool AndroidMediaStore::prepareWrite(const QString &relativePath, QString &error)
{
    const QString dir = QFileInfo(absolutePath(relativePath)).absolutePath();
    if (QDir().mkpath(dir))
        return true;
    error = QCoreApplication::translate("AndroidMediaStore", "Cannot create folder %1").arg(dir);
    return false;
}

// Asks the media scanner to (re-)index the given absolute paths.
void AndroidMediaStore::scanPaths(const QStringList &absolutePaths)
{
    const QJniObject paths = Jni::stringArray(absolutePaths);
    QJniObject::callStaticMethod<void>(
        "android/media/MediaScannerConnection", "scanFile",
        "(Landroid/content/Context;[Ljava/lang/String;[Ljava/lang/String;"
        "Landroid/media/MediaScannerConnection$OnScanCompletedListener;)V",
        Jni::context().object(), paths.object(), static_cast<jobject>(nullptr),
        static_cast<jobject>(nullptr));
}

// Asks the media scanner to index a new or changed file.
void AndroidMediaStore::fileWritten(const QString &relativePath)
{
    scanPaths({ absolutePath(relativePath) });
}

// Tries to move a file into the MediaStore trash; returns true on success.
bool AndroidMediaStore::moveToTrash(const QString &relativePath) const
{
    const QString folder = PathUtils::parentPath(relativePath) + QLatin1Char('/');
    const QString name = PathUtils::normalizeRelative(relativePath).section(QLatin1Char('/'), -1);
    QJniObject cursor = query({ QStringLiteral("_id") },
                              QStringLiteral("relative_path=? AND _display_name=?"),
                              { folder, name });
    if (!cursor.isValid())
        return false;
    jlong id = -1;
    if (cursor.callMethod<jboolean>("moveToFirst", "()Z"))
        id = cursor.callMethod<jlong>("getLong", "(I)J", jint(0));
    cursor.callMethod<void>("close", "()V");
    if (id < 0)
        return false;

    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/content/ContentUris", "withAppendedId",
        "(Landroid/net/Uri;J)Landroid/net/Uri;", filesUri().object(), id);
    QJniObject values("android/content/ContentValues", "()V");
    const QJniObject one = QJniObject::callStaticObjectMethod(
        "java/lang/Integer", "valueOf", "(I)Ljava/lang/Integer;", jint(1));
    values.callMethod<void>("put", "(Ljava/lang/String;Ljava/lang/Integer;)V",
                            Jni::string(QStringLiteral("is_trashed")).object<jstring>(),
                            one.object());
    const jint rows = contentResolver().callMethod<jint>(
        "update",
        "(Landroid/net/Uri;Landroid/content/ContentValues;Ljava/lang/String;"
        "[Ljava/lang/String;)I",
        uri.object(), values.object(), static_cast<jobject>(nullptr),
        static_cast<jobject>(nullptr));
    return rows > 0;
}

// Moves a file to the gallery's trash (or deletes it if that is not possible).
bool AndroidMediaStore::remove(const QString &relativePath, QString &error)
{
    const QString path = absolutePath(relativePath);
    if (!QFileInfo::exists(path))
        return true;
    if (moveToTrash(relativePath) && !QFileInfo::exists(path))
        return true;
    QFile file(path);
    if (!file.remove()) {
        error = file.errorString();
        return false;
    }
    scanPaths({ path });
    return true;
}

// Renames a file and updates the media index.
bool AndroidMediaStore::rename(const QString &fromRelative, const QString &toRelative,
                               QString &error)
{
    if (!prepareWrite(toRelative, error))
        return false;
    QFile file(absolutePath(fromRelative));
    if (!file.rename(absolutePath(toRelative))) {
        error = file.errorString();
        return false;
    }
    scanPaths({ absolutePath(fromRelative), absolutePath(toRelative) });
    return true;
}

// Reads size and modification time of one file.
bool AndroidMediaStore::stat(const QString &relativePath, LocalFile &file) const
{
    const QFileInfo info(absolutePath(relativePath));
    if (!info.exists() || !info.isFile())
        return false;
    file.relativePath = PathUtils::normalizeRelative(relativePath);
    file.size = info.size();
    file.mtime = info.lastModified().toSecsSinceEpoch();
    return true;
}

} // namespace PicSync
