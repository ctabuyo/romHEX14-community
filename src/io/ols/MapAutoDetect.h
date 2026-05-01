/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QVector>
#include <cstdint>

class Project;

namespace ols {

/// One auto-detected calibration map candidate.
///
/// Mirrors the WinOLS-style map scanner output: an axis (or pair of axes)
/// found by scanning monotonic byte sequences in the ROM, plus a data block
/// adjacent to the axis whose values vary smoothly across cells.
struct MapCandidate {
    QString  name;            ///< auto-generated, e.g. "KFR_001A30"
    quint32  romAddress = 0;  ///< absolute flash address (= baseAddress + ROM offset)
    quint32  width  = 1;
    quint32  height = 1;
    quint8   cellBytes  = 2;  ///< 1 or 2
    bool     cellSigned = false;
    bool     bigEndian  = false;
    double   score      = 0;  ///< 0..100, higher = more confident
    QString  reason;          ///< short human description ("2D map 16×12 at 0x1A30, 2-byte cells")
};

/// Tunable knobs for the scan; defaults mirror Pro behaviour.
struct MapAutoDetectOptions {
    bool tryBigEndianAxes        = false;  ///< also test u16 BE axes (rare; Mazda Denso)
    int  minScore2D              = 60;     ///< drop 2D candidates below this
    int  minScore1D              = 65;     ///< drop 1D curves below this
    int  maxCandidatesPerRegion  = 20000;
    int  maxAxesPerRegion        = 2048;   ///< cap axis-search work per cell-type
};

/// Heuristic ROM scanner that finds calibration-map candidates without an A2L.
///
/// Strategy: walk the ROM byte-by-byte looking for strictly monotonic
/// sequences of 5..32 values (u8 and u16 LE/BE).  Each hit is treated as a
/// candidate axis.  For pairs of axes lying within 256 bytes of each other
/// the scanner tests an N×M block of cells immediately after the second axis;
/// for lonely axes it tests a single row of N cells.  Each candidate is
/// scored on axis quality (range, length) and block smoothness (mean
/// neighbour delta vs. block range).  Candidates passing the score thresholds
/// are returned, sorted high-to-low.
class MapAutoDetect {
    Q_DECLARE_TR_FUNCTIONS(ols::MapAutoDetect)
public:
    /// Scan the given ROM bytes.  @p baseAddress is added to every detected
    /// offset to produce MapCandidate::romAddress (so callers can subtract
    /// it back when populating MapInfo::address).
    static QVector<MapCandidate> scan(const QByteArray &rom,
                                       quint32 baseAddress,
                                       const MapAutoDetectOptions &opts = {});

    /// Convenience wrapper that scans @p project->currentData.
    static QVector<MapCandidate> scanProject(const Project &project,
                                              const MapAutoDetectOptions &opts = {});
};

} // namespace ols
