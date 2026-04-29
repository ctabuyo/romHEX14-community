/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include "romdata.h"

class QCloseEvent;

class MapPropertiesDialog : public QDialog {
    Q_OBJECT

public:
    explicit MapPropertiesDialog(const MapInfo &map, ByteOrder byteOrder,
                                 QWidget *parent = nullptr);

    MapInfo   result()    const { return m_result; }
    ByteOrder byteOrder() const { return m_byteOrder; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void apply();

private:
    // ── helpers ───────────────────────────────────────────────────────────────
    QWidget *buildMapTab();
    QWidget *buildAxisTab(bool isX);
    QWidget *buildCommentTab();
    QWidget *buildToolsTab();

    static QWidget *makeSeparator();

    void populateMap();
    void populateAxis(bool isX);
    void populateComment();

    void collectMap();
    void collectAxis(bool isX);
    void collectComment();

    int  precisionFromFormat(const QString &fmt) const;
    QString formatFromPrecision(int prec) const;

    // ── data ──────────────────────────────────────────────────────────────────
    MapInfo   m_result;
    ByteOrder m_byteOrder;

    // Map tab
    QLineEdit        *m_nameEdit       = nullptr;
    QLineEdit        *m_descEdit       = nullptr;
    QLineEdit        *m_unitEdit       = nullptr;
    QLabel           *m_idLabel        = nullptr;
    QLineEdit        *m_addrEdit       = nullptr;
    QComboBox        *m_typeCombo      = nullptr;
    QSpinBox         *m_colsSpin       = nullptr;
    QSpinBox         *m_rowsSpin       = nullptr;
    QComboBox        *m_dataOrgCombo   = nullptr;
    QSpinBox         *m_skipBytesSpin  = nullptr;
    QComboBox        *m_numFmtCombo    = nullptr;
    QCheckBox        *m_signCheck      = nullptr;
    QCheckBox        *m_diffCheck      = nullptr;
    QCheckBox        *m_oriCheck       = nullptr;
    QCheckBox        *m_pctCheck       = nullptr;
    QDoubleSpinBox   *m_factorSpin     = nullptr;
    QDoubleSpinBox   *m_offsetSpin     = nullptr;
    QSpinBox         *m_precSpin       = nullptr;

    // X-Axis tab
    QLineEdit        *m_xDescEdit      = nullptr;
    QLineEdit        *m_xUnitEdit      = nullptr;
    QLabel           *m_xIdLabel       = nullptr;
    QLineEdit        *m_xAddrEdit      = nullptr;
    QComboBox        *m_xDataOrgCombo  = nullptr;
    QCheckBox        *m_xSignCheck     = nullptr;
    QDoubleSpinBox   *m_xFactorSpin    = nullptr;
    QDoubleSpinBox   *m_xOffsetSpin    = nullptr;
    QSpinBox         *m_xPrecSpin      = nullptr;

    // Y-Axis tab
    QLineEdit        *m_yDescEdit      = nullptr;
    QLineEdit        *m_yUnitEdit      = nullptr;
    QLabel           *m_yIdLabel       = nullptr;
    QLineEdit        *m_yAddrEdit      = nullptr;
    QComboBox        *m_yDataOrgCombo  = nullptr;
    QCheckBox        *m_ySignCheck     = nullptr;
    QDoubleSpinBox   *m_yFactorSpin    = nullptr;
    QDoubleSpinBox   *m_yOffsetSpin    = nullptr;
    QSpinBox         *m_yPrecSpin      = nullptr;

    // Comment tab
    QPlainTextEdit   *m_commentEdit    = nullptr;
};
