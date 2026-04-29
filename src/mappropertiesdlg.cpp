/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mappropertiesdlg.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFrame>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFont>
#include <QSettings>
#include <QCloseEvent>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static const QString kStyle =
    "QDialog          { background:#1c2128; color:#e6edf3; }"
    "QTabWidget::pane { border:1px solid #30363d; background:#1c2128; }"
    "QTabBar::tab     { background:#161b22; color:#8b949e; padding:5px 14px; "
    "                   border:1px solid #30363d; border-bottom:none; }"
    "QTabBar::tab:selected { background:#1c2128; color:#e6edf3; }"
    "QLineEdit, QPlainTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {"
    "  background:#0d1117; color:#e6edf3; border:1px solid #30363d; "
    "  border-radius:4px; padding:2px 4px; }"
    "QComboBox::drop-down { border:none; }"
    "QLabel  { color:#8b949e; }"
    "QLabel[class='id'] { color:#58a6ff; font-family:Consolas; }"
    "QGroupBox { border:1px solid #30363d; border-radius:4px; "
    "            margin-top:8px; color:#8b949e; }"
    "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }"
    "QCheckBox { color:#e6edf3; spacing:6px; }"
    "QCheckBox::indicator { width:14px; height:14px; border:1px solid #30363d; "
    "  border-radius:3px; background:#0d1117; }"
    "QCheckBox::indicator:checked { background:#388bfd; border-color:#388bfd; }"
    "QPushButton { background:#21262d; color:#e6edf3; border:1px solid #30363d; "
    "              border-radius:4px; padding:4px 12px; }"
    "QPushButton:hover { background:#2d333b; }"
    "QPushButton:pressed { background:#161b22; }"
    "QPushButton[default='true'] { border-color:#388bfd; }";

static QComboBox *dataOrgCombo()
{
    auto *c = new QComboBox();
    c->addItem("8 Bit",              8);
    c->addItem("16 Bit (LoHi)",     16);
    c->addItem("16 Bit (HiLo)",    -16);
    c->addItem("32 Bit (LoHi)",     32);
    c->addItem("32 Bit (HiLo)",    -32);
    return c;
}

static void setDataOrg(QComboBox *c, int dataSize, ByteOrder bo)
{
    int bits = dataSize * 8;
    int code = (bo == ByteOrder::LittleEndian) ? bits : -bits;
    if (dataSize == 1) code = 8;
    for (int i = 0; i < c->count(); ++i)
        if (c->itemData(i).toInt() == code) { c->setCurrentIndex(i); return; }
    c->setCurrentIndex(1); // fallback 16-bit LoHi
}

static QPair<int,ByteOrder> fromDataOrg(QComboBox *c)
{
    int code = c->currentData().toInt();
    if (code ==  8)  return {1, ByteOrder::LittleEndian};
    if (code ==  16) return {2, ByteOrder::LittleEndian};
    if (code == -16) return {2, ByteOrder::BigEndian};
    if (code ==  32) return {4, ByteOrder::LittleEndian};
    if (code == -32) return {4, ByteOrder::BigEndian};
    return {2, ByteOrder::LittleEndian};
}

QWidget *MapPropertiesDialog::makeSeparator()
{
    auto *f = new QFrame();
    f->setFrameShape(QFrame::HLine);
    f->setStyleSheet("color:#30363d;");
    return f;
}

int MapPropertiesDialog::precisionFromFormat(const QString &fmt) const
{
    int dot = fmt.indexOf('.');
    if (dot < 0) return 2;
    int end = dot + 1;
    while (end < fmt.size() && fmt[end].isDigit()) ++end;
    return fmt.mid(dot + 1, end - dot - 1).toInt();
}

QString MapPropertiesDialog::formatFromPrecision(int prec) const
{
    return QString("%1.%2f").arg(1).arg(prec);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

MapPropertiesDialog::MapPropertiesDialog(const MapInfo &map, ByteOrder byteOrder,
                                         QWidget *parent)
    : QDialog(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
    , m_result(map)
    , m_byteOrder(byteOrder)
{
    setWindowTitle(tr("Properties of…  %1").arg(map.name));
    setMinimumSize(520, 580);
    setStyleSheet(kStyle);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *tabs = new QTabWidget();
    tabs->addTab(buildMapTab(),     tr("Map"));
    tabs->addTab(buildAxisTab(true),  tr("X-Axis"));
    tabs->addTab(buildAxisTab(false), tr("Y-Axis"));
    tabs->addTab(buildCommentTab(), tr("Comment"));
    tabs->addTab(buildToolsTab(),   tr("Tools"));
    root->addWidget(tabs, 1);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok |
                                    QDialogButtonBox::Cancel |
                                    QDialogButtonBox::Help);
    bb->button(QDialogButtonBox::Ok)->setDefault(true);
    root->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, this, &MapPropertiesDialog::apply);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populateMap();
    populateAxis(true);
    populateAxis(false);
    populateComment();

    restoreGeometry(QSettings("CT14", "RX14")
                    .value("dialogGeometry/MapPropertiesDlg").toByteArray());
}

void MapPropertiesDialog::closeEvent(QCloseEvent *event)
{
    QSettings("CT14", "RX14")
        .setValue("dialogGeometry/MapPropertiesDlg", saveGeometry());
    QDialog::closeEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// Build tabs
// ─────────────────────────────────────────────────────────────────────────────

QWidget *MapPropertiesDialog::buildMapTab()
{
    auto *w  = new QWidget();
    auto *fl = new QFormLayout(w);
    fl->setLabelAlignment(Qt::AlignRight);
    fl->setSpacing(6);
    fl->setContentsMargins(12, 12, 12, 12);

    // Name row with ">" button
    auto *nameRow = new QHBoxLayout();
    m_nameEdit = new QLineEdit();
    auto *nameFwd = new QPushButton(">");
    nameFwd->setFixedWidth(26); nameFwd->setToolTip(tr("Copy to all linked maps"));
    nameRow->addWidget(m_nameEdit);
    nameRow->addWidget(nameFwd);
    fl->addRow(tr("Name:"), nameRow);

    // Description
    auto *descRow = new QHBoxLayout();
    m_descEdit = new QLineEdit();
    auto *descFwd = new QPushButton(">");
    descFwd->setFixedWidth(26);
    descRow->addWidget(m_descEdit);
    descRow->addWidget(descFwd);
    fl->addRow(tr("Description:"), descRow);

    // Unit + Id on same row
    auto *unitIdRow = new QHBoxLayout();
    m_unitEdit = new QLineEdit(); m_unitEdit->setFixedWidth(80);
    auto *unitFwd = new QPushButton(">"); unitFwd->setFixedWidth(26);
    auto *idLbl   = new QLabel(tr("Id:"));
    idLbl->setStyleSheet("color:#8b949e;");
    m_idLabel = new QLabel();
    m_idLabel->setStyleSheet("color:#58a6ff; font-family:Consolas;");
    auto *idFwd = new QPushButton(">"); idFwd->setFixedWidth(26);
    unitIdRow->addWidget(m_unitEdit);
    unitIdRow->addWidget(unitFwd);
    unitIdRow->addSpacing(8);
    unitIdRow->addWidget(idLbl);
    unitIdRow->addWidget(m_idLabel, 1);
    unitIdRow->addWidget(idFwd);
    fl->addRow(tr("Unit:"), unitIdRow);

    fl->addRow(makeSeparator());

    // Start address
    auto *addrRow = new QHBoxLayout();
    m_addrEdit = new QLineEdit();
    m_addrEdit->setFixedWidth(120);
    m_addrEdit->setFont(QFont("Consolas", 9));
    auto *addrFwd = new QPushButton(tr("From hexdump cursor"));
    addrRow->addWidget(m_addrEdit);
    addrRow->addWidget(addrFwd);
    addrRow->addStretch();
    fl->addRow(tr("Start address:"), addrRow);

    // Type
    m_typeCombo = new QComboBox();
    m_typeCombo->addItems({"MAP", "CURVE", "VALUE", "VAL_BLK",
                           "2D Inverse", "3D", "3D Inverse"});
    fl->addRow(tr("Type:"), m_typeCombo);

    // Columns × rows
    auto *dimRow = new QHBoxLayout();
    m_colsSpin = new QSpinBox(); m_colsSpin->setRange(1, 256); m_colsSpin->setFixedWidth(70);
    m_rowsSpin = new QSpinBox(); m_rowsSpin->setRange(1, 256); m_rowsSpin->setFixedWidth(70);
    auto *xLbl = new QLabel("x"); xLbl->setStyleSheet("color:#e6edf3;");
    dimRow->addWidget(m_colsSpin);
    dimRow->addWidget(xLbl);
    dimRow->addWidget(m_rowsSpin);
    dimRow->addStretch();
    fl->addRow(tr("Columns × rows:"), dimRow);

    // Data organization + skip bytes
    auto *dataOrgRow = new QHBoxLayout();
    m_dataOrgCombo  = dataOrgCombo(); m_dataOrgCombo->setMinimumWidth(160);
    auto *skipLbl   = new QLabel(tr("Skip bytes:"));
    skipLbl->setStyleSheet("color:#8b949e;");
    m_skipBytesSpin = new QSpinBox(); m_skipBytesSpin->setRange(0, 1024); m_skipBytesSpin->setFixedWidth(60);
    dataOrgRow->addWidget(m_dataOrgCombo);
    dataOrgRow->addSpacing(8);
    dataOrgRow->addWidget(skipLbl);
    dataOrgRow->addWidget(m_skipBytesSpin);
    dataOrgRow->addStretch();
    fl->addRow(tr("Data organization:"), dataOrgRow);

    // Number format
    m_numFmtCombo = new QComboBox();
    m_numFmtCombo->addItem(tr("Decimal   (Base 10 System)"), 0);
    m_numFmtCombo->addItem(tr("Hex       (Base 16 System)"), 1);
    m_numFmtCombo->addItem(tr("Binary    (Base 2 System)"),  2);
    fl->addRow(tr("Number format:"), m_numFmtCombo);

    fl->addRow(makeSeparator());

    // Checkboxes row
    auto *chkRow = new QHBoxLayout();
    m_signCheck = new QCheckBox(tr("Sign"));
    m_diffCheck = new QCheckBox(tr("Difference"));
    m_oriCheck  = new QCheckBox(tr("Original values"));
    m_pctCheck  = new QCheckBox(tr("Percent"));
    chkRow->addWidget(m_signCheck);
    chkRow->addSpacing(16);
    chkRow->addWidget(m_diffCheck);
    chkRow->addStretch();
    fl->addRow(QString(), chkRow);
    auto *chkRow2 = new QHBoxLayout();
    chkRow2->addWidget(m_oriCheck);
    chkRow2->addSpacing(16);
    chkRow2->addWidget(m_pctCheck);
    chkRow2->addStretch();
    fl->addRow(QString(), chkRow2);

    fl->addRow(makeSeparator());

    // Factor / offset group
    auto *fctGroup = new QGroupBox(tr("Factor, offset"));
    fctGroup->setCheckable(true);
    auto *fgl = new QVBoxLayout(fctGroup);
    fgl->setSpacing(4);

    auto *fctRow = new QHBoxLayout();
    m_factorSpin = new QDoubleSpinBox();
    m_factorSpin->setRange(-1e9, 1e9); m_factorSpin->setDecimals(8);
    m_factorSpin->setSingleStep(0.001); m_factorSpin->setFixedWidth(140);
    auto *mulLbl = new QLabel("× EPROM");
    mulLbl->setStyleSheet("color:#8b949e;");
    fctRow->addWidget(m_factorSpin);
    fctRow->addWidget(mulLbl);
    fctRow->addStretch();
    fgl->addLayout(fctRow);

    auto *offRow = new QHBoxLayout();
    auto *valLbl = new QLabel(tr("Value ="));
    valLbl->setStyleSheet("color:#8b949e;");
    m_offsetSpin = new QDoubleSpinBox();
    m_offsetSpin->setRange(-1e9, 1e9); m_offsetSpin->setDecimals(8);
    m_offsetSpin->setSingleStep(0.001); m_offsetSpin->setFixedWidth(140);
    auto *plusLbl = new QLabel("+");
    plusLbl->setStyleSheet("color:#8b949e;");
    offRow->addWidget(valLbl);
    offRow->addSpacing(8);
    offRow->addWidget(plusLbl);
    offRow->addWidget(m_offsetSpin);
    offRow->addStretch();
    fgl->addLayout(offRow);
    fl->addRow(fctGroup);

    fl->addRow(makeSeparator());

    // Precision
    auto *precRow = new QHBoxLayout();
    m_precSpin = new QSpinBox(); m_precSpin->setRange(0, 8); m_precSpin->setFixedWidth(60);
    precRow->addWidget(new QLabel(tr("Decimals:")));
    precRow->addWidget(m_precSpin);
    precRow->addStretch();
    fl->addRow(tr("Precision:"), precRow);

    return w;
}

QWidget *MapPropertiesDialog::buildAxisTab(bool isX)
{
    auto *w  = new QWidget();
    auto *fl = new QFormLayout(w);
    fl->setLabelAlignment(Qt::AlignRight);
    fl->setSpacing(6);
    fl->setContentsMargins(12, 12, 12, 12);

    QLineEdit       *&descEdit     = isX ? m_xDescEdit     : m_yDescEdit;
    QLineEdit       *&unitEdit     = isX ? m_xUnitEdit     : m_yUnitEdit;
    QLabel          *&idLabel      = isX ? m_xIdLabel      : m_yIdLabel;
    QLineEdit       *&addrEdit     = isX ? m_xAddrEdit     : m_yAddrEdit;
    QComboBox       *&dataOrgCb    = isX ? m_xDataOrgCombo : m_yDataOrgCombo;
    QCheckBox       *&signCheck    = isX ? m_xSignCheck    : m_ySignCheck;
    QDoubleSpinBox  *&factorSpin   = isX ? m_xFactorSpin   : m_yFactorSpin;
    QDoubleSpinBox  *&offsetSpin   = isX ? m_xOffsetSpin   : m_yOffsetSpin;
    QSpinBox        *&precSpin     = isX ? m_xPrecSpin     : m_yPrecSpin;

    // Description
    auto *descRow = new QHBoxLayout();
    descEdit = new QLineEdit();
    descRow->addWidget(descEdit);
    descRow->addWidget(new QPushButton(">"));
    fl->addRow(tr("Description:"), descRow);

    // Unit + Id
    auto *unitIdRow = new QHBoxLayout();
    unitEdit = new QLineEdit(); unitEdit->setFixedWidth(80);
    idLabel  = new QLabel();
    idLabel->setStyleSheet("color:#58a6ff; font-family:Consolas;");
    unitIdRow->addWidget(unitEdit);
    unitIdRow->addWidget(new QPushButton(">"));
    unitIdRow->addSpacing(8);
    unitIdRow->addWidget(new QLabel(tr("Id:")));
    unitIdRow->addWidget(idLabel, 1);
    unitIdRow->addWidget(new QPushButton(">"));
    fl->addRow(tr("Unit:"), unitIdRow);

    // Data source (fixed = EPROM)
    auto *srcRow = new QHBoxLayout();
    auto *srcCombo = new QComboBox(); srcCombo->addItem("EPROM");
    srcCombo->setMinimumWidth(140);
    srcRow->addWidget(srcCombo);
    srcRow->addWidget(new QPushButton("…"));
    srcRow->addStretch();
    fl->addRow(tr("Data source:"), srcRow);

    fl->addRow(makeSeparator());

    // Start address
    auto *addrRow = new QHBoxLayout();
    addrEdit = new QLineEdit(); addrEdit->setFixedWidth(120);
    addrEdit->setFont(QFont("Consolas", 9));
    addrRow->addWidget(addrEdit);
    addrRow->addWidget(new QPushButton(tr("From hexdump cursor")));
    addrRow->addStretch();
    fl->addRow(tr("Start address:"), addrRow);

    // Mirror map + search axis
    auto *mirrorRow = new QHBoxLayout();
    auto *mirrorCheck = new QCheckBox(tr("Mirror map"));
    auto *searchBtn   = new QPushButton(tr("Search axis…"));
    mirrorRow->addWidget(mirrorCheck);
    mirrorRow->addStretch();
    mirrorRow->addWidget(searchBtn);
    fl->addRow(QString(), mirrorRow);

    fl->addRow(makeSeparator());

    // Data organization
    dataOrgCb = dataOrgCombo();
    fl->addRow(tr("Data organization:"), dataOrgCb);

    // Number format (read-only label — shares main map's setting)
    auto *numFmtCb = new QComboBox();
    numFmtCb->addItem(tr("Decimal   (Base 10 System)"), 0);
    numFmtCb->addItem(tr("Hex       (Base 16 System)"), 1);
    fl->addRow(tr("Number format:"), numFmtCb);

    signCheck = new QCheckBox(tr("Sign"));
    fl->addRow(QString(), signCheck);

    fl->addRow(makeSeparator());

    // Factor / offset
    auto *fctGroup = new QGroupBox(tr("Factor, offset"));
    auto *fgl = new QVBoxLayout(fctGroup);
    fgl->setSpacing(4);

    auto *fctRow = new QHBoxLayout();
    factorSpin = new QDoubleSpinBox();
    factorSpin->setRange(-1e9, 1e9); factorSpin->setDecimals(8);
    factorSpin->setSingleStep(0.001); factorSpin->setFixedWidth(140);
    fctRow->addWidget(factorSpin);
    fctRow->addWidget(new QLabel("× EPROM"));
    fctRow->addStretch();
    fgl->addLayout(fctRow);

    auto *offRow = new QHBoxLayout();
    offsetSpin = new QDoubleSpinBox();
    offsetSpin->setRange(-1e9, 1e9); offsetSpin->setDecimals(8);
    offsetSpin->setSingleStep(0.001); offsetSpin->setFixedWidth(140);
    offRow->addWidget(new QLabel(tr("Value =")));
    offRow->addSpacing(8);
    offRow->addWidget(new QLabel("+"));
    offRow->addWidget(offsetSpin);
    offRow->addStretch();
    fgl->addLayout(offRow);
    fl->addRow(fctGroup);

    // Precision
    auto *precRow = new QHBoxLayout();
    precSpin = new QSpinBox(); precSpin->setRange(0, 8); precSpin->setFixedWidth(60);
    precRow->addWidget(precSpin);
    precRow->addStretch();
    fl->addRow(tr("Precision:"), precRow);

    return w;
}

QWidget *MapPropertiesDialog::buildCommentTab()
{
    auto *w  = new QWidget();
    auto *vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(6);

    auto *lbl = new QLabel(tr("User notes / comments:"));
    lbl->setStyleSheet("color:#8b949e;");
    m_commentEdit = new QPlainTextEdit();
    m_commentEdit->setFont(QFont("Segoe UI", 9));
    vl->addWidget(lbl);
    vl->addWidget(m_commentEdit, 1);
    return w;
}

QWidget *MapPropertiesDialog::buildToolsTab()
{
    auto *w  = new QWidget();
    auto *vl = new QVBoxLayout(w);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(6);

    auto *exportBtn = new QPushButton(tr("Export map data to CSV…"));
    auto *copyBtn   = new QPushButton(tr("Copy raw values to clipboard"));
    auto *findBtn   = new QPushButton(tr("Search axis in ROM…"));

    vl->addWidget(exportBtn);
    vl->addWidget(copyBtn);
    vl->addWidget(findBtn);
    vl->addStretch();
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate from m_result
// ─────────────────────────────────────────────────────────────────────────────

void MapPropertiesDialog::populateMap()
{
    const MapInfo &m = m_result;
    m_nameEdit->setText(m.name);
    m_descEdit->setText(m.description);
    m_unitEdit->setText(m.scaling.unit);
    m_idLabel->setText(m.name);

    // OLS convention: show file offset of where map DATA starts
    m_addrEdit->setText(QString("%1").arg(m.address + m.mapDataOffset, 0, 16).toUpper());

    // Type combo
    int typeIdx = m_typeCombo->findText(m.type);
    m_typeCombo->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);

    m_colsSpin->setValue(m.dimensions.x);
    m_rowsSpin->setValue(m.dimensions.y);

    setDataOrg(m_dataOrgCombo, m.dataSize, m_byteOrder);
    m_skipBytesSpin->setValue((int)m.mapDataOffset);

    m_factorSpin->setValue(m.scaling.linA);
    m_offsetSpin->setValue(m.scaling.linB);
    m_precSpin->setValue(precisionFromFormat(m.scaling.format));
}

void MapPropertiesDialog::populateAxis(bool isX)
{
    const AxisInfo &ax = isX ? m_result.xAxis : m_result.yAxis;

    QLineEdit      *descEdit   = isX ? m_xDescEdit   : m_yDescEdit;
    QLineEdit      *unitEdit   = isX ? m_xUnitEdit   : m_yUnitEdit;
    QLabel         *idLabel    = isX ? m_xIdLabel    : m_yIdLabel;
    QLineEdit      *addrEdit   = isX ? m_xAddrEdit   : m_yAddrEdit;
    QComboBox      *dataOrgCb  = isX ? m_xDataOrgCombo : m_yDataOrgCombo;
    QDoubleSpinBox *factorSpin = isX ? m_xFactorSpin : m_yFactorSpin;
    QDoubleSpinBox *offsetSpin = isX ? m_xOffsetSpin : m_yOffsetSpin;
    QSpinBox       *precSpin   = isX ? m_xPrecSpin   : m_yPrecSpin;

    descEdit->setText(ax.inputName);
    unitEdit->setText(ax.scaling.unit);
    idLabel->setText(ax.inputName);
    addrEdit->setText(ax.hasPtsAddress
        ? QString("0x%1").arg(ax.ptsAddress, 0, 16).toUpper()
        : QString());
    setDataOrg(dataOrgCb, ax.ptsDataSize, m_byteOrder);
    factorSpin->setValue(ax.scaling.linA);
    offsetSpin->setValue(ax.scaling.linB);
    precSpin->setValue(precisionFromFormat(ax.scaling.format));
}

void MapPropertiesDialog::populateComment()
{
    m_commentEdit->setPlainText(m_result.userNotes);
}

// ─────────────────────────────────────────────────────────────────────────────
// Collect → apply → accept
// ─────────────────────────────────────────────────────────────────────────────

void MapPropertiesDialog::collectMap()
{
    m_result.name        = m_nameEdit->text().trimmed();
    m_result.description = m_descEdit->text().trimmed();
    m_result.scaling.unit = m_unitEdit->text().trimmed();

    // Address: user sees address+mapDataOffset (OLS convention), reverse to get address
    bool ok;
    uint32_t displayAddr = m_addrEdit->text().trimmed().toUInt(&ok, 16);
    if (ok) {
        uint32_t oldOffset = m_result.mapDataOffset; // read before overwrite below
        uint32_t newAddress = (displayAddr >= oldOffset) ? displayAddr - oldOffset : displayAddr;
        int32_t delta = (int32_t)newAddress - (int32_t)m_result.address;
        m_result.rawAddress = (uint32_t)((int32_t)m_result.rawAddress + delta);
        m_result.address    = newAddress;
    }

    // Type
    const QString t = m_typeCombo->currentText();
    if (t == "MAP" || t == "CURVE" || t == "VALUE" || t == "VAL_BLK")
        m_result.type = t;

    m_result.dimensions.x = m_colsSpin->value();
    m_result.dimensions.y = m_rowsSpin->value();

    auto [ds, bo] = fromDataOrg(m_dataOrgCombo);
    m_result.dataSize  = ds;
    m_byteOrder        = bo;
    m_result.mapDataOffset = (uint32_t)m_skipBytesSpin->value();

    m_result.scaling.linA  = m_factorSpin->value();
    m_result.scaling.linB  = m_offsetSpin->value();
    m_result.scaling.format = formatFromPrecision(m_precSpin->value());
    if (m_result.scaling.linA != 1.0 || m_result.scaling.linB != 0.0)
        m_result.scaling.type = CompuMethod::Type::Linear;
    m_result.hasScaling = (m_result.scaling.type != CompuMethod::Type::Identical);
}

void MapPropertiesDialog::collectAxis(bool isX)
{
    AxisInfo &ax = isX ? m_result.xAxis : m_result.yAxis;

    QLineEdit      *descEdit   = isX ? m_xDescEdit   : m_yDescEdit;
    QLineEdit      *unitEdit   = isX ? m_xUnitEdit   : m_yUnitEdit;
    QLineEdit      *addrEdit   = isX ? m_xAddrEdit   : m_yAddrEdit;
    QComboBox      *dataOrgCb  = isX ? m_xDataOrgCombo : m_yDataOrgCombo;
    QDoubleSpinBox *factorSpin = isX ? m_xFactorSpin : m_yFactorSpin;
    QDoubleSpinBox *offsetSpin = isX ? m_xOffsetSpin : m_yOffsetSpin;
    QSpinBox       *precSpin   = isX ? m_xPrecSpin   : m_yPrecSpin;

    ax.inputName     = descEdit->text().trimmed();
    ax.scaling.unit  = unitEdit->text().trimmed();

    bool ok;
    uint32_t addr = addrEdit->text().trimmed().toUInt(&ok, 16);
    if (ok) { ax.ptsAddress = addr; ax.hasPtsAddress = true; }

    auto [ds, bo] = fromDataOrg(dataOrgCb);
    ax.ptsDataSize = ds;

    ax.scaling.linA   = factorSpin->value();
    ax.scaling.linB   = offsetSpin->value();
    ax.scaling.format = formatFromPrecision(precSpin->value());
    if (ax.scaling.linA != 1.0 || ax.scaling.linB != 0.0)
        ax.scaling.type = CompuMethod::Type::Linear;
    ax.hasScaling = (ax.scaling.type != CompuMethod::Type::Identical);
}

void MapPropertiesDialog::collectComment()
{
    m_result.userNotes = m_commentEdit->toPlainText();
}

void MapPropertiesDialog::apply()
{
    collectMap();
    collectAxis(true);
    collectAxis(false);
    collectComment();
    accept();
}
