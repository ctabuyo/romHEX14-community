/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mappackdlg.h"
#include "romdata.h"
#include "uiwidgets.h"
#include <QSet>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QApplication>

static QString safeFileName(const QString &label)
{
    QString s = label;
    // Strip characters illegal in Windows filenames
    static const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (QChar c : illegal)
        s.remove(c);
    // Remove control characters
    s.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1f]")));
    // Trim leading/trailing dots and spaces (Windows disallows them)
    while (!s.isEmpty() && (s.front() == '.' || s.front() == ' '))
        s.remove(0, 1);
    while (!s.isEmpty() && (s.back() == '.' || s.back() == ' '))
        s.chop(1);
    if (s.isEmpty())
        s = QStringLiteral("mappack");
    return s;
}

// ── Static factory methods ────────────────────────────────────────────────────

void MapPackDlg::exportFromDiffs(const QVector<MapDiff> &diffs,
                                  const QByteArray &cmpRom,
                                  ByteOrder bo,
                                  const QString &cmpLabel,
                                  Project *project,
                                  QWidget *parent)
{
    auto *dlg = new MapPackDlg(parent);
    dlg->m_project    = project;
    dlg->m_exportMode = true;
    dlg->m_pack       = MapPack::fromDiffs(diffs, cmpRom, bo, cmpLabel);
    dlg->m_labelEdit->setText(cmpLabel + " — Map Pack");
    dlg->m_titleLabel->setText(
        tr("<b>Export map pack</b>  ·  %1 changed map(s) from <i>%2</i>")
        .arg(diffs.size()).arg(cmpLabel));
    dlg->populateTable(dlg->m_pack);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MapPackDlg::importPack(Project *project, QWidget *parent)
{
    QString path = QFileDialog::getOpenFileName(
        parent, QObject::tr("Import Map Pack"), {},
        QObject::tr("Map packs (*.rxpack);;All files (*)"));
    if (path.isEmpty()) return;

    QString err;
    MapPack pack = MapPack::load(path, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("Import failed"), err);
        return;
    }

    auto *dlg = new MapPackDlg(parent);
    dlg->m_project    = project;
    dlg->m_exportMode = false;
    dlg->m_pack       = pack;
    dlg->m_labelEdit->setText(pack.label);
    dlg->m_labelEdit->setReadOnly(true);
    dlg->m_titleLabel->setText(
        tr("<b>Import map pack</b>  ·  %1 map(s)  ·  created %2")
        .arg(pack.maps.size()).arg(pack.created));
    dlg->populateTable(pack);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// ── Constructor ───────────────────────────────────────────────────────────────

MapPackDlg::MapPackDlg(QWidget *parent)
    : QDialog(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
              | Qt::WindowMaximizeButtonHint)
{
    setWindowTitle(tr("Map Pack"));
    resize(680, 480);
    buildUi();
}

void MapPackDlg::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // Title
    m_titleLabel = new QLabel();
    m_titleLabel->setStyleSheet("color:#c9d1d9; font-size:9pt;");
    root->addWidget(m_titleLabel);

    // Label
    auto *labelRow = new QHBoxLayout();
    labelRow->addWidget(new QLabel(tr("Pack label:")));
    m_labelEdit = new QLineEdit();
    labelRow->addWidget(m_labelEdit, 1);
    root->addLayout(labelRow);

    // Map table
    m_table = new QTableWidget();
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Include"), tr("Map"), tr("Size"), tr("Data")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setStyleSheet(
        "QTableWidget { background:#0d1117; color:#c9d1d9; gridline-color:#21262d; font-size:8pt; }"
        "QTableWidget::item:selected { background:#1f6feb; }"
        "QHeaderView::section { background:#161b22; color:#8b949e; border:none; padding:4px; }");
    root->addWidget(m_table, 1);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color:#8b949e; font-size:8pt;");
    root->addWidget(m_statusLabel);

    // Buttons
    auto *btnRow = new QHBoxLayout();
    auto *allBtn  = new QPushButton(tr("Select all"));
    auto *noneBtn = new QPushButton(tr("Select none"));
    allBtn->setFixedWidth(88);
    noneBtn->setFixedWidth(88);

    auto *actionBtn = new QPushButton();    // "Save" or "Apply"
    actionBtn->setStyleSheet(
        "QPushButton { background:#238636; color:#fff; border-radius:4px; padding:4px 12px; }"
        "QPushButton:hover { background:#2ea043; }");

    auto *closeBtn = new QPushButton(tr("Close"));
    closeBtn->setFixedWidth(72);

    connect(allBtn,   &QPushButton::clicked, this, &MapPackDlg::onSelectAll);
    connect(noneBtn,  &QPushButton::clicked, this, &MapPackDlg::onSelectNone);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnRow->addWidget(allBtn);
    btnRow->addWidget(noneBtn);
    btnRow->addStretch();
    btnRow->addWidget(actionBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    // Wire action button after we know the mode — done in populateTable
    // Store ref via property so populateTable can relabel it
    actionBtn->setProperty("isActionBtn", true);
    connect(actionBtn, &QPushButton::clicked, this, [this] {
        m_exportMode ? onSave() : onApply();
    });
    actionBtn->setText(m_exportMode ? tr("Save .rxpack…") : tr("Apply selected to ROM"));
}

// ── Populate ──────────────────────────────────────────────────────────────────

void MapPackDlg::populateTable(const MapPack &pack)
{
    m_table->setRowCount(pack.maps.size());
    for (int i = 0; i < pack.maps.size(); ++i) {
        const MapPackEntry &e = pack.maps[i];

        auto *chk = new QCheckBox();
        chk->setChecked(true);
        chk->setStyleSheet("margin-left:8px;");
        m_table->setCellWidget(i, 0, chk);

        m_table->setItem(i, 1, new QTableWidgetItem(e.name));
        m_table->setItem(i, 2, new QTableWidgetItem(
            QString("%1×%2").arg(e.cols).arg(e.rows)));
        m_table->setItem(i, 3, new QTableWidgetItem(
            QString("%1B  (%2 bytes)").arg(e.dataSize)
            .arg(e.data.size())));
    }

    m_statusLabel->setText(tr("%1 map(s)  ·  created %2")
                           .arg(pack.maps.size()).arg(pack.created));

    // Relabel action button
    for (auto *btn : findChildren<QPushButton *>()) {
        if (btn->property("isActionBtn").toBool())
            btn->setText(m_exportMode ? tr("Save .rxpack…") : tr("Apply selected to ROM"));
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QVector<int> MapPackDlg::selectedRows() const
{
    QVector<int> rows;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto *chk = qobject_cast<QCheckBox *>(m_table->cellWidget(i, 0));
        if (chk && chk->isChecked()) rows.append(i);
    }
    return rows;
}

void MapPackDlg::onSelectAll()
{
    for (int i = 0; i < m_table->rowCount(); ++i)
        if (auto *c = qobject_cast<QCheckBox *>(m_table->cellWidget(i, 0))) c->setChecked(true);
}

void MapPackDlg::onSelectNone()
{
    for (int i = 0; i < m_table->rowCount(); ++i)
        if (auto *c = qobject_cast<QCheckBox *>(m_table->cellWidget(i, 0))) c->setChecked(false);
}

// ── Save ──────────────────────────────────────────────────────────────────────

void MapPackDlg::onSave()
{
    auto rows = selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Nothing selected"),
            tr("Check at least one map to include in the pack."));
        return;
    }

    // Build a subset pack with only selected maps
    MapPack sub;
    sub.version = m_pack.version;
    sub.created = m_pack.created;
    sub.label   = m_labelEdit->text();
    for (int r : rows) sub.maps.append(m_pack.maps[r]);

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Map Pack"), safeFileName(sub.label) + ".rxpack",
        tr("Map packs (*.rxpack);;All files (*)"));
    if (path.isEmpty()) return;

    QString err;
    if (!sub.save(path, &err))
        QMessageBox::warning(this, tr("Save failed"), err);
    else
        m_statusLabel->setText(tr("Saved %1 map(s) to %2").arg(rows.size()).arg(path));
}

// ── Apply ─────────────────────────────────────────────────────────────────────

void MapPackDlg::onApply()
{
    if (!m_project) return;

    auto rows = selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Nothing selected"),
            tr("Check at least one map to apply."));
        return;
    }

    // Subset pack
    MapPack sub;
    sub.label = m_pack.label;
    for (int r : rows) sub.maps.append(m_pack.maps[r]);

    // Build rows for confirm dialog
    QVector<UI::RiskyChangeConfirmDialog::ChangeRow> changeRows;
    changeRows.reserve(sub.maps.size());
    int totalCells = 0;
    for (const MapPackEntry &e : sub.maps) {
        totalCells += e.cols * e.rows;
        UI::RiskyChangeConfirmDialog::ChangeRow cr;
        cr.label    = e.name;
        cr.oldValue = QStringLiteral("%1×%2").arg(e.cols).arg(e.rows);
        cr.newValue = QStringLiteral("%1B").arg(e.dataSize);
        cr.delta    = QString();
        changeRows.append(cr);
    }

    UI::RiskyChangeConfirmDialog confirm(this);
    confirm.setHeadline(tr("Apply map pack: %1").arg(sub.label));
    confirm.setDescription(tr(
        "This will overwrite %1 map(s) (%2 cell(s)) in the current ROM. "
        "A version snapshot will let you revert if needed.")
        .arg(sub.maps.size()).arg(totalCells));
    confirm.setRisk(UI::RiskyChangeConfirmDialog::Risk::Caution);
    confirm.setChanges(changeRows);
    confirm.setSnapshotOption(true, true);
    confirm.setActionText(tr("Apply"));
    if (sub.maps.size() > 50)
        confirm.setRequireTypedConfirmation(QStringLiteral("APPLY"));

    if (confirm.exec() != QDialog::Accepted)
        return;

    if (confirm.snapshotChecked())
        m_project->snapshotVersion(tr("Before map pack: %1").arg(sub.label));

    // ── Chunked apply with cancellable progress ───────────────────────────
    QProgressDialog progress(tr("Applying changes..."), tr("Cancel"),
                             0, sub.maps.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setValue(0);

    QByteArray rollback = m_project->currentData;
    QStringList warnings;
    bool canceled = false;
    for (int i = 0; i < sub.maps.size(); ++i) {
        if (progress.wasCanceled()) { canceled = true; break; }

        MapPack slice;
        slice.label = sub.label;
        slice.maps.append(sub.maps[i]);
        warnings << slice.apply(m_project->currentData,
                                m_project->maps,
                                m_project->byteOrder);

        progress.setValue(i + 1);
        QCoreApplication::processEvents();
    }
    progress.close();
    QApplication::restoreOverrideCursor();

    if (canceled) {
        m_project->currentData = rollback;
        m_statusLabel->setText(tr("Apply canceled — changes rolled back"));
        return;
    }

    // Register any maps from the pack that are not yet in the project's map list.
    // This makes them appear in the left panel and 2D overlay even without an A2L.
    QSet<QString> existingNames;
    for (const MapInfo &mi : m_project->maps)
        existingNames.insert(mi.name);

    for (const MapPackEntry &e : sub.maps) {
        if (existingNames.contains(e.name))
            continue;
        if (e.address == 0 && e.mapDataOffset == 0)
            continue;  // no address info — can't place it in the ROM view

        MapInfo mi;
        mi.name          = e.name;
        mi.description   = e.description;
        mi.address       = e.address;
        mi.rawAddress    = e.address;
        mi.mapDataOffset = e.mapDataOffset;
        mi.dimensions    = { e.cols, e.rows };
        mi.dataSize      = e.dataSize;
        mi.columnMajor   = e.columnMajor;
        mi.length        = (int)e.mapDataOffset + e.cols * e.rows * e.dataSize;
        mi.hasScaling    = e.hasScaling;
        mi.scaling       = e.scaling;

        // Restore axis labels from pack metadata
        if (!e.xAxisValues.isEmpty()) {
            mi.xAxis.fixedValues = e.xAxisValues;
            mi.xAxis.inputName   = e.xAxisName;
            mi.xAxis.hasScaling  = !e.xAxisUnit.isEmpty();
            mi.xAxis.scaling.unit = e.xAxisUnit;
        }
        if (!e.yAxisValues.isEmpty()) {
            mi.yAxis.fixedValues = e.yAxisValues;
            mi.yAxis.inputName   = e.yAxisName;
            mi.yAxis.hasScaling  = !e.yAxisUnit.isEmpty();
            mi.yAxis.scaling.unit = e.yAxisUnit;
        }

        if (e.rows > 1)
            mi.type = QStringLiteral("MAP");
        else if (e.cols > 1)
            mi.type = QStringLiteral("CURVE");
        else
            mi.type = QStringLiteral("VALUE");

        m_project->maps.append(mi);
        existingNames.insert(mi.name);
    }

    emit m_project->dataChanged();

    QString msg = tr("Applied %1 map(s).").arg(rows.size() - warnings.size());
    if (!warnings.isEmpty())
        msg += tr("\n\nWarnings:\n") + warnings.join('\n');
    QMessageBox::information(this, tr("Map Pack Applied"), msg);
    m_statusLabel->setText(tr("Applied %1 map(s) at %2")
                           .arg(rows.size())
                           .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}
