/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Map fingerprint + similarity  (Sprint F)
 * =========================================
 *
 * Compute a cheap structural fingerprint per map (histogram + first
 * derivative) so the user can ask "show me other maps that look like
 * this one" — useful when an A2L is incomplete and a target map is
 * duplicated elsewhere as a limp-mode / override / calibration backup.
 *
 * Fingerprint per map:
 *   * cols × rows × dataSize          — exact dimensions
 *   * 32-bucket value histogram       — normalised by cell count
 *   * 64-bucket signed-delta histogram of cell-to-cell transitions —
 *     captures the shape of the surface (zigzag, monotonic ramp, ...)
 *
 * Similarity:
 *   * weighted cosine of histogram + derivative vectors, plus a
 *     dimension-mismatch penalty (1.0 for matching dims, 0.7 if any
 *     dim differs, 0.4 if both differ — heuristic, tunable).
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include "romdata.h"

struct MapFingerprint {
    QString name;
    int     cols     = 0;
    int     rows     = 0;
    int     dataSize = 0;
    QVector<float> histogram;     // 32 buckets, sums to ~1.0
    QVector<float> derivative;    // 64 buckets, signed [-32 .. +31]

    bool isValid() const {
        return cols > 0 && rows > 0
               && histogram.size() == 32
               && derivative.size() == 64;
    }
};

namespace MapFingerprintEngine {

/// Build a fingerprint from a single map's bytes within a ROM.
MapFingerprint computeFor(const QByteArray &romData,
                          const MapInfo    &m,
                          ByteOrder bo);

/// Cosine-style similarity of two fingerprints.  0 = unrelated,
/// 1 = effectively identical.  Pure function — does not depend on
/// the order of arguments.
double similarity(const MapFingerprint &a, const MapFingerprint &b);

}  // namespace MapFingerprintEngine
