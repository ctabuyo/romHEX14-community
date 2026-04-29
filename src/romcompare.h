/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QVector>
#include <QMap>
#include "romdata.h"

struct ByteDiff {
    uint32_t offset  = 0;
    uint8_t  refByte = 0;
    uint8_t  cmpByte = 0;
};

// Lightweight diff summary — avoids allocating a per-byte QVector when the
// caller only needs aggregate stats (e.g. for status bars or jump-to-first).
struct ByteDiffSummary {
    qint64 count       = 0;     // number of differing bytes in overlapping region
    qint64 firstOffset = -1;    // -1 if no diffs (or zero overlap)
};

// A contiguous run of differing bytes between two ROMs.
struct DiffRun {
    qint64 offset = 0;   // start offset of the run
    qint64 length = 0;   // number of consecutive differing bytes
};

struct MapDiff {
    MapInfo  map;
    int      changedCells = 0;
    double   maxAbsDelta  = 0.0;
    double   avgAbsDelta  = 0.0;
    // Physical-unit delta per cell (index matches flat cell order: row-major)
    QVector<double> cellDeltas;
    // Raw addresses in each ROM
    uint32_t refOffset = 0;   // map offset in reference ROM
    uint32_t cmpOffset = 0;   // map offset in compare ROM
};

class RomCompare {
public:
    // Raw byte differences between two ROMs (same-length regions)
    static QVector<ByteDiff> diffBytes(const QByteArray &ref, const QByteArray &cmp);

    // Aggregate stats only — orders of magnitude cheaper than diffBytes() on
    // ROMs with millions of differing bytes (no QVector allocation).
    static ByteDiffSummary diffBytesSummary(const QByteArray &ref, const QByteArray &cmp);

    // All contiguous runs of differing bytes. Order: ascending by offset.
    // O(n) over the overlapping region, O(R) memory where R = number of runs.
    static QVector<DiffRun> diffRuns(const QByteArray &ref, const QByteArray &cmp);

    // Per-map value differences.
    // cmpOffsets: map name → file offset in cmpRom (from RomLinkSession).
    //   If empty, assumes the same offsets as the reference maps.
    static QVector<MapDiff> diffMaps(
        const QByteArray &refRom,
        const QByteArray &cmpRom,
        const QVector<MapInfo> &maps,
        ByteOrder byteOrder,
        const QMap<QString, uint32_t> &cmpOffsets = {}
    );
};
