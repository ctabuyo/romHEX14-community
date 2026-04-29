/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QVector>
#include <QStringList>

class Project;
struct MapInfo;

// ── Function descriptor ──────────────────────────────────────────────────────
struct AIFunction {
    QString id;             // internal key
    QString emoji;          // icon emoji
    QString name;           // display name (translated)
    QString description;    // one-line description (translated)
    QStringList patterns;   // wildcard patterns for map search
    QString actionType;     // "zero" or "limiter"
};

// ── Confirmation dialog (shown after pattern search) ─────────────────────────
class AIFunctionConfirmDlg : public QDialog {
    Q_OBJECT
public:
    explicit AIFunctionConfirmDlg(const AIFunction &func,
                                  const QVector<MapInfo> &matched,
                                  Project *project,
                                  const QString &targetName,
                                  QWidget *parent = nullptr);

    /// Apply checked changes to project ROM data; returns number of maps written.
    /// If `progress` is non-null, updates it per map and aborts if canceled.
    int applyChanges(class QProgressDialog *progress = nullptr);

    /// Describes one pending write for the universal risky-change confirm dialog.
    struct PendingRow {
        QString mapName;
        QString oldValue;
        QString newValue;
    };
    QVector<PendingRow> pendingRows() const;

private:
    void populateTree(const QVector<MapInfo> &matched);
    double firstCellValue(const MapInfo &mi) const;

    const AIFunction &m_func;
    Project          *m_project;
    QString           m_targetName;
    QTreeWidget      *m_tree   = nullptr;
    QLabel           *m_summary = nullptr;

    // Keep matched MapInfo copies so we can write back
    QVector<MapInfo>  m_maps;
};

// ── Main function grid dialog ────────────────────────────────────────────────
class AIFunctionsDlg : public QDialog {
    Q_OBJECT
public:
    explicit AIFunctionsDlg(const QVector<Project*> &projects, Project *activeProject,
                            QWidget *parent = nullptr);

signals:
    void projectModified();

private:
    void buildFunctions();
    void onFunctionClicked(int index);
    QVector<MapInfo> searchMaps(const QStringList &patterns) const;
    bool wildcardMatch(const QString &name, const QString &pattern) const;
    Project *selectedProject() const;

    QVector<Project*>    m_projects;
    Project             *m_project = nullptr;   // currently selected
    QVector<AIFunction>  m_functions;
    QGridLayout         *m_grid = nullptr;
    QComboBox           *m_targetCombo = nullptr;
};
