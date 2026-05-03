/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "projectpropertiesdlg.h"
#include "appconstants.h"
#include "vehicledb.h"
#include "io/ols/EcuAutoDetect.h"
#include <QCompleter>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFont>
#include <QDateTime>
#include <QFileInfo>
#include <QPainter>
#include <QSettings>
#include <QCloseEvent>

// Colored vertical side-bar that paints its label rotated 90°
class SideBar : public QWidget {
public:
    SideBar(const QString &text, const QColor &color, QWidget *parent = nullptr)
        : QWidget(parent), m_text(text), m_color(color)
    { setFixedWidth(26); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), m_color);
        p.save();
        p.translate(width() / 2.0, height() / 2.0);
        p.rotate(-90);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSize(8);
        p.setFont(f);
        p.drawText(QRect(-height()/2, -width()/2, height(), width()),
                   Qt::AlignCenter, m_text);
        p.restore();
    }
private:
    QString m_text;
    QColor  m_color;
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static const QString kDlgStyle =
    "QDialog          { background:#1c2128; color:#e6edf3; }"
    "QScrollArea       { background:#1c2128; border:none; }"
    "QWidget#inner     { background:#1c2128; }"
    "QLineEdit, QComboBox, QSpinBox {"
    "  background:#0d1117; color:#e6edf3; border:1px solid #30363d; "
    "  border-radius:3px; padding:2px 5px; }"
    "QLabel[role='hdr'] { color:#ffffff; font-weight:bold; font-size:9pt; }"
    "QLabel[role='key'] { color:#8b949e; }"
    "QLabel[role='val'] { color:#e6edf3; font-family:Consolas; font-size:9pt; }"
    "QGroupBox { border:none; margin:0; padding:0; }"
    "QPlainTextEdit { background:#0d1117; color:#e6edf3; border:1px solid #30363d; border-radius:3px; }"
    "QPushButton { background:#21262d; color:#e6edf3; border:1px solid #30363d; "
    "              border-radius:4px; padding:4px 14px; }"
    "QPushButton:hover   { background:#2d333b; }"
    "QPushButton[default='true'] { border-color:#388bfd; }";

QLineEdit *ProjectPropertiesDialog::field(const QString &placeholder)
{
    auto *e = new QLineEdit();
    if (!placeholder.isEmpty()) e->setPlaceholderText(placeholder);
    return e;
}

// A colored side-bar section like OLS
QWidget *ProjectPropertiesDialog::buildSection(const QString &label,
                                                const QColor  &color,
                                                QWidget       *content)
{
    auto *row = new QWidget();
    auto *hl  = new QHBoxLayout(row);
    hl->setContentsMargins(0, 4, 0, 4);
    hl->setSpacing(0);

    auto *bar = new SideBar(label, color);

    auto *contentWrap = new QWidget();
    contentWrap->setStyleSheet("background:#161b22; border-radius:0 3px 3px 0;");
    auto *wl = new QVBoxLayout(contentWrap);
    wl->setContentsMargins(8, 6, 8, 6);
    wl->addWidget(content);

    hl->addWidget(bar);
    hl->addWidget(contentWrap, 1);
    return row;
}

static QFormLayout *makeForm()
{
    auto *fl = new QFormLayout();
    fl->setLabelAlignment(Qt::AlignRight);
    fl->setSpacing(5);
    fl->setContentsMargins(0, 0, 0, 0);
    return fl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ProjectPropertiesDialog::ProjectPropertiesDialog(Project *project, QWidget *parent)
    : QDialog(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                     | Qt::WindowMaximizeButtonHint)
    , m_project(project)
{
    setWindowTitle(tr("Project Properties  —  %1").arg(project->fullTitle()));
    setMinimumSize(1100, 660);
    resize(1180, 720);
    setStyleSheet(kDlgStyle);

    // ── Build all field widgets first ─────────────────────────────────────────

    // Client
    m_clientName = field(tr("e.g. John Q. Public"));
    m_clientNr   = field(tr("e.g. 12345"));
    m_clientLic  = field(tr("e.g. WES-H4900"));
    m_vin        = field(tr("e.g. WAUZZZ8E35A2354"));

    // Vehicle
    m_vehicleType = new QComboBox();
    m_vehicleType->addItems({tr("Passenger car"), tr("Truck"), tr("Motorcycle"),
                              tr("Bus"), tr("Agricultural"), tr("Other")});
    m_brand = new QComboBox();
    m_brand->setEditable(true);
    m_brand->addItems(VehicleDatabase::instance().brands());
    m_brand->setCurrentIndex(-1);
    m_brand->lineEdit()->setPlaceholderText(tr("e.g. Porsche"));
    setupCompleter(m_brand);
    m_model = new QComboBox();
    m_model->setEditable(true);
    m_model->lineEdit()->setPlaceholderText(tr("e.g. Panamera S Hybrid"));
    setupCompleter(m_model);
    m_vehicleBuild = field(tr("e.g. 6"));
    m_vehicleModel = field(tr("e.g. 4S"));
    m_vehicleChar  = field(tr("e.g. R20"));
    m_year         = new QSpinBox();
    m_year->setRange(1980, 2040); m_year->setSpecialValueText(" "); m_year->setValue(2000);

    // ECU
    m_ecuUse = new QComboBox();
    m_ecuUse->addItems({tr("Engine"), tr("Gearbox"), tr("ABS"), tr("Airbag"),
                         tr("Instrument"), tr("Body"), tr("Other")});
    m_ecuProducer = new QComboBox();
    m_ecuProducer->setEditable(true);
    m_ecuProducer->setInsertPolicy(QComboBox::NoInsert);
    m_ecuProducer->lineEdit()->setPlaceholderText(tr("e.g. Bosch"));
    m_ecuProducer->addItem(QString());                // empty-default
    m_ecuProducer->addItems(ols::EcuAutoDetect::knownProducers());
    m_ecuProducer->setCurrentIndex(0);
    {
        auto *prodCompleter = new QCompleter(ols::EcuAutoDetect::knownProducers(), m_ecuProducer);
        prodCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        prodCompleter->setFilterMode(Qt::MatchContains);
        m_ecuProducer->setCompleter(prodCompleter);
    }

    m_ecuType = new QComboBox();
    m_ecuType->setEditable(true);
    m_ecuType->setInsertPolicy(QComboBox::NoInsert);
    m_ecuType->lineEdit()->setPlaceholderText(tr("e.g. MED17.1"));
    m_ecuType->addItem(QString());
    {
        // Merge the auto-detect ECU list with the historical VehicleDatabase
        // names so users keep the existing dataset coverage while gaining
        // every signature we can recognise.
        QStringList ecus = ols::EcuAutoDetect::knownEcus();
        const QStringList vdb = VehicleDatabase::instance().ecuNamesFlat();
        for (const QString &v : vdb) if (!ecus.contains(v)) ecus.append(v);
        ecus.sort();
        m_ecuType->addItems(ecus);
        auto *ecuCompleter = new QCompleter(ecus, m_ecuType);
        ecuCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        ecuCompleter->setFilterMode(Qt::MatchContains);
        m_ecuType->setCompleter(ecuCompleter);
    }
    m_ecuType->setCurrentIndex(0);
    m_ecuNrProd   = field(tr("e.g. 03G906016GN"));
    m_ecuNrEcu    = field(tr("e.g. 0281012113"));
    m_ecuSwNum    = field(tr("e.g. 518901"));
    m_ecuSwVer    = field(tr("e.g. 0003"));
    m_ecuProc     = new QLabel(); m_ecuProc->setProperty("role", "val");
    m_ecuChecksum = new QLabel(); m_ecuChecksum->setProperty("role", "val");

    // Engine
    m_engProducer  = field(tr("e.g. Deutz"));
    m_engCode      = field(tr("e.g. Z19DTH"));
    m_engType      = field(tr("e.g. Turbo-Diesel"));
    m_displacement = field(tr("e.g. 2.0"));
    m_outputPS = new QSpinBox(); m_outputPS->setRange(0,2000); m_outputPS->setSuffix(" PS");
    m_outputKW = new QSpinBox(); m_outputKW->setRange(0,1500); m_outputKW->setSuffix(" kW");
    m_maxTorque= new QSpinBox(); m_maxTorque->setRange(0,3000);m_maxTorque->setSuffix(" Nm");
    m_emission = new QComboBox(); m_emission->setEditable(true);
    m_emission->addItems({"Euro 1","Euro 2","Euro 3","Euro 4","Euro 5","Euro 6","Euro 6d"});
    m_transmission = new QComboBox(); m_transmission->setEditable(true);
    m_transmission->addItems({tr("Manual"),tr("Automatic"),tr("DSG/PDK"),tr("CVT"),tr("Switch gear")});

    // Project
    m_projectType = new QComboBox();
    m_projectType->addItems({tr("in development"),tr("released"),tr("archived"),tr("for sale"),tr("prototype")});
    m_mapLanguage = new QComboBox();
    m_mapLanguage->addItems({"English","Deutsch","Español","Français","Italiano","Português","中文","日本語"});

    // User defined
    for (int i = 0; i < 6; ++i) m_user[i] = new QLineEdit();

    // Notes
    m_notes = new QPlainTextEdit();
    m_notes->setPlaceholderText(tr("Notes…"));

    // ── Column 1: Client + Vehicle + User Defined ─────────────────────────────
    auto *col1 = new QVBoxLayout();
    col1->setSpacing(6);
    {
        auto *w = new QWidget(); auto *fl = makeForm(); w->setLayout(fl);
        fl->addRow(tr("Name:"),         m_clientName);
        fl->addRow(tr("Customer nr.:"), m_clientNr);
        fl->addRow(tr("Licence:"),      m_clientLic);
        fl->addRow(tr("VIN:"),          m_vin);
        col1->addWidget(buildSection(tr("Client"), QColor("#5a5a2a"), w));
    }
    {
        auto *w = new QWidget(); auto *fl = makeForm(); w->setLayout(fl);
        fl->addRow(tr("Type:"),           m_vehicleType);
        fl->addRow(tr("Producer:"),       m_brand);
        fl->addRow(tr("Series:"),         m_model);
        fl->addRow(tr("Build:"),          m_vehicleBuild);
        fl->addRow(tr("Model:"),          m_vehicleModel);
        fl->addRow(tr("Characteristic:"), m_vehicleChar);
        fl->addRow(tr("Model year:"),     m_year);
        col1->addWidget(buildSection(tr("Vehicle"), QColor("#8b1a1a"), w));
    }
    {
        auto *w = new QWidget(); auto *fl = makeForm(); w->setLayout(fl);
        for (int i = 0; i < 6; ++i)
            fl->addRow(tr("User %1:").arg(i+1), m_user[i]);
        col1->addWidget(buildSection(tr("User\ndefined"), QColor("#1a6a2a"), w));
    }
    col1->addStretch();

    // ── Column 2: ECU + Engine ────────────────────────────────────────────────
    auto *col2 = new QVBoxLayout();
    col2->setSpacing(6);
    {
        auto *w = new QWidget(); auto *fl = makeForm(); w->setLayout(fl);
        // ECU metadata is auto-populated every time the dialog opens by
        // loadFromProject() running the 73-detector chain + legacy
        // byte-heuristic fallback. No manual buttons — the user asked for
        // fully automatic behaviour.
        fl->addRow(tr("Use:"),           m_ecuUse);
        fl->addRow(tr("Producer:"),      m_ecuProducer);
        fl->addRow(tr("Build:"),         m_ecuType);
        fl->addRow(tr("ECU-Nr. Prod.:"), m_ecuNrProd);
        fl->addRow(tr("ECU-Nr. ECU.:"),  m_ecuNrEcu);
        fl->addRow(tr("Software:"),      m_ecuSwNum);
        fl->addRow(tr("...version:"),    m_ecuSwVer);
        fl->addRow(tr("Processor:"),     m_ecuProc);
        fl->addRow(tr("8-Bit sum:"),     m_ecuChecksum);
        col2->addWidget(buildSection(tr("ECU"), QColor("#1a4a8a"), w));
    }
    {
        auto *w = new QWidget(); auto *fl = makeForm(); w->setLayout(fl);
        auto *pwrRow = new QHBoxLayout();
        pwrRow->addWidget(m_outputPS); pwrRow->addWidget(m_outputKW); pwrRow->addStretch();
        fl->addRow(tr("Producer:"),     m_engProducer);
        fl->addRow(tr("Motorcode:"),    m_engCode);
        fl->addRow(tr("Type:"),         m_engType);
        fl->addRow(tr("Displacement:"), m_displacement);
        fl->addRow(tr("Output:"),       pwrRow);
        fl->addRow(tr("Max. Torque:"),  m_maxTorque);
        fl->addRow(tr("Emission:"),     m_emission);
        fl->addRow(tr("Transmission:"), m_transmission);
        col2->addWidget(buildSection(tr("Engine"), QColor("#1a3a5a"), w));
    }
    col2->addStretch();

    // ── Column 3: Project info + Notes ────────────────────────────────────────
    auto *col3 = new QVBoxLayout();
    col3->setSpacing(6);
    {
        auto *w = new QWidget(); auto *fl = makeForm(); w->setLayout(fl);

        auto *romFileLabel = new QLabel(QFileInfo(project->romPath).fileName().isEmpty()
            ? tr("—") : QFileInfo(project->romPath).fileName());
        romFileLabel->setProperty("role", "val");

        auto *fileLabel = new QLabel(QFileInfo(project->filePath).fileName().isEmpty()
            ? tr("(not saved yet)") : QFileInfo(project->filePath).fileName());
        fileLabel->setProperty("role", "val");

        auto *folderLabel = new QLabel(QFileInfo(project->filePath).absolutePath());
        folderLabel->setProperty("role", "val");
        folderLabel->setWordWrap(true);

        QString dtFmt = "dd/MM/yyyy  (hh:mm:ss)";
        auto *createdLbl = new QLabel(project->createdAt.isValid()
            ? project->createdAt.toString(dtFmt) : tr("—"));
        createdLbl->setProperty("role", "val");
        auto *createdByLbl = new QLabel(project->createdBy.isEmpty() ? tr("—") : project->createdBy);
        createdByLbl->setProperty("role", "val");
        auto *changedLbl = new QLabel(project->changedAt.isValid()
            ? project->changedAt.toString(dtFmt) : tr("—"));
        changedLbl->setProperty("role", "val");
        auto *changedByLbl = new QLabel(project->changedBy.isEmpty() ? tr("—") : project->changedBy);
        changedByLbl->setProperty("role", "val");

        // ROM size
        QString sizeStr = project->currentData.isEmpty() ? tr("—")
            : QString("%1 bytes  (%2 kB)").arg(project->currentData.size())
                .arg(project->currentData.size() / 1024.0, 0, 'f', 1);
        auto *sizeLbl = new QLabel(sizeStr);
        sizeLbl->setProperty("role", "val");

        fl->addRow(tr("ROM file:"),      romFileLabel);
        fl->addRow(tr("File:"),          fileLabel);
        fl->addRow(tr("Folder:"),        folderLabel);
        fl->addRow(tr("Created:"),       createdLbl);
        fl->addRow(tr("...by:"),         createdByLbl);
        fl->addRow(tr("Changed:"),       changedLbl);
        fl->addRow(tr("...by:"),         changedByLbl);
        fl->addRow(tr("Software size:"), sizeLbl);
        fl->addRow(tr("Project type:"),  m_projectType);
        fl->addRow(tr("Map lang.:"),     m_mapLanguage);
        col3->addWidget(buildSection(tr("Project"), QColor("#1a5a4a"), w));
    }
    {
        // Notes in its own mini-section
        auto *w = new QWidget(); auto *vl2 = new QVBoxLayout(w); vl2->setContentsMargins(0,0,0,0);
        vl2->addWidget(m_notes);
        col3->addWidget(buildSection(tr("Notes"), QColor("#3a3a4a"), w), 1);
    }

    // ── Assemble 3-column layout in a scroll area ─────────────────────────────
    auto *inner = new QWidget();
    inner->setObjectName("inner");
    auto *colsLayout = new QHBoxLayout(inner);
    colsLayout->setContentsMargins(10, 10, 10, 10);
    colsLayout->setSpacing(8);
    colsLayout->addLayout(col1, 1);
    colsLayout->addLayout(col2, 1);
    colsLayout->addLayout(col3, 1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inner);

    // ── Root layout ───────────────────────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 8);
    root->addWidget(scroll, 1);

    // Dialog footer — auto-fill button removed (fields populate automatically
    // in loadFromProject()). Only Finish + Cancel remain.
    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(10, 0, 10, 0);
    m_okBtn         = new QPushButton(tr("Finish"));
    m_okBtn->setDefault(true);
    m_okBtn->setStyleSheet("border-color:#388bfd; color:#58a6ff;");
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    btnRow->addStretch();
    btnRow->addWidget(m_okBtn);
    btnRow->addWidget(cancelBtn);
    root->addLayout(btnRow);

    connect(m_okBtn,   &QPushButton::clicked, this, &ProjectPropertiesDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_brand, &QComboBox::currentTextChanged,
            this, &ProjectPropertiesDialog::onBrandChanged);

    // ── Required-fields validation: Producer + Build must be filled ──────
    // Highlight in red while empty and gate the Finish button on them.
    connect(m_ecuProducer, &QComboBox::currentTextChanged,
            this, [this](const QString &){ validateRequiredFields(); });
    connect(m_ecuProducer->lineEdit(), &QLineEdit::textChanged,
            this, [this](const QString &){ validateRequiredFields(); });
    connect(m_ecuType, &QComboBox::currentTextChanged,
            this, [this](const QString &){ validateRequiredFields(); });
    connect(m_ecuType->lineEdit(), &QLineEdit::textChanged,
            this, [this](const QString &){ validateRequiredFields(); });

    // ── Cascade Build dropdown when Producer changes ─────────────────────
    // Picking "Bosch" filters the Build list to Bosch-only ECUs (EDC17,
    // MED17, MD1, MG1, EDC15, …) so the user doesn't see Continental SIDs
    // or Marelli MJDs in the same dropdown.  Empty Producer = full list.
    connect(m_ecuProducer, &QComboBox::currentTextChanged,
            this, [this](const QString &producer) {
                const QString prevBuild = m_ecuType->currentText();
                QStringList ecus = ols::EcuAutoDetect::knownEcusForProducer(producer.trimmed());
                if (producer.trimmed().isEmpty()) {
                    const QStringList vdb = VehicleDatabase::instance().ecuNamesFlat();
                    for (const QString &v : vdb) if (!ecus.contains(v)) ecus.append(v);
                    ecus.sort();
                }
                QSignalBlocker block(m_ecuType);
                m_ecuType->clear();
                m_ecuType->addItem(QString());
                m_ecuType->addItems(ecus);
                m_ecuType->setCurrentText(prevBuild);
                auto *cmp = new QCompleter(ecus, m_ecuType);
                cmp->setCaseSensitivity(Qt::CaseInsensitive);
                cmp->setFilterMode(Qt::MatchContains);
                m_ecuType->setCompleter(cmp);
                validateRequiredFields();
            });

    populate();
    validateRequiredFields();

    restoreGeometry(rx14::appSettings()
                    .value("dialogGeometry/ProjectPropertiesDlg").toByteArray());
}

void ProjectPropertiesDialog::closeEvent(QCloseEvent *event)
{
    rx14::appSettings()
        .setValue("dialogGeometry/ProjectPropertiesDlg", saveGeometry());
    QDialog::closeEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// Required-fields validation: Producer + Build (ECU type) must be filled
// before the dialog can be accepted.  Both combos are highlighted red while
// empty, and the Finish button stays disabled.
// ─────────────────────────────────────────────────────────────────────────────
void ProjectPropertiesDialog::validateRequiredFields()
{
    const bool prodEmpty  = m_ecuProducer && m_ecuProducer->currentText().trimmed().isEmpty();
    const bool buildEmpty = m_ecuType     && m_ecuType->currentText().trimmed().isEmpty();

    auto paint = [](QComboBox *cb, bool missing) {
        if (!cb) return;
        if (missing) {
            // Inline stylesheet on a QComboBox shadows the global
            // style.qss rules, so we MUST re-declare the popup palette
            // (matched to resources/style.qss:206-213) — otherwise the
            // dropdown ends up white-on-white as soon as we set any
            // QComboBox QSS rule on this widget.
            cb->setStyleSheet(QStringLiteral(
                "QComboBox {"
                " border: 2px solid #ff4040;"
                " background-color: rgba(255, 64, 64, 0.22);"
                " color: #f0f0f0;"
                " border-radius: 3px;"
                " padding: 2px 6px;"
                "}"
                "QComboBox QAbstractItemView {"
                " background: #161b22;"
                " color: #c9d1d9;"
                " border: 1px solid #30363d;"
                " selection-background-color: #1f6feb;"
                " selection-color: #ffffff;"
                " outline: none;"
                "}"));
            cb->setToolTip(QObject::tr("Required — pick an ECU before continuing"));
        } else {
            cb->setStyleSheet(QString());
            cb->setToolTip(QString());
        }
    };
    paint(m_ecuProducer, prodEmpty);
    paint(m_ecuType,     buildEmpty);

    if (m_okBtn) {
        const bool blocked = prodEmpty || buildEmpty;
        m_okBtn->setEnabled(!blocked);
        if (blocked) {
            m_okBtn->setToolTip(tr("Fill in Producer and Build (ECU) to continue"));
            m_okBtn->setStyleSheet("color: #888;");
        } else {
            m_okBtn->setToolTip(QString());
            m_okBtn->setStyleSheet("border-color:#388bfd; color:#58a6ff;");
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// VehicleDatabase cascading helpers
// ─────────────────────────────────────────────────────────────────────────────

void ProjectPropertiesDialog::setupCompleter(QComboBox *combo)
{
    auto *c = new QCompleter(combo->model(), combo);
    c->setCaseSensitivity(Qt::CaseInsensitive);
    c->setFilterMode(Qt::MatchContains);
    c->setCompletionMode(QCompleter::PopupCompletion);
    combo->setCompleter(c);
}

void ProjectPropertiesDialog::onBrandChanged(const QString &brand)
{
    if (brand.isEmpty())
        return;

    const auto &vdb = VehicleDatabase::instance();

    // Cascade: populate model combo with models for this brand
    const QString currentModel = m_model->currentText();
    m_model->clear();
    m_model->addItems(vdb.models(brand));
    if (!currentModel.isEmpty())
        m_model->setCurrentText(currentModel);
    setupCompleter(m_model);

    // Cascade: populate transmission combo with gearboxes for this brand
    const QString currentTrans = m_transmission->currentText();
    m_transmission->clear();
    m_transmission->addItems(vdb.gearboxes(brand));
    if (m_transmission->count() == 0)
        m_transmission->addItems({tr("Manual"), tr("Automatic"), tr("DSG/PDK"),
                                  tr("CVT"), tr("Switch gear")});
    if (!currentTrans.isEmpty())
        m_transmission->setCurrentText(currentTrans);
}

// ─────────────────────────────────────────────────────────────────────────────
// Auto-fill from ROM
// ─────────────────────────────────────────────────────────────────────────────

QString ProjectPropertiesDialog::detectProducer(const QString &fam) const
{
    if (fam.startsWith("MED") || fam.startsWith("ME") ||
        fam.startsWith("EDC") || fam.startsWith("DDE"))
        return "Bosch";
    if (fam.startsWith("SIM") || fam.startsWith("SDI"))
        return "Siemens / Continental";
    if (fam.startsWith("MM") || fam.startsWith("IAW"))
        return "Magneti Marelli";
    if (fam.startsWith("Delphi") || fam.startsWith("DCM"))
        return "Delphi";
    return {};
}

QString ProjectPropertiesDialog::detectProcessor(const QString &fam) const
{
    if (fam.startsWith("MED17") || fam.startsWith("ME17") ||
        fam.startsWith("EDC17") || fam.startsWith("MED9"))
        return "Infineon Tricore";
    if (fam.startsWith("ME7") || fam.startsWith("MED7") ||
        fam.startsWith("EDC16") || fam.startsWith("EDC15"))
        return "STMicroelectronics ST10";
    if (fam.startsWith("SIM"))
        return "ST10 / TriCore";
    return {};
}

QString ProjectPropertiesDialog::scanSwNumber() const
{
    const QByteArray &rom = m_project->currentData;
    if (rom.isEmpty()) return {};

    // Look for a run of 6-8 printable ASCII digits (typical Bosch SW number pattern)
    // Near the ECU identifier string
    for (int i = 0; i < rom.size() - 8; ++i) {
        bool allDigits = true;
        int len = 0;
        for (int j = i; j < qMin(i + 10, (int)rom.size()); ++j) {
            char c = rom[j];
            if (c >= '0' && c <= '9') ++len;
            else { allDigits = (len >= 6 && len <= 8); break; }
        }
        if (allDigits && len >= 6 && len <= 8) {
            // Validate: must be surrounded by non-digit or be at boundary
            bool leftOk  = (i == 0 || !(rom[i-1] >= '0' && rom[i-1] <= '9'));
            bool rightOk = (i + len >= rom.size() || !(rom[i+len] >= '0' && rom[i+len] <= '9'));
            if (leftOk && rightOk)
                return QString::fromLatin1(rom.mid(i, len));
        }
    }
    return {};
}

QString ProjectPropertiesDialog::computeChecksum() const
{
    const QByteArray &rom = m_project->currentData;
    if (rom.isEmpty()) return {};
    uint32_t sum = 0;
    for (unsigned char b : rom) sum += b;
    return QString("%1").arg(sum & 0xFFFF);
}

void ProjectPropertiesDialog::autoFill()
{
    if (m_project->currentData.isEmpty()) return;

    // Primary: use the same 150-signature table the DTC engine uses, so
    // the user-selected (Producer, Build) pair stays in sync with what the
    // wizard later treats as authoritative.  Fall back to the legacy
    // ecudetector.h heuristics only if EcuAutoDetect didn't fire.
    const ols::EcuDetectionResult ecu =
        ols::EcuAutoDetect::detect(m_project->currentData);

    QString fam, ecuName, producer, hwNumber, swNumber, swVersion, prodNo;
    if (ecu.ok) {
        fam       = ecu.family;
        ecuName   = ecu.ecuName;          // family + variant joined
        // Producer comes from the same family→producer map EcuAutoDetect uses
        // when applyToFields() runs — request it through the field-applier
        // by routing into a temporary string so we get the canonical name.
        ols::EcuMetadataFields tmp;
        QString tmpProd; tmp.producer = &tmpProd;
        ols::EcuAutoDetect::applyToFields(ecu, tmp, /*overwrite=*/true);
        producer  = tmpProd;
        hwNumber  = ecu.hwNumber;
        swNumber  = ecu.swNumber;
        swVersion = ecu.swVersion;
        prodNo    = ecu.productionNo;
    } else {
        // Legacy heuristic fallback (the old `ecudetector.h` path).
        const ECUDetection det = detectECU(m_project->currentData);
        fam     = det.family;
        ecuName = det.identifier;
        if (!fam.isEmpty()) producer = detectProducer(fam);
    }

    if (!ecuName.isEmpty() && m_ecuType->currentText().isEmpty())
        m_ecuType->setCurrentText(ecuName);
    if (!producer.isEmpty() && m_ecuProducer->currentText().isEmpty())
        m_ecuProducer->setCurrentText(producer);
    if (!fam.isEmpty()) {
        const QString proc = detectProcessor(fam);
        if (!proc.isEmpty()) m_ecuProc->setText(proc);
    }
    if (!hwNumber.isEmpty() && m_ecuNrEcu->text().isEmpty())
        m_ecuNrEcu->setText(hwNumber);
    if (!swNumber.isEmpty() && m_ecuSwNum->text().isEmpty())
        m_ecuSwNum->setText(swNumber);
    if (!swVersion.isEmpty() && m_ecuSwVer->text().isEmpty())
        m_ecuSwVer->setText(swVersion);
    if (!prodNo.isEmpty() && m_ecuNrProd->text().isEmpty())
        m_ecuNrProd->setText(prodNo);

    m_ecuUse->setCurrentText("Engine");

    if (m_ecuSwNum->text().isEmpty()) {
        const QString sw = scanSwNumber();
        if (!sw.isEmpty()) m_ecuSwNum->setText(sw);
    }

    m_ecuChecksum->setText(computeChecksum());
}

// ─────────────────────────────────────────────────────────────────────────────
// Auto-detect ECU (ols::EcuAutoDetect — 73-detector chain)
// ─────────────────────────────────────────────────────────────────────────────
// Runs the same detector chain as the Misc → Auto-detect ECU menu item but
// scoped to this project's ROM, and pushes the result into the dialog's
// editable fields. Existing non-empty user input is preserved (overwrite=
// false in EcuAutoDetect::applyToFields).
void ProjectPropertiesDialog::autoDetectEcu()
{
    if (m_project->currentData.isEmpty()) {
        QMessageBox::information(this, tr("Auto-detect ECU"),
                                 tr("This project has no ROM data loaded."));
        return;
    }

    // currentData is already decoded — don't double-decode via the filename
    // hint (see populate() for the full bug explanation).
    ols::EcuDetectionResult res =
        ols::EcuAutoDetect::detect(m_project->currentData);
    if (!res.ok) {
        QMessageBox::information(this, tr("Auto-detect ECU"),
            tr("No detector matched. The ROM does not contain any of the "
               "73 known ECU family anchors."));
        return;
    }

    // Stage results in scratch QStrings (mirroring the live UI values),
    // run applyToFields, then copy back to the widgets. This keeps the
    // overwrite policy in one place.
    QString producer    = m_ecuProducer->currentText().trimmed();
    QString ecuName     = m_ecuType->currentText().trimmed();
    QString hwNumber    = m_ecuNrEcu->text().trimmed();
    QString swNumber    = m_ecuSwNum->text().trimmed();
    QString swVersion   = m_ecuSwVer->text().trimmed();
    QString productionNo = m_ecuNrProd->text().trimmed();

    // 'slots' is reserved as a Qt keyword (Q_OBJECT) — use 'fields'
    ols::EcuMetadataFields fields;
    fields.producer     = &producer;
    fields.ecuName      = &ecuName;
    fields.hwNumber     = &hwNumber;
    fields.swNumber     = &swNumber;
    fields.swVersion    = &swVersion;
    fields.productionNo = &productionNo;
    // engineCode lives on the engine page (not edited here).
    const int filled =
        ols::EcuAutoDetect::applyToFields(res, fields, /*overwrite=*/false);

    if (!producer.isEmpty())     m_ecuProducer->setCurrentText(producer);
    if (!ecuName.isEmpty())      m_ecuType->setCurrentText(ecuName);
    if (!hwNumber.isEmpty())     m_ecuNrEcu->setText(hwNumber);
    if (!swNumber.isEmpty())     m_ecuSwNum->setText(swNumber);
    if (!swVersion.isEmpty())    m_ecuSwVer->setText(swVersion);
    if (!productionNo.isEmpty()) m_ecuNrProd->setText(productionNo);

    // Refresh the read-only processor/checksum labels too.
    const QString proc = detectProcessor(res.family);
    if (!proc.isEmpty()) m_ecuProc->setText(proc);
    m_ecuChecksum->setText(computeChecksum());

    QMessageBox::information(this, tr("Auto-detect ECU"),
        tr("ECU detected: %1\n%2 field(s) updated.").arg(res.family).arg(filled));
}

// ─────────────────────────────────────────────────────────────────────────────
// Populate / collect
// ─────────────────────────────────────────────────────────────────────────────

void ProjectPropertiesDialog::populate()
{
    Project *p = m_project;
    m_clientName->setText(p->clientName);
    m_clientNr->setText(p->clientNr);
    m_clientLic->setText(p->clientLicence);
    m_vin->setText(p->vin);

    m_vehicleType->setCurrentText(p->vehicleType.isEmpty() ? tr("Passenger car") : p->vehicleType);
    m_brand->setCurrentText(p->brand);
    m_model->setCurrentText(p->model);
    m_vehicleBuild->setText(p->vehicleBuild);
    m_vehicleModel->setText(p->vehicleModel);
    m_vehicleChar->setText(p->vehicleCharacteristic);
    if (p->year > 0) m_year->setValue(p->year);

    m_ecuUse->setCurrentText(p->ecuUse.isEmpty() ? "Engine" : p->ecuUse);
    m_ecuProducer->setCurrentText(p->ecuProducer);
    m_ecuType->setCurrentText(p->ecuType);
    m_ecuNrProd->setText(p->ecuNrProd);
    m_ecuNrEcu->setText(p->ecuNrEcu);
    m_ecuSwNum->setText(p->ecuSwNumber);
    m_ecuSwVer->setText(p->ecuSwVersion);
    m_ecuProc->setText(p->ecuProcessor);
    m_ecuChecksum->setText(p->ecuChecksum.isEmpty() ? computeChecksum() : p->ecuChecksum);

    m_engProducer->setText(p->engineProducer);
    m_engCode->setText(p->engineCode);
    m_engType->setText(p->engineType);
    m_displacement->setText(p->displacement);
    m_outputPS->setValue(p->outputPS);
    m_outputKW->setValue(p->outputKW);
    m_maxTorque->setValue(p->maxTorque);
    if (!p->emission.isEmpty())    m_emission->setCurrentText(p->emission);
    if (!p->transmission.isEmpty()) m_transmission->setCurrentText(p->transmission);

    m_projectType->setCurrentText(p->projectType.isEmpty() ? tr("in development") : p->projectType);
    m_mapLanguage->setCurrentText(p->mapLanguage.isEmpty() ? "English" : p->mapLanguage);

    for (int i = 0; i < 6; ++i) {
        const QString *fields[] = {&p->user1, &p->user2, &p->user3,
                                    &p->user4, &p->user5, &p->user6};
        m_user[i]->setText(*fields[i]);
    }

    m_notes->setPlainText(p->notes);

    // Always pre-populate ECU metadata from the ROM when the dialog opens.
    // The 73-detector ols::EcuAutoDetect chain extracts family, HW#, SW#,
    // SW version, production-no and engine-code from the Bosch P010 ID
    // block (mirrors the Python reference in
    // ecu_autodetect.py). We run
    // it UNCONDITIONALLY — `overwrite=false` guarantees user-typed values
    // aren't clobbered. This covers both the new-project flow (ROM freshly
    // loaded) AND the case where loadROMIntoProject ran but only partially
    // populated the project (e.g. older detector path).
    if (!p->currentData.isEmpty()) {
        // CRITICAL: do NOT call decodeRom() here. p->currentData is ALREADY
        // the decoded flat binary (parseROMFile() in loadROMIntoProject
        // strips any Intel-HEX/SREC envelope). decodeRom inspects the
        // filename extension; if .hex it tries to decode again, which
        // mangles the binary and P010 ends up at bogus offsets. Pass the
        // binary straight to detect().
        ols::EcuDetectionResult res =
            ols::EcuAutoDetect::detect(p->currentData);
        // Surface exactly what the 73-detector chain produced so users can
        // tell at a glance whether a blank field is a bug (detector returned
        // a value we failed to display) or a data limitation (detector
        // didn't recognise the ROM's ID block). Also logs the full result
        // to Console/stderr for when the user attaches a repro file.
        qInfo().noquote() << QStringLiteral(
            "[ECU auto-detect] ok=%1 family='%2' detector='%3'\n"
            "    hw='%4' sw='%5' ver='%6' prod='%7' eng='%8' P010@=%9")
            .arg(res.ok ? "true" : "false")
            .arg(res.family, res.detector,
                 res.hwNumber, res.swNumber, res.swVersion,
                 res.productionNo, res.engineCode)
            .arg(p->currentData.indexOf("P010"));
        if (res.ok) {
            // Even when overwrite=false would skip a filled slot, we want
            // to FORCE the detector's extraction to win for freshly-loaded
            // ROMs where the widget was only prefilled from populate()'s
            // p->ecuNrEcu (which may already be stale from a previous
            // detection pass). Pass empty scratch strings so applyToFields
            // unconditionally writes what it has, then copy to widgets.
            QString producer, ecuName, hwNumber, swNumber, swVersion,
                    productionNo, engineCode;
            ols::EcuMetadataFields fields;
            fields.producer     = &producer;
            fields.ecuName      = &ecuName;
            fields.hwNumber     = &hwNumber;
            fields.swNumber     = &swNumber;
            fields.swVersion    = &swVersion;
            fields.productionNo = &productionNo;
            fields.engineCode   = &engineCode;
            ols::EcuAutoDetect::applyToFields(res, fields, /*overwrite=*/true);
            if (!producer.isEmpty())     m_ecuProducer->setCurrentText(producer);
            if (!ecuName.isEmpty())      m_ecuType->setCurrentText(ecuName);
            if (!hwNumber.isEmpty())     m_ecuNrEcu->setText(hwNumber);
            if (!swNumber.isEmpty())     m_ecuSwNum->setText(swNumber);
            if (!swVersion.isEmpty())    m_ecuSwVer->setText(swVersion);
            if (!productionNo.isEmpty()) m_ecuNrProd->setText(productionNo);
            if (!engineCode.isEmpty())   m_engCode->setText(engineCode);
            const QString proc = detectProcessor(res.family);
            if (!proc.isEmpty()) m_ecuProc->setText(proc);
            m_ecuChecksum->setText(computeChecksum());
            // Diagnostic tooltip on the family field so the user can hover
            // to see the raw ID block the detector extracted (invaluable
            // for "why isn't this filling?" troubleshooting).
            const QString tip = tr(
                "Detector: %1\nFamily: %2\nHW: %3\nSW: %4\n"
                "Version: %5\nProduction: %6\nEngine code: %7\n"
                "ID block offset: %8\nRaw ID: %9")
                .arg(res.detector, res.family, res.hwNumber, res.swNumber,
                     res.swVersion, res.productionNo, res.engineCode)
                .arg(res.idBlockOffset, 0, 16)
                .arg(QString::fromLatin1(res.rawIdBlock.left(200)));
            m_ecuType->setToolTip(tip);
            m_ecuProducer->setToolTip(tip);
            m_ecuNrEcu->setToolTip(tip);
            m_ecuSwNum->setToolTip(tip);
            m_ecuSwVer->setToolTip(tip);
            m_ecuNrProd->setToolTip(tip);
            m_engCode->setToolTip(tip);
        } else {
            qWarning() << "[ECU auto-detect] 73-detector chain did NOT match"
                       << "on" << p->romPath
                       << "size=" << p->currentData.size();
        }
    }
    // (User requested: no legacy byte-heuristic fallback. Only the
    //  73-detector chain populates ECU fields.  If the chain misses a
    //  field it stays empty — better to be honest than to guess.)
    // Persist anything we just auto-filled back onto the Project so the
    // .rx14proj saved at the end of new-project carries the ECU metadata
    // and subsequent opens don't need to re-run detection.
    if (m_project) {
        if (!m_ecuProducer->currentText().trimmed().isEmpty())
            m_project->ecuProducer = m_ecuProducer->currentText().trimmed();
        if (!m_ecuType->currentText().trimmed().isEmpty())
            m_project->ecuType = m_ecuType->currentText().trimmed();
        if (!m_ecuNrEcu->text().trimmed().isEmpty())
            m_project->ecuNrEcu = m_ecuNrEcu->text().trimmed();
        if (!m_ecuSwNum->text().trimmed().isEmpty())
            m_project->ecuSwNumber = m_ecuSwNum->text().trimmed();
        if (!m_ecuSwVer->text().trimmed().isEmpty())
            m_project->ecuSwVersion = m_ecuSwVer->text().trimmed();
        if (!m_ecuNrProd->text().trimmed().isEmpty())
            m_project->ecuNrProd = m_ecuNrProd->text().trimmed();
        if (!m_engCode->text().trimmed().isEmpty())
            m_project->engineCode = m_engCode->text().trimmed();
        if (!m_ecuProc->text().trimmed().isEmpty())
            m_project->ecuProcessor = m_ecuProc->text().trimmed();
    }
}

void ProjectPropertiesDialog::collect()
{
    Project *p = m_project;
    p->clientName   = m_clientName->text().trimmed();
    p->clientNr     = m_clientNr->text().trimmed();
    p->clientLicence = m_clientLic->text().trimmed();
    p->vin          = m_vin->text().trimmed();

    p->vehicleType          = m_vehicleType->currentText();
    p->brand                = m_brand->currentText().trimmed();
    p->model                = m_model->currentText().trimmed();
    p->vehicleBuild         = m_vehicleBuild->text().trimmed();
    p->vehicleModel         = m_vehicleModel->text().trimmed();
    p->vehicleCharacteristic = m_vehicleChar->text().trimmed();
    p->year = (m_year->value() > 2000) ? m_year->value() : 0;

    p->ecuUse       = m_ecuUse->currentText();
    p->ecuProducer  = m_ecuProducer->currentText().trimmed();
    p->ecuType      = m_ecuType->currentText().trimmed();
    p->ecuNrProd    = m_ecuNrProd->text().trimmed();
    p->ecuNrEcu     = m_ecuNrEcu->text().trimmed();
    p->ecuSwNumber  = m_ecuSwNum->text().trimmed();
    p->ecuSwVersion = m_ecuSwVer->text().trimmed();
    p->ecuProcessor = m_ecuProc->text();
    p->ecuChecksum  = m_ecuChecksum->text();

    p->engineProducer = m_engProducer->text().trimmed();
    p->engineCode     = m_engCode->text().trimmed();
    p->engineType     = m_engType->text().trimmed();
    p->displacement   = m_displacement->text().trimmed();
    p->outputPS       = m_outputPS->value();
    p->outputKW       = m_outputKW->value();
    p->maxTorque      = m_maxTorque->value();
    p->emission       = m_emission->currentText();
    p->transmission   = m_transmission->currentText();

    p->projectType  = m_projectType->currentText();
    p->mapLanguage  = m_mapLanguage->currentText();

    p->user1 = m_user[0]->text(); p->user2 = m_user[1]->text();
    p->user3 = m_user[2]->text(); p->user4 = m_user[3]->text();
    p->user5 = m_user[4]->text(); p->user6 = m_user[5]->text();

    p->notes = m_notes->toPlainText();

    // Update changed timestamp
    p->changedAt = QDateTime::currentDateTime();
    if (p->createdAt.isNull()) p->createdAt = p->changedAt;

    p->modified = true;
    emit p->dataChanged();
}

void ProjectPropertiesDialog::accept()
{
    collect();
    QDialog::accept();
}
