/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QLabel>
#include <QLineEdit>
#include <QVector>
#include <QSet>
#include "romdata.h"
#include "a2lparser.h"

class QCloseEvent;

class A2LImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit A2LImportDialog(const QVector<MapInfo>   &maps,
                             const QVector<A2LGroup>  &groups,
                             uint32_t                  baseAddress,
                             QWidget                  *parent = nullptr);

    // Returns only the maps the user left checked
    QVector<MapInfo> selectedMaps() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildTree();
    void addGroupNode(QTreeWidgetItem          *parent,
                      const QString            &groupName,
                      const QMap<QString, int> &groupIdx,
                      const QMap<QString, int> &mapIdx,
                      QSet<QString>            &placed);
    void addMapLeaf(QTreeWidgetItem *parent, int mapIndex);

    // Check-state management
    void onItemChanged(QTreeWidgetItem *item, int col);
    void cascadeDown(QTreeWidgetItem *item, Qt::CheckState state);
    void refreshAncestors(QTreeWidgetItem *item);
    Qt::CheckState childrenState(QTreeWidgetItem *group) const;

    // Selection helpers
    void selectAll(bool checked);
    void updateCount();
    void applyFilter(const QString &text);
    void refreshGroupVisibility(QTreeWidgetItem *group);

    QTreeWidget *m_tree       = nullptr;
    QLabel      *m_countLabel = nullptr;
    QLineEdit   *m_searchBox  = nullptr;

    QVector<MapInfo>  m_maps;
    QVector<A2LGroup> m_groups;
};
