/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include "romdata.h"

class Map3DWidget : public QWidget {
    Q_OBJECT

public:
    explicit Map3DWidget(QWidget *parent = nullptr);

    void showMap(const QByteArray &romData, const MapInfo &map);
    void clear();
    void setWireframe(bool on) { m_wireframe = on; update(); }
    void resetView();

    /// Sprint B: when set, faces of cells whose value differs from the
    /// project's originalData get a red alpha-blended tint.  Toggled
    /// from MainWindow View menu.
    void setShowOriginalDiffOverlay(bool on);
    /// Sprint B: original ROM bytes — needed so the 3D widget can paint
    /// per-face delta highlights without round-tripping through Project.
    void setOriginalData(const QByteArray &original) { m_originalData = original; update(); }

signals:
    /// Fired from the right-click "Edit map" submenu.  MainWindow listens
    /// (via ProjectView signal forwarding) and routes through the same
    /// `applyEditOp()` dispatcher used by the global Selection menu — so
    /// every view shares one undo stack per project.  The 3D view has no
    /// per-cell selection, so the operation runs over the *entire current
    /// map's* byte range automatically.  @p opCode mirrors
    /// MainWindow::EditOp.
    void editOpRequested(int opCode);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct Point3D { double x, y, depth; };
    struct Face {
        Point3D pts[4];
        double value;
        double depth;
        bool   modified = false;   // Sprint B — any of the 4 corner cells differ from original
    };

    QVector<QVector<double>> extractGrid() const;
    Point3D project(double x, double y, double z, int cols, int rows,
                    double minZ, double rangeZ) const;
    QColor surfaceColor(double pct, double depth) const;
    QColor wireColor(double pct) const;

    QByteArray m_data;
    QByteArray m_originalData;          // Sprint B — for diff overlay
    MapInfo m_map;
    bool m_hasMap = false;
    int m_cellSize = 2;
    ByteOrder m_byteOrder = ByteOrder::BigEndian;
    bool m_showOriginalDiff = false;    // Sprint B

    double m_rotationX = -35;
    double m_rotationZ = 45;
    double m_zoom = 1.0;
    bool m_wireframe = false;
    bool m_dragging = false;
    QPoint m_lastMouse;
};
