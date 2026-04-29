/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include "romdata.h"

class Map3DSimWidget : public QWidget {
    Q_OBJECT

public:
    explicit Map3DSimWidget(QWidget *parent = nullptr);

    void showMap(const QByteArray &romData, const MapInfo &map,
                 ByteOrder byteOrder);
    void clear();

signals:
    void cellHovered(int row, int col, double value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Vec3 { double x, y, z; };
    struct Pt2  { double x, y; };

    void rebuildGrid();
    Pt2  project(double x, double y, double z) const;
    QColor heatColor(double t) const;

    void drawSurface(QPainter &p);
    void drawGlassWalls(QPainter &p);
    void drawSlicePlanes(QPainter &p);
    void drawSurfaceIntersections(QPainter &p);
    void drawSliceCurves(QPainter &p);
    void drawCrosshair(QPainter &p);
    void drawAxes(QPainter &p);
    void drawValueLabel(QPainter &p);
    void drawMiniHeatmap(QPainter &p);
    void updateValueLabel();

    double interpZ(double normX, double normY) const;

    // Data
    QByteArray m_data;
    MapInfo    m_map;
    ByteOrder  m_byteOrder = ByteOrder::BigEndian;
    bool       m_hasData = false;

    // Grid of physical values
    int m_cols = 0, m_rows = 0;
    QVector<QVector<double>> m_grid;   // [row][col]
    double m_minVal = 0, m_maxVal = 1;
    QVector<double> m_xAxis, m_yAxis;  // axis breakpoints

    // View parameters
    double m_rotX = -30;   // tilt angle (negative = looking down)
    double m_rotZ = 35;    // rotation around Z axis
    double m_zoom = 1.0;
    double m_panX = 0, m_panY = 0;
    static constexpr double kZScale = 1.05; // height scale for terrain

    // Crosshair
    double m_crossX = 0.5;  // 0..1 normalized position
    double m_crossY = 0.5;
    bool   m_crossDragging = false;

    // Interaction
    bool   m_rotating = false;
    QPoint m_lastMouse;

    // Controls
    QSlider *m_sliderX = nullptr;
    QSlider *m_sliderY = nullptr;
    QLabel  *m_labelValue = nullptr;
    QLabel  *m_labelX = nullptr;
    QLabel  *m_labelY = nullptr;
};
