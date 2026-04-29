/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "configdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QColorDialog>
#include <QFrame>
#include <QFormLayout>
#include <QSettings>
#include <QCloseEvent>
#include <QPainter>

static void applySwatchStyle(QPushButton *btn, const QColor &col)
{
    btn->setStyleSheet(QString(
        "QPushButton { background:%1; border:1px solid #555; border-radius:2px; }"
        "QPushButton:hover { border-color:#58a6ff; }").arg(col.name(QColor::HexRgb)));
}

static QPushButton *makeSwatchBtn(const QColor &col)
{
    auto *btn = new QPushButton;
    btn->setFixedSize(48, 20);
    btn->setCursor(Qt::PointingHandCursor);
    applySwatchStyle(btn, col);
    return btn;
}

// ── ConfigDialog ────────────────────────────────────────────────────────────

ConfigDialog::ConfigDialog(QWidget *parent)
    : QDialog(parent)
    , m_working(AppConfig::instance().colors)
{
    setWindowTitle(tr("Configuration"));
    setMinimumSize(660, 560);
    resize(660, 560);
    setModal(true);

    setStyleSheet(
        "QDialog { background:#0d1117; }"
        "QGroupBox { color:#8b949e; border:1px solid #30363d; border-radius:4px;"
        "  margin-top:10px; font-size:8pt; padding-top:6px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }"
        "QLabel { color:#c9d1d9; background:transparent; }"
        "QPushButton { background:#21262d; color:#c9d1d9; border:1px solid #30363d;"
        "  border-radius:4px; padding:4px 12px; }"
        "QPushButton:hover { border-color:#58a6ff; color:#58a6ff; }"
        "QPushButton:pressed { background:#1f6feb; }"
        "QScrollArea { background:#0d1117; border:none; }"
        "QScrollBar:vertical { background:#0d1117; width:8px; }"
        "QScrollBar::handle:vertical { background:#30363d; border-radius:4px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    m_nav = new QListWidget(this);
    m_nav->setFixedWidth(140);
    m_nav->setStyleSheet(
        "QListWidget { background:#0d1117; border:none;"
        "  border-right:1px solid #30363d; outline:none; }"
        "QListWidget::item { color:#8b949e; padding:10px 16px; font-size:9pt; }"
        "QListWidget::item:selected { background:#1f6feb; color:#ffffff;"
        "  border-left:3px solid #58a6ff; }"
        "QListWidget::item:hover:!selected { background:#161b22; color:#c9d1d9; }");

    m_nav->addItem(tr("Colors"));
    m_nav->addItem(tr("Display"));
    m_nav->addItem(tr("AI"));
    m_nav->setCurrentRow(0);

    m_stack = new QStackedWidget(this);
    buildColorsPage();
    buildDisplayPage();
    buildAIPage();

    auto *btnReset  = new QPushButton(tr("Reset Defaults"));
    auto *btnCancel = new QPushButton(tr("Cancel"));
    auto *btnApply  = new QPushButton(tr("Apply"));
    btnApply->setStyleSheet(
        "QPushButton { background:#1f6feb; color:#fff; border:none;"
        "  border-radius:4px; padding:4px 16px; }"
        "QPushButton:hover { background:#388bfd; }");

    connect(btnReset, &QPushButton::clicked, this, [this]() {
        AppConfig::instance().resetToDefaults();
        AppConfig::instance().save();
        accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &ConfigDialog::reject);
    connect(btnApply,  &QPushButton::clicked, this, [this]() {
        AppConfig::instance().colors = m_working;
        if (m_showLongNamesCheck)
            AppConfig::instance().showLongMapNames = m_showLongNamesCheck->isChecked();
        AppConfig::instance().save();
        AppConfig::instance().colorsChanged();
        AppConfig::instance().displaySettingsChanged();
        saveAISettings();
        accept();
    });

    auto *btnRow = new QHBoxLayout;
    btnRow->setContentsMargins(8, 8, 8, 8);
    btnRow->addWidget(btnReset);
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addSpacing(8);
    btnRow->addWidget(btnApply);

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#30363d;");

    auto *split = new QHBoxLayout;
    split->setContentsMargins(0,0,0,0);
    split->setSpacing(0);
    split->addWidget(m_nav);
    split->addWidget(m_stack, 1);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    root->addLayout(split, 1);
    root->addWidget(sep);
    root->addLayout(btnRow);

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);

    restoreGeometry(QSettings("CT14", "RX14")
                    .value("dialogGeometry/ConfigDialog").toByteArray());
}

void ConfigDialog::closeEvent(QCloseEvent *event)
{
    QSettings("CT14", "RX14")
        .setValue("dialogGeometry/ConfigDialog", saveGeometry());
    QDialog::closeEvent(event);
}

QWidget *ConfigDialog::makeColorRow(const QString &label, QColor &colorRef)
{
    auto *row = new QWidget;
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(4, 2, 4, 2);
    lay->setSpacing(10);

    auto *lbl = new QLabel(label);
    lbl->setFixedWidth(180);

    auto *btn = makeSwatchBtn(colorRef);
    connect(btn, &QPushButton::clicked, this, [this, btn, &colorRef]() {
        QColor c = QColorDialog::getColor(colorRef, this, tr("Choose Color"));
        if (c.isValid()) {
            colorRef = c;
            applySwatchStyle(btn, c);
        }
    });

    lay->addWidget(lbl);
    lay->addWidget(btn);
    lay->addStretch();
    return row;
}

static QWidget *makeSectionNote(const QString &text)
{
    auto *lbl = new QLabel(text);
    lbl->setStyleSheet("color:#6e7681; font-size:7pt; padding:0 4px;");
    lbl->setWordWrap(true);
    return lbl;
}

// ── AI support-tier helpers ───────────────────────────────────────────────────
// tier 0 = green/best  1 = amber/good  2 = red/limited
static const QColor kTierColors[] = {
    QColor(0x3f, 0xb9, 0x50),   // green
    QColor(0xd2, 0x99, 0x22),   // amber
    QColor(0xf8, 0x51, 0x49),   // red
};

static QIcon tierIcon(int tier)
{
    QPixmap pm(12, 12);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(kTierColors[qBound(0, tier, 2)]);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 10, 10);
    return QIcon(pm);
}

static QWidget *makeLegendRow(int tier, const QString &text)
{
    QPixmap pm(12, 12);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(kTierColors[qBound(0, tier, 2)]);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 10, 10);

    auto *row  = new QWidget;
    auto *hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(0, 2, 0, 2);
    hbox->setSpacing(8);

    auto *dot = new QLabel;
    dot->setPixmap(pm);
    dot->setFixedSize(14, 14);
    dot->setAlignment(Qt::AlignCenter);

    auto *lbl = new QLabel(text);
    lbl->setStyleSheet("color:#8b949e; font-size:8pt;");

    hbox->addWidget(dot);
    hbox->addWidget(lbl);
    hbox->addStretch();
    return row;
}

void ConfigDialog::buildColorsPage()
{
    auto *content = new QWidget;
    content->setStyleSheet("background:#0d1117;");
    auto *vbox = new QVBoxLayout(content);
    vbox->setContentsMargins(14, 14, 14, 14);
    vbox->setSpacing(10);

    // ── Map Highlight Bands ──────────────────────────────────────────────────
    auto *grpBands = new QGroupBox(tr("Map Highlight Bands"));
    auto *bandsLay = new QVBoxLayout(grpBands);
    bandsLay->setContentsMargins(8, 4, 8, 8);
    bandsLay->setSpacing(2);
    bandsLay->addWidget(makeSectionNote(
        tr("Applied to map regions in the hex editor (cell tint + bar fill), "
           "2D waveform bands, and map overlay table.")));
    bandsLay->addSpacing(4);

    bandsLay->addWidget(makeColorRow(tr("Band 1 — Reds (maps 1, 6, 11...)"),    m_working.mapBand[0]));
    bandsLay->addWidget(makeColorRow(tr("Band 2 — Blues (maps 2, 7, 12...)"),   m_working.mapBand[1]));
    bandsLay->addWidget(makeColorRow(tr("Band 3 — Greens (maps 3, 8, 13...)"),  m_working.mapBand[2]));
    bandsLay->addWidget(makeColorRow(tr("Band 4 — Ambers (maps 4, 9, 14...)"),  m_working.mapBand[3]));
    bandsLay->addWidget(makeColorRow(tr("Band 5 — Purples (maps 5, 10, 15...)"),m_working.mapBand[4]));
    vbox->addWidget(grpBands);

    // ── Waveform Curve Colors ────────────────────────────────────────────────
    auto *grpWave = new QGroupBox(tr("2D View — Curve Colors"));
    auto *waveGrid = new QGridLayout(grpWave);
    waveGrid->setContentsMargins(8, 4, 8, 8);
    waveGrid->setHorizontalSpacing(8);
    waveGrid->setVerticalSpacing(2);

    waveGrid->addWidget(makeColorRow(tr("Curve 1 — Row 0 (front)"), m_working.waveRow[0]), 0, 0);
    waveGrid->addWidget(makeColorRow(tr("Curve 2 — Row 1"),        m_working.waveRow[1]), 0, 1);
    waveGrid->addWidget(makeColorRow(tr("Curve 3 — Row 2"),        m_working.waveRow[2]), 1, 0);
    waveGrid->addWidget(makeColorRow(tr("Curve 4 — Row 3"),        m_working.waveRow[3]), 1, 1);
    waveGrid->addWidget(makeColorRow(tr("Curve 5 — Row 4"),        m_working.waveRow[4]), 2, 0);
    waveGrid->addWidget(makeColorRow(tr("Curve 6 — Row 5"),        m_working.waveRow[5]), 2, 1);
    waveGrid->addWidget(makeColorRow(tr("Curve 7 — Row 6"),        m_working.waveRow[6]), 3, 0);
    waveGrid->addWidget(makeColorRow(tr("Curve 8 — Row 7 (back)"), m_working.waveRow[7]), 3, 1);
    vbox->addWidget(grpWave);

    // ── Hex Editor ──────────────────────────────────────────────────────────
    auto *grpHex = new QGroupBox(tr("Hex Editor"));
    auto *hexLay = new QVBoxLayout(grpHex);
    hexLay->setContentsMargins(8, 4, 8, 8);
    hexLay->setSpacing(2);
    hexLay->addWidget(makeColorRow(tr("Cell area background"),      m_working.hexBg));
    hexLay->addWidget(makeColorRow(tr("Normal byte text"),          m_working.hexText));
    hexLay->addWidget(makeColorRow(tr("Modified byte text / bar"),  m_working.hexModified));
    hexLay->addWidget(makeColorRow(tr("Selected cell fill"),        m_working.hexSelected));
    hexLay->addWidget(makeColorRow(tr("Offset column + sidebar"),   m_working.hexOffset));
    hexLay->addWidget(makeColorRow(tr("Column header background"),  m_working.hexHeaderBg));
    hexLay->addWidget(makeColorRow(tr("Column header text"),        m_working.hexHeaderText));
    hexLay->addWidget(makeColorRow(tr("Bar view — default bar"),    m_working.hexBarDefault));
    vbox->addWidget(grpHex);

    // ── Map Overlay ────────────────────────────────────────────────────────
    auto *grpMap = new QGroupBox(tr("Map Overlay"));
    auto *mapLay = new QVBoxLayout(grpMap);
    mapLay->setContentsMargins(8, 4, 8, 8);
    mapLay->setSpacing(2);
    mapLay->addWidget(makeColorRow(tr("Cell background (heat off)"),     m_working.mapCellBg));
    mapLay->addWidget(makeColorRow(tr("Cell text (heat off)"),           m_working.mapCellText));
    mapLay->addWidget(makeColorRow(tr("Modified cell text (heat off)"),  m_working.mapCellModified));
    mapLay->addWidget(makeColorRow(tr("Grid lines (heat off)"),          m_working.mapGridLine));
    mapLay->addWidget(makeColorRow(tr("X axis header background"),       m_working.mapAxisXBg));
    mapLay->addWidget(makeColorRow(tr("X axis header text"),             m_working.mapAxisXText));
    mapLay->addWidget(makeColorRow(tr("Y axis header background"),       m_working.mapAxisYBg));
    mapLay->addWidget(makeColorRow(tr("Y axis header text"),             m_working.mapAxisYText));
    vbox->addWidget(grpMap);

    // ── 2D Waveform View ────────────────────────────────────────────────────
    auto *grpWv = new QGroupBox(tr("2D Waveform View"));
    auto *wvLay = new QVBoxLayout(grpWv);
    wvLay->setContentsMargins(8, 4, 8, 8);
    wvLay->setSpacing(2);
    wvLay->addWidget(makeColorRow(tr("Plot background"),           m_working.waveBg));
    wvLay->addWidget(makeColorRow(tr("Major grid lines"),          m_working.waveGridMajor));
    wvLay->addWidget(makeColorRow(tr("Minor grid lines"),          m_working.waveGridMinor));
    wvLay->addWidget(makeColorRow(tr("ROM waveform line"),         m_working.waveLine));
    wvLay->addWidget(makeColorRow(tr("Overview / minimap strip"),  m_working.waveOverviewBg));
    vbox->addWidget(grpWv);

    // ── General UI ──────────────────────────────────────────────────────────
    auto *grpUi = new QGroupBox(tr("General UI"));
    auto *uiLay = new QVBoxLayout(grpUi);
    uiLay->setContentsMargins(8, 4, 8, 8);
    uiLay->setSpacing(2);
    uiLay->addWidget(makeSectionNote(
        tr("Main window backgrounds, panels, borders, and text.")));
    uiLay->addSpacing(4);
    uiLay->addWidget(makeColorRow(tr("Window / MDI background"),    m_working.uiBg));
    uiLay->addWidget(makeColorRow(tr("Panel / toolbar background"), m_working.uiPanel));
    uiLay->addWidget(makeColorRow(tr("Borders and dividers"),       m_working.uiBorder));
    uiLay->addWidget(makeColorRow(tr("Primary text"),               m_working.uiText));
    uiLay->addWidget(makeColorRow(tr("Secondary / dimmed text"),    m_working.uiTextDim));
    uiLay->addWidget(makeColorRow(tr("Accent (links, selection)"),  m_working.uiAccent));
    vbox->addWidget(grpUi);

    vbox->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_stack->addWidget(scroll);
}

void ConfigDialog::buildDisplayPage()
{
    auto *page = new QWidget;
    page->setStyleSheet("background:#0d1117;");
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(12);

    auto *mapGroup = new QGroupBox(tr("Map List"));
    mapGroup->setStyleSheet(
        "QGroupBox { color:#8b949e; border:1px solid #30363d; border-radius:4px;"
        "  margin-top:10px; font-size:8pt; padding-top:6px; }"
        "QGroupBox::title { subcontrol-origin:margin; left:8px; padding:0 4px; }");
    auto *mapLay = new QVBoxLayout(mapGroup);

    m_showLongNamesCheck = new QCheckBox(tr("Show long map names (description)"));
    m_showLongNamesCheck->setStyleSheet("color:#c9d1d9; font-size:9pt;");
    m_showLongNamesCheck->setChecked(AppConfig::instance().showLongMapNames);
    mapLay->addWidget(m_showLongNamesCheck);

    auto *hint = new QLabel(tr("When enabled, shows the full description (e.g. \"Kennfeld Momentenindizierter Motor\") instead of the short identifier (e.g. \"KFMIOP\")."));
    hint->setStyleSheet("color:#6e7681; font-size:8pt;");
    hint->setWordWrap(true);
    mapLay->addWidget(hint);

    lay->addWidget(mapGroup);
    lay->addStretch();
    m_stack->addWidget(page);
}

// ── XOR obfuscation (mirrors aiassistant.cpp) ─────────────────────────────────
static const quint8 OBF_KEY[] = {0xA3,0x7F,0x1C,0xD2,0x56,0x8B,0x4E,0x93,
                                  0xC1,0x2A,0xF7,0x65,0x3D,0xB8,0x0E,0x49};
static constexpr int OBF_LEN = sizeof(OBF_KEY);

static QByteArray obfuscate(const QByteArray &data)
{
    QByteArray out = data;
    for (int i = 0; i < out.size(); ++i)
        out[i] = out[i] ^ OBF_KEY[i % OBF_LEN];
    return out.toBase64();
}

static QByteArray deobfuscate(const QByteArray &data)
{
    QByteArray raw = QByteArray::fromBase64(data);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = raw[i] ^ OBF_KEY[i % OBF_LEN];
    return raw;
}

// ── AI Settings page ──────────────────────────────────────────────────────────

void ConfigDialog::buildAIPage()
{
    // Provider registry — same order and defaults as AIAssistant
    m_aiProviders = {
        //  name        label                         baseUrl                                                     defaultModel                isClaude  tier
        {"claude",   tr("Claude (Anthropic)"),   "",                                                          "claude-sonnet-4-6",         true,  0},
        {"openai",   tr("OpenAI (GPT-4o)"),      "https://api.openai.com/v1",                                 "gpt-4o",                    false, 1},
        {"qwen",     tr("Qwen (Alibaba)"),       "https://dashscope.aliyuncs.com/compatible-mode/v1",         "qwen-plus",                 false, 2},
        {"deepseek", tr("DeepSeek"),             "https://api.deepseek.com/v1",                               "deepseek-chat",             false, 2},
        {"gemini",   tr("Gemini (Google)"),      "https://generativelanguage.googleapis.com/v1beta/openai/",  "gemini-2.0-flash",          false, 2},
        {"groq",     tr("Groq"),                 "https://api.groq.com/openai/v1",                            "llama-3.3-70b-versatile",   false, 2},
        {"ollama",   tr("Ollama (local)"),       "http://localhost:11434/v1",                                 "llama3.2",                  false, 2},
        {"lmstudio", tr("LM Studio (local)"),   "http://localhost:1234/v1",                                  "local-model",               false, 2},
        {"custom",   tr("Custom OpenAI-compat"), "",                                                          "",                          false, 2},
    };

    auto *page = new QWidget;
    page->setStyleSheet("background:#0d1117;");

    auto *vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(24, 24, 24, 24);
    vbox->setSpacing(14);

    auto *hdr = new QLabel(tr("AI Provider Configuration"));
    hdr->setStyleSheet("color:#e6edf3; font-size:11pt; font-weight:bold;");
    vbox->addWidget(hdr);

    auto *desc = new QLabel(tr("Configure the AI provider used by the AI Assistant panel. "
                               "Settings are shared with the assistant."));
    desc->setStyleSheet("color:#8b949e; font-size:8pt;");
    desc->setWordWrap(true);
    vbox->addWidget(desc);

    auto *grp = new QGroupBox(tr("Provider Settings"));
    auto *form = new QFormLayout(grp);
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(10);
    form->setContentsMargins(12, 16, 12, 12);

    // Provider combo
    m_aiProviderCombo = new QComboBox;
    m_aiProviderCombo->setStyleSheet(
        "QComboBox { background:#21262d; color:#c9d1d9; border:1px solid #30363d; "
        "            border-radius:4px; padding:4px 8px; font-size:9pt; }"
        "QComboBox:hover { border-color:#58a6ff; }"
        "QComboBox QAbstractItemView { background:#21262d; color:#c9d1d9; "
        "  selection-background-color:#1f6feb; border:1px solid #30363d; }");
    for (int i = 0; i < m_aiProviders.size(); ++i)
        m_aiProviderCombo->addItem(tierIcon(m_aiProviders[i].tier), m_aiProviders[i].label);
    form->addRow(tr("Provider:"), m_aiProviderCombo);

    // Live support badge — updates when provider changes
    m_supportLabel = new QLabel;
    m_supportLabel->setTextFormat(Qt::RichText);
    m_supportLabel->setStyleSheet("font-size:8pt; padding:0 2px;");
    form->addRow("", m_supportLabel);

    // API Key
    m_aiKeyEdit = new QLineEdit;
    m_aiKeyEdit->setEchoMode(QLineEdit::Password);
    m_aiKeyEdit->setPlaceholderText("sk-…");
    m_aiKeyEdit->setStyleSheet(
        "QLineEdit { background:#21262d; color:#c9d1d9; border:1px solid #30363d; "
        "            border-radius:4px; padding:4px 8px; font-size:9pt; }"
        "QLineEdit:focus { border-color:#58a6ff; }");
    form->addRow(tr("API Key:"), m_aiKeyEdit);

    // Model
    m_aiModelEdit = new QLineEdit;
    m_aiModelEdit->setStyleSheet(m_aiKeyEdit->styleSheet());
    form->addRow(tr("Model:"), m_aiModelEdit);

    // Base URL
    m_aiUrlEdit = new QLineEdit;
    m_aiUrlEdit->setStyleSheet(m_aiKeyEdit->styleSheet());
    form->addRow(tr("Base URL:"), m_aiUrlEdit);

    vbox->addWidget(grp);

    // ── Support Level Legend ──────────────────────────────────────────────────
    auto *legendGrp = new QGroupBox(tr("Support Level Legend"));
    auto *legendLay = new QVBoxLayout(legendGrp);
    legendLay->setContentsMargins(12, 10, 12, 10);
    legendLay->setSpacing(4);
    legendLay->addWidget(makeLegendRow(0, tr("Best — native API, full tool-calling and streaming")));
    legendLay->addWidget(makeLegendRow(1, tr("Good — OpenAI-compatible, tool-calling available")));
    legendLay->addWidget(makeLegendRow(2, tr("Limited — compatibility varies, some features may not work")));
    vbox->addWidget(legendGrp);

    auto *hint = new QLabel(tr("API keys are stored locally with obfuscation. "
                               "Changes take effect when you click Apply."));
    hint->setStyleSheet("color:#6e7681; font-size:7pt;");
    hint->setWordWrap(true);
    vbox->addWidget(hint);

    vbox->addStretch();

    m_stack->addWidget(page);

    // Load saved provider index
    QSettings s("CT14", "romHEX14");
    s.beginGroup("AIAssistant");
    int savedIdx = s.value("provider", 0).toInt();
    s.endGroup();
    savedIdx = qBound(0, savedIdx, m_aiProviders.size() - 1);
    m_aiProviderCombo->setCurrentIndex(savedIdx);
    loadAIProviderFields(savedIdx);

    // When provider changes, reload fields from saved settings
    connect(m_aiProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConfigDialog::loadAIProviderFields);
}

void ConfigDialog::loadAIProviderFields(int index)
{
    if (index < 0 || index >= m_aiProviders.size()) return;
    const AIProviderEntry &p = m_aiProviders[index];

    QSettings s("CT14", "romHEX14");
    s.beginGroup("AIAssistant");
    QString key     = QString::fromUtf8(deobfuscate(s.value(p.name + "/apiKey").toByteArray()));
    QString model   = s.value(p.name + "/model", p.defaultModel).toString();
    QString baseUrl = s.value(p.name + "/baseUrl", p.baseUrl).toString();
    s.endGroup();

    m_aiKeyEdit->setText(key);
    m_aiModelEdit->setText(model.isEmpty() ? p.defaultModel : model);
    m_aiModelEdit->setPlaceholderText(p.defaultModel);
    m_aiUrlEdit->setText(baseUrl.isEmpty() ? p.baseUrl : baseUrl);
    m_aiUrlEdit->setPlaceholderText(p.isClaude ? "https://api.anthropic.com" : p.baseUrl);
    m_aiUrlEdit->setEnabled(!p.isClaude);

    // Update live support badge
    if (m_supportLabel) {
        static const char* const kHex[] = { "#3fb950", "#d29922", "#f85149" };
        const QString tierText =
            (p.tier == 0) ? tr("Best — native API, full tool-calling and streaming")
          : (p.tier == 1) ? tr("Good — OpenAI-compatible, tool-calling available")
                          : tr("Limited — compatibility varies, some features may not work");
        m_supportLabel->setText(
            QString("<span style='color:%1;'>&#9679;</span>&nbsp;%2")
                .arg(QLatin1String(kHex[p.tier])).arg(tierText));
    }
}

void ConfigDialog::saveAISettings()
{
    int idx = m_aiProviderCombo->currentIndex();
    if (idx < 0 || idx >= m_aiProviders.size()) return;
    const AIProviderEntry &p = m_aiProviders[idx];

    QSettings s("CT14", "romHEX14");
    s.beginGroup("AIAssistant");
    s.setValue("provider", idx);
    s.setValue(p.name + "/apiKey",  QString::fromLatin1(obfuscate(m_aiKeyEdit->text().trimmed().toUtf8())));
    s.setValue(p.name + "/model",   m_aiModelEdit->text().trimmed());
    s.setValue(p.name + "/baseUrl", m_aiUrlEdit->text().trimmed());
    s.endGroup();
}
