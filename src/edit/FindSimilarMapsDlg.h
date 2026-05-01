/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Find Similar Maps dialog (Sprint F)
 * ====================================
 *
 * Modal dialog driven by `MapFingerprintEngine::similarity()`.  User
 * picks a "reference" map (passed in by MainWindow), the dialog
 * computes the reference's fingerprint plus fingerprints for every
 * other map in the project, then renders a sortable list of matches
 * filtered by a tunable similarity threshold (default 80 %).
 *
 * Activating a row emits `goToRequested(MapInfo)` so MainWindow can
 * highlight the candidate in the project tree.
 */

#pragma once

#include <QDialog>
#include <QVector>
#include "romdata.h"

class Project;
class QTableWidget;
class QLabel;
class QSlider;

class FindSimilarMapsDlg : public QDialog {
    Q_OBJECT
public:
    FindSimilarMapsDlg(const Project *project,
                       const MapInfo &reference,
                       QWidget *parent = nullptr);

signals:
    void goToRequested(const MapInfo &map);

private slots:
    void onThresholdChanged(int sliderValue);
    void onRowActivated(int row);

private:
    void buildUi();
    void computeAll();
    void rebuildTable();

    struct Match {
        MapInfo map;
        double  score;
    };

    const Project *m_project = nullptr;
    MapInfo        m_reference;
    QVector<Match> m_matches;        // sorted descending by score

    QTableWidget *m_table       = nullptr;
    QLabel       *m_summary     = nullptr;
    QLabel       *m_thresholdLb = nullptr;
    QSlider      *m_threshold   = nullptr;
};
