// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "dashboardpage.h"
#include "appsettings.h"
#include "pathutils.h"
#include "platform/platform.h"
#include "statestore.h"
#include "synchistory.h"
#include "syncsession.h"
#include "uihelpers.h"

#include <QMap>
#include <QSettings>
#include <QStyle>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <utility>

namespace PicSync {

namespace {
const int kIconSize = 22;
}

// Builds the page.
DashboardPage::DashboardPage()
    : m_layout(std::make_unique<QVBoxLayout>())
    , m_scroll(std::make_unique<QScrollArea>())
    , m_content(std::make_unique<QWidget>())
    , m_contentLayout(std::make_unique<QVBoxLayout>())
    , m_permCard(std::make_unique<QFrame>())
    , m_permLayout(std::make_unique<QHBoxLayout>())
    , m_permIcon(std::make_unique<QLabel>())
    , m_permText(std::make_unique<QLabel>())
    , m_permButton(std::make_unique<QPushButton>())
    , m_serverCard(std::make_unique<QFrame>())
    , m_serverLayout(std::make_unique<QGridLayout>())
    , m_serverIcon(std::make_unique<QLabel>())
    , m_serverTitle(std::make_unique<QLabel>())
    , m_serverStatus(std::make_unique<QLabel>())
    , m_serverDetail(std::make_unique<QLabel>())
    , m_storageCard(std::make_unique<QFrame>())
    , m_storageLayout(std::make_unique<QGridLayout>())
    , m_storageIcon(std::make_unique<QLabel>())
    , m_storageTitle(std::make_unique<QLabel>())
    , m_storageBar(std::make_unique<QProgressBar>())
    , m_storageDetail(std::make_unique<QLabel>())
    , m_foldersCard(std::make_unique<QFrame>())
    , m_foldersLayout(std::make_unique<QGridLayout>())
    , m_foldersIcon(std::make_unique<QLabel>())
    , m_foldersTitle(std::make_unique<QLabel>())
    , m_foldersCount(std::make_unique<QLabel>())
    , m_conditionsCard(std::make_unique<QFrame>())
    , m_conditionsLayout(std::make_unique<QGridLayout>())
    , m_conditionsIcon(std::make_unique<QLabel>())
    , m_conditionsTitle(std::make_unique<QLabel>())
    , m_conditionsDetail(std::make_unique<QLabel>())
    , m_lastSync(std::make_unique<QLabel>())
    , m_phase(std::make_unique<QLabel>())
    , m_progress(std::make_unique<QProgressBar>())
    , m_syncButton(std::make_unique<QPushButton>())
{
    setLayout(m_layout.get());
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Ui::enableTouchScrolling(*m_scroll);
    m_content->setObjectName(QStringLiteral("page"));
    m_content->setLayout(m_contentLayout.get());
    m_contentLayout->setContentsMargins(14, 14, 14, 14);
    m_contentLayout->setSpacing(12);
    m_scroll->setWidget(m_content.get());
    m_layout->addWidget(m_scroll.get(), 1);

    // Warning card shown while the storage permission is missing.
    m_permCard->setObjectName(QStringLiteral("warningCard"));
    m_permCard->setLayout(m_permLayout.get());
    m_permIcon->setPixmap(Ui::pixmap(QStringLiteral("warn"), kIconSize));
    m_permText->setWordWrap(true);
    Ui::allowShrink(*m_permText);
    m_permText->setText(tr("PicSync needs access to all files to synchronise the gallery."));
    m_permButton->setText(tr("Grant"));
    m_permLayout->addWidget(m_permIcon.get());
    m_permLayout->addWidget(m_permText.get(), 1);
    m_permLayout->addWidget(m_permButton.get());
    connect(m_permButton.get(), &QPushButton::clicked, this, &DashboardPage::onGrantStorage);
    m_contentLayout->addWidget(m_permCard.get());

    // Connection card.
    setupCard(*m_serverCard, *m_serverLayout, *m_serverIcon, *m_serverTitle,
              QStringLiteral("server"), QString());
    m_serverLayout->addWidget(m_serverStatus.get(), 0, 2, Qt::AlignRight);
    m_serverDetail->setObjectName(QStringLiteral("detail"));
    m_serverDetail->setWordWrap(true);
    Ui::allowShrink(*m_serverDetail);
    m_serverLayout->addWidget(m_serverDetail.get(), 1, 1, 1, 2);
    m_contentLayout->addWidget(m_serverCard.get());

    // Storage card.
    setupCard(*m_storageCard, *m_storageLayout, *m_storageIcon, *m_storageTitle,
              QStringLiteral("storage"), tr("Storage"));
    m_storageBar->setRange(0, 1000);
    m_storageBar->setTextVisible(false);
    m_storageBar->setFixedHeight(8);
    m_storageLayout->addWidget(m_storageBar.get(), 1, 1, 1, 2);
    m_storageDetail->setObjectName(QStringLiteral("detail"));
    m_storageDetail->setWordWrap(true);
    Ui::allowShrink(*m_storageDetail);
    m_storageLayout->addWidget(m_storageDetail.get(), 2, 1, 1, 2);
    m_contentLayout->addWidget(m_storageCard.get());

    // Folders card.
    setupCard(*m_foldersCard, *m_foldersLayout, *m_foldersIcon, *m_foldersTitle,
              QStringLiteral("folder"), tr("Folders"));
    m_foldersCount->setObjectName(QStringLiteral("detail"));
    m_foldersLayout->addWidget(m_foldersCount.get(), 0, 2, Qt::AlignRight);
    m_contentLayout->addWidget(m_foldersCard.get());

    // Conditions card.
    setupCard(*m_conditionsCard, *m_conditionsLayout, *m_conditionsIcon, *m_conditionsTitle,
              QStringLiteral("wifi"), tr("Automatic sync"));
    m_conditionsDetail->setObjectName(QStringLiteral("detail"));
    m_conditionsDetail->setWordWrap(true);
    Ui::allowShrink(*m_conditionsDetail);
    m_conditionsLayout->addWidget(m_conditionsDetail.get(), 1, 1, 1, 2);
    m_lastSync->setObjectName(QStringLiteral("detail"));
    m_lastSync->setWordWrap(true);
    Ui::allowShrink(*m_lastSync);
    m_conditionsLayout->addWidget(m_lastSync.get(), 2, 1, 1, 2);
    m_contentLayout->addWidget(m_conditionsCard.get());
    m_contentLayout->addStretch(1);

    // Bottom area: progress and the big button.
    m_phase->setObjectName(QStringLiteral("phase"));
    m_phase->setAlignment(Qt::AlignCenter);
    m_phase->setWordWrap(true);
    Ui::allowShrink(*m_phase);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_syncButton->setObjectName(QStringLiteral("primaryButton"));
    m_syncButton->setIcon(Ui::icon(QStringLiteral("sync_white")));
    m_syncButton->setIconSize(QSize(22, 22));
    m_syncButton->setMinimumHeight(52);
    connect(m_syncButton.get(), &QPushButton::clicked, this, &DashboardPage::onSyncButton);
    m_layout->addWidget(m_phase.get());
    m_layout->addWidget(m_progress.get());
    m_layout->addSpacing(8);
    m_layout->addWidget(m_syncButton.get());
    m_layout->setContentsMargins(0, 0, 0, 0);

    setRunning(false);
    refresh();
}

// Destroys the page.
DashboardPage::~DashboardPage() = default;

// Creates an empty card with icon and title in its first row.
void DashboardPage::setupCard(QFrame &card, QGridLayout &layout, QLabel &icon, QLabel &title,
                              const QString &iconName, const QString &text)
{
    card.setObjectName(QStringLiteral("card"));
    card.setLayout(&layout);
    layout.setContentsMargins(14, 12, 14, 12);
    layout.setHorizontalSpacing(12);
    layout.setVerticalSpacing(6);
    layout.setColumnStretch(1, 1);
    icon.setPixmap(Ui::pixmap(iconName, kIconSize));
    icon.setFixedWidth(kIconSize + 4);
    title.setObjectName(QStringLiteral("cardTitle"));
    title.setText(text);
    layout.addWidget(&icon, 0, 0, Qt::AlignTop);
    layout.addWidget(&title, 0, 1);
}

// Re-reads settings, state and history and updates all cards.
void DashboardPage::refresh()
{
    QSettings qsettings;
    const AppSettings settings = AppSettings::load(qsettings, Platform::deviceName());
    SyncHistory history(SyncSession::historyFilePath());
    history.load();
    const bool hasRun = !history.entries().isEmpty();
    const SyncResult last = hasRun ? history.entries().first() : SyncResult();

    m_permCard->setVisible(!Platform::hasStorageAccess());

    // Connection.
    if (settings.isComplete()) {
        const QUrl url(PathUtils::normalizeServerUrl(settings.serverUrl));
        m_serverTitle->setText(url.host().isEmpty() ? settings.serverUrl : url.host());
        m_serverDetail->setText(settings.user + QStringLiteral(" · /") + settings.remoteFolder);
    } else {
        m_serverTitle->setText(tr("Not connected"));
        m_serverDetail->setText(tr("Open the settings to connect your Nextcloud."));
    }
    QString status;
    if (!hasRun)
        status = QString();
    else if (!last.success)
        status = QStringLiteral("error");
    else if (last.pendingDeletions > 0)
        status = QStringLiteral("warn");
    else
        status = QStringLiteral("ok");
    Ui::allowShrink(*m_serverTitle);
    m_serverStatus->setPixmap(status.isEmpty() ? QPixmap() : Ui::pixmap(status, kIconSize));

    // Storage: account quota from the last run, file count from the state.
    StateStore state(SyncSession::stateFilePath());
    state.load();
    qint64 syncedBytes = 0;
    for (auto it = state.entries().constBegin(); it != state.entries().constEnd(); ++it)
        syncedBytes += it.value().localSize;
    const QString files = tr("%n file(s) synchronised (%1)", nullptr,
                             int(state.entries().size())).arg(Ui::sizeText(syncedBytes));
    QuotaInfo quota;
    for (const SyncResult &r : history.entries()) {
        if (r.quota.valid) {
            quota = r.quota;
            break;
        }
    }
    if (quota.valid && quota.availableBytes >= 0) {
        const qint64 total = quota.usedBytes + quota.availableBytes;
        m_storageBar->setValue(total > 0 ? int(quota.usedBytes * 1000 / total) : 0);
        m_storageBar->setVisible(true);
        m_storageDetail->setText(tr("%1 of %2 used").arg(Ui::sizeText(quota.usedBytes),
                                                         Ui::sizeText(total))
                                 + QStringLiteral("\n") + files);
    } else if (quota.valid) {
        m_storageBar->setVisible(false);
        m_storageDetail->setText(tr("%1 used (no limit)").arg(Ui::sizeText(quota.usedBytes))
                                 + QStringLiteral("\n") + files);
    } else {
        m_storageBar->setVisible(false);
        m_storageDetail->setText(files);
    }

    refreshFolders();

    // Conditions and last run.
    QStringList conditions;
    conditions << tr("Runs %1").arg(Ui::intervalText(settings.intervalMinutes));
    conditions << (settings.wifiOnly ? tr("Wi-Fi only") : tr("Wi-Fi and mobile data"));
    if (settings.chargingOnly)
        conditions << tr("only while charging");
    m_conditionsDetail->setText(conditions.join(QStringLiteral(" · ")));

    if (!hasRun) {
        m_lastSync->setText(tr("Not synchronised yet."));
        m_lastSync->setProperty("error", false);
    } else if (last.success) {
        m_lastSync->setText(tr("Last sync %1: %2 up, %3 down, %4 deleted")
                                .arg(Ui::timeText(last.finishedAt))
                                .arg(last.uploaded).arg(last.downloaded)
                                .arg(last.deletedLocal + last.deletedRemote));
        m_lastSync->setProperty("error", false);
    } else {
        m_lastSync->setText(tr("Last sync %1 failed: %2")
                                .arg(Ui::timeText(last.finishedAt), last.errorMessage));
        m_lastSync->setProperty("error", true);
    }
    m_lastSync->style()->unpolish(m_lastSync.get());
    m_lastSync->style()->polish(m_lastSync.get());
}

// Updates the folder list of the folders card: every folder with its number of
// files, the space they take and waiting deletions.
void DashboardPage::refreshFolders()
{
    StateStore state(SyncSession::stateFilePath());
    state.load();
    QMap<QString, int> counts;
    QMap<QString, qint64> bytes;
    QMap<QString, int> pending;
    for (auto it = state.entries().constBegin(); it != state.entries().constEnd(); ++it) {
        const QString folder = PathUtils::parentPath(it.key());
        counts[folder] += 1;
        bytes[folder] += it.value().localSize;
    }
    for (auto it = state.pending().constBegin(); it != state.pending().constEnd(); ++it)
        pending[PathUtils::parentPath(it.key())] += 1;

    // Short names ("Camera") unless two folders share one, e.g. two "Signal" folders.
    QMap<QString, int> nameUses;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        nameUses[Ui::folderName(it.key())] += 1;

    // Biggest folders (by number of files) first.
    std::vector<std::pair<int, QString>> bySize;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        bySize.emplace_back(it.value(), it.key());
    std::stable_sort(bySize.begin(), bySize.end(), std::greater<std::pair<int, QString>>());

    m_folderLabels.clear();
    m_foldersCount->setText(QString::number(bySize.size()));

    int row = 1;
    for (const std::pair<int, QString> &entry : bySize) {
        const QString &folder = entry.second;
        const QString shortName = Ui::folderName(folder);
        auto name = std::make_unique<QLabel>(nameUses.value(shortName) > 1 ? folder : shortName);
        name->setToolTip(folder);
        name->setWordWrap(true);
        Ui::allowShrink(*name);
        auto info = std::make_unique<QLabel>(
            tr("%n file(s) · %1", nullptr, counts.value(folder))
                .arg(Ui::sizeText(bytes.value(folder))));
        info->setObjectName(QStringLiteral("detail"));
        info->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_foldersLayout->addWidget(name.get(), row, 1);
        m_foldersLayout->addWidget(info.get(), row, 2, Qt::AlignRight);
        m_folderLabels.push_back(std::move(name));
        m_folderLabels.push_back(std::move(info));
        ++row;
        if (pending.value(folder) > 0) {
            auto waiting = std::make_unique<QLabel>(
                tr("%n deletion(s) waiting", nullptr, pending.value(folder)));
            waiting->setObjectName(QStringLiteral("warning"));
            m_foldersLayout->addWidget(waiting.get(), row, 1, 1, 2);
            m_folderLabels.push_back(std::move(waiting));
            ++row;
        }
    }
    if (bySize.empty()) {
        auto empty = std::make_unique<QLabel>(tr("Folders appear after the first sync."));
        empty->setObjectName(QStringLiteral("detail"));
        empty->setWordWrap(true);
        Ui::allowShrink(*empty);
        m_foldersLayout->addWidget(empty.get(), 1, 1, 1, 2);
        m_folderLabels.push_back(std::move(empty));
    }
}

// Switches between the idle and the running look.
void DashboardPage::setRunning(bool running)
{
    m_running = running;
    m_syncButton->setText(running ? tr("Stop") : tr("Sync now"));
    m_progress->setVisible(running);
    m_phase->setVisible(running);
    if (running) {
        m_progress->setRange(0, 0);
        m_phase->setText(tr("Starting…"));
    }
}

// Shows the progress of a running synchronisation.
void DashboardPage::setProgress(int done, int total, const QString &currentFile)
{
    if (total <= 0)
        return;
    m_progress->setRange(0, total);
    m_progress->setValue(done);
    const QString text = currentFile.isEmpty()
            ? tr("%1 of %2 files").arg(done).arg(total)
            : tr("%1 of %2: %3").arg(done + 1).arg(total)
                  .arg(currentFile.section(QLatin1Char('/'), -1));
    // Long file names have no spaces to wrap at: shorten them in the middle.
    const int available = qMax(120, width() - 40);
    m_phase->setText(m_phase->fontMetrics().elidedText(text, Qt::ElideMiddle, available));
    m_phase->setToolTip(text);
}

// Shows the current phase of a running synchronisation.
void DashboardPage::setPhase(const QString &phase)
{
    m_phase->setText(phase + QStringLiteral("…"));
}

// Starts or stops a synchronisation, depending on the current state.
void DashboardPage::onSyncButton()
{
    if (m_running) {
        m_syncButton->setText(tr("Stopping…"));
        Q_EMIT stopRequested();
    } else {
        Q_EMIT syncRequested();
    }
}

// Opens the system page for the storage permission.
void DashboardPage::onGrantStorage()
{
    Platform::requestStorageAccess();
}

} // namespace PicSync
