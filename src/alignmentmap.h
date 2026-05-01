/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Per-pair offset alignment between two ROMs (A and B), with optional
 * "target" project C inheriting the same alignment.
 *
 * Use case
 * ────────
 * Tuner has two ROMs that contain the same calibration tables but at
 * different offsets — e.g. a different firmware build, an SP2 vs SP1
 * calibration package, or two ECUs of the same family.  The Differences
 * panel needs to compare them not at identical addresses but at *aligned*
 * addresses.
 *
 * Model
 * ─────
 * A list of `AlignRegion`s, each saying: "in this slice of A, the same
 * data sits at A + delta in B".  Regions cover non-overlapping ranges
 * of A; outside any region, the alignment is undefined and the diff
 * computation skips that area (or treats it as "fully different",
 * depending on caller policy).
 *
 * The simplest case is a single region that covers the whole file with
 * a uniform delta — that's what `setGlobal()` produces.  Multiple
 * regions allow piecewise alignment, e.g. when section X shifts by
 * 0x100 bytes but section Y shifts by 0x200.
 */

#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

class QJsonObject;

struct AlignRegion {
    qint64  rangeAStart = 0;     ///< inclusive start in A
    qint64  length      = 0;     ///< covers A[rangeAStart .. rangeAStart+length)
    qint64  deltaB      = 0;     ///< B address = A address + deltaB
    qint64  deltaC      = 0;     ///< C address = A address + deltaC
    int     confidence  = 100;   ///< 0..100; 100 = manual / verified
    QString source;              ///< "manual", "auto-corr", "ghost", "transitive"
    QString note;                ///< optional user label (e.g. "Injection map")

    qint64 endA() const { return rangeAStart + length; }
    bool   containsA(qint64 a) const {
        return a >= rangeAStart && a < rangeAStart + length;
    }
};

class AlignmentMap {
public:
    AlignmentMap();

    // ── Lookup ────────────────────────────────────────────────────────
    /// Translate an address in A to the corresponding address in B.
    /// Returns -1 if @p addrA is not covered by any region.
    qint64 mapAtoB(qint64 addrA) const;
    qint64 mapAtoC(qint64 addrA) const;

    /// Reverse lookup — find the address in A whose region covers
    /// @p addrB in B (returns -1 if no region maps).
    qint64 mapBtoA(qint64 addrB) const;

    /// Returns the index of the region that contains @p addrA in A,
    /// or -1.  Stable across in-place edits because removeRegion()
    /// shifts trailing entries down.
    int regionIndexForA(qint64 addrA) const;

    // ── Mutation ──────────────────────────────────────────────────────
    void clear();

    /// Replace all regions with one that spans @p totalSize bytes of A
    /// at uniform deltas.  This is the simplest layer-1 form of
    /// alignment.  @p totalSize is typically `min(A.size, B.size)`.
    void setGlobal(qint64 deltaB, qint64 deltaC, qint64 totalSize);

    /// Adjust the global / single-region offset by @p stepB / @p stepC
    /// without rebuilding from scratch.  No-op if there are zero or
    /// multiple regions (only meaningful in the layer-1 case).  Returns
    /// the new (deltaB, deltaC) of the (single) region or (0,0).
    QPair<qint64, qint64> nudgeGlobal(qint64 stepB, qint64 stepC);

    /// Insert @p r in address order; returns inserted index.  Splits or
    /// trims any overlapping existing region so the invariant holds.
    int  addRegion(const AlignRegion &r);
    void removeRegion(int idx);
    void replaceRegion(int idx, const AlignRegion &r);

    // ── Inspection ────────────────────────────────────────────────────
    const QVector<AlignRegion> &regions() const { return m_regions; }
    int   regionCount() const { return m_regions.size(); }
    bool  isEmpty()     const { return m_regions.isEmpty(); }
    bool  isGlobal()    const { return m_regions.size() == 1; }
    qint64 globalDeltaB() const { return isGlobal() ? m_regions[0].deltaB : 0; }
    qint64 globalDeltaC() const { return isGlobal() ? m_regions[0].deltaC : 0; }

    // ── Persistence (sidecar JSON) ────────────────────────────────────
    /// Compute the canonical sidecar path `<dir>/<projA>~<projB>.align.json`
    /// given the two project file paths.  Lexicographic order is used so
    /// (A,B) and (B,A) collapse to the same file.
    static QString sidecarPath(const QString &projAPath,
                               const QString &projBPath);
    bool saveTo(const QString &path) const;
    bool loadFrom(const QString &path);

    QJsonObject toJson() const;
    bool        fromJson(const QJsonObject &obj);

private:
    QVector<AlignRegion> m_regions;   ///< sorted by rangeAStart, non-overlapping
};
