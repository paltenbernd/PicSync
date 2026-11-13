# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
#
# PicSync application (Qt Widgets). On Android it also contains the
# headless background sync service (same library, started with "-service").

include(../../common.pri)
include(../core/core.pri)

TEMPLATE = app
TARGET = PicSync
QT += core gui widgets network

INCLUDEPATH += $$PWD

HEADERS += \
    dashboardpage.h \
    historypage.h \
    mainwindow.h \
    platform/platform.h \
    settingspage.h \
    synccontroller.h \
    syncsession.h \
    uihelpers.h

SOURCES += \
    dashboardpage.cpp \
    historypage.cpp \
    main.cpp \
    mainwindow.cpp \
    settingspage.cpp \
    synccontroller.cpp \
    syncsession.cpp \
    uihelpers.cpp

RESOURCES += picsync.qrc

# Translations: compiled by lrelease and embedded under :/i18n.
TRANSLATIONS += \
    translations/picsync_de.ts \
    translations/picsync_en.ts
CONFIG += lrelease embed_translations
QM_FILES_RESOURCE_PREFIX = /i18n

android {
    QT += core-private
    HEADERS += \
        platform/androidcredentialstore.h \
        platform/androidmediastore.h \
        platform/jniutils.h \
        servicerunner.h
    SOURCES += \
        platform/androidcredentialstore.cpp \
        platform/androidmediastore.cpp \
        platform/jniutils.cpp \
        platform/platform_android.cpp \
        servicerunner.cpp

    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    # Qt does not ship OpenSSL for Android, but HTTPS needs it. The libraries
    # come from https://github.com/KDAB/android_openssl (path in ANDROID_OPENSSL_DIR).
    OPENSSL_DIR = $$(ANDROID_OPENSSL_DIR)
    isEmpty(OPENSSL_DIR): OPENSSL_DIR = $$PWD/../../3rdparty/android_openssl
    exists($$OPENSSL_DIR/ssl_3/arm64-v8a/libssl_3.so) {
        ANDROID_EXTRA_LIBS += \
            $$OPENSSL_DIR/ssl_3/arm64-v8a/libcrypto_3.so \
            $$OPENSSL_DIR/ssl_3/arm64-v8a/libssl_3.so
    } else {
        warning("OpenSSL for Android not found - HTTPS will not work. Set ANDROID_OPENSSL_DIR.")
    }
    ANDROID_VERSION_NAME = $$PICSYNC_VERSION
    isEmpty(ANDROID_VERSION_CODE): ANDROID_VERSION_CODE = $$PICSYNC_VERSION_CODE
    DEFINES += PICSYNC_BUILD=\\\"$$ANDROID_VERSION_CODE\\\"
    ANDROID_MIN_SDK_VERSION = 33
    ANDROID_TARGET_SDK_VERSION = 34

    DISTFILES += \
        android/AndroidManifest.xml \
        android/res/drawable/ic_launcher_foreground.xml \
        android/res/drawable/ic_stat_picsync.xml \
        android/res/mipmap-anydpi-v26/ic_launcher.xml \
        android/res/values/ic_launcher_background.xml \
        android/src/de/altenbernd/picsync/PicSyncJobService.java \
        android/src/de/altenbernd/picsync/PicSyncService.java
} else {
    SOURCES += platform/platform_desktop.cpp
    DEFINES += PICSYNC_BUILD=\\\"dev\\\"
}
