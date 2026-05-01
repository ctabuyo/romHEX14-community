/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "savepoints/SavepointsPanel.h"

#include "savepoints/SavepointManager.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SavepointsPanel::SavepointsPanel(QWidget *parent) : QWidget(parent)
{
    buildUi();
    refresh();
}

void SavepointsPanel::setManager(SavepointManager *mgr)
{
    if (m_mgr == mgr) return;
    if (m_mgr) m_mgr->disconnect(this);
    m_mgr = mgr;
    if (m_mgr) {
        connect(m_mgr, &SavepointManager::savepointsChanged,
                this, &SavepointsPanel::refresh);
        connect(m_mgr, &SavepointManager::currentChanged,
                this, [this](const QString &) { refresh(); });
    }
    refresh();
}

void SavepointsPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Create row ───────────────────────────────────────────────────
    auto *row1 = new QHBoxLayout();
    m_inputLabel = new QLineEdit();
    m_inputLabel->setPlaceholderText(tr("Branch name (e.g. 'boost +0.2 trial')"));
    m_inputLabel->setClearButtonEnabled(true);
    connect(m_inputLabel, &QLineEdit::returnPressed,
            this, &SavepointsPanel::onCreateClicked);
    row1->addWidget(m_inputLabel, 1);

    m_btnCreate = new QPushButton(tr("Save current"));
    m_btnCreate->setToolTip(
        tr("Snapshot the project's current ROM state under this name."));
    connect(m_btnCreate, &QPushButton::clicked,
            this, &SavepointsPanel::onCreateClicked);
    row1->addWidget(m_btnCreate);
    root->addLayout(row1);

    // ── Summary ──────────────────────────────────────────────────────
    m_summary = new QLabel();
    m_summary->setStyleSheet("color:#8b949e; font-size:9pt;");
    root->addWidget(m_summary);

    // ── List ─────────────────────────────────────────────────────────
    m_list = new QListWidget();
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setStyleSheet(
        "QListWidget {"
        "  background: #0d1117;"
        "  border: 1px solid #30363d;"
        "  border-radius: 4px;"
        "}"
        "QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #21262d; }"
        "QListWidget::item:selected { background: #1f6feb; color: white; }");
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &SavepointsPanel::onItemDoubleClicked);
    connect(m_list, &QListWidget::itemSelectionChanged,
            this, &SavepointsPanel::onSelectionChanged);
    root->addWidget(m_list, 1);

    // ── Action row ───────────────────────────────────────────────────
    auto *row2 = new QHBoxLayout();
    m_btnSwitch = new QPushButton(tr("Switch to"));
    m_btnSwitch->setEnabled(false);
    m_btnSwitch->setToolTip(
        tr("Restore the project's ROM data to this savepoint."));
    connect(m_btnSwitch, &QPushButton::clicked,
            this, &SavepointsPanel::onSwitchClicked);
    row2->addWidget(m_btnSwitch);

    m_btnRename = new QPushButton(tr("Rename"));
    m_btnRename->setEnabled(false);
    connect(m_btnRename, &QPushButton::clicked,
            this, &SavepointsPanel::onRenameClicked);
    row2->addWidget(m_btnRename);

    m_btnDelete = new QPushButton(tr("Delete"));
    m_btnDelete->setEnabled(false);
    connect(m_btnDelete, &QPushButton::clicked,
            this, &SavepointsPanel::onDeleteClicked);
    row2->addWidget(m_btnDelete);

    row2->addStretch();
    root->addLayout(row2);
}

// ─── Slots ──────────────────────────────────────────────────────────────────

void SavepointsPanel::onCreateClicked()
{
    if (!m_mgr) {
        QMessageBox::information(this, tr("Tuning Branches"),
            tr("Open a project first."));
        return;
    }
    const QString label = m_inputLabel->text().trimmed();
    m_mgr->create(label);
    m_inputLabel->clear();
}

void SavepointsPanel::onSwitchClicked()
{
    if (!m_mgr) return;
    auto *itm = m_list->currentItem();
    if (!itm) return;
    const QString id = itm->data(Qt::UserRole).toString();
    if (!m_mgr->switchTo(id)) {
        QMessageBox::warning(this, tr("Tuning Branches"),
            tr("Failed to switch — savepoint may have been created against "
               "a different ROM size."));
    }
}

void SavepointsPanel::onRenameClicked()
{
    if (!m_mgr) return;
    auto *itm = m_list->currentItem();
    if (!itm) return;
    const QString id = itm->data(Qt::UserRole).toString();
    Savepoint sp = m_mgr->find(id);
    bool ok = false;
    QString newLabel = QInputDialog::getText(this, tr("Rename branch"),
        tr("New name:"), QLineEdit::Normal, sp.label, &ok);
    if (!ok || newLabel.trimmed().isEmpty()) return;
    m_mgr->rename(id, newLabel);
}

void SavepointsPanel::onDeleteClicked()
{
    if (!m_mgr) return;
    auto *itm = m_list->currentItem();
    if (!itm) return;
    const QString id = itm->data(Qt::UserRole).toString();
    Savepoint sp = m_mgr->find(id);
    auto reply = QMessageBox::question(this, tr("Delete branch"),
        tr("Delete savepoint \"%1\"?\n\nThis cannot be undone.").arg(sp.label));
    if (reply != QMessageBox::Yes) return;
    m_mgr->deleteSavepoint(id);
}

void SavepointsPanel::onItemDoubleClicked(QListWidgetItem * /*item*/)
{
    onSwitchClicked();
}

void SavepointsPanel::onSelectionChanged()
{
    bool hasSel = m_list->currentItem() != nullptr;
    m_btnSwitch->setEnabled(hasSel && m_mgr != nullptr);
    m_btnRename->setEnabled(hasSel && m_mgr != nullptr);
    m_btnDelete->setEnabled(hasSel && m_mgr != nullptr);
}

void SavepointsPanel::refresh()
{
    m_list->clear();
    if (!m_mgr) {
        m_summary->setText(tr("No project bound — open a project to use savepoints."));
        m_btnCreate->setEnabled(false);
        onSelectionChanged();
        return;
    }
    m_btnCreate->setEnabled(true);

    const auto &all = m_mgr->all();
    const QString currentId = m_mgr->currentId();
    if (all.isEmpty()) {
        m_summary->setText(tr("No savepoints yet — type a name and click Save current."));
    } else {
        m_summary->setText(tr("%1 savepoint(s)").arg(all.size()));
    }

    for (const Savepoint &sp : all) {
        QString display = sp.label;
        if (sp.id == currentId) display = QStringLiteral("● ") + display;
        QString detail = tr("  ·  %1 byte%2  ·  %3")
                              .arg(sp.byteCount())
                              .arg(sp.byteCount() == 1 ? "" : "s")
                              .arg(sp.createdAt.toLocalTime().toString("yyyy-MM-dd hh:mm"));
        auto *itm = new QListWidgetItem(display + detail);
        itm->setData(Qt::UserRole, sp.id);
        if (!sp.note.isEmpty()) itm->setToolTip(sp.note);
        if (sp.id == currentId) {
            QFont f = itm->font();
            f.setBold(true);
            itm->setFont(f);
            itm->setForeground(QColor(0x3f, 0xb9, 0x50));   // green active marker
        }
        m_list->addItem(itm);
    }
    onSelectionChanged();
}
