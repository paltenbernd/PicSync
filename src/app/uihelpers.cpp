// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "uihelpers.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <memory>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QAbstractScrollArea>

namespace PicSync {
namespace Ui {

namespace {

/// Modal message shown inside the main window instead of in a window of its
/// own: separate Android windows of Qt Widgets could stay on screen without
/// reacting to taps (e.g. after the app had been in the background).
class OverlayDialog : public QWidget
{
public:
    // Creates the dimmed overlay covering the whole window of the parent.
    explicit OverlayDialog(QWidget &parent) : QWidget(parent.window())
    {
        setObjectName(QStringLiteral("dialogOverlay"));
        setAttribute(Qt::WA_StyledBackground);
        setFocusPolicy(Qt::StrongFocus);
        parentWidget()->installEventFilter(this);
        setGeometry(parentWidget()->rect());
    }

    // Shows the overlay and waits until a button (or back) was pressed.
    bool exec()
    {
        show();
        raise();
        setFocus();
        m_loop.exec();
        hide();
        return m_accepted;
    }

    // Ends the dialog with the given result.
    void finish(bool accepted)
    {
        m_accepted = accepted;
        m_loop.quit();
    }

    // Ends the dialog with "yes"/"OK".
    void accept() { finish(true); }

    // Ends the dialog with "no" (also the back key).
    void reject() { finish(false); }

protected:
    // Keeps the overlay as large as the window.
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return QWidget::eventFilter(watched, event);
    }

    // Closes the dialog with the Android back key.
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
            event->accept();
            reject();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    // Taps beside the message do nothing (but never reach the page below).
    void mousePressEvent(QMouseEvent *event) override { event->accept(); }

private:
    QEventLoop m_loop;
    bool m_accepted = false;
};

} // namespace

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

// Enables finger scrolling without delaying taps and without stale paint.
void enableTouchScrolling(QAbstractScrollArea &area)
{
    const QPointer<QWidget> viewport(area.viewport());   // owned by the scroll area
    QScroller::grabGesture(viewport.data(), QScroller::LeftMouseButtonGesture);
    const QPointer<QScroller> scroller(QScroller::scroller(viewport.data()));   // owned by Qt
    QScrollerProperties properties = scroller->scrollerProperties();
    // Deliver taps immediately, so text fields get the cursor where the finger is.
    properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.0);
    properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.004);
    const QVariant off = QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, off);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, off);
    scroller->setScrollerProperties(properties);
    // Repaint the whole viewport on every scroll step; Android otherwise
    // occasionally keeps old pixels (e.g. checkboxes drawn over text fields).
    QObject::connect(area.verticalScrollBar(), &QScrollBar::valueChanged, viewport.data(),
                     qOverload<>(&QWidget::update));
}

// Lets a text label shrink below the width of its longest word, so long file
// names or paths never widen the window beyond the screen.
void allowShrink(QLabel &label)
{
    QSizePolicy policy = label.sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Ignored);
    label.setSizePolicy(policy);
    label.setMinimumWidth(0);
}

namespace {

// Shows a dialog with title, (scrollable) text and one or two buttons; returns
// true if the primary button was pressed. Uses no QMessageBox, because extra
// windows of Qt Widgets need the full-flush workaround on Android.
bool runDialog(QWidget &parent, const QString &title, const QString &text,
               const QString &primaryText, const QString &secondaryText)
{
    OverlayDialog overlay(parent);
    // The white message card in the middle of the dimmed overlay.
    auto outer = std::make_unique<QVBoxLayout>();
    auto dialog = std::make_unique<QFrame>();
    dialog->setObjectName(QStringLiteral("infoDialog"));
    // Declared parents-first, so children are destroyed before their parents.
    auto layout = std::make_unique<QVBoxLayout>();
    auto heading = std::make_unique<QLabel>(title);
    auto scroll = std::make_unique<QScrollArea>();
    auto body = std::make_unique<QLabel>(text);
    auto buttons = std::make_unique<QHBoxLayout>();
    auto secondary = std::make_unique<QPushButton>(secondaryText);
    auto primary = std::make_unique<QPushButton>(primaryText);

    dialog->setLayout(layout.get());
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);
    heading->setObjectName(QStringLiteral("cardTitle"));
    heading->setWordWrap(true);
    allowShrink(*heading);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    allowShrink(*body);
    scroll->setWidget(body.get());
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    enableTouchScrolling(*scroll);
    primary->setObjectName(QStringLiteral("primaryButton"));
    primary->setProperty("role", QStringLiteral("dialogPrimary"));
    secondary->setProperty("role", QStringLiteral("dialogSecondary"));
    QObject::connect(primary.get(), &QPushButton::clicked, &overlay, &OverlayDialog::accept);
    QObject::connect(secondary.get(), &QPushButton::clicked, &overlay, &OverlayDialog::reject);
    secondary->setVisible(!secondaryText.isEmpty());
    buttons->setSpacing(10);
    buttons->addWidget(secondary.get(), 1);
    buttons->addWidget(primary.get(), 1);
    layout->addWidget(heading.get());
    layout->addWidget(scroll.get(), 1);
    layout->addLayout(buttons.get());

    const QSize area = parent.window()->size();
    const int width = qMax(260, area.width() - 32);
    const int bodyHeight = body->heightForWidth(width - 40);
    const int height = qMin(area.height() * 4 / 5, bodyHeight + 150);
    dialog->setFixedSize(width, height);
    overlay.setLayout(outer.get());
    outer->addWidget(dialog.get(), 0, Qt::AlignCenter);
    return overlay.exec();
}

} // namespace

// Asks a yes/no question with translated buttons; returns true for "yes".
bool confirm(QWidget &parent, const QString &title, const QString &text)
{
    return runDialog(parent, title, text, QCoreApplication::translate("Ui", "Yes"),
                     QCoreApplication::translate("Ui", "No"));
}

// Shows an information message with a translated OK button. The dialog uses
// almost the full window width and scrolls long texts.
void inform(QWidget &parent, const QString &title, const QString &text)
{
    runDialog(parent, title, text, QCoreApplication::translate("Ui", "OK"), QString());
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
