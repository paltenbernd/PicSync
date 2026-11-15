// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.
//
// PicSync – two-way synchronisation of the /e/OS gallery with Nextcloud.
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version. See the file LICENSE for details.

#include "appsettings.h"
#include "mainwindow.h"
#include "platform/platform.h"

#include <QApplication>
#include <QFile>
#include <QLocale>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QTranslator>

#ifdef Q_OS_ANDROID
#include "servicerunner.h"
#include <QTimer>
#include <QtCore/private/qandroidextras_p.h>
#endif

namespace {

// Sets the names under which settings and data are stored.
void setIdentity()
{
    QCoreApplication::setOrganizationName(QStringLiteral("Altenbernd"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("altenbernd.de"));
    QCoreApplication::setApplicationName(QStringLiteral("PicSync"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PICSYNC_VERSION));
}

// Loads the translation that matches the phone's language (English otherwise).
void installTranslation(QCoreApplication &app, QTranslator &translator)
{
    if (translator.load(QLocale(), QStringLiteral("picsync"), QStringLiteral("_"),
                        QStringLiteral(":/i18n")))
        app.installTranslator(&translator);
}

// Loads the application style sheet from the resources.
QString styleSheet()
{
    QFile file(QStringLiteral(":/style.qss"));
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(file.readAll());
}

#ifdef PICSYNC_TEST_HOOKS
// Test builds only: "--test-config=<server>|<user>|<password>|<folder>" stores a
// complete configuration, so the emulator check can run without typing.
void applyTestConfiguration()
{
    const QStringList arguments = QCoreApplication::arguments();
    for (const QString &argument : arguments) {
        if (!argument.startsWith(QLatin1String("--test-config=")))
            continue;
        const QStringList parts = argument.mid(14).split(QLatin1Char('|'));
        if (parts.size() != 4)
            continue;
        QSettings qsettings;
        PicSync::AppSettings settings = PicSync::AppSettings::load(qsettings, QString());
        settings.serverUrl = parts.at(0);
        settings.user = parts.at(1);
        settings.remoteFolder = parts.at(3);
        settings.save(qsettings);
        PicSync::Platform::createCredentialStore()->save(parts.at(1), parts.at(2));
    }
}
#endif

#ifdef Q_OS_ANDROID
// Runs the headless background synchronisation in the ":sync" service process.
int runService(int argc, char **argv)
{
    QAndroidService app(argc, argv);
    QTranslator translator;
    installTranslation(app, translator);
    PicSync::ServiceRunner runner;
    QTimer::singleShot(0, &runner, &PicSync::ServiceRunner::start);
    return app.exec();
}
#endif

} // namespace

// Starts either the user interface or (on Android) the background sync service.
int main(int argc, char *argv[])
{
    setIdentity();
#ifdef Q_OS_ANDROID
    if (PicSync::Platform::isServiceProcess(argc, argv))
        return runService(argc, argv);
#endif

#ifdef Q_OS_ANDROID
    // Repaint scrolled areas instead of shifting pixels in the backing store;
    // on Android the shifted pixels occasionally left stale content behind.
    qputenv("QT_NO_FAST_SCROLL", "1");
    qputenv("QT_NO_FAST_MOVE", "1");
#endif
    QApplication app(argc, argv);
    QTranslator translator;
    installTranslation(app, translator);
    app.setStyle(QStringLiteral("Fusion"));
    app.setStyleSheet(styleSheet());

#ifdef PICSYNC_TEST_HOOKS
    applyTestConfiguration();
#endif
    PicSync::MainWindow window;
#ifdef PICSYNC_TEST_HOOKS
    if (QCoreApplication::arguments().contains(QStringLiteral("--test-sync")))
        QTimer::singleShot(4000, &window, &PicSync::MainWindow::startSync);
#endif
#ifdef Q_OS_ANDROID
    // Never wider than the screen, whatever a label asks for.
    if (window.screen())
        window.setMaximumWidth(window.screen()->availableGeometry().width());
    window.showMaximized();
#else
    window.resize(420, 820);
    window.show();
#endif
    return app.exec();
}
