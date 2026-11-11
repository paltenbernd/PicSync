// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>

class QWidget;

namespace PicSync {

/// Small formatting and resource helpers shared by the pages.
namespace Ui {

// Returns one of the bundled icons (e.g. "sync", "settings_white") as pixmap of the given size.
QPixmap pixmap(const QString &name, int logicalSize);

// Returns one of the bundled icons as QIcon.
QIcon icon(const QString &name);

// Describes a sync interval, e.g. "every hour".
QString intervalText(int minutes);

// Formats a point in time relative to today, e.g. "today, 21:40".
QString timeText(qint64 secondsSinceEpoch);

// Formats a number of bytes, e.g. "7.6 GB".
QString sizeText(qint64 bytes);

// Asks a yes/no question with translated buttons; returns true for "yes".
bool confirm(QWidget &parent, const QString &title, const QString &text);

// Returns the "pretty" name of a gallery folder, e.g. "Camera" for "DCIM/Camera".
QString folderName(const QString &relativeFolder);

} // namespace Ui
} // namespace PicSync
