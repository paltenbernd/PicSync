// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#include "historypage.h"
#include "synchistory.h"
#include "syncsession.h"
#include "uihelpers.h"

#include <QHeaderView>

namespace PicSync {

// Builds the page.
HistoryPage::HistoryPage()
    : m_layout(std::make_unique<QVBoxLayout>())
    , m_empty(std::make_unique<QLabel>())
    , m_tree(std::make_unique<QTreeWidget>())
    , m_clear(std::make_unique<QPushButton>())
{
    setObjectName(QStringLiteral("page"));
    setLayout(m_layout.get());
    m_layout->setContentsMargins(14, 14, 14, 14);
    m_layout->setSpacing(10);

    m_empty->setObjectName(QStringLiteral("detail"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setText(tr("No synchronisation has run yet."));
    m_layout->addWidget(m_empty.get());

    m_tree->setObjectName(QStringLiteral("history"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setRootIsDecorated(true);
    m_tree->setWordWrap(true);
    m_tree->setUniformRowHeights(false);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->header()->setStretchLastSection(true);
    Ui::enableTouchScrolling(*m_tree);
    m_layout->addWidget(m_tree.get(), 1);

    m_clear->setText(tr("Clear history"));
    m_layout->addWidget(m_clear.get());
    connect(m_clear.get(), &QPushButton::clicked, this, &HistoryPage::onClear);

    refresh();
}

// Destroys the page.
HistoryPage::~HistoryPage() = default;

// Re-reads the history file.
void HistoryPage::refresh()
{
    SyncHistory history(SyncSession::historyFilePath());
    history.load();
    m_tree->clear();
    for (const SyncResult &r : history.entries()) {
        QString title = Ui::timeText(r.finishedAt) + QStringLiteral(" · ")
                + (r.manual ? tr("manual") : tr("automatic"));
        QString summary;
        if (r.success) {
            summary = tr("↑ %1  ↓ %2  ✕ %3").arg(r.uploaded).arg(r.downloaded)
                          .arg(r.deletedLocal + r.deletedRemote);
            if (r.conflicts > 0)
                summary += tr("  ⚠ %n conflict(s)", nullptr, r.conflicts);
            if (r.pendingDeletions > 0)
                summary += tr("  ⏳ %n waiting", nullptr, r.pendingDeletions);
            if (r.skippedFiles > 0)
                summary += tr("  – %n server only", nullptr, r.skippedFiles);
        } else {
            summary = r.errorMessage;
        }

        auto item = std::make_unique<QTreeWidgetItem>();
        item->setText(0, title + QLatin1Char('\n') + summary);
        item->setToolTip(0, summary);
        item->setIcon(0, Ui::icon(r.success ? (r.pendingDeletions > 0 ? QStringLiteral("warn")
                                                                       : QStringLiteral("ok"))
                                            : QStringLiteral("error")));
        for (const QString &file : r.changedFiles) {
            auto child = std::make_unique<QTreeWidgetItem>();
            child->setText(0, file);
            child->setToolTip(0, file);
            item->addChild(child.release());   // ownership passes to the parent item
        }
        m_tree->addTopLevelItem(item.release());   // ownership passes to the tree
    }
    const bool empty = history.entries().isEmpty();
    m_empty->setVisible(empty);
    m_tree->setVisible(!empty);
    m_clear->setEnabled(!empty);
}

// Deletes the stored history after confirmation.
void HistoryPage::onClear()
{
    if (!Ui::confirm(*this, tr("Clear history"), tr("Delete the sync history?")))
        return;
    SyncHistory history(SyncSession::historyFilePath());
    history.clear();
    refresh();
}

} // namespace PicSync
