/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "edit/FindSimilarMapsDlg.h"
#include "edit/MapFingerprint.h"
#include "project.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

FindSimilarMapsDlg::FindSimilarMapsDlg(const Project *project,
                                       const MapInfo &reference,
                                       QWidget *parent)
    : QDialog(parent), m_project(project), m_reference(reference)
{
    setWindowTitle(tr("Find similar maps — %1").arg(reference.name));
    resize(640, 460);
    buildUi();
    computeAll();
    rebuildTable();
}

void FindSimilarMapsDlg::buildUi()
{
    auto *root = new QVBoxLayout(this);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    root->addWidget(m_summary);

    auto *hdr = new QHBoxLayout();
    m_thresholdLb = new QLabel(tr("Threshold: 80%"), this);
    m_threshold = new QSlider(Qt::Horizontal, this);
    m_threshold->setRange(50, 99);
    m_threshold->setValue(80);
    connect(m_threshold, &QSlider::valueChanged,
            this, &FindSimilarMapsDlg::onThresholdChanged);
    hdr->addWidget(m_thresholdLb);
    hdr->addWidget(m_threshold, 1);
    root->addLayout(hdr);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        {tr("Map"), tr("Address"), tr("Dims"), tr("Match")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(3, 80);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) { onRowActivated(row); });
    root->addWidget(m_table, 1);

    auto *btns = new QHBoxLayout();
    auto *goBtn    = new QPushButton(tr("Go to map"), this);
    auto *closeBtn = new QPushButton(tr("Close"), this);
    connect(goBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_table->currentRow();
        if (row >= 0) onRowActivated(row);
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btns->addStretch(1);
    btns->addWidget(goBtn);
    btns->addWidget(closeBtn);
    root->addLayout(btns);
}

void FindSimilarMapsDlg::computeAll()
{
    m_matches.clear();
    if (!m_project) return;
    const auto bo  = m_project->byteOrder;
    const auto &rom = m_project->currentData;
    const auto refFp = MapFingerprintEngine::computeFor(rom, m_reference, bo);
    if (!refFp.isValid()) {
        m_summary->setText(tr("Reference map could not be fingerprinted "
                              "(invalid bounds or zero data)."));
        return;
    }
    m_matches.reserve(m_project->maps.size());
    for (const MapInfo &m : m_project->maps) {
        if (m.name == m_reference.name && m.address == m_reference.address)
            continue;
        const auto fp = MapFingerprintEngine::computeFor(rom, m, bo);
        if (!fp.isValid()) continue;
        const double s = MapFingerprintEngine::similarity(refFp, fp);
        m_matches.append({m, s});
    }
    std::sort(m_matches.begin(), m_matches.end(),
              [](const Match &a, const Match &b) { return a.score > b.score; });
    m_summary->setText(
        tr("Reference: <b>%1</b> — %2×%3 cells, %4 bytes.  "
           "Comparing against %5 other map%6 in the project.")
            .arg(m_reference.name)
            .arg(m_reference.dimensions.x).arg(m_reference.dimensions.y)
            .arg(m_reference.length)
            .arg(m_matches.size())
            .arg(m_matches.size() == 1 ? "" : "s"));
}

void FindSimilarMapsDlg::rebuildTable()
{
    const double cutoff = m_threshold->value() / 100.0;
    m_table->setRowCount(0);
    int shown = 0;
    for (const Match &m : m_matches) {
        if (m.score < cutoff) continue;
        const int row = shown++;
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(m.map.name));
        m_table->setItem(row, 1, new QTableWidgetItem(
            QString("0x%1").arg(m.map.address, 8, 16, QChar('0')).toUpper()));
        m_table->setItem(row, 2, new QTableWidgetItem(
            QString("%1×%2").arg(m.map.dimensions.x).arg(m.map.dimensions.y)));
        auto *score = new QTableWidgetItem(
            QString::number(int(m.score * 100)) + "%");
        score->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, 3, score);
    }
    if (shown == 0) {
        m_summary->setText(m_summary->text()
                           + tr(" — no matches at this threshold."));
    }
}

void FindSimilarMapsDlg::onThresholdChanged(int v)
{
    m_thresholdLb->setText(tr("Threshold: %1%").arg(v));
    rebuildTable();
}

void FindSimilarMapsDlg::onRowActivated(int row)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    const QString name = m_table->item(row, 0)->text();
    for (const Match &m : m_matches) {
        if (m.map.name == name) {
            emit goToRequested(m.map);
            return;
        }
    }
}
