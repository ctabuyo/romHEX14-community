/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include "romdata.h"

class Project;

struct DtcEntry {
    QString dfcName;         // e.g. "DFC_ACCIBrkErr"
    QString pCode;           // e.g. "P0642" (from DFES_DTCX)
    uint32_t ctlMskAddr = 0; // ROM address of DFC_CtlMsk2.DFC_XXX_C
    uint32_t disblMskAddr = 0;
    uint32_t dtcxAddr = 0;   // ROM address of DFES_DTCX.DFC_XXX_C
    uint8_t  ctlMskOriginal = 0xFF;
    uint8_t  ctlMskCurrent = 0xFF;
    bool     hasCtlMsk = false;
    bool     hasDisblMsk = false;
    bool     hasDtcx = false;
    bool     isEnabled() const { return ctlMskCurrent != 0; }
};

class DtcDialog : public QDialog {
    Q_OBJECT
public:
    explicit DtcDialog(Project *project, QWidget *parent = nullptr);

private:
    void buildUi();
    void loadDtcEntries();
    void populateTable(const QString &filter = {});
    void toggleSelected();
    void toggleEntry(int row);
    void applyChanges();
    QString rawToPCode(uint16_t raw) const;

    Project *m_project = nullptr;
    QVector<DtcEntry> m_entries;

    QLineEdit    *m_search = nullptr;
    QTableWidget *m_table  = nullptr;
    QLabel       *m_stats  = nullptr;
    QPushButton  *m_btnToggle = nullptr;
    QPushButton  *m_btnAllOff = nullptr;
    QPushButton  *m_btnRestore = nullptr;
    QPushButton  *m_btnApply = nullptr;
};
