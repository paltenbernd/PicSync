// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "uihelpers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>

namespace PicSync {
namespace Ui {

// Returns one of the bundled icons (e.g. "sync", "settings_white") as pixmap of the given size.
QPixmap pixmap(const QString &name, int logicalSize)
{
    const qreal ratio = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    QPixmap source(QStringLiteral(":/icons/%1.png").arg(name));
    const int physical = qRound(logicalSize * ratio);
    QPixmap scaled = source.scaled(physical, physical, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(ratio);
    return scaled;
}

// Returns one of the bundled icons as QIcon.
QIcon icon(const QString &name)
{
    return QIcon(QStringLiteral(":/icons/%1.png").arg(name));
}

// Describes a sync interval, e.g. "every hour".
QString intervalText(int minutes)
{
    if (minutes < 60)
        return QCoreApplication::translate("Ui", "every %n minute(s)", nullptr, minutes);
    if (minutes == 60)
        return QCoreApplication::translate("Ui", "every hour");
    const int hours = minutes / 60;
    if (minutes < 1440)
        return QCoreApplication::translate("Ui", "every %n hour(s)", nullptr, hours);
    return QCoreApplication::translate("Ui", "once a day");
}

// Formats a point in time relative to today, e.g. "today, 21:40".
QString timeText(qint64 secondsSinceEpoch)
{
    if (secondsSinceEpoch <= 0)
        return QCoreApplication::translate("Ui", "never");
    const QDateTime when = QDateTime::fromSecsSinceEpoch(secondsSinceEpoch);
    const QLocale locale;
    const QString time = locale.toString(when.time(), QLocale::ShortFormat);
    const qint64 days = when.date().daysTo(QDate::currentDate());
    if (days == 0)
        return QCoreApplication::translate("Ui", "today, %1").arg(time);
    if (days == 1)
        return QCoreApplication::translate("Ui", "yesterday, %1").arg(time);
    return locale.toString(when, QLocale::ShortFormat);
}

// Formats a number of bytes, e.g. "7.6 GB".
QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeSIFormat);
}

// Asks a yes/no question with translated buttons; returns true for "yes".
bool confirm(QWidget &parent, const QString &title, const QString &text)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, &parent);
    const QPointer<QPushButton> yes(box.addButton(QCoreApplication::translate("Ui", "Yes"),
                                                  QMessageBox::YesRole));
    box.addButton(QCoreApplication::translate("Ui", "No"), QMessageBox::NoRole);
    box.exec();
    return box.clickedButton() == yes.data();
}

// Returns the "pretty" name of a gallery folder, e.g. "Camera" for "DCIM/Camera".
QString folderName(const QString &relativeFolder)
{
    if (relativeFolder.isEmpty())
        return QCoreApplication::translate("Ui", "(main folder)");
    return relativeFolder.section(QLatin1Char('/'), -1);
}

} // namespace Ui
} // namespace PicSync
