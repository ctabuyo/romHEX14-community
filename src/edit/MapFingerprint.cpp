/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "edit/MapFingerprint.h"

#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

constexpr int kHistBuckets = 32;
constexpr int kDerivBuckets = 64;
constexpr int kDerivCenter  = 32;        // map [-32..+31] → [0..63]

double readCell(const uint8_t *raw, int dz, int off, int len, ByteOrder bo,
                bool isSigned)
{
    if (off + dz > len) return 0.0;
    uint32_t v = readRomValue(raw, len, off, dz, bo);
    if (isSigned) {
        if (dz == 1) return static_cast<int8_t>(v);
        if (dz == 2) return static_cast<int16_t>(v);
        if (dz == 4) return static_cast<int32_t>(v);
    }
    return static_cast<double>(v);
}

}  // namespace

namespace MapFingerprintEngine {

MapFingerprint computeFor(const QByteArray &rom, const MapInfo &m, ByteOrder bo)
{
    MapFingerprint fp;
    fp.name     = m.name;
    fp.cols     = m.dimensions.x;
    fp.rows     = m.dimensions.y;
    fp.dataSize = m.dataSize;
    fp.histogram.fill(0.0f, kHistBuckets);
    fp.derivative.fill(0.0f, kDerivBuckets);
    if (m.length <= 0 || m.dataSize <= 0) return fp;
    if (m.address + m.length > static_cast<uint32_t>(rom.size())) return fp;
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(rom.constData())
                          + m.address;
    const int dz = m.dataSize;
    QVector<double> values;
    values.reserve(m.length / dz);
    double mn =  std::numeric_limits<double>::max();
    double mx = -std::numeric_limits<double>::max();
    for (int off = 0; off + dz <= m.length; off += dz) {
        double v = readCell(raw, dz, off, m.length, bo, m.dataSigned);
        values.append(v);
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    if (values.isEmpty()) return fp;
    double range = mx - mn;
    if (range <= 0.0) range = 1.0;

    // Histogram: normalised by cell count
    for (double v : values) {
        int b = static_cast<int>(((v - mn) / range) * (kHistBuckets - 1));
        b = qBound(0, b, kHistBuckets - 1);
        fp.histogram[b] += 1.0f;
    }
    const float invN = 1.0f / values.size();
    for (float &x : fp.histogram) x *= invN;

    // Derivative histogram: signed quantised cell-to-cell deltas, bucketed
    // around 0.  Step over both axes (right neighbour + down neighbour) so
    // 1×N and M×N maps both contribute meaningful shape data.
    auto addDelta = [&](double d) {
        // Normalise by the value range so the histogram is scale-invariant
        const double dn = (d / range) * 16.0;        // ~half-bucket per 1/32 of range
        int b = kDerivCenter + static_cast<int>(qFloor(dn));
        b = qBound(0, b, kDerivBuckets - 1);
        fp.derivative[b] += 1.0f;
    };
    int derivCount = 0;
    if (fp.cols > 1 || fp.rows > 1) {
        // 1-D walk of the value array; covers both ROW_DIR and COLUMN_DIR
        // adequately for fingerprint purposes.
        for (int i = 1; i < values.size(); ++i) {
            addDelta(values[i] - values[i - 1]);
            ++derivCount;
        }
        // Also row-axis transitions for true 2-D maps.
        if (fp.cols > 0 && fp.rows > 1) {
            for (int r = 1; r < fp.rows; ++r) {
                for (int c = 0; c < fp.cols; ++c) {
                    const int idx     =  r      * fp.cols + c;
                    const int idxPrev = (r - 1) * fp.cols + c;
                    if (idx < values.size() && idxPrev < values.size()) {
                        addDelta(values[idx] - values[idxPrev]);
                        ++derivCount;
                    }
                }
            }
        }
    }
    if (derivCount > 0) {
        const float invD = 1.0f / derivCount;
        for (float &x : fp.derivative) x *= invD;
    }
    return fp;
}

double similarity(const MapFingerprint &a, const MapFingerprint &b)
{
    if (!a.isValid() || !b.isValid()) return 0.0;

    // Cosine on the histogram alone — captures value distribution.
    auto cosine = [](const QVector<float> &x, const QVector<float> &y) {
        double dot = 0, nx = 0, ny = 0;
        const int n = qMin(x.size(), y.size());
        for (int i = 0; i < n; ++i) {
            dot += double(x[i]) * y[i];
            nx  += double(x[i]) * x[i];
            ny  += double(y[i]) * y[i];
        }
        if (nx <= 0 || ny <= 0) return 0.0;
        return dot / (qSqrt(nx) * qSqrt(ny));
    };
    const double cosHist  = cosine(a.histogram,  b.histogram);
    const double cosDeriv = cosine(a.derivative, b.derivative);

    // 60% histogram + 40% derivative — derivative is noisier on small maps.
    double score = 0.6 * cosHist + 0.4 * cosDeriv;

    // Dimension penalty
    int colMatch = (a.cols == b.cols) ? 1 : 0;
    int rowMatch = (a.rows == b.rows) ? 1 : 0;
    if (colMatch == 0 && rowMatch == 0) score *= 0.40;
    else if (colMatch == 0 || rowMatch == 0) score *= 0.70;

    // Data size mismatch — trickier comparison, mild penalty.
    if (a.dataSize != b.dataSize) score *= 0.85;

    return qBound(0.0, score, 1.0);
}

}  // namespace MapFingerprintEngine
