/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include "project.h"
#include "mappack.h"

// Map pack dialog — two modes:
//   Export: builds a pack from diff results or all project maps, lets user save.
//   Import: loads a .rxpack file, shows contents, lets user apply to current ROM.
class MapPackDlg : public QDialog {
    Q_OBJECT

public:
    // Export mode — call with diffs from a compare session.
    // cmpRom: the compare ROM whose map data will be packed.
    static void exportFromDiffs(const QVector<MapDiff> &diffs,
                                const QByteArray &cmpRom,
                                ByteOrder bo,
                                const QString &cmpLabel,
                                Project *project,
                                QWidget *parent = nullptr);

    // Import mode — opens file picker and applies to the current project.
    static void importPack(Project *project, QWidget *parent = nullptr);

    // Internal constructor (use the static factory methods above).
    explicit MapPackDlg(QWidget *parent = nullptr);

private slots:
    void onSave();
    void onApply();
    void onSelectAll();
    void onSelectNone();

private:
    void buildUi();
    void populateTable(const MapPack &pack);
    QVector<int> selectedRows() const;

    Project      *m_project  = nullptr;
    MapPack       m_pack;
    bool          m_exportMode = true;

    QLabel       *m_titleLabel  = nullptr;
    QLineEdit    *m_labelEdit   = nullptr;
    QTableWidget *m_table       = nullptr;
    QLabel       *m_statusLabel = nullptr;
};
