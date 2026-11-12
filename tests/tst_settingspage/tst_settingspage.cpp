// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "appsettings.h"
#include "settingspage.h"

#include <QApplication>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScroller>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace PicSync;

/// Widget tests of the settings page (run offscreen on the build machine).
class TestSettingsPage : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // Creates a fresh, visible settings page with empty settings.
    void init();
    // Releases the page.
    void cleanup();

    // Every text field asks the keyboard to commit keys immediately (no suggestions).
    void fieldsDisableSuggestions();
    // Typed text appears in the field right away.
    void typingShowsText();
    // Tapping into a field inside the scroll area focuses it without delay.
    void tapFocusesFieldImmediately();
    // Saving stores normalised values and the password; reloading shows them.
    void saveAndReload();
    // Saving with missing values shows an error and stores nothing.
    void missingValuesAreRejected();

private:
    // Returns the line edit with the given object name.
    QPointer<QLineEdit> field(const char *name) const;
    // Returns the status line below the page.
    QPointer<QLabel> message() const;

    std::unique_ptr<SettingsPage> m_page;
};

// Creates a fresh, visible settings page with empty settings.
void TestSettingsPage::init()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    m_page = std::make_unique<SettingsPage>();
    m_page->resize(420, 800);
    m_page->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_page.get()));
    m_page->activateWindow();
}

// Releases the page.
void TestSettingsPage::cleanup()
{
    m_page.reset();
}

// Returns the line edit with the given object name.
QPointer<QLineEdit> TestSettingsPage::field(const char *name) const
{
    return QPointer<QLineEdit>(m_page->findChild<QLineEdit *>(QString::fromLatin1(name)));
}

// Returns the status line below the page.
QPointer<QLabel> TestSettingsPage::message() const
{
    const QList<QLabel *> labels = m_page->findChildren<QLabel *>();
    for (int i = 0; i < labels.size(); ++i) {
        const QPointer<QLabel> label(labels.at(i));
        if (label->property("role").toString() == QLatin1String("message"))
            return label;
    }
    return QPointer<QLabel>();
}

// Every text field asks the keyboard to commit keys immediately (no suggestions).
void TestSettingsPage::fieldsDisableSuggestions()
{
    const QList<QLineEdit *> edits = m_page->findChildren<QLineEdit *>();
    QVERIFY(edits.size() >= 4);
    for (int i = 0; i < edits.size(); ++i) {
        const QPointer<QLineEdit> edit(edits.at(i));
        if (edit->objectName().isEmpty())
            continue;   // internal line edits of combo boxes
        QVERIFY2(edit->inputMethodHints().testFlag(Qt::ImhNoPredictiveText),
                 qPrintable(edit->objectName()));
        // URL/e-mail field types make some Android keyboards ignore "no suggestions".
        QVERIFY2(!edit->inputMethodHints().testFlag(Qt::ImhUrlCharactersOnly),
                 qPrintable(edit->objectName()));
        QVERIFY2(!edit->inputMethodHints().testFlag(Qt::ImhEmailCharactersOnly),
                 qPrintable(edit->objectName()));
    }
}

// Typed text appears in the field right away.
void TestSettingsPage::typingShowsText()
{
    const QPointer<QLineEdit> url = field("serverUrl");
    QVERIFY(url);
    url->setFocus();
    QTest::keyClicks(url.data(), QStringLiteral("magentacloud.de"));
    QCOMPARE(url->text(), QStringLiteral("magentacloud.de"));
    QTest::keyClick(url.data(), Qt::Key_Backspace);
    QCOMPARE(url->displayText(), QStringLiteral("magentacloud.d"));
}

// Tapping into a field inside the scroll area focuses it without delay.
void TestSettingsPage::tapFocusesFieldImmediately()
{
    const QPointer<QScrollArea> area(m_page->findChild<QScrollArea *>());
    QVERIFY(area);
    QVERIFY(QScroller::hasScroller(area->viewport()));
    const QVariant delay = QScroller::scroller(area->viewport())->scrollerProperties()
                               .scrollMetric(QScrollerProperties::MousePressEventDelay);
    QCOMPARE(delay.toReal(), 0.0);

    const QPointer<QLineEdit> user = field("userName");
    QVERIFY(user);
    QTest::mouseClick(user.data(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTRY_VERIFY(user->hasFocus());
}

// Saving stores normalised values and the password; reloading shows them.
void TestSettingsPage::saveAndReload()
{
    field("serverUrl")->setText(QStringLiteral("https://magentacloud.de/remote.php/webdav"));
    field("userName")->setText(QStringLiteral(" 120049 "));
    field("appPassword")->setText(QStringLiteral("abcde-fghij"));
    field("remoteFolder")->setText(QStringLiteral("/PicSync/Peter Fairphone/"));
    const QPointer<QPushButton> save(m_page->findChild<QPushButton *>(QStringLiteral("saveButton")));
    QVERIFY(save);
    QTest::mouseClick(save.data(), Qt::LeftButton);

    QVERIFY(message());
    QVERIFY(!message()->property("error").toBool());
    QSettings settings;
    const AppSettings stored = AppSettings::load(settings, QStringLiteral("x"));
    QCOMPARE(stored.serverUrl, QStringLiteral("https://magentacloud.de"));
    QCOMPARE(stored.user, QStringLiteral("120049"));
    QCOMPARE(stored.remoteFolder, QStringLiteral("PicSync/Peter Fairphone"));

    // A new page shows the stored values; the password stays hidden but known.
    m_page = std::make_unique<SettingsPage>();
    QCOMPARE(field("serverUrl")->text(), QStringLiteral("https://magentacloud.de"));
    QCOMPARE(field("userName")->text(), QStringLiteral("120049"));
    QVERIFY(field("appPassword")->text().isEmpty());
    QVERIFY(!field("appPassword")->placeholderText().isEmpty());
    QCOMPARE(field("appPassword")->echoMode(), QLineEdit::Password);
}

// Saving with missing values shows an error and stores nothing.
void TestSettingsPage::missingValuesAreRejected()
{
    field("serverUrl")->clear();
    field("userName")->setText(QStringLiteral("peter"));
    const QPointer<QPushButton> save(m_page->findChild<QPushButton *>(QStringLiteral("saveButton")));
    QVERIFY(save);
    QTest::mouseClick(save.data(), Qt::LeftButton);
    QVERIFY(message());
    QVERIFY(message()->isVisible());
    QVERIFY(message()->property("error").toBool());
    QSettings settings;
    QVERIFY(!settings.contains(QStringLiteral("server/user")));
}

// Runs the tests offscreen with private, temporary settings.
int main(int argc, char *argv[])
{
    const QTemporaryDir home;
    qputenv("XDG_CONFIG_HOME", home.filePath(QStringLiteral("config")).toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath(QStringLiteral("data")).toUtf8());
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("PicSyncTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PicSyncTest"));
    TestSettingsPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_settingspage.moc"
