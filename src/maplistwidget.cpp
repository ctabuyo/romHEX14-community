/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "maplistwidget.h"
#include "appconfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QHeaderView>
#include <QFont>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QMenu>

// ── Inline delegate: dim address + description in a single column ─────────────
static const int kAddrRole = Qt::UserRole + 1;

class MapItemDelegate : public QStyledItemDelegate {
    QFont m_addrFont;
    int   m_addrW = 0;
public:
    explicit MapItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        m_addrFont = QFont("Consolas", 7);
        m_addrFont.setStyleHint(QFont::Monospace);
        m_addrW = QFontMetrics(m_addrFont).horizontalAdvance("0x00000000") + 6;
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (index.column() != 0) {
            QStyledItemDelegate::paint(p, option, index);
            return;
        }
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, p, opt.widget);

        const QString addr = index.data(kAddrRole).toString();
        const QString text = index.data(Qt::DisplayRole).toString();
        const bool    sel  = opt.state & QStyle::State_Selected;
        const QRect   r    = opt.rect.adjusted(4, 0, -2, 0);

        if (addr.isEmpty()) {
            // No address (group header or empty) — plain text
            p->setPen(sel ? Qt::white : QColor(201, 209, 217));
            p->setFont(opt.font);
            p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                        opt.fontMetrics.elidedText(text, Qt::ElideRight, r.width()));
            return;
        }

        // Address: fixed-width monospace, dim colour (fonts cached in constructor)
        QRect addrRect = r;
        addrRect.setWidth(m_addrW);
        p->setFont(m_addrFont);
        p->setPen(sel ? QColor(160, 190, 230) : QColor(88, 110, 145));
        p->drawText(addrRect, Qt::AlignVCenter | Qt::AlignLeft, addr);

        // Description: normal font, full brightness
        QRect descRect = r;
        descRect.setLeft(r.left() + m_addrW + 4);
        p->setFont(opt.font);
        p->setPen(sel ? Qt::white : QColor(201, 209, 217));
        p->drawText(descRect, Qt::AlignVCenter | Qt::AlignLeft,
                    opt.fontMetrics.elidedText(text, Qt::ElideRight, descRect.width()));
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        return { s.width(), 18 };
    }
};

MapListWidget::MapListWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Search row
    auto *searchRow = new QWidget();
    auto *searchLayout = new QHBoxLayout(searchRow);
    searchLayout->setContentsMargins(4, 4, 4, 4);
    searchLayout->setSpacing(4);
    m_searchLabel = new QLabel(tr("Find:"));
    m_searchLabel->setFixedWidth(30);
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText(tr("Filter maps…"));
    m_searchBox->setEnabled(false);
    searchLayout->addWidget(m_searchLabel);
    searchLayout->addWidget(m_searchBox);
    layout->addWidget(searchRow);

    // Tree widget with columns like OLS
    m_tree = new QTreeWidget();
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({tr("Description"), tr("Type"), tr("Address"), tr("Dims")});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::AscendingOrder);
    m_tree->setUniformRowHeights(true);
    // Sprint E — multi-select for bulk edit (Ctrl+click / Shift+click).
    // Single-select callers (currentItem()) keep working unchanged.
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->setColumnWidth(0, 240);
    m_tree->setColumnWidth(1, 44);
    m_tree->setColumnWidth(3, 52);
    m_tree->hideColumn(2);   // address is rendered inline in column 0
    m_tree->setItemDelegate(new MapItemDelegate(m_tree));
    // Compact OLS-style row height and font
    QFont treeFont("Segoe UI", 8);
    m_tree->setFont(treeFont);
    m_tree->setStyleSheet(
        "QTreeWidget { font-size: 8pt; }"
        "QTreeWidget::item { padding-top: 0px; padding-bottom: 0px; min-height: 17px; }"
        "QTreeWidget::item:selected { background: #1f6feb; color: #ffffff; }");
    layout->addWidget(m_tree);

    // Status bar
    m_statusLabel = new QLabel();
    m_statusLabel->setContentsMargins(6, 2, 6, 2);
    m_statusLabel->setStyleSheet("font-size: 8pt; color: #555;");
    layout->addWidget(m_statusLabel);

    // Progress bar (hidden by default)
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedHeight(14);
    m_progressBar->setTextVisible(true);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);

    m_searchTimer.setSingleShot(true);
    m_searchTimer.setInterval(200);
    connect(&m_searchTimer, &QTimer::timeout, this, &MapListWidget::onSearchChanged);
    connect(m_searchBox, &QLineEdit::textChanged, this, [this]() { m_searchTimer.start(); });
    connect(m_tree, &QTreeWidget::itemClicked, this, &MapListWidget::onItemClicked);
    connect(m_tree, &QTreeWidget::itemActivated, this, &MapListWidget::onItemClicked);
    connect(&AppConfig::instance(), &AppConfig::displaySettingsChanged,
            this, &MapListWidget::populateTree);

    // Sprint E — context menu with bulk-edit entry.  Single-select rows
    // get a single "Edit map…" entry that just fires `mapSelected`;
    // multi-select rows get "Bulk edit N maps…" which emits the new
    // `bulkEditRequested` signal.
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
                const QVector<MapInfo> sel = selectedMaps();
                if (sel.isEmpty()) return;
                QMenu menu(m_tree);
                if (sel.size() >= 2) {
                    menu.addAction(tr("Bulk edit %1 maps…").arg(sel.size()),
                                   [this, sel]() {
                                       emit bulkEditRequested(sel);
                                   });
                } else {
                    menu.addAction(tr("Open map"), [this, sel]() {
                        emit mapSelected(sel.first());
                    });
                }
                menu.exec(m_tree->viewport()->mapToGlobal(pos));
            });
}

QVector<MapInfo> MapListWidget::selectedMaps() const
{
    QVector<MapInfo> out;
    const auto items = m_tree->selectedItems();
    out.reserve(items.size());
    for (auto *it : items) {
        bool ok = false;
        const int idx = it->data(0, Qt::UserRole).toInt(&ok);
        if (!ok || idx < 0 || idx >= m_allMaps.size()) continue;
        out.append(m_allMaps[idx]);
    }
    return out;
}

void MapListWidget::setMaps(const QVector<MapInfo> &maps, uint32_t baseAddress)
{
    m_allMaps = maps;
    m_baseAddress = baseAddress;
    m_progressBar->hide();
    m_searchBox->setEnabled(true);
    m_searchBox->clear();
    m_statusLabel->setText(tr("%1 maps  |  Base: 0x%2")
        .arg(maps.size())
        .arg(baseAddress, 0, 16).toUpper());
    populateTree();
}

void MapListWidget::clear()
{
    m_allMaps.clear();
    m_tree->clear();
    m_searchBox->setEnabled(false);
    m_progressBar->hide();
    m_statusLabel->clear();
}

void MapListWidget::setProgressMessage(const QString &msg, int pct)
{
    m_progressBar->show();
    m_progressBar->setValue(pct);
    m_progressBar->setFormat(msg + "  %p%");
}

void MapListWidget::onItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    int idx = item->data(0, Qt::UserRole).toInt();
    if (idx >= 0 && idx < m_allMaps.size())
        emit mapSelected(m_allMaps[idx]);
}

void MapListWidget::onSearchChanged()
{
    filterMaps();
}

void MapListWidget::populateTree()
{
    m_tree->clear();
    QFont addrFont("Consolas", 8);
    addrFont.setStyleHint(QFont::Monospace);

    for (int i = 0; i < m_allMaps.size(); i++) {
        const auto &m = m_allMaps[i];
        auto *item = new QTreeWidgetItem(m_tree);

        const bool showLong = AppConfig::instance().showLongMapNames;
        // Primary label: description when showLong (or no name), else short name
        QString label = showLong
            ? (m.description.isEmpty() ? m.name : m.description)
            : (m.name.isEmpty() ? m.description : m.name);
        const QString addrStr = QString("0x%1").arg(m.address, 8, 16, QChar('0')).toUpper();
        item->setText(0, label);
        item->setData(0, kAddrRole, addrStr);          // for the inline delegate
        item->setToolTip(0, m.name + (m.description.isEmpty() ? "" : "\n" + m.description));
        item->setText(1, m.type);
        item->setText(2, addrStr);  // kept in model for sort/search; column is hidden
        QString dims = (m.dimensions.y > 1)
            ? QString("%1×%2").arg(m.dimensions.x).arg(m.dimensions.y)
            : QString::number(m.dimensions.x);
        item->setText(3, dims);
        item->setFont(2, addrFont);
        item->setData(0, Qt::UserRole, i);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    }
    m_tree->sortByColumn(m_tree->header()->sortIndicatorSection(),
                         m_tree->header()->sortIndicatorOrder());
}

void MapListWidget::retranslateUi()
{
    if (m_searchLabel) m_searchLabel->setText(tr("Find:"));
    if (m_searchBox)   m_searchBox->setPlaceholderText(tr("Filter maps…"));
    if (m_tree)
        m_tree->setHeaderLabels({tr("Description"), tr("Type"), tr("Address"), tr("Dims")});
}

// Strip separators for normalized matching
static QString stripSeparators(const QString &s)
{
    QString n;
    n.reserve(s.size());
    for (auto c : s)
        if (c != '_' && c != '.' && c != '-' && c != ' ')
            n.append(c);
    return n;
}

// Simple fuzzy match: checks if all chars of needle appear in haystack in order
// Returns -1 if no match, otherwise a score (lower = better)
static int fuzzyMatch(const QString &needle, const QString &haystack)
{
    int ni = 0, score = 0;
    bool prevMatched = false;
    for (int hi = 0; hi < haystack.size() && ni < needle.size(); hi++) {
        if (haystack[hi] == needle[ni]) {
            // Bonus for consecutive matches
            score += prevMatched ? 0 : 10;
            // Bonus for matching at word boundary (after _ . - space or uppercase)
            if (hi == 0 || haystack[hi-1] == '_' || haystack[hi-1] == '.'
                || haystack[hi-1] == '-' || haystack[hi-1] == ' '
                || (haystack[hi].isLower() && hi > 0 && haystack[hi-1].isUpper()))
                score -= 5;
            ni++;
            prevMatched = true;
        } else {
            score += 1; // gap penalty
            prevMatched = false;
        }
    }
    return (ni == needle.size()) ? score : -1;
}

// Score a query against a map entry. Higher = better match. 0 = no match.
static int scoreMap(const QStringList &tokens, const QStringList &normTokens,
                    const QString &name, const QString &desc,
                    const QString &type, const QString &addr)
{
    QString nameLow = name.toLower();
    QString descLow = desc.toLower();
    QString nameNorm = stripSeparators(nameLow);

    // Build full haystack for substring matching
    QString haystack = nameLow + " " + descLow + " " + type.toLower() + " " + addr.toLower();
    QString haystackNorm = stripSeparators(haystack);

    int totalScore = 0;

    for (int ti = 0; ti < tokens.size(); ti++) {
        const auto &tok = tokens[ti];
        const auto &normTok = normTokens[ti];
        int bestScore = 0;

        // Exact match in name (best)
        if (nameLow == tok)
            bestScore = 1000;
        // Name starts with token
        else if (nameLow.startsWith(tok))
            bestScore = 800;
        // Token at word boundary in name (e.g. "torque" matches "Copom_Torque")
        else if (nameLow.contains("_" + tok) || nameLow.contains("." + tok)
                 || nameLow.contains("-" + tok) || nameLow.contains(" " + tok))
            bestScore = 600;
        // Substring in name
        else if (nameLow.contains(tok))
            bestScore = 500;
        // Normalized match (separators stripped)
        else if (nameNorm.contains(normTok))
            bestScore = 400;
        // Match in description or type or address
        else if (haystack.contains(tok))
            bestScore = 300;
        // Normalized full haystack match
        else if (haystackNorm.contains(normTok))
            bestScore = 250;
        // Subsequence fuzzy match in name
        else {
            int fs = fuzzyMatch(tok, nameLow);
            if (fs >= 0)
                bestScore = qMax(1, 200 - fs);
            else {
                // Fuzzy on normalized name
                fs = fuzzyMatch(normTok, nameNorm);
                if (fs >= 0)
                    bestScore = qMax(1, 150 - fs);
            }
        }

        if (bestScore == 0)
            return 0; // all tokens must match

        totalScore += bestScore;
    }

    return totalScore;
}

void MapListWidget::filterMaps()
{
    QString query = m_searchBox->text().trimmed().toLower();
    if (query.isEmpty()) {
        // Reset to natural order — re-sort by current column
        for (int i = 0; i < m_tree->topLevelItemCount(); i++)
            m_tree->topLevelItem(i)->setHidden(false);
        m_tree->sortByColumn(m_tree->header()->sortIndicatorSection(),
                             m_tree->header()->sortIndicatorOrder());
        m_statusLabel->setText(tr("%1 maps  |  Base: 0x%2")
            .arg(m_allMaps.size())
            .arg(m_baseAddress, 0, 16).toUpper());
        return;
    }

    QStringList tokens = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QStringList normTokens;
    for (const auto &t : tokens)
        normTokens.append(stripSeparators(t));

    // Score all items
    struct ScoredItem { QTreeWidgetItem *item; int score; };
    QVector<ScoredItem> scored;

    int visible = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        auto *item = m_tree->topLevelItem(i);
        int idx = item->data(0, Qt::UserRole).toInt();
        const auto &m = m_allMaps[idx];
        QString addr = QString("0x%1").arg(m.address, 0, 16);

        int s = scoreMap(tokens, normTokens, m.name, m.description, m.type, addr);
        item->setHidden(s == 0);
        if (s > 0) {
            scored.append({item, s});
            visible++;
        }
    }

    // Sort by score (highest first) — re-order tree items
    std::sort(scored.begin(), scored.end(),
              [](const ScoredItem &a, const ScoredItem &b) { return a.score > b.score; });

    // Reorder: take all items out, re-add in score order
    // (Only when actively searching — don't mess with user's sort preference)
    if (!scored.isEmpty()) {
        QList<QTreeWidgetItem *> items;
        while (m_tree->topLevelItemCount() > 0)
            items.append(m_tree->takeTopLevelItem(0));

        // Add scored items first (in order), then hidden ones
        for (const auto &si : scored)
            m_tree->addTopLevelItem(si.item);
        for (auto *item : items)
            if (item->isHidden())
                m_tree->addTopLevelItem(item);
    }

    m_statusLabel->setText(tr("%1 of %2 maps shown").arg(visible).arg(m_allMaps.size()));
}
