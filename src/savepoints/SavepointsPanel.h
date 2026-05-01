/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Right-side dock widget that visualises a SavepointManager — list of
 * named tuning branches, with switch / rename / delete / new actions.
 */

#pragma once

#include <QWidget>

class SavepointManager;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

class SavepointsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SavepointsPanel(QWidget *parent = nullptr);

    /// Bind to a manager.  The panel listens to its signals and keeps
    /// the list in sync.  Pass nullptr to detach.
    void setManager(SavepointManager *mgr);

private slots:
    void onCreateClicked();
    void onSwitchClicked();
    void onRenameClicked();
    void onDeleteClicked();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onSelectionChanged();
    void refresh();

private:
    void buildUi();

    SavepointManager *m_mgr = nullptr;

    // ── UI ────────────────────────────────────────────────────────────
    QLineEdit   *m_inputLabel    = nullptr;
    QPushButton *m_btnCreate     = nullptr;
    QListWidget *m_list          = nullptr;
    QPushButton *m_btnSwitch     = nullptr;
    QPushButton *m_btnRename     = nullptr;
    QPushButton *m_btnDelete     = nullptr;
    QLabel      *m_summary       = nullptr;
};
