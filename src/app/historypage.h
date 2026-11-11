// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Peter Altenbernd <peter.altenbernd@h-da.de>
// This file is part of PicSync.

#pragma once

#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

namespace PicSync {

/// History screen: one entry per sync run; tapping an entry shows the files.
class HistoryPage : public QWidget
{
    Q_OBJECT
public:
    // Builds the page.
    HistoryPage();

    // Destroys the page.
    ~HistoryPage() override;

    // Re-reads the history file.
    void refresh();

private Q_SLOTS:
    // Deletes the stored history after confirmation.
    void onClear();

private:
    std::unique_ptr<QVBoxLayout> m_layout;
    std::unique_ptr<QLabel> m_empty;
    std::unique_ptr<QTreeWidget> m_tree;
    std::unique_ptr<QPushButton> m_clear;
};

} // namespace PicSync
