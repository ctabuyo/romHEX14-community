/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MapAutoDetect.h"
#include "../../project.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ols {

namespace {


static inline double readCell(const uint8_t *p, int cellBytes,
                               bool bigEndian, bool isSigned)
{
    if (cellBytes == 1) {
        return isSigned ? double(int8_t(p[0])) : double(p[0]);
    }
    if (cellBytes == 2) {
        const uint16_t raw = bigEndian
            ? uint16_t((uint16_t(p[0]) << 8) | p[1])
            : uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
        return isSigned ? double(int16_t(raw)) : double(raw);
    }
    const uint32_t raw = bigEndian
        ? (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
        | (uint32_t(p[2]) << 8)  |  uint32_t(p[3])
        :  uint32_t(p[0])        | (uint32_t(p[1]) << 8)
        | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    return isSigned ? double(int32_t(raw)) : double(raw);
}


struct AxisHit {
    int    offset    = 0;
    int    count     = 0;
    int    cellBytes = 2;
    bool   bigEndian = false;
    bool   isSigned  = false;
    bool   ascending = true;
    double score     = 0;
};


static double scoreAxis(const uint8_t *p, int count, int cellBytes,
                         bool bigEndian, bool isSigned)
{
    if (count < 5) return 0;

    double minV = readCell(p, cellBytes, bigEndian, isSigned);
    double maxV = minV;
    double prev = minV;
    bool   weaklyAscending  = true;
    bool   weaklyDescending = true;
    int    strictSteps      = 0;

    for (int i = 1; i < count; ++i) {
        const double v = readCell(p + i * cellBytes,
                                   cellBytes, bigEndian, isSigned);
        if (v < prev) weaklyAscending  = false;
        if (v > prev) weaklyDescending = false;
        if (v != prev) ++strictSteps;
        if (v < minV) minV = v;
        if (v > maxV) maxV = v;
        prev = v;
    }
    if (!weaklyAscending && !weaklyDescending)
        return 0;

    const double range = maxV - minV;
    if (range < 1) return 0;
    if (cellBytes == 1 && range < 3)  return 0;
    if (cellBytes == 2 && range < 10) return 0;
    if (cellBytes == 4 && range < 50) return 0;
    if (minV == maxV) return 0;

    if (strictSteps * 5 < (count - 1) * 4) return 0;

    double s = 55.0;
    s += std::min(30.0, double(count));
    if (range > 5 && range < 1.0e7) s += 10.0;
    if (strictSteps == count - 1)   s += 5.0;
    return std::min(100.0, s);
}

static double scoreBlockSmoothness(const uint8_t *p, int N, int M,
                                    int cellBytes, bool bigEndian, bool isSigned)
{
    if (N < 1 || M < 1) return 0;

    const int total = N * M;
    QVector<double> vals;
    vals.reserve(total);
    double minV =  std::numeric_limits<double>::infinity();
    double maxV = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < total; ++i) {
        const double v = readCell(p + i * cellBytes,
                                   cellBytes, bigEndian, isSigned);
        vals.append(v);
        if (v < minV) minV = v;
        if (v > maxV) maxV = v;
    }
    const double range = maxV - minV;
    if (range < 1) return 0;

    double sumDiff   = 0;
    int    countDiff = 0;
    for (int row = 0; row < M; ++row) {
        for (int col = 1; col < N; ++col) {
            sumDiff += std::abs(vals[row * N + col] - vals[row * N + col - 1]);
            ++countDiff;
        }
    }
    for (int row = 1; row < M; ++row) {
        for (int col = 0; col < N; ++col) {
            sumDiff += std::abs(vals[row * N + col] - vals[(row - 1) * N + col]);
            ++countDiff;
        }
    }
    if (countDiff == 0) return 50;

    const double meanDiff   = sumDiff / countDiff;
    const double smoothness = 1.0 - std::clamp(meanDiff / range, 0.0, 1.0);
    return smoothness * 100.0;
}


static QVector<AxisHit> findAxes(const QByteArray &rom,
                                  int cellBytes, bool bigEndian, bool isSigned,
                                  int minLen, int maxLen, int maxHits)
{
    QVector<AxisHit> hits;
    const int n = rom.size();
    const auto *p = reinterpret_cast<const uint8_t *>(rom.constData());

    int off = 0;
    while (off + cellBytes * minLen <= n
           && hits.size() < maxHits) {
        const double v0 = readCell(p + off, cellBytes, bigEndian, isSigned);
        const double v1 = readCell(p + off + cellBytes,
                                    cellBytes, bigEndian, isSigned);
        if (v1 == v0) { ++off; continue; }

        const bool ascending = (v1 > v0);
        int count = 2;
        double prev = v1;
        while (count < maxLen
               && off + (count + 1) * cellBytes <= n) {
            const double cur = readCell(p + off + count * cellBytes,
                                         cellBytes, bigEndian, isSigned);
            if ((ascending && cur < prev) || (!ascending && cur > prev))
                break;
            prev = cur;
            ++count;
        }

        if (count >= minLen) {
            const double s = scoreAxis(p + off, count,
                                         cellBytes, bigEndian, isSigned);
            if (s > 0) {
                AxisHit h;
                h.offset    = off;
                h.count     = count;
                h.cellBytes = cellBytes;
                h.bigEndian = bigEndian;
                h.isSigned  = isSigned;
                h.ascending = ascending;
                h.score     = s;
                hits.append(h);
            }
            off += count * cellBytes;
        } else {
            ++off;
        }
    }
    return hits;
}


static QString synthName(const char *prefix, int blockOff)
{
    return QStringLiteral("%1_%2")
        .arg(QLatin1String(prefix))
        .arg(blockOff, 6, 16, QLatin1Char('0'))
        .toUpper();
}


static int candidateByteRange(const MapCandidate &c, int &outBegin)
{
    const int len = int(c.width) * int(c.height) * int(c.cellBytes);
    outBegin = int(c.romAddress);  // ROM-relative address (caller subtracted base if needed)
    return len;
}

static QVector<MapCandidate> suppressOverlaps(QVector<MapCandidate> cands)
{
    if (cands.size() < 2) return cands;
    std::sort(cands.begin(), cands.end(),
              [](const MapCandidate &a, const MapCandidate &b) {
                  return a.score > b.score;
              });
    QVector<MapCandidate> kept;
    kept.reserve(cands.size());
    for (const auto &c : cands) {
        int cBegin = 0;
        const int cLen = candidateByteRange(c, cBegin);
        const int cEnd = cBegin + cLen;
        bool drop = false;
        for (const auto &k : kept) {
            int kBegin = 0;
            const int kLen = candidateByteRange(k, kBegin);
            const int kEnd = kBegin + kLen;
            const int overlapBegin = std::max(cBegin, kBegin);
            const int overlapEnd   = std::min(cEnd,   kEnd);
            if (overlapEnd <= overlapBegin) continue;
            const int overlap = overlapEnd - overlapBegin;
            if (overlap * 2 >= cLen) { drop = true; break; }
        }
        if (!drop) kept.append(c);
    }
    return kept;
}


struct BlockSpec {
    int  byteOffset;     // ROM offset of cell-data start
    int  N;              // width
    int  M;              // height
    int  cellBytes;      // 1, 2, 4
    bool bigEndian;
    bool isSigned;
};

static double scoreCombined(double axisScore, double smooth, int cellPenalty)
{
    double s = axisScore * 0.55 + smooth * 0.45;
    s -= cellPenalty;
    return s;
}

} // namespace


QVector<MapCandidate> MapAutoDetect::scan(const QByteArray &rom,
                                           quint32 baseAddress,
                                           const MapAutoDetectOptions &opts)
{
    QVector<MapCandidate> result;
    if (rom.size() < 64) return result;

    constexpr int kMinAxisLen = 5;
    constexpr int kMaxAxisLen = 96;
    constexpr int kMaxPairGap = 256;

    QVector<AxisHit> axes;
    axes += findAxes(rom, 1, /*BE*/false, /*sgn*/false,
                     kMinAxisLen, kMaxAxisLen, opts.maxAxesPerRegion);
    axes += findAxes(rom, 2, /*BE*/false, /*sgn*/false,
                     kMinAxisLen, kMaxAxisLen, opts.maxAxesPerRegion);
    if (opts.tryBigEndianAxes) {
        axes += findAxes(rom, 2, /*BE*/true, /*sgn*/false,
                         kMinAxisLen, kMaxAxisLen, opts.maxAxesPerRegion);
    }
    std::sort(axes.begin(), axes.end(),
              [](const AxisHit &a, const AxisHit &b) {
                  return a.offset < b.offset;
              });

    const auto *romBytes =
        reinterpret_cast<const uint8_t *>(rom.constData());
    const int romSize = rom.size();

    auto tryEmit2D = [&](const AxisHit &ax, const AxisHit &ay,
                          int blockOff, int dataCellBytes,
                          const char *layoutTag) {
        const int N = ax.count;
        const int M = ay.count;
        const int blockBytes = N * M * dataCellBytes;
        if (blockOff < 0 || blockOff + blockBytes > romSize) return;

        const double smooth = scoreBlockSmoothness(romBytes + blockOff,
                                                     N, M, dataCellBytes,
                                                     ax.bigEndian, false);
        if (smooth <= 0) return;
        const double axMean = (ax.score + ay.score) * 0.5;
        const int cellPen = (dataCellBytes == ax.cellBytes) ? 0 : 5;
        const double total = scoreCombined(axMean, smooth, cellPen);
        if (total < opts.minScore2D) return;

        MapCandidate c;
        c.name        = synthName("KFR", blockOff);
        c.romAddress  = baseAddress + quint32(blockOff);
        c.width       = quint32(N);
        c.height      = quint32(M);
        c.cellBytes   = quint8(dataCellBytes);
        c.cellSigned  = false;
        c.bigEndian   = ax.bigEndian;
        c.score       = total;
        c.reason      = MapAutoDetect::tr(
                            "2D %1 %2×%3 @ 0x%4, %5B %6 cells")
                            .arg(QLatin1String(layoutTag))
                            .arg(N).arg(M)
                            .arg(blockOff, 0, 16)
                            .arg(dataCellBytes)
                            .arg(ax.bigEndian ? QStringLiteral("BE")
                                              : QStringLiteral("LE"));
        result.append(c);
    };

    auto tryEmit1D = [&](const AxisHit &ax, int blockOff, int dataCellBytes,
                          const char *layoutTag) {
        const int N = ax.count;
        const int blockBytes = N * dataCellBytes;
        if (blockOff < 0 || blockOff + blockBytes > romSize) return;

        const double smooth = scoreBlockSmoothness(romBytes + blockOff,
                                                     N, 1, dataCellBytes,
                                                     ax.bigEndian, false);
        if (smooth <= 0) return;
        const int cellPen = (dataCellBytes == ax.cellBytes) ? 0 : 5;
        const double total = scoreCombined(ax.score, smooth, cellPen);
        if (total < opts.minScore1D) return;

        MapCandidate c;
        c.name        = synthName("KFC", blockOff);
        c.romAddress  = baseAddress + quint32(blockOff);
        c.width       = quint32(N);
        c.height      = 1;
        c.cellBytes   = quint8(dataCellBytes);
        c.cellSigned  = false;
        c.bigEndian   = ax.bigEndian;
        c.score       = total;
        c.reason      = MapAutoDetect::tr(
                            "1D %1 %2pt @ 0x%3, %4B %5 cells")
                            .arg(QLatin1String(layoutTag))
                            .arg(N)
                            .arg(blockOff, 0, 16)
                            .arg(dataCellBytes)
                            .arg(ax.bigEndian ? QStringLiteral("BE")
                                              : QStringLiteral("LE"));
        result.append(c);
    };

    constexpr int kCellSizes[] = {1, 2, 4};

    for (int i = 0; i < axes.size()
                    && result.size() < opts.maxCandidatesPerRegion; ++i) {
        const AxisHit &ax = axes[i];
        const int axEnd = ax.offset + ax.count * ax.cellBytes;

        for (int j = i + 1; j < axes.size(); ++j) {
            const AxisHit &ay = axes[j];
            if (ay.offset < axEnd) continue;
            if (ay.offset > axEnd + kMaxPairGap) break;
            if (ay.cellBytes != ax.cellBytes) continue;
            if (ay.bigEndian != ax.bigEndian) continue;

            const int blockOff = ay.offset + ay.count * ay.cellBytes;
            for (int dcb : kCellSizes) {
                tryEmit2D(ax, ay, blockOff, dcb, "axis-then-data");
                if (result.size() >= opts.maxCandidatesPerRegion) break;
            }
            if (result.size() >= opts.maxCandidatesPerRegion) break;
        }
    }

    for (int i = 0; i < axes.size()
                    && result.size() < opts.maxCandidatesPerRegion; ++i) {
        const AxisHit &ax = axes[i];

        for (int j = i + 1; j < axes.size(); ++j) {
            const AxisHit &ay = axes[j];
            const int axEnd = ax.offset + ax.count * ax.cellBytes;
            if (ay.offset < axEnd) continue;
            if (ay.offset > axEnd + kMaxPairGap) break;
            if (ay.cellBytes != ax.cellBytes) continue;
            if (ay.bigEndian != ax.bigEndian) continue;

            const int N = ax.count;
            const int M = ay.count;
            for (int dcb : kCellSizes) {
                const int blockBytes = N * M * dcb;
                const int blockOff = ax.offset - blockBytes;
                tryEmit2D(ax, ay, blockOff, dcb, "data-then-axis");
                if (result.size() >= opts.maxCandidatesPerRegion) break;
            }
            if (result.size() >= opts.maxCandidatesPerRegion) break;
        }
    }

    for (int i = 0; i < axes.size()
                    && result.size() < opts.maxCandidatesPerRegion; ++i) {
        const AxisHit &ax = axes[i];
        const int axEnd = ax.offset + ax.count * ax.cellBytes;
        for (int dcb : kCellSizes) {
            tryEmit1D(ax, axEnd, dcb, "axis-then-data");
            if (result.size() >= opts.maxCandidatesPerRegion) break;
        }
    }

    for (int i = 0; i < axes.size()
                    && result.size() < opts.maxCandidatesPerRegion; ++i) {
        const AxisHit &ax = axes[i];
        const int N = ax.count;
        for (int dcb : kCellSizes) {
            const int blockOff = ax.offset - N * dcb;
            tryEmit1D(ax, blockOff, dcb, "data-then-axis");
            if (result.size() >= opts.maxCandidatesPerRegion) break;
        }
    }

    for (auto &c : result) c.romAddress -= baseAddress;
    result = suppressOverlaps(std::move(result));
    for (auto &c : result) c.romAddress += baseAddress;

    if (result.size() > opts.maxCandidatesPerRegion)
        result.resize(opts.maxCandidatesPerRegion);

    return result;
}

QVector<MapCandidate> MapAutoDetect::scanProject(const Project &project,
                                                  const MapAutoDetectOptions &opts)
{
    return scan(project.currentData, project.baseAddress, opts);
}

} // namespace ols
