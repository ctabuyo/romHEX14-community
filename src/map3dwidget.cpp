/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "map3dwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <cmath>
#include <algorithm>

Map3DWidget::Map3DWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

void Map3DWidget::showMap(const QByteArray &romData, const MapInfo &map)
{
    m_data = romData;
    m_map = map;
    m_cellSize = map.dataSize;
    m_hasMap = true;
    update();
}

void Map3DWidget::clear()
{
    m_hasMap = false;
    update();
}

void Map3DWidget::resetView()
{
    m_rotationX = -35;
    m_rotationZ = 45;
    m_zoom = 1.0;
    update();
}

QVector<QVector<double>> Map3DWidget::extractGrid() const
{
    const uint8_t *raw = reinterpret_cast<const uint8_t*>(m_data.constData());
    int dataLen = m_data.size();
    int cols = m_map.dimensions.x;
    int rows = qMax(1, m_map.dimensions.y);
    QVector<QVector<double>> grid(rows);

    for (int r = 0; r < rows; r++) {
        grid[r].resize(cols);
        for (int c = 0; c < cols; c++) {
            uint32_t offset = m_map.address + m_map.mapDataOffset
                             + uint32_t(m_map.columnMajor ? c * rows + r : r * cols + c) * m_cellSize;
            uint32_t rv = readRomValue(raw, dataLen, offset, m_cellSize, m_byteOrder);
            grid[r][c] = m_map.hasScaling
                         ? m_map.scaling.toPhysical(signExtendRaw(rv, m_cellSize, m_map.dataSigned))
                         : signExtendRaw(rv, m_cellSize, m_map.dataSigned);
        }
    }
    return grid;
}

Map3DWidget::Point3D Map3DWidget::project(double x, double y, double z,
                                           int cols, int rows,
                                           double minZ, double rangeZ) const
{
    double nx = (x / (cols - 1)) * 2.0 - 1.0;
    double ny = (y / (rows - 1)) * 2.0 - 1.0;
    double nz = ((z - minZ) / rangeZ) * 1.2;

    double radX = m_rotationX * M_PI / 180.0;
    double radZ = m_rotationZ * M_PI / 180.0;
    double cosX = std::cos(radX), sinX = std::sin(radX);
    double cosZ = std::cos(radZ), sinZ = std::sin(radZ);

    double rx = nx * cosZ - ny * sinZ;
    double ry = nx * sinZ + ny * cosZ;

    double fy = ry * cosX - nz * sinX;
    double fz = ry * sinX + nz * cosX;

    double scale = qMin(width(), height()) * 0.35 * m_zoom;
    double cx = width() / 2.0;
    double cy = height() / 2.0;

    return { cx + rx * scale, cy - fz * scale, fy };
}

QColor Map3DWidget::surfaceColor(double pct, double depth) const
{
    double shade = 0.6 + (depth + 1.0) * 0.2;
    int r = qBound(0, (int)(qMin(1.0, pct * 500.0 / 255.0) * 255.0 * shade), 255);
    int g = qBound(0, (int)(qMin(1.0, (1.0 - std::abs(pct - 0.5) * 2.0)) * 255.0 * shade), 255);
    int b = qBound(0, (int)(qMin(1.0, (1.0 - pct) * 400.0 / 255.0) * 255.0 * shade), 255);
    return QColor(r, g, b);
}

QColor Map3DWidget::wireColor(double pct) const
{
    int r = (int)(pct * 255);
    int b = (int)((1.0 - pct) * 255);
    return QColor(qBound(0, r, 255), 80, qBound(0, b, 255), 200);
}

void Map3DWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Background
    p.fillRect(0, 0, w, h, QColor(7, 11, 20));

    if (!m_hasMap || m_data.isEmpty()) {
        p.setPen(QColor(136, 136, 160));
        p.drawText(rect(), Qt::AlignCenter, "Select a map to view in 3D");
        return;
    }

    auto grid = extractGrid();
    int rows = grid.size();
    int cols = grid[0].size();

    if (rows < 2 || cols < 2) {
        p.setPen(QColor(136, 136, 160));
        p.drawText(rect(), Qt::AlignCenter, "Map needs at least 2x2 dimensions for 3D view");
        return;
    }

    // Find min/max
    double minV = 1e18, maxV = -1e18;
    for (const auto &row : grid) {
        for (auto v : row) {
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }
    }
    double range = (maxV > minV) ? (maxV - minV) : 1.0;

    // Build faces with depth sorting
    QVector<Face> faces;
    faces.reserve((rows - 1) * (cols - 1));

    // Sprint B helper: does cell (r,c) differ from m_originalData?  Cell
    // address derives from the map's base + linear index × cellSize.
    auto cellModified = [&](int r, int c) -> bool {
        if (!m_showOriginalDiff) return false;
        if (m_originalData.isEmpty()) return false;
        const qint64 addr = static_cast<qint64>(m_map.address)
                          + static_cast<qint64>(r * cols + c) * m_cellSize;
        if (addr < 0 || addr + m_cellSize > m_data.size()) return false;
        if (addr + m_cellSize > m_originalData.size())     return false;
        return std::memcmp(m_data.constData() + addr,
                           m_originalData.constData() + addr, m_cellSize) != 0;
    };

    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            Face f;
            f.pts[0] = project(c, r, grid[r][c], cols, rows, minV, range);
            f.pts[1] = project(c + 1, r, grid[r][c + 1], cols, rows, minV, range);
            f.pts[2] = project(c + 1, r + 1, grid[r + 1][c + 1], cols, rows, minV, range);
            f.pts[3] = project(c, r + 1, grid[r + 1][c], cols, rows, minV, range);

            f.value = (grid[r][c] + grid[r][c + 1] + grid[r + 1][c + 1] + grid[r + 1][c]) / 4.0;
            f.depth = (f.pts[0].depth + f.pts[1].depth + f.pts[2].depth + f.pts[3].depth) / 4.0;
            f.modified = cellModified(r, c) || cellModified(r, c + 1)
                      || cellModified(r + 1, c) || cellModified(r + 1, c + 1);
            faces.append(f);
        }
    }

    // Painter's algorithm: sort by depth (back to front)
    std::sort(faces.begin(), faces.end(), [](const Face &a, const Face &b) {
        return a.depth < b.depth;
    });

    // Draw faces
    for (const auto &face : faces) {
        double pct = (face.value - minV) / range;
        QPolygonF poly;
        for (int i = 0; i < 4; i++)
            poly << QPointF(face.pts[i].x, face.pts[i].y);

        if (!m_wireframe) {
            p.setBrush(surfaceColor(pct, face.depth));
            p.setPen(QPen(QColor(0, 0, 0, 76), 0.5));
        } else {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(wireColor(pct), 1));
        }
        p.drawPolygon(poly);

        // Sprint B — overlay tint on modified faces.  Drawn after the
        // base face so it composites on top; alpha keeps the underlying
        // height colour readable.
        if (face.modified) {
            p.setBrush(QColor(0xff, 0x55, 0x55, 110));
            p.setPen(QPen(QColor(0xff, 0x33, 0x33, 200), 1.4));
            p.drawPolygon(poly);
        }
    }

    // Axis labels
    p.setBrush(Qt::NoBrush);
    QFont labelFont("Segoe UI", 11);
    p.setFont(labelFont);

    auto xEnd = project(cols - 1, 0, minV, cols, rows, minV, range);
    auto yEnd = project(0, rows - 1, minV, cols, rows, minV, range);
    auto origin = project(0, 0, minV, cols, rows, minV, range);
    auto top = project(0, 0, maxV, cols, rows, minV, range);

    p.setPen(QColor(58, 145, 208));
    p.drawText(QPointF(xEnd.x, xEnd.y + 20), QString("X (%1)").arg(cols));
    p.setPen(QColor(124, 58, 237));
    p.drawText(QPointF(yEnd.x - 20, yEnd.y), QString("Y (%1)").arg(rows));
    p.setPen(QColor(107, 127, 163));
    p.drawText(QPointF(origin.x - 30, origin.y + 5), QString::number(minV, 'g', 6));
    p.drawText(QPointF(top.x - 30, top.y + 5), QString::number(maxV, 'g', 6));
}

void Map3DWidget::mousePressEvent(QMouseEvent *event)
{
    m_dragging = true;
    m_lastMouse = event->pos();
    setCursor(Qt::ClosedHandCursor);
}

void Map3DWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) return;
    int dx = event->pos().x() - m_lastMouse.x();
    int dy = event->pos().y() - m_lastMouse.y();
    m_rotationZ += dx * 0.5;
    m_rotationX += dy * 0.5;
    m_rotationX = qBound(-89.0, m_rotationX, -5.0);
    m_lastMouse = event->pos();
    update();
}

void Map3DWidget::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
}

void Map3DWidget::wheelEvent(QWheelEvent *event)
{
    m_zoom *= event->angleDelta().y() > 0 ? 1.08 : 0.92;
    m_zoom = qBound(0.3, m_zoom, 3.0);
    update();
}

void Map3DWidget::setShowOriginalDiffOverlay(bool on)
{
    if (m_showOriginalDiff == on) return;
    m_showOriginalDiff = on;
    update();
}

void Map3DWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_hasMap) return;   // nothing meaningful to edit
    QMenu menu(this);
    auto *editMenu = menu.addMenu(tr("Edit map"));
    struct E { const char *label; int code; };
    const E entries[] = {
        { QT_TR_NOOP("Value +1"),                 0 },
        { QT_TR_NOOP("Value −1"),                 1 },
        { QT_TR_NOOP("Change absolute…"),         2 },
        { QT_TR_NOOP("Change relative…"),         3 },
        { QT_TR_NOOP("Change by slider…"),        4 },
        { QT_TR_NOOP("Round / limit…"),           5 },
        { QT_TR_NOOP("Restore original value"),   6 },
        { QT_TR_NOOP("Interpolate"),              7 },
        { QT_TR_NOOP("Smooth"),                   8 },
        { QT_TR_NOOP("Flatten"),                  9 },
    };
    for (const E &e : entries) {
        QAction *a = editMenu->addAction(tr(e.label));
        const int code = e.code;
        connect(a, &QAction::triggered, this,
                [this, code]() { emit editOpRequested(code); });
    }

    menu.addSeparator();
    QAction *resetView = menu.addAction(tr("Reset view"));
    QAction *toggleWire = menu.addAction(tr("Wireframe"));
    toggleWire->setCheckable(true);
    toggleWire->setChecked(m_wireframe);

    QAction *picked = menu.exec(event->globalPos());
    if (picked == resetView) {
        this->resetView();
    } else if (picked == toggleWire) {
        m_wireframe = !m_wireframe;
        update();
    }
}
