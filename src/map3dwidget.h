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

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Point3D { double x, y, depth; };
    struct Face {
        Point3D pts[4];
        double value;
        double depth;
    };

    QVector<QVector<double>> extractGrid() const;
    Point3D project(double x, double y, double z, int cols, int rows,
                    double minZ, double rangeZ) const;
    QColor surfaceColor(double pct, double depth) const;
    QColor wireColor(double pct) const;

    QByteArray m_data;
    MapInfo m_map;
    bool m_hasMap = false;
    int m_cellSize = 2;
    ByteOrder m_byteOrder = ByteOrder::BigEndian;

    double m_rotationX = -35;
    double m_rotationZ = 45;
    double m_zoom = 1.0;
    bool m_wireframe = false;
    bool m_dragging = false;
    QPoint m_lastMouse;
};
