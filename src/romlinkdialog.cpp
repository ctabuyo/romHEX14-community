/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "romlinkdialog.h"
#include "appconstants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QFile>
#include <QDialogButtonBox>
#include <QSettings>
#include <QCloseEvent>

RomLinkDialog::RomLinkDialog(Project *project, QWidget *parent)
    : QDialog(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
    , m_project(project)
{
    setWindowTitle(tr("Link ROM to Project — %1").arg(project->fullTitle()));
    setMinimumSize(700, 500);
    resize(780, 540);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    m_stack = new QStackedWidget();
    root->addWidget(m_stack, 1);

    // ── Button row ─────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *btnCancel = new QPushButton(tr("Cancel"));
    m_btnNext       = new QPushButton(tr("Link"));
    m_btnNext->setDefault(true);
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(m_btnNext);
    root->addLayout(btnRow);

    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnNext, &QPushButton::clicked, this, [this]() {
        if (m_stack->currentIndex() == 0) startLink();
        else accept();
    });

    buildPage0();

    m_linker = new RomLinker(this);
    connect(m_linker, &RomLinker::progress, this, &RomLinkDialog::onProgress);
    connect(m_linker, &RomLinker::finished, this, &RomLinkDialog::onFinished);

    restoreGeometry(rx14::appSettings()
                    .value("dialogGeometry/RomLinkDialog").toByteArray());
}

void RomLinkDialog::closeEvent(QCloseEvent *event)
{
    rx14::appSettings()
        .setValue("dialogGeometry/RomLinkDialog", saveGeometry());
    QDialog::closeEvent(event);
}

void RomLinkDialog::buildPage0()
{
    auto *page = new QWidget();
    auto *vl   = new QVBoxLayout(page);
    vl->setSpacing(8);

    // Info label
    auto *info = new QLabel(tr(
        "<b>Step 1 — Select a ROM file to link.</b><br>"
        "The linker will locate each A2L map in the target ROM automatically."));
    info->setWordWrap(true);
    info->setStyleSheet("color:#c9d1d9;");
    vl->addWidget(info);

    // Form
    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);

    auto *pathRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText(tr("Path to .bin / .hex ROM file…"));
    auto *browseBtn = new QPushButton(tr("Browse…"));
    browseBtn->setFixedWidth(80);
    pathRow->addWidget(m_pathEdit);
    pathRow->addWidget(browseBtn);
    connect(browseBtn, &QPushButton::clicked, this, &RomLinkDialog::browseFile);

    m_labelEdit = new QLineEdit();
    m_labelEdit->setPlaceholderText(tr("e.g. Panamera ORI"));

    m_refCheck = new QCheckBox(tr("Mark as reference / factory ROM"));
    m_refCheck->setStyleSheet("color:#8b949e;");

    form->addRow(tr("ROM file:"),  pathRow);
    form->addRow(tr("Label:"),     m_labelEdit);
    form->addRow(QString(),        m_refCheck);
    vl->addLayout(form);

    // Progress
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(true);
    m_progress->hide();
    vl->addWidget(m_progress);

    m_log = new QPlainTextEdit();
    m_log->setReadOnly(true);
    m_log->setFont(QFont("Consolas", 8));
    m_log->setStyleSheet("background:#0d1117; color:#8b949e; border:1px solid #21262d;");
    m_log->hide();
    vl->addWidget(m_log, 1);

    vl->addStretch();
    m_stack->addWidget(page);
}

void RomLinkDialog::buildPage1(const RomLinkSession &session)
{
    // Remove old page1 if re-running
    if (m_stack->count() > 1) {
        auto *old = m_stack->widget(1);
        m_stack->removeWidget(old);
        old->deleteLater();
    }

    auto *page = new QWidget();
    auto *vl   = new QVBoxLayout(page);
    vl->setSpacing(8);

    m_summaryL = new QLabel();
    m_summaryL->setWordWrap(true);
    m_summaryL->setStyleSheet("color:#c9d1d9;");
    vl->addWidget(m_summaryL);

    m_table = new QTableWidget();
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Map"), tr("Status"), tr("Confidence"), tr("Address in Target")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setRowCount(session.results.size());
    m_table->setStyleSheet(
        "QTableWidget { background:#0d1117; color:#c9d1d9; gridline-color:#21262d; }"
        "QTableWidget::item:alternate { background:#161b22; }"
        "QHeaderView::section { background:#161b22; color:#8b949e; border:none; padding:4px; }");

    for (int i = 0; i < session.results.size(); ++i) {
        const auto &r = session.results[i];
        auto *nameItem   = new QTableWidgetItem(r.mapName);
        QString statusStr;
        QColor  statusCol;
        switch (r.status) {
        case MapLinkResult::Exact:
            statusStr = tr("Exact");    statusCol = QColor(0x3f, 0xb9, 0x50); break;
        case MapLinkResult::Fuzzy:
            statusStr = tr("Fuzzy");    statusCol = QColor(0xd2, 0x9a, 0x22); break;
        default:
            statusStr = tr("Not found"); statusCol = QColor(0xf8, 0x51, 0x49); break;
        }
        auto *statusItem = new QTableWidgetItem(statusStr);
        statusItem->setForeground(statusCol);
        auto *confItem   = new QTableWidgetItem(QString("%1%").arg(r.confidence));
        auto *addrItem   = new QTableWidgetItem(
            r.status != MapLinkResult::NotFound
            ? QString("0x%1").arg(r.linkedAddress, 8, 16, QChar('0')).toUpper()
            : tr("—"));

        m_table->setItem(i, 0, nameItem);
        m_table->setItem(i, 1, statusItem);
        m_table->setItem(i, 2, confItem);
        m_table->setItem(i, 3, addrItem);
    }
    vl->addWidget(m_table, 1);
    m_stack->addWidget(page);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void RomLinkDialog::browseFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select ROM File"),
        QString(), tr("ROM files (*.bin *.hex *.rom *.ori *.mpc);;All files (*)"));
    if (!path.isEmpty()) {
        m_pathEdit->setText(path);
        if (m_labelEdit->text().trimmed().isEmpty()) {
            QFileInfo fi(path);
            m_labelEdit->setText(fi.baseName());
        }
    }
}

void RomLinkDialog::startLink()
{
    QString path  = m_pathEdit->text().trimmed();
    QString label = m_labelEdit->text().trimmed();

    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("No file"), tr("Please select a ROM file first."));
        return;
    }
    if (label.isEmpty()) label = QFileInfo(path).baseName();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Cannot open file:\n%1").arg(path));
        return;
    }
    QByteArray targetRom = f.readAll();
    f.close();

    if (m_project->currentData.isEmpty()) {
        QMessageBox::warning(this, tr("No reference ROM"),
            tr("The project has no ROM data loaded yet.\n"
               "Please load the reference HEX file into the project first."));
        return;
    }

    // Prepare result struct
    m_result.label       = label;
    m_result.filePath    = path;
    m_result.data        = targetRom;
    m_result.importedAt  = QDateTime::currentDateTime();
    m_result.isReference = m_refCheck->isChecked();

    // Show progress UI
    m_progress->setValue(0);
    m_progress->show();
    m_log->clear();
    m_log->show();
    m_btnNext->setEnabled(false);

    m_linker->linkAsync(m_project->currentData, targetRom,
                        m_project->maps, label);
}

void RomLinkDialog::onProgress(const QString &msg, int pct)
{
    m_progress->setValue(pct);
    m_log->appendPlainText(msg);
}

void RomLinkDialog::onFinished(const RomLinkSession &session)
{
    m_session = session;
    applyResult(session);
    buildPage1(session);
    m_stack->setCurrentIndex(1);

    m_summaryL->setText(tr(
        "<b>Linking complete.</b>  Matched <b>%1</b> of <b>%2</b> maps.  "
        "Dominant address delta: <b>%3%4</b>  "
        "<span style='color:#8b949e;'>(click Accept to add this ROM to the project)</span>")
        .arg(session.matchedCount)
        .arg(session.totalCount)
        .arg(session.dominantDelta >= 0 ? "+" : "")
        .arg(session.dominantDelta));

    m_btnNext->setText(tr("Accept"));
    m_btnNext->setEnabled(true);
}

void RomLinkDialog::applyResult(const RomLinkSession &session)
{
    for (const auto &r : session.results) {
        // Always record confidence for every map that was processed
        if (r.status != MapLinkResult::NotFound)
            m_result.mapConfidence[r.mapName] = r.confidence;
        // Commit maps with confidence ≥ 50 (neighbour-interpolated or better).
        // 40% maps are disagreeing-neighbour guesses — excluded to avoid wrong data.
        if (r.status != MapLinkResult::NotFound && r.confidence >= 50)
            m_result.mapOffsets[r.mapName] = r.linkedAddress;
    }
}


