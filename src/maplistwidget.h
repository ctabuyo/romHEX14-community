/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QVector>
#include <QTimer>
#include <QTreeWidget>
#include <QProgressBar>
#include "romdata.h"

class MapListWidget : public QWidget {
    Q_OBJECT

public:
    explicit MapListWidget(QWidget *parent = nullptr);

    void setMaps(const QVector<MapInfo> &maps, uint32_t baseAddress);
    void clear();
    void setProgressMessage(const QString &msg, int pct);
    void retranslateUi();

signals:
    void mapSelected(const MapInfo &map);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onSearchChanged();

private:
    void filterMaps();
    void populateTree();

    QLabel       *m_searchLabel  = nullptr;
    QLineEdit    *m_searchBox    = nullptr;
    QLabel       *m_statusLabel  = nullptr;
    QProgressBar *m_progressBar  = nullptr;
    QTreeWidget  *m_tree         = nullptr;

    QVector<MapInfo> m_allMaps;
    uint32_t m_baseAddress = 0;
    QTimer m_searchTimer;
};
