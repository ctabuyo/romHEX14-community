/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QAbstractScrollArea>
#include <QWidget>
#include <QFont>
#include <QSet>
#include <QVector>
#include "romdata.h"

class HexWidget;

// ── Vertical overview minimap (sits next to the scrollbar) ──────────────────
class HexOverviewBar : public QWidget {
    Q_OBJECT
public:
    explicit HexOverviewBar(HexWidget *parent);
    void rebuild();

signals:
    void scrollRequested(int row);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QSize sizeHint() const override { return QSize(48, 100); }

private:
    int rowAtY(int y) const;
    HexWidget *m_hex;
    QVector<float> m_cache;    // per-pixel intensity (0-1)
    bool m_dragging = false;
};

// ── Undo entry for hex edits ────────────────────────────────────────────────
struct HexUndoEntry {
    int offset;
    QByteArray before;
    QByteArray after;
};

// ── Hex dump view ───────────────────────────────────────────────────────────
class HexWidget : public QAbstractScrollArea {
    Q_OBJECT

public:
    enum class SidebarMode { ASCII, Bars };

    explicit HexWidget(QWidget *parent = nullptr);

    void loadData(const QByteArray &data, uint32_t baseAddress = 0);
    void setBaseAddress(uint32_t baseAddress);
    void setDisplayParams(int cellSize, ByteOrder bo,
                          int displayFmt = 0, bool isSigned = false);
    void setFontSize(int pt);
    QByteArray getData() const;
    QByteArray getOriginalData() const;
    QByteArray exportBinary() const;
    void goToAddress(uint32_t offset);
    void setMapRegions(const QVector<MapRegion> &regions);
    int modificationCount() const { return m_modifications.size(); }

    // Selection helpers
    bool hasSelection() const { return m_selectionStart >= 0 && m_selectionEnd >= 0; }
    int selStart() const { return qMin(m_selectionStart, m_selectionEnd); }
    int selEnd() const { return qMax(m_selectionStart, m_selectionEnd); }

    // Accessed by HexOverviewBar
    const QByteArray &romData() const { return m_data; }
    const QVector<MapRegion> &mapRegions() const { return m_mapRegions; }
    uint32_t baseAddress() const { return m_baseAddress; }
    int bytesPerRow() const { return m_bytesPerRow; }
    int headerHeight() const { return m_headerHeight; }
    int rowHeight() const { return m_rowHeight; }

signals:
    void offsetSelected(uint32_t offset, uint8_t value);
    void dataModified(int modCount);
    void bytesModified(int start, int length);

public slots:
    void setComparisonData(const QByteArray &data);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void updateScrollBar();
    void renderRow(QPainter &p, int row, int y);
    int mapRegionForOffset(uint32_t offset) const;
    QRect byteRect(int col, int y) const;
    void clearSelection();
    void pushUndo(int offset, const QByteArray &before, const QByteArray &after);
    void updateModifications(int offset, int length);

    QByteArray m_data;
    QByteArray m_originalData;
    QSet<uint32_t> m_modifications;
    QByteArray m_comparisonData;
    QVector<MapRegion> m_mapRegions;

    int m_bytesPerRow = 16;
    int m_rowHeight = 22;
    int m_headerHeight = 24;
    int m_offsetWidth = 90;
    int m_byteWidth = 26;
    int m_separatorWidth = 10;
    int m_asciiCharWidth = 9;

    SidebarMode m_sidebarMode = SidebarMode::Bars;
    int       m_groupSize   = 1;
    ByteOrder m_byteOrder   = ByteOrder::BigEndian;
    int       m_displayFmt  = 0;
    bool      m_isSigned    = false;

    uint32_t m_baseAddress   = 0;
    int32_t m_selectedOffset = -1;
    bool m_editing = false;
    int m_editNibble = 0;
    uint8_t m_editByteBefore = 0;  // byte value before nibble editing started

    // Selection range (-1 = no selection)
    int m_selectionStart = -1;
    int m_selectionEnd   = -1;

    // Undo/redo
    QVector<HexUndoEntry> m_undoStack;
    int m_undoIndex = -1;
    static const int kMaxUndo = 100;

    HexOverviewBar *m_overviewBar = nullptr;

    QFont m_monoFont;
};
