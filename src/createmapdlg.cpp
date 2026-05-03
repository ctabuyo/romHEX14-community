/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "createmapdlg.h"
#include "appconstants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QListWidget>
#include <QSettings>
#include <QCloseEvent>

CreateMapDlg::CreateMapDlg(uint32_t startAddress, int selectionLength,
                           int cellSize, QWidget *parent)
    : QDialog(parent)
    , m_startAddress(startAddress)
    , m_selectionLength(selectionLength)
{
    setWindowTitle(tr("Create Map"));
    setMinimumSize(480, 520);

    auto *lay = new QVBoxLayout(this);
    lay->setSpacing(12);

    // ── Preset templates ────────────────────────────────────────────
    auto *presetGroup = new QGroupBox(tr("Map templates"));
    auto *presetLay = new QVBoxLayout(presetGroup);
    auto *presetList = new QListWidget();
    presetList->setMaximumHeight(110);

    struct Preset { QString label; int cols; int rows; };
    QVector<Preset> presets = {
        {tr("1D: %1×1 (curve)").arg(selectionLength / cellSize), selectionLength / cellSize, 1},
        {tr("User 1: 10×10"), 10, 10},
        {tr("User 2: 16×16"), 16, 16},
        {tr("User 3: 20×20"), 20, 20},
        {tr("User 4: 8×8"),   8,  8},
        {tr("User 5: 12×12"), 12, 12},
    };
    for (const auto &p : presets)
        presetList->addItem(p.label);

    presetLay->addWidget(presetList);
    lay->addWidget(presetGroup);

    // ── Map properties ──────────────────────────────────────────────
    auto *grid = new QGridLayout();
    grid->setVerticalSpacing(8);
    grid->setHorizontalSpacing(12);
    int row = 0;

    // Name
    grid->addWidget(new QLabel(tr("Name:")), row, 0);
    m_nameEdit = new QLineEdit();
    m_nameEdit->setPlaceholderText(tr("e.g. KFMIOP_Torque_Request"));
    grid->addWidget(m_nameEdit, row, 1, 1, 3);
    row++;

    // Start address
    grid->addWidget(new QLabel(tr("Start address:")), row, 0);
    m_addrEdit = new QLineEdit(QString("%1").arg(startAddress, 0, 16).toUpper());
    m_addrEdit->setMaximumWidth(120);
    QFont mono("Consolas", 10);
    m_addrEdit->setFont(mono);
    grid->addWidget(m_addrEdit, row, 1);
    row++;

    // Columns x Rows
    grid->addWidget(new QLabel(tr("Columns × Rows:")), row, 0);
    auto *dimRow = new QHBoxLayout();
    m_colsSpin = new QSpinBox();
    m_colsSpin->setRange(1, 9999);
    m_colsSpin->setValue(16);
    m_rowsSpin = new QSpinBox();
    m_rowsSpin->setRange(1, 9999);
    m_rowsSpin->setValue(qMax(1, selectionLength / (16 * cellSize)));
    dimRow->addWidget(m_colsSpin);
    dimRow->addWidget(new QLabel("×"));
    dimRow->addWidget(m_rowsSpin);
    dimRow->addStretch();
    grid->addLayout(dimRow, row, 1, 1, 3);
    row++;

    // Cell size
    grid->addWidget(new QLabel(tr("Cell size:")), row, 0);
    m_cellSizeCombo = new QComboBox();
    m_cellSizeCombo->addItem("8-bit (1 byte)", 1);
    m_cellSizeCombo->addItem("16-bit (2 bytes)", 2);
    m_cellSizeCombo->addItem("32-bit (4 bytes)", 4);
    m_cellSizeCombo->setCurrentIndex(cellSize == 1 ? 0 : cellSize == 4 ? 2 : 1);
    grid->addWidget(m_cellSizeCombo, row, 1, 1, 2);
    row++;

    // Data type
    grid->addWidget(new QLabel(tr("Data type:")), row, 0);
    m_dataTypeCombo = new QComboBox();
    m_dataTypeCombo->addItem(tr("Unsigned"));
    m_dataTypeCombo->addItem(tr("Signed"));
    grid->addWidget(m_dataTypeCombo, row, 1, 1, 2);
    row++;

    lay->addLayout(grid);

    // ── Preview ─────────────────────────────────────────────────────
    m_previewLabel = new QLabel();
    m_previewLabel->setStyleSheet("color:#6b7a96; font-size:9pt; padding:8px;");
    lay->addWidget(m_previewLabel);

    lay->addStretch();

    // ── Buttons ─────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    auto *okBtn = new QPushButton(tr("Create Map"));
    okBtn->setDefault(true);
    okBtn->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #3a91d0,stop:1 #2563eb);"
        "color:white;border:none;border-radius:8px;padding:10px 32px;font-weight:700}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #4da8e8,stop:1 #3b7bf5)}");
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setStyleSheet(
        "QPushButton{background:rgba(15,22,41,0.6);border:1px solid rgba(231,238,252,0.1);"
        "color:#a9b6d3;border-radius:8px;padding:10px 24px}"
        "QPushButton:hover{border-color:#3a91d0;color:#e7eefc}");
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    lay->addLayout(btnRow);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // ── Connect signals ─────────────────────────────────────────────
    connect(m_colsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { updatePreview(); });
    connect(m_rowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { updatePreview(); });
    connect(m_cellSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { updatePreview(); });

    connect(presetList, &QListWidget::currentRowChanged, this, [this, presets](int idx) {
        if (idx >= 0 && idx < presets.size()) {
            m_colsSpin->setValue(presets[idx].cols);
            m_rowsSpin->setValue(presets[idx].rows);
        }
    });

    updatePreview();

    restoreGeometry(rx14::appSettings()
                    .value("dialogGeometry/CreateMapDlg").toByteArray());
}

void CreateMapDlg::closeEvent(QCloseEvent *event)
{
    rx14::appSettings()
        .setValue("dialogGeometry/CreateMapDlg", saveGeometry());
    QDialog::closeEvent(event);
}

void CreateMapDlg::updatePreview()
{
    int cs = m_cellSizeCombo->currentData().toInt();
    int cols = m_colsSpin->value();
    int rows = m_rowsSpin->value();
    int totalBytes = cols * rows * cs;

    QString info = tr("Total: %1 cells = %2 bytes").arg(cols * rows).arg(totalBytes);
    if (m_selectionLength > 0) {
        double pct = (double)totalBytes / m_selectionLength * 100.0;
        info += tr("  |  Selection: %1 bytes (%2%)")
            .arg(m_selectionLength)
            .arg(pct, 0, 'f', 0);
    }
    m_previewLabel->setText(info);
}

MapInfo CreateMapDlg::resultMap() const
{
    MapInfo m;
    m.name = m_nameEdit->text().trimmed();
    if (m.name.isEmpty())
        m.name = QString("UserMap_0x%1").arg(m_startAddress, 0, 16).toUpper();

    bool ok;
    m.rawAddress = m_addrEdit->text().toUInt(&ok, 16);
    m.address = m.rawAddress;

    int cs = m_cellSizeCombo->currentData().toInt();
    m.dataSize = cs;
    m.dataSigned = (m_dataTypeCombo->currentIndex() == 1);

    m.dimensions.x = m_colsSpin->value();
    m.dimensions.y = m_rowsSpin->value();

    m.length = m.dimensions.x * m.dimensions.y * cs;

    if (m.dimensions.y <= 1)
        m.type = "CURVE";
    else
        m.type = "MAP";

    m.linkConfidence = 100; // user-created

    return m;
}
