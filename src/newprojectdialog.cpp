/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "newprojectdialog.h"
#include "vehicledb.h"
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QDate>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFile>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QCompleter>
#include <QSettings>
#include <QCloseEvent>

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Project"));
    setMinimumWidth(480);

    auto &vdb = VehicleDatabase::instance();

    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);

    // ── ROM file ──────────────────────────────────────────────────────
    auto *fileBox = new QGroupBox(tr("ROM File"));
    auto *fileLay = new QHBoxLayout(fileBox);
    m_romPath = new QLineEdit();
    m_romPath->setPlaceholderText(tr("Select ROM binary file…"));
    m_romPath->setReadOnly(true);
    auto *btnBrowse = new QPushButton(tr("Browse…"));
    fileLay->addWidget(m_romPath);
    fileLay->addWidget(btnBrowse);
    root->addWidget(fileBox);

    // ── Vehicle / ECU info ────────────────────────────────────────────
    auto *infoBox = new QGroupBox(tr("Vehicle / ECU Information"));
    auto *form    = new QFormLayout(infoBox);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);

    // Brand — populated from VehicleDatabase
    m_brand = new QComboBox();
    m_brand->setEditable(true);
    m_brand->addItem(QString());               // empty first item
    m_brand->addItems(vdb.brands());
    m_brand->lineEdit()->setPlaceholderText(tr("Select or type brand…"));
    setupCompleter(m_brand);
    form->addRow(tr("Brand:"), m_brand);

    // Model — cascaded from brand selection
    m_model = new QComboBox();
    m_model->setEditable(true);
    m_model->lineEdit()->setPlaceholderText(tr("e.g.  911 Turbo S,  Golf R,  M3 Competition"));
    setupCompleter(m_model);
    form->addRow(tr("Model:"), m_model);

    // Engine — cascaded from brand + model
    m_engine = new QComboBox();
    m_engine->setEditable(true);
    m_engine->lineEdit()->setPlaceholderText(tr("e.g.  2.0 TDI,  3.0 V6 TFSI"));
    setupCompleter(m_engine);
    form->addRow(tr("Engine:"), m_engine);

    // ECU Type — flat list of all known ECUs
    m_ecuType = new QComboBox();
    m_ecuType->setEditable(true);
    m_ecuType->addItem(QString());
    m_ecuType->addItems(vdb.ecuNamesFlat());
    m_ecuType->lineEdit()->setPlaceholderText(tr("e.g.  Bosch ME7.8,  Siemens SDI7,  Bosch EDC17"));
    setupCompleter(m_ecuType);
    form->addRow(tr("ECU Type:"), m_ecuType);

    // ECU Software Number
    m_swNumber = new QLineEdit();
    m_swNumber->setPlaceholderText(tr("e.g. 06E906023A, 0261S04567"));
    form->addRow(tr("SW Number:"), m_swNumber);

    // Transmission — cascaded from brand
    m_transmission = new QComboBox();
    m_transmission->setEditable(true);
    m_transmission->lineEdit()->setPlaceholderText(tr("e.g.  6-speed manual,  DSG 7,  ZF 8HP"));
    setupCompleter(m_transmission);
    form->addRow(tr("Transmission:"), m_transmission);

    m_displacement = new QLineEdit();
    m_displacement->setPlaceholderText(tr("e.g.  3.8L,  2000cc,  1984cc"));
    form->addRow(tr("Displacement:"), m_displacement);

    m_year = new QSpinBox();
    m_year->setRange(0, 2035);
    m_year->setValue(QDate::currentDate().year());
    m_year->setSpecialValueText("—");
    m_year->setMinimum(0);
    form->addRow(tr("Year:"), m_year);

    m_notes = new QPlainTextEdit();
    m_notes->setPlaceholderText(tr("Optional notes (tune purpose, owner, mods…)"));
    m_notes->setMaximumHeight(64);
    form->addRow(tr("Notes:"), m_notes);

    root->addWidget(infoBox);

    // ── Buttons ───────────────────────────────────────────────────────
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_btnOk = btns->button(QDialogButtonBox::Ok);
    m_btnOk->setText(tr("Create Project"));
    m_btnOk->setEnabled(false);
    root->addWidget(btns);

    // ── Connections ───────────────────────────────────────────────────
    connect(btnBrowse, &QPushButton::clicked, this, &NewProjectDialog::browseROM);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_brand, &QComboBox::currentTextChanged,
            this, &NewProjectDialog::onBrandChanged);
    connect(m_model, &QComboBox::currentTextChanged,
            this, &NewProjectDialog::onModelChanged);

    restoreGeometry(QSettings("CT14", "RX14")
                    .value("dialogGeometry/NewProjectDialog").toByteArray());
}

void NewProjectDialog::closeEvent(QCloseEvent *event)
{
    QSettings("CT14", "RX14")
        .setValue("dialogGeometry/NewProjectDialog", saveGeometry());
    QDialog::closeEvent(event);
}

// ── Cascade: brand changed → refresh model, engine, transmission ─────
void NewProjectDialog::onBrandChanged(const QString &brand)
{
    auto &vdb = VehicleDatabase::instance();
    const QString trimmed = brand.trimmed();

    // Refresh model combo
    const QString prevModel = m_model->currentText();
    m_model->blockSignals(true);
    m_model->clear();
    m_model->addItem(QString());
    m_model->addItems(vdb.models(trimmed));
    m_model->setCurrentText(prevModel);
    m_model->blockSignals(false);
    setupCompleter(m_model);

    // Refresh transmission combo
    const QString prevTrans = m_transmission->currentText();
    m_transmission->blockSignals(true);
    m_transmission->clear();
    m_transmission->addItem(QString());
    m_transmission->addItems(vdb.gearboxes(trimmed));
    m_transmission->setCurrentText(prevTrans);
    m_transmission->blockSignals(false);
    setupCompleter(m_transmission);

    // Also refresh engine for current model
    onModelChanged(m_model->currentText());
}

// ── Cascade: model changed → refresh engine ──────────────────────────
void NewProjectDialog::onModelChanged(const QString &model)
{
    auto &vdb = VehicleDatabase::instance();
    const QString brand = m_brand->currentText().trimmed();
    const QString trimmed = model.trimmed();

    const QString prevEngine = m_engine->currentText();
    m_engine->blockSignals(true);
    m_engine->clear();
    m_engine->addItem(QString());
    m_engine->addItems(vdb.engines(brand, trimmed));
    m_engine->setCurrentText(prevEngine);
    m_engine->blockSignals(false);
    setupCompleter(m_engine);
}

// ── Helper: attach a case-insensitive contains-mode completer ────────
void NewProjectDialog::setupCompleter(QComboBox *combo)
{
    auto *completer = new QCompleter(combo->model(), combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    combo->setCompleter(completer);
}

// ── Browse ROM ───────────────────────────────────────────────────────
void NewProjectDialog::browseROM()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select ROM File"), {},
        tr("ROM Files (*.bin *.rom *.hex *.dat *.ori *.mod *.full *.mpc);;All Files (*)"));
    if (path.isEmpty()) return;
    m_romPath->setText(path);
    m_btnOk->setEnabled(true);

    if (m_model->currentText().isEmpty()) {
        QString base = QFileInfo(path).baseName();
        base.remove(QRegularExpression(R"(_?(ori|stock|mod|original|backup|bak)$)",
                                       QRegularExpression::CaseInsensitiveOption));
        m_model->setCurrentText(base);
    }

    // ── Auto-detect ECU type and SW number from ROM data ─────────────
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QByteArray romData = f.readAll();
    f.close();

    // Auto-detect ECU type
    if (m_ecuType->currentText().isEmpty()) {
        const QString ecu = VehicleDatabase::instance().detectEcuFromRom(romData);
        if (!ecu.isEmpty())
            m_ecuType->setCurrentText(ecu);
    }

    // Auto-detect SW number — scan first 64 KB for common patterns
    if (m_swNumber->text().isEmpty()) {
        const int scanLen = qMin(romData.size(), 65536);
        const QString header = QString::fromLatin1(romData.constData(), scanLen);

        // Look near known markers: "SW:", "HW:", "Cal:", "EPK"
        static const QRegularExpression markerRe(
            R"((?:SW|HW|Cal|EPK)[:\s]*(\d{3}[A-Z]\d{5,6}|\d{10}|\w{3}-\w{3}-\w{3}-\w{2}))",
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch m = markerRe.match(header);
        if (m.hasMatch()) {
            m_swNumber->setText(m.captured(1));
        } else {
            // Fallback: Bosch-style part numbers (e.g. 06E906023A, 0261S04567)
            static const QRegularExpression boschRe(
                R"(\b(\d{3}[A-Z]\d{5,6})\b)");
            QRegularExpressionMatch bm = boschRe.match(header);
            if (bm.hasMatch())
                m_swNumber->setText(bm.captured(1));
        }
    }
}

QString NewProjectDialog::romPath() const { return m_romPath->text(); }

void NewProjectDialog::applyTo(Project *project) const
{
    project->brand        = m_brand->currentText().trimmed();
    project->model        = m_model->currentText().trimmed();
    project->ecuType      = m_ecuType->currentText().trimmed();
    project->ecuSwNumber  = m_swNumber->text().trimmed();
    project->displacement = m_displacement->text().trimmed();
    project->year         = m_year->value();
    project->notes        = m_notes->toPlainText().trimmed();
    project->transmission = m_transmission->currentText().trimmed();

    // Store engine info if the user typed/selected one
    const QString eng = m_engine->currentText().trimmed();
    if (!eng.isEmpty())
        project->engineType = eng;

    QString n = project->brand;
    if (!project->model.isEmpty())
        n += (n.isEmpty() ? "" : " ") + project->model;
    project->name = n.isEmpty() ? QFileInfo(m_romPath->text()).baseName() : n;
}
