/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "commandpalette.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// Custom roles on the QStandardItem for the delegate. We don't need the
// full PaletteEntry (it's stored separately in m_entries) — just the
// pieces the delegate paints.
constexpr int RoleKindLabel  = Qt::UserRole + 1;  // "[Map]" etc.
constexpr int RoleSubtitle   = Qt::UserRole + 2;
constexpr int RoleShortcut   = Qt::UserRole + 3;
constexpr int RoleEntryIndex = Qt::UserRole + 4;  // index into m_entries

// VSCode-ish 24px row delegate. Paints:
//   [Kind] Name   subtitle                       shortcut
//   ──────────────────────────────────────────────────────────
class PaletteDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &) const override {
        return QSize(opt.rect.width(), 24);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override
    {
        p->save();

        const bool selected = opt.state & QStyle::State_Selected;
        if (selected) {
            p->fillRect(opt.rect, QColor("#2a3f5f"));
        } else if (opt.state & QStyle::State_MouseOver) {
            p->fillRect(opt.rect, QColor("#1f2a3a"));
        }

        const QString kind     = idx.data(RoleKindLabel).toString();
        const QString name     = idx.data(Qt::DisplayRole).toString();
        const QString subtitle = idx.data(RoleSubtitle).toString();
        const QString shortcut = idx.data(RoleShortcut).toString();

        const int padX = 10;
        QRect r = opt.rect.adjusted(padX, 0, -padX, 0);

        QFont kindFont = opt.font;
        kindFont.setBold(true);
        QFontMetrics kfm(kindFont);
        QFontMetrics fm(opt.font);

        // Kind label (e.g. "[Map]") in muted blue
        p->setFont(kindFont);
        p->setPen(QColor("#58a6ff"));
        const int kindW = kfm.horizontalAdvance(kind);
        p->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, kind);

        // Name in primary text color
        p->setFont(opt.font);
        p->setPen(selected ? QColor("#ffffff") : QColor("#e6edf3"));
        QRect nameRect = r.adjusted(kindW + 8, 0, 0, 0);
        const int nameW = fm.horizontalAdvance(name);
        p->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft, name);

        // Shortcut hint flush right (dim)
        int rightInset = 0;
        if (!shortcut.isEmpty()) {
            p->setPen(QColor("#7d8590"));
            const int scW = fm.horizontalAdvance(shortcut);
            QRect scRect = r.adjusted(0, 0, 0, 0);
            p->drawText(scRect, Qt::AlignVCenter | Qt::AlignRight, shortcut);
            rightInset = scW + 12;
        }

        // Subtitle in middle, dimmed, between name and shortcut
        if (!subtitle.isEmpty()) {
            p->setPen(QColor("#8b949e"));
            QRect subRect = nameRect.adjusted(nameW + 12, 0, -rightInset, 0);
            const QString shown = fm.elidedText(subtitle, Qt::ElideRight,
                                                subRect.width());
            p->drawText(subRect, Qt::AlignVCenter | Qt::AlignLeft, shown);
        }

        p->restore();
    }
};

} // namespace

CommandPalette::CommandPalette(QWidget *parent)
    : QDialog(parent)
{
    // Frameless centered popup, drop-shadow.
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    resize(640, 420);

    auto *root = new QWidget(this);
    root->setObjectName("PaletteRoot");
    root->setStyleSheet(
        "#PaletteRoot { background: #161b22; border: 1px solid #30363d; "
        "               border-radius: 8px; }"
        "QLineEdit { background: #0d1117; color: #e6edf3; border: none; "
        "            border-bottom: 1px solid #30363d; padding: 10px 12px; "
        "            font-size: 14px; }"
        "QListView { background: #161b22; color: #e6edf3; border: none; "
        "            outline: 0; }"
        "QListView::item { border: none; }"
    );

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->addWidget(root);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 180));
    root->setGraphicsEffect(shadow);

    auto *vbox = new QVBoxLayout(root);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_search = new QLineEdit(root);
    m_search->setPlaceholderText(
        tr("Search projects, maps, settings, actions…"));
    m_search->installEventFilter(this);
    vbox->addWidget(m_search);

    m_list = new QListView(root);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setMouseTracking(true);
    m_list->setItemDelegate(new PaletteDelegate(m_list));
    vbox->addWidget(m_list, 1);

    m_model = new QStandardItemModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    // Match against the DisplayRole (the name field) — fuzzy enough for now.
    m_proxy->setFilterRole(Qt::DisplayRole);
    m_list->setModel(m_proxy);

    connect(m_search, &QLineEdit::textChanged,
            this, &CommandPalette::onTextChanged);
    connect(m_search, &QLineEdit::returnPressed,
            this, &CommandPalette::activateCurrent);
    connect(m_list, &QAbstractItemView::activated,
            this, [this](const QModelIndex &) { activateCurrent(); });
}

void CommandPalette::setEntries(const QVector<PaletteEntry> &entries)
{
    m_entries = entries;
    m_model->clear();

    auto kindLabel = [this](PaletteEntry::Kind k) -> QString {
        switch (k) {
            case PaletteEntry::Kind::Project: return tr("[Project]");
            case PaletteEntry::Kind::Map:     return tr("[Map]");
            case PaletteEntry::Kind::Action:  return tr("[Action]");
            case PaletteEntry::Kind::Setting: return tr("[Setting]");
        }
        return {};
    };

    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &e = m_entries[i];
        auto *it = new QStandardItem(e.name);
        it->setData(kindLabel(e.kind), RoleKindLabel);
        it->setData(e.subtitle,        RoleSubtitle);
        it->setData(e.shortcut,        RoleShortcut);
        it->setData(i,                 RoleEntryIndex);
        it->setEditable(false);
        m_model->appendRow(it);
    }

    m_search->clear();   // also triggers filter reset
    selectFirstRow();
}

void CommandPalette::onTextChanged(const QString &text)
{
    // Case-insensitive substring on the name field. Real fuzzy is overkill
    // for a few hundred rows; "contains" handles 99% of palette use cases.
    m_proxy->setFilterFixedString(text);
    selectFirstRow();
}

void CommandPalette::selectFirstRow()
{
    if (m_proxy->rowCount() == 0) return;
    const QModelIndex first = m_proxy->index(0, 0);
    m_list->setCurrentIndex(first);
}

void CommandPalette::activateCurrent()
{
    const QModelIndex idx = m_list->currentIndex();
    if (!idx.isValid()) {
        // No matches → just close.
        reject();
        return;
    }
    const QModelIndex src = m_proxy->mapToSource(idx);
    const int entryIdx = src.data(RoleEntryIndex).toInt();
    if (entryIdx < 0 || entryIdx >= m_entries.size()) {
        reject();
        return;
    }
    const PaletteEntry chosen = m_entries[entryIdx];
    accept();
    emit activated(chosen);
}

void CommandPalette::showEvent(QShowEvent *ev)
{
    QDialog::showEvent(ev);
    // Center on parent (or screen if no parent).
    if (auto *p = parentWidget()) {
        const QRect pg = p->geometry();
        move(pg.center() - rect().center());
    }
    m_search->setFocus();
    m_search->selectAll();
    selectFirstRow();
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_search && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        switch (ke->key()) {
            case Qt::Key_Down: {
                const int rows = m_proxy->rowCount();
                if (rows == 0) return true;
                int row = m_list->currentIndex().row();
                row = qMin(rows - 1, row + 1);
                m_list->setCurrentIndex(m_proxy->index(row, 0));
                return true;
            }
            case Qt::Key_Up: {
                const int rows = m_proxy->rowCount();
                if (rows == 0) return true;
                int row = m_list->currentIndex().row();
                row = qMax(0, row - 1);
                m_list->setCurrentIndex(m_proxy->index(row, 0));
                return true;
            }
            case Qt::Key_Escape:
                reject();
                return true;
            case Qt::Key_PageDown: {
                const int rows = m_proxy->rowCount();
                if (rows == 0) return true;
                int row = qMin(rows - 1, m_list->currentIndex().row() + 8);
                m_list->setCurrentIndex(m_proxy->index(row, 0));
                return true;
            }
            case Qt::Key_PageUp: {
                int row = qMax(0, m_list->currentIndex().row() - 8);
                m_list->setCurrentIndex(m_proxy->index(row, 0));
                return true;
            }
            default:
                break;
        }
    }
    return QDialog::eventFilter(obj, ev);
}
