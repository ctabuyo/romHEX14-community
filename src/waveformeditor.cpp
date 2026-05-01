/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "waveformeditor.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <QtGlobal>

WaveformEditor::WaveformEditor(QObject *parent)
    : QObject(parent)
{
}

// ── Data binding ────────────────────────────────────────────────────────

void WaveformEditor::setData(QByteArray *data, const QByteArray *originalData)
{
    m_data = data;
    m_originalData = originalData;
    m_undoStack.clear();
    m_undoIndex = -1;
    m_drawing = false;
    m_drawSnapshot.clear();
    emit undoStateChanged();
}

void WaveformEditor::setCellSize(int bytes)
{
    if (bytes == 1 || bytes == 2 || bytes == 4)
        m_cellSize = bytes;
}

void WaveformEditor::setByteOrder(ByteOrder bo)
{
    m_byteOrder = bo;
}

void WaveformEditor::setSigned(bool isSigned)
{
    m_signed = isSigned;
}

// ── Value access ────────────────────────────────────────────────────────

double WaveformEditor::readValue(int offset) const
{
    if (!m_data || offset < 0 || offset + m_cellSize > m_data->size())
        return 0.0;
    const char *p = m_data->constData() + offset;
    if (m_cellSize == 1) {
        return m_signed ? (double)(int8_t)p[0] : (double)(uint8_t)p[0];
    }
    if (m_cellSize == 2) {
        uint16_t raw;
        if (m_byteOrder == ByteOrder::BigEndian)
            raw = ((uint8_t)p[0] << 8) | (uint8_t)p[1];
        else
            raw = ((uint8_t)p[1] << 8) | (uint8_t)p[0];
        return m_signed ? (double)(int16_t)raw : (double)raw;
    }
    if (m_cellSize == 4) {
        uint32_t raw;
        if (m_byteOrder == ByteOrder::BigEndian)
            raw = ((uint32_t)(uint8_t)p[0] << 24) | ((uint32_t)(uint8_t)p[1] << 16)
                | ((uint32_t)(uint8_t)p[2] << 8) | (uint8_t)p[3];
        else
            raw = ((uint32_t)(uint8_t)p[3] << 24) | ((uint32_t)(uint8_t)p[2] << 16)
                | ((uint32_t)(uint8_t)p[1] << 8) | (uint8_t)p[0];
        return m_signed ? (double)(int32_t)raw : (double)raw;
    }
    return 0.0;
}

double WaveformEditor::clampValue(double value) const
{
    if (m_cellSize == 1) {
        if (m_signed) return qBound(-128.0, std::round(value), 127.0);
        else          return qBound(0.0, std::round(value), 255.0);
    }
    if (m_cellSize == 2) {
        if (m_signed) return qBound(-32768.0, std::round(value), 32767.0);
        else          return qBound(0.0, std::round(value), 65535.0);
    }
    if (m_cellSize == 4) {
        if (m_signed) return qBound((double)INT32_MIN, std::round(value), (double)INT32_MAX);
        else          return qBound(0.0, std::round(value), (double)UINT32_MAX);
    }
    return value;
}

void WaveformEditor::writeValue(int offset, double value)
{
    if (!m_data || offset < 0 || offset + m_cellSize > m_data->size())
        return;

    value = clampValue(value);
    uint32_t raw;
    if (m_signed) {
        int32_t sv = (int32_t)value;
        raw = (uint32_t)sv;
    } else {
        raw = (uint32_t)value;
    }

    char *p = m_data->data() + offset;
    if (m_cellSize == 1) {
        p[0] = (char)(raw & 0xFF);
    } else if (m_cellSize == 2) {
        if (m_byteOrder == ByteOrder::BigEndian) {
            p[0] = (char)((raw >> 8) & 0xFF);
            p[1] = (char)(raw & 0xFF);
        } else {
            p[0] = (char)(raw & 0xFF);
            p[1] = (char)((raw >> 8) & 0xFF);
        }
    } else if (m_cellSize == 4) {
        if (m_byteOrder == ByteOrder::BigEndian) {
            p[0] = (char)((raw >> 24) & 0xFF);
            p[1] = (char)((raw >> 16) & 0xFF);
            p[2] = (char)((raw >> 8) & 0xFF);
            p[3] = (char)(raw & 0xFF);
        } else {
            p[0] = (char)(raw & 0xFF);
            p[1] = (char)((raw >> 8) & 0xFF);
            p[2] = (char)((raw >> 16) & 0xFF);
            p[3] = (char)((raw >> 24) & 0xFF);
        }
    }
}

// ── Batch editing operations ────────────────────────────────────────────

void WaveformEditor::setAbsolute(int start, int end, double value)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const QByteArray before = snapshot(start, end - start);

    for (int off = start; off < end; off += m_cellSize)
        writeValue(off, value);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::addDelta(int start, int end, double delta)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const QByteArray before = snapshot(start, end - start);

    for (int off = start; off < end; off += m_cellSize)
        writeValue(off, readValue(off) + delta);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::roundLimit(int start, int end, int multiple,
                                 double minVal, double maxVal)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const QByteArray before = snapshot(start, end - start);

    for (int off = start; off < end; off += m_cellSize) {
        double v = readValue(off);
        if (multiple > 0)
            v = std::round(v / multiple) * multiple;
        v = qBound(minVal, v, maxVal);
        writeValue(off, v);
    }

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::scale(int start, int end, double factor)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const QByteArray before = snapshot(start, end - start);

    for (int off = start; off < end; off += m_cellSize)
        writeValue(off, readValue(off) * factor);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::interpolate(int start, int end)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const int count = cellCount(start, end);
    if (count < 2) return;

    const QByteArray before = snapshot(start, end - start);

    const double first = readValue(start);
    const double last  = readValue(end - m_cellSize);

    for (int i = 0; i < count; ++i) {
        double t = (double)i / (double)(count - 1);
        double v = first + t * (last - first);
        writeValue(start + i * m_cellSize, v);
    }

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::smooth(int start, int end, int windowCells)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const int count = cellCount(start, end);
    if (count < 2) return;

    const QByteArray before = snapshot(start, end - start);

    // Read all values first
    QVector<double> vals(count);
    for (int i = 0; i < count; ++i)
        vals[i] = readValue(start + i * m_cellSize);

    // Compute smoothed values
    QVector<double> smoothed(count);
    for (int i = 0; i < count; ++i) {
        double sum = 0.0;
        int n = 0;
        for (int j = i - windowCells; j <= i + windowCells; ++j) {
            if (j >= 0 && j < count) {
                sum += vals[j];
                ++n;
            }
        }
        smoothed[i] = sum / n;
    }

    // Write back
    for (int i = 0; i < count; ++i)
        writeValue(start + i * m_cellSize, smoothed[i]);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::flatten(int start, int end)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const int count = cellCount(start, end);
    if (count == 0) return;

    const QByteArray before = snapshot(start, end - start);

    double sum = 0.0;
    for (int off = start; off < end; off += m_cellSize)
        sum += readValue(off);
    double avg = sum / count;

    for (int off = start; off < end; off += m_cellSize)
        writeValue(off, avg);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::restoreOriginal(int start, int end)
{
    if (!m_data || !m_originalData) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;
    if (start + (end - start) > m_originalData->size()) return;

    const QByteArray before = snapshot(start, end - start);

    memcpy(m_data->data() + start, m_originalData->constData() + start, end - start);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

void WaveformEditor::increment(int start, int end, int delta)
{
    if (!m_data) return;
    start = alignDown(start);
    end   = alignUp(end);
    if (start >= end) return;

    const QByteArray before = snapshot(start, end - start);

    for (int off = start; off < end; off += m_cellSize)
        writeValue(off, readValue(off) + delta);

    const QByteArray after = snapshot(start, end - start);
    pushUndo(start, before, after);
    emitModified(start, end);
}

// ── Free-form drawing ───────────────────────────────────────────────────

void WaveformEditor::beginDraw(int offset, double value)
{
    if (!m_data) return;
    m_drawing = true;
    m_drawStart = alignDown(offset);
    m_drawEnd   = m_drawStart + m_cellSize;
    m_drawSnapshot = snapshot(m_drawStart, m_drawEnd - m_drawStart);
    writeValue(m_drawStart, value);
    emitModified(m_drawStart, m_drawEnd);
}

void WaveformEditor::continueDraw(int offset, double value)
{
    if (!m_drawing || !m_data) return;
    offset = alignDown(offset);
    if (offset < 0 || offset + m_cellSize > m_data->size()) return;

    int newStart = qMin(m_drawStart, offset);
    int newEnd   = qMax(m_drawEnd, offset + m_cellSize);
    if (newStart < m_drawStart || newEnd > m_drawEnd) {
        // Extend snapshot: the already-covered part uses m_drawSnapshot,
        // newly-covered parts use current m_data bytes (not yet drawn, so
        // they represent the pre-draw state).
        QByteArray extended(newEnd - newStart, 0);
        int oldOff = m_drawStart - newStart;
        memcpy(extended.data() + oldOff, m_drawSnapshot.constData(), m_drawSnapshot.size());
        if (newStart < m_drawStart) {
            memcpy(extended.data(), m_data->constData() + newStart, m_drawStart - newStart);
        }
        if (newEnd > m_drawEnd) {
            int tailOff = m_drawEnd - newStart;
            memcpy(extended.data() + tailOff, m_data->constData() + m_drawEnd, newEnd - m_drawEnd);
        }
        m_drawSnapshot = extended;
        m_drawStart = newStart;
        m_drawEnd   = newEnd;
    }
    writeValue(offset, value);
    emitModified(offset, offset + m_cellSize);
}

void WaveformEditor::endDraw()
{
    if (!m_drawing) return;
    m_drawing = false;
    const QByteArray after = snapshot(m_drawStart, m_drawEnd - m_drawStart);
    pushUndo(m_drawStart, m_drawSnapshot, after);
    m_drawSnapshot.clear();
}

// ── Undo / redo ─────────────────────────────────────────────────────────

bool WaveformEditor::canUndo() const
{
    return m_undoIndex >= 0;
}

bool WaveformEditor::canRedo() const
{
    return m_undoIndex + 1 < m_undoStack.size();
}

void WaveformEditor::undo()
{
    if (!canUndo() || !m_data) return;
    const auto &e = m_undoStack[m_undoIndex];
    restore(e.offset, e.before);
    --m_undoIndex;
    emitModified(e.offset, e.offset + e.before.size());
    emit undoStateChanged();
}

void WaveformEditor::redo()
{
    if (!canRedo() || !m_data) return;
    ++m_undoIndex;
    const auto &e = m_undoStack[m_undoIndex];
    restore(e.offset, e.after);
    emitModified(e.offset, e.offset + e.after.size());
    emit undoStateChanged();
}

// ── Helpers ─────────────────────────────────────────────────────────────

int WaveformEditor::cellCount(int start, int end) const
{
    if (m_cellSize <= 0) return 0;
    return (end - start) / m_cellSize;
}

int WaveformEditor::alignDown(int offset) const
{
    if (m_cellSize <= 1) return offset;
    return (offset / m_cellSize) * m_cellSize;
}

int WaveformEditor::alignUp(int offset) const
{
    if (m_cellSize <= 1) return offset;
    return ((offset + m_cellSize - 1) / m_cellSize) * m_cellSize;
}

QByteArray WaveformEditor::snapshot(int offset, int length) const
{
    if (!m_data || offset < 0 || offset + length > m_data->size())
        return QByteArray();
    return m_data->mid(offset, length);
}

void WaveformEditor::restore(int offset, const QByteArray &bytes)
{
    if (!m_data || offset < 0 || offset + bytes.size() > m_data->size())
        return;
    memcpy(m_data->data() + offset, bytes.constData(), bytes.size());
}

void WaveformEditor::pushUndo(int offset, const QByteArray &before, const QByteArray &after)
{
    if (before == after) return;  // no change
    // Truncate any redo entries beyond current position
    if (m_undoIndex + 1 < m_undoStack.size())
        m_undoStack.resize(m_undoIndex + 1);
    m_undoStack.append({offset, before, after});
    if (m_undoStack.size() > kMaxUndo) {
        m_undoStack.removeFirst();
        --m_undoIndex; // adjust since we removed from the front
    }
    m_undoIndex = m_undoStack.size() - 1;
    emit undoStateChanged();
}

void WaveformEditor::emitModified(int start, int end)
{
    emit dataModified(start, end);
}
