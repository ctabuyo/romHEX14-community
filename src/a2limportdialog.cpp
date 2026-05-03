/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "a2limportdialog.h"
#include "appconstants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFont>
#include <QMap>
#include <QSettings>
#include <QCloseEvent>

// ── Column indices ────────────────────────────────────────────────────────────
enum Col { ColName = 0, ColType, ColAddress, ColSize, ColDesc };
static const int kColCount = 5;

// ── Construction ──────────────────────────────────────────────────────────────
A2LImportDialog::A2LImportDialog(const QVector<MapInfo>  &maps,
                                 const QVector<A2LGroup> &groups,
                                 uint32_t                 baseAddress,
                                 QWidget                 *parent)
    : QDialog(parent), m_maps(maps), m_groups(groups)
{
    setWindowTitle(tr("Import A2L – Select Maps"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(820, 560);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(6);

    // ── Info ──────────────────────────────────────────────────────────
    auto *info = new QLabel(
        tr("<b>%1</b> characteristics found.  "
           "Base address: <b>0x%2</b>.  "
           "Check the groups or individual maps you want to import.")
            .arg(maps.size())
            .arg(baseAddress, 0, 16).toUpper());
    info->setWordWrap(true);
    root->addWidget(info);

    // ── Filter ────────────────────────────────────────────────────────
    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(tr("Filter:")));
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText(tr("Type to filter by name or description…"));
    filterRow->addWidget(m_searchBox);
    root->addLayout(filterRow);

    // ── Tree ──────────────────────────────────────────────────────────
    m_tree = new QTreeWidget();
    m_tree->setColumnCount(kColCount);
    m_tree->setHeaderLabels({tr("Name"), tr("Type"), tr("Address"), tr("Size"), tr("Description")});
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSortingEnabled(false); // keep group order
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->setColumnWidth(ColName,    200);
    m_tree->setColumnWidth(ColType,     56);
    m_tree->setColumnWidth(ColAddress,  88);
    m_tree->setColumnWidth(ColSize,     56);
    m_tree->setColumnWidth(ColDesc,    260);
    root->addWidget(m_tree);

    // ── Build the actual tree content ─────────────────────────────────
    buildTree();

    // ── Buttons + count ───────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();
    auto *btnAll    = new QPushButton(tr("Select All"));
    auto *btnNone   = new QPushButton(tr("Select None"));
    auto *btnInvert = new QPushButton(tr("Invert"));
    btnAll->setFixedWidth(88);
    btnNone->setFixedWidth(88);
    btnInvert->setFixedWidth(70);
    btnRow->addWidget(btnAll);
    btnRow->addWidget(btnNone);
    btnRow->addWidget(btnInvert);
    btnRow->addStretch();
    m_countLabel = new QLabel();
    btnRow->addWidget(m_countLabel);
    root->addLayout(btnRow);

    // ── OK / Cancel ───────────────────────────────────────────────────
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Import Selected"));
    root->addWidget(bb);

    // ── Connections ───────────────────────────────────────────────────
    connect(btnAll,  &QPushButton::clicked, this, [this]() { selectAll(true);  });
    connect(btnNone, &QPushButton::clicked, this, [this]() { selectAll(false); });
    connect(btnInvert, &QPushButton::clicked, this, [this]() {
        m_tree->blockSignals(true);
        QTreeWidgetItemIterator it(m_tree);
        while (*it) {
            auto *item = *it;
            if (item->childCount() == 0 && !item->isHidden()) {
                item->setCheckState(ColName,
                    item->checkState(ColName) == Qt::Checked
                    ? Qt::Unchecked : Qt::Checked);
            }
            ++it;
        }
        m_tree->blockSignals(false);
        // Refresh all group states
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            refreshAncestors(m_tree->topLevelItem(i)->child(0)); // trigger from first leaf
        for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
            auto *g = m_tree->topLevelItem(i);
            if (g->childCount() > 0)
                g->setCheckState(ColName, childrenState(g));
        }
        updateCount();
    });

    connect(m_tree, &QTreeWidget::itemChanged,
            this,   &A2LImportDialog::onItemChanged);

    connect(m_searchBox, &QLineEdit::textChanged,
            this,        &A2LImportDialog::applyFilter);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateCount();

    restoreGeometry(rx14::appSettings()
                    .value("dialogGeometry/A2LImportDialog").toByteArray());
}

void A2LImportDialog::closeEvent(QCloseEvent *event)
{
    rx14::appSettings()
        .setValue("dialogGeometry/A2LImportDialog", saveGeometry());
    QDialog::closeEvent(event);
}

// ── Tree building ─────────────────────────────────────────────────────────────

void A2LImportDialog::buildTree()
{
    // Map from characteristic name → index in m_maps
    QMap<QString, int> mapIdx;
    for (int i = 0; i < m_maps.size(); i++)
        mapIdx[m_maps[i].name] = i;

    // Map from group name → index in m_groups
    QMap<QString, int> groupIdx;
    for (int i = 0; i < m_groups.size(); i++)
        groupIdx[m_groups[i].name] = i;

    // Find root groups: those not referenced as a sub-group by any other group
    QSet<QString> referencedAsSub;
    for (const auto &g : m_groups)
        for (const auto &s : g.subGroups)
            referencedAsSub.insert(s);

    QSet<QString> placed; // characteristic names already added

    if (!m_groups.isEmpty()) {
        // Add root groups (and recursively their sub-groups)
        for (const auto &g : m_groups) {
            if (!referencedAsSub.contains(g.name))
                addGroupNode(nullptr, g.name, groupIdx, mapIdx, placed);
        }
    }

    // Maps not placed in any group → "(Ungrouped)" folder
    QVector<int> ungrouped;
    for (int i = 0; i < m_maps.size(); i++)
        if (!placed.contains(m_maps[i].name))
            ungrouped.append(i);

    if (!ungrouped.isEmpty()) {
        auto *ug = new QTreeWidgetItem(m_tree);
        ug->setText(ColName, m_groups.isEmpty()
                    ? tr("All Maps  (%1)").arg(ungrouped.size())
                    : tr("(Ungrouped)  (%1)").arg(ungrouped.size()));
        ug->setCheckState(ColName, Qt::Checked);
        ug->setFlags(ug->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        ug->setExpanded(true);
        QFont f = ug->font(ColName);
        f.setBold(true);
        ug->setFont(ColName, f);
        for (int idx : ungrouped)
            addMapLeaf(ug, idx);
    }
}

void A2LImportDialog::addGroupNode(QTreeWidgetItem          *parent,
                                   const QString            &groupName,
                                   const QMap<QString, int> &groupIdx,
                                   const QMap<QString, int> &mapIdx,
                                   QSet<QString>            &placed)
{
    if (!groupIdx.contains(groupName)) return;
    const A2LGroup &g = m_groups[groupIdx[groupName]];

    // Count how many maps will actually be in this node (for label)
    // (do a quick count first)
    int mapCount = 0;
    for (const auto &cn : g.characteristics)
        if (mapIdx.contains(cn) && !placed.contains(cn)) mapCount++;

    auto makeGroupItem = [&](QTreeWidgetItem *p) -> QTreeWidgetItem * {
        auto *item = p ? new QTreeWidgetItem(p) : new QTreeWidgetItem(m_tree);
        QString label = g.description.isEmpty()
                        ? g.name
                        : QString("%1  –  %2").arg(g.name, g.description);
        item->setText(ColName, label);
        item->setCheckState(ColName, Qt::Checked);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setExpanded(true);
        QFont f = item->font(ColName);
        f.setBold(true);
        item->setFont(ColName, f);
        return item;
    };

    auto *groupItem = makeGroupItem(parent);

    // Recurse into sub-groups first
    for (const auto &sub : g.subGroups)
        addGroupNode(groupItem, sub, groupIdx, mapIdx, placed);

    // Then add this group's own characteristics
    for (const auto &cn : g.characteristics) {
        if (!mapIdx.contains(cn) || placed.contains(cn)) continue;
        placed.insert(cn);
        addMapLeaf(groupItem, mapIdx[cn]);
    }

    // Update label with actual count
    int actual = groupItem->childCount();
    groupItem->setText(ColName,
        (g.description.isEmpty()
             ? g.name
             : QString("%1  –  %2").arg(g.name, g.description))
        + QString("   (%1)").arg(actual));
}

void A2LImportDialog::addMapLeaf(QTreeWidgetItem *parent, int idx)
{
    const MapInfo &m = m_maps[idx];
    auto *item = new QTreeWidgetItem(parent);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setCheckState(ColName, Qt::Checked);
    item->setText(ColName,    m.name);
    item->setText(ColType,    m.type);
    item->setText(ColAddress, QString("0x%1").arg(m.address, 0, 16).toUpper());
    item->setText(ColSize,    QString::number(m.length) + " B");
    item->setText(ColDesc,    m.description);
    item->setData(ColName, Qt::UserRole, idx);
    item->setToolTip(ColName, m.description.isEmpty() ? m.name : m.description);

    QFont mono("Consolas", 9);
    mono.setStyleHint(QFont::Monospace);
    item->setFont(ColAddress, mono);
    item->setTextAlignment(ColAddress, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(ColSize,    Qt::AlignRight | Qt::AlignVCenter);
}

// ── Check-state logic ─────────────────────────────────────────────────────────

void A2LImportDialog::onItemChanged(QTreeWidgetItem *item, int col)
{
    if (col != ColName) return;
    m_tree->blockSignals(true);

    if (item->childCount() > 0) {
        // Group toggled → cascade to all leaf descendants
        cascadeDown(item, item->checkState(ColName));
        // Update ancestors above this group
        refreshAncestors(item);
    } else {
        // Leaf toggled → update all ancestors
        refreshAncestors(item);
    }

    m_tree->blockSignals(false);
    updateCount();
}

void A2LImportDialog::cascadeDown(QTreeWidgetItem *item, Qt::CheckState state)
{
    // Don't cascade PartiallyChecked down — only Checked / Unchecked
    if (state == Qt::PartiallyChecked) return;
    for (int i = 0; i < item->childCount(); i++) {
        auto *child = item->child(i);
        child->setCheckState(ColName, state);
        if (child->childCount() > 0)
            cascadeDown(child, state);
    }
}

void A2LImportDialog::refreshAncestors(QTreeWidgetItem *item)
{
    auto *p = item ? item->parent() : nullptr;
    while (p) {
        p->setCheckState(ColName, childrenState(p));
        p = p->parent();
    }
}

Qt::CheckState A2LImportDialog::childrenState(QTreeWidgetItem *group) const
{
    int checked = 0, unchecked = 0, total = 0;

    // Recursively count visible leaf nodes
    std::function<void(QTreeWidgetItem*)> count = [&](QTreeWidgetItem *node) {
        for (int i = 0; i < node->childCount(); i++) {
            auto *child = node->child(i);
            if (child->isHidden()) continue;
            if (child->childCount() > 0) {
                count(child);
            } else {
                total++;
                if      (child->checkState(ColName) == Qt::Checked)   checked++;
                else if (child->checkState(ColName) == Qt::Unchecked)  unchecked++;
            }
        }
    };
    count(group);

    if (total == 0)              return Qt::Unchecked;
    if (checked == total)        return Qt::Checked;
    if (unchecked == total)      return Qt::Unchecked;
    return Qt::PartiallyChecked;
}

// ── Select All / None ─────────────────────────────────────────────────────────

void A2LImportDialog::selectAll(bool checked)
{
    m_tree->blockSignals(true);
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        auto *item = *it;
        if (item->childCount() == 0 && !item->isHidden())
            item->setCheckState(ColName, checked ? Qt::Checked : Qt::Unchecked);
        ++it;
    }
    m_tree->blockSignals(false);

    // Refresh all group states
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        auto *g = m_tree->topLevelItem(i);
        if (g->childCount() > 0)
            g->setCheckState(ColName, childrenState(g));
    }
    updateCount();
}

// ── Filter ────────────────────────────────────────────────────────────────────

void A2LImportDialog::applyFilter(const QString &text)
{
    const QString q = text.trimmed().toLower();

    m_tree->blockSignals(true);

    // First pass: show/hide leaf items
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        auto *item = *it;
        if (item->childCount() == 0) {
            bool visible = q.isEmpty()
                || item->text(ColName).toLower().contains(q)
                || item->text(ColDesc).toLower().contains(q);
            item->setHidden(!visible);
        }
        ++it;
    }

    // Second pass: hide groups that have no visible children
    for (int i = 0; i < m_tree->topLevelItemCount(); i++)
        refreshGroupVisibility(m_tree->topLevelItem(i));

    m_tree->blockSignals(false);

    // Refresh group check states based on visible children
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        auto *g = m_tree->topLevelItem(i);
        if (g->childCount() > 0 && !g->isHidden())
            g->setCheckState(ColName, childrenState(g));
    }

    updateCount();
}

void A2LImportDialog::refreshGroupVisibility(QTreeWidgetItem *group)
{
    // Recurse into sub-groups first
    for (int i = 0; i < group->childCount(); i++) {
        auto *child = group->child(i);
        if (child->childCount() > 0)
            refreshGroupVisibility(child);
    }

    // A group is visible if at least one child is visible
    bool anyVisible = false;
    for (int i = 0; i < group->childCount(); i++) {
        if (!group->child(i)->isHidden()) { anyVisible = true; break; }
    }
    group->setHidden(!anyVisible);
}

// ── Count ─────────────────────────────────────────────────────────────────────

void A2LImportDialog::updateCount()
{
    int checked = 0, total = 0;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        auto *item = *it;
        if (item->childCount() == 0 && !item->isHidden()) {
            total++;
            if (item->checkState(ColName) == Qt::Checked) checked++;
        }
        ++it;
    }
    m_countLabel->setText(tr("%1 of %2 selected").arg(checked).arg(total));
}

// ── Result ────────────────────────────────────────────────────────────────────

QVector<MapInfo> A2LImportDialog::selectedMaps() const
{
    QVector<MapInfo> result;
    QSet<int> added;
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        auto *item = *it;
        if (item->childCount() == 0
            && !item->isHidden()
            && item->checkState(ColName) == Qt::Checked)
        {
            int idx = item->data(ColName, Qt::UserRole).toInt();
            if (idx >= 0 && !added.contains(idx)) {
                result.append(m_maps[idx]);
                added.insert(idx);
            }
        }
        ++it;
    }
    return result;
}
