/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include "romdata.h"

class QCloseEvent;

class CreateMapDlg : public QDialog {
    Q_OBJECT
public:
    explicit CreateMapDlg(uint32_t startAddress, int selectionLength,
                          int cellSize, QWidget *parent = nullptr);

    MapInfo resultMap() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void updatePreview();

    QLineEdit *m_nameEdit    = nullptr;
    QLineEdit *m_addrEdit    = nullptr;
    QSpinBox  *m_colsSpin    = nullptr;
    QSpinBox  *m_rowsSpin    = nullptr;
    QComboBox *m_cellSizeCombo = nullptr;
    QComboBox *m_dataTypeCombo = nullptr;
    QCheckBox *m_signedCheck = nullptr;
    QLabel    *m_previewLabel = nullptr;

    uint32_t m_startAddress;
    int m_selectionLength;
};
