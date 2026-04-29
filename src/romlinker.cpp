/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "romlinker.h"
#include <QtConcurrent/QtConcurrent>
#include <QMap>
#include <QMutex>
#include <algorithm>
#include <numeric>

RomLinker::RomLinker(QObject *parent) : QObject(parent) {}

// ── Helpers ───────────────────────────────────────────────────────────────────

// Returns the file offset where `needle` first appears inside `haystack`,
// starting the search at `startOffset`. Returns -1 if not found.
static qint64 searchBytes(const QByteArray &haystack,
                           const QByteArray &needle,
                           qint64 startOffset = 0)
{
    if (needle.isEmpty() || needle.size() > haystack.size()) return -1;
    const char *h = haystack.constData();
    const char *n = needle.constData();
    qint64 hLen = haystack.size();
    qint64 nLen = needle.size();
    for (qint64 i = startOffset; i <= hLen - nLen; ++i) {
        if (memcmp(h + i, n, nLen) == 0) return i;
    }
    return -1;
}

// Build a fingerprint from refRom at the given map.
// Uses the actual cell data (skipping any inline axis header via mapDataOffset),
// capped at 64 bytes. Cell data is more stable for matching than axis headers.
static QByteArray makeFingerprint(const QByteArray &refRom, const MapInfo &m)
{
    uint32_t dataStart = m.address + m.mapDataOffset;
    int len = qMin(m.length, 64);
    if (len < 2) return {};   // too short to be a reliable fingerprint
    if ((int)dataStart + len > refRom.size()) return {};
    return refRom.mid((int)dataStart, len);
}

// ── Core algorithm ────────────────────────────────────────────────────────────

RomLinkSession RomLinker::link(const QByteArray &refRom,
                               const QByteArray &targetRom,
                               const QVector<MapInfo> &maps,
                               const QString &label,
                               std::function<void(const QString &, int)> progressCb)
{
    RomLinkSession session;
    session.label      = label;
    session.totalCount = maps.size();

    if (maps.isEmpty() || refRom.isEmpty() || targetRom.isEmpty())
        return session;

    auto emit_progress = [&](const QString &msg, int pct) {
        if (progressCb) progressCb(msg, pct);
    };

    // ── Phase 1: exact fingerprint search (parallel) ─────────────────────────
    emit_progress(QObject::tr("Phase 1: searching for exact map fingerprints…"), 0);

    QMap<int64_t, int> deltaHistogram;   // delta → count
    QVector<MapLinkResult> results(maps.size());

    // Initialise result stubs (no data race: each slot written once by one thread)
    for (int i = 0; i < maps.size(); ++i) {
        results[i].mapName    = maps[i].name;
        results[i].refAddress = maps[i].address;
        results[i].status     = MapLinkResult::NotFound;
    }

    {
        QMutex histMutex;
        QVector<int> indices(maps.size());
        std::iota(indices.begin(), indices.end(), 0);

        QtConcurrent::blockingMap(indices, [&](int i) {
            const MapInfo &m = maps[i];
            QByteArray fp = makeFingerprint(refRom, m);
            if (fp.isEmpty()) return;

            qint64 found = searchBytes(targetRom, fp);
            if (found < 0) return;

            uint32_t blockStart = (uint32_t)qMax((qint64)0, found - (qint64)m.mapDataOffset);
            int64_t  delta      = (int64_t)blockStart - (int64_t)m.address;

            // results[i] is exclusively owned by this thread — no lock needed
            results[i].linkedAddress = blockStart;
            results[i].status        = MapLinkResult::Exact;
            results[i].confidence    = (blockStart == m.address) ? 100 : 95;

            QMutexLocker lk(&histMutex);
            deltaHistogram[delta]++;
        });
    }

    // ── Phase 2: find dominant delta ─────────────────────────────────────────
    emit_progress(QObject::tr("Phase 2: computing dominant address delta…"), 40);

    int64_t dominantDelta = 0;
    int     bestCount     = 0;
    for (auto it = deltaHistogram.constBegin(); it != deltaHistogram.constEnd(); ++it) {
        if (it.value() > bestCount) {
            bestCount     = it.value();
            dominantDelta = it.key();
        }
    }
    session.dominantDelta = dominantDelta;

    // ── Phase 3: fuzzy search for unmatched maps (parallel) ──────────────────
    emit_progress(QObject::tr("Phase 3: fuzzy search for unmatched maps…"), 50);

    const int kFuzzyWindow = 0x4000;  // ±16 KB around dominant delta hint

    {
        // Collect indices of still-unmatched maps
        QVector<int> unmatched;
        unmatched.reserve(maps.size());
        for (int i = 0; i < maps.size(); ++i)
            if (results[i].status == MapLinkResult::NotFound)
                unmatched.append(i);

        QtConcurrent::blockingMap(unmatched, [&](int i) {
            const MapInfo &m = maps[i];
            MapLinkResult &r = results[i];   // exclusively owned by this thread

            QByteArray fp = makeFingerprint(refRom, m);
            if (fp.isEmpty()) return;

            int64_t expectedCellAddr = (int64_t)m.address + m.mapDataOffset + dominantDelta;
            int64_t winStart = qMax((int64_t)0, expectedCellAddr - kFuzzyWindow);
            int64_t winEnd   = qMin((int64_t)targetRom.size() - fp.size(),
                                    expectedCellAddr + kFuzzyWindow);

            if (winStart > winEnd) {
                qint64 found = searchBytes(targetRom, fp);
                if (found >= 0) {
                    r.linkedAddress = (uint32_t)qMax((qint64)0, found - (qint64)m.mapDataOffset);
                    r.status        = MapLinkResult::Fuzzy;
                    r.confidence    = 60;
                }
                return;
            }

            QByteArray slice = targetRom.mid((int)winStart, (int)(winEnd - winStart + fp.size()));
            qint64 found = searchBytes(slice, fp);
            if (found >= 0) {
                uint32_t cellAddr = (uint32_t)(winStart + found);
                r.linkedAddress   = (uint32_t)qMax((qint64)0, (qint64)cellAddr - (qint64)m.mapDataOffset);
                r.status          = MapLinkResult::Fuzzy;
                r.confidence      = 80;
            }
        });
    }

    // ── Phase 4 (moved up): independently locate each unique axis-data block ──
    // Axis breakpoints (RPM, load, etc.) are usually identical between ROM
    // variants even when calibration values change. Run this BEFORE the delta
    // fallback so Phase 3.5 can use the results to recover unmatched maps.
    emit_progress(QObject::tr("Phase 4: locating axis data independently…"), 88);

    {
        // Collect unique axis descriptors first (fast, sequential)
        struct AxisTask { uint32_t refAddr; int count; int dsize; };
        QVector<AxisTask> axisTasks;
        QSet<uint32_t> seenAxes;
        for (const MapInfo &m : maps) {
            auto collect = [&](uint32_t refAddr, int count, int dsize) {
                if (!seenAxes.contains(refAddr)) {
                    seenAxes.insert(refAddr);
                    axisTasks.append({refAddr, count, dsize});
                }
            };
            if (m.xAxis.hasPtsAddress && m.xAxis.ptsCount > 0)
                collect(m.xAxis.ptsAddress, m.xAxis.ptsCount, m.xAxis.ptsDataSize);
            if (m.yAxis.hasPtsAddress && m.yAxis.ptsCount > 0)
                collect(m.yAxis.ptsAddress, m.yAxis.ptsCount, m.yAxis.ptsDataSize);
        }

        // Search each unique axis in parallel
        QMutex axMutex;
        QtConcurrent::blockingMap(axisTasks, [&](const AxisTask &t) {
            int len = qMin(t.count * t.dsize, 32);
            if (len < 4) return;
            if ((int)t.refAddr + len > refRom.size()) return;
            QByteArray fp = refRom.mid((int)t.refAddr, len);
            qint64 found = searchBytes(targetRom, fp);
            if (found >= 0) {
                QMutexLocker lk(&axMutex);
                session.axisOffsets[t.refAddr] = (uint32_t)found;
            }
        });
    }

    // ── Phase 3.5: recover unmatched maps via their STD_AXIS location ─────────
    // Maps with inline axes (mapDataOffset > 0) can be located by finding where
    // their x-axis data landed in the target ROM, then stepping back by the
    // known offset of the axis within the block. This recovers maps whose
    // calibration values changed (cell fingerprint fails) but whose axis
    // breakpoints didn't (axis fingerprint succeeds — the common case).
    emit_progress(QObject::tr("Phase 3.5: recovering unmatched maps via axis data…"), 91);

    for (int i = 0; i < maps.size(); ++i) {
        if (results[i].status != MapLinkResult::NotFound) continue;

        const MapInfo &m = maps[i];
        MapLinkResult &r = results[i];

        // Only applicable to maps with inline STD_AXIS (mapDataOffset > 0)
        if (m.mapDataOffset == 0 || !m.xAxis.hasPtsAddress) continue;

        if (!session.axisOffsets.contains(m.xAxis.ptsAddress)) continue;

        uint32_t targetAxisAddr = session.axisOffsets[m.xAxis.ptsAddress];
        // Axis offset within the block = ptsAddress - m.address
        uint32_t axisOffInBlock = m.xAxis.ptsAddress - m.address;
        if (targetAxisAddr < axisOffInBlock) continue;  // would underflow
        uint32_t blockStart = targetAxisAddr - axisOffInBlock;

        // Sanity check: verify the target ROM has room for the full block
        uint32_t blockTotal = m.mapDataOffset + (uint32_t)m.length;
        if ((int64_t)blockStart + blockTotal > (int64_t)targetRom.size()) continue;

        // Optional: verify count byte(s) at blockStart match expected dimensions.
        // For MAP: bytes 0-1 = count_x (big-endian UWORD), bytes 2-3 = count_y.
        // For CURVE: bytes 0-1 = count_x.
        // Only check if the axis offset is > 0 (i.e., count bytes precede it).
        if (axisOffInBlock >= 2) {
            const uint8_t *tgt = reinterpret_cast<const uint8_t *>(targetRom.constData());
            uint16_t nx_check = (uint16_t)((tgt[blockStart] << 8) | tgt[blockStart + 1]);
            if (nx_check != (uint16_t)m.dimensions.x) continue; // count mismatch
        }

        r.linkedAddress = blockStart;
        r.status        = MapLinkResult::Fuzzy;
        r.confidence    = 75;   // high confidence — axis located exactly
        deltaHistogram[(int64_t)blockStart - (int64_t)m.address]++;
    }

    // ── Phase 5: neighbour-interpolated delta for still-unmatched maps ───────
    emit_progress(QObject::tr("Phase 5: neighbour-interpolated delta for remaining maps…"), 95);

    // Build sorted anchor list from all successfully matched maps
    struct Anchor { uint32_t refAddr; int64_t delta; };
    QVector<Anchor> anchors;
    anchors.reserve(results.size());
    for (int i = 0; i < maps.size(); ++i) {
        if (results[i].status != MapLinkResult::NotFound) {
            anchors.append({maps[i].address,
                            (int64_t)results[i].linkedAddress - (int64_t)maps[i].address});
        }
    }
    std::sort(anchors.begin(), anchors.end(),
              [](const Anchor &a, const Anchor &b) { return a.refAddr < b.refAddr; });

    for (int i = 0; i < maps.size(); ++i) {
        if (results[i].status != MapLinkResult::NotFound) continue;
        const MapInfo &m = maps[i];
        MapLinkResult &r = results[i];

        int64_t localDelta = dominantDelta;
        int     localConf  = 40;

        if (!anchors.isEmpty()) {
            // Binary search: first anchor with refAddr >= m.address
            int lo = 0, hi = anchors.size();
            while (lo < hi) {
                int mid = (lo + hi) / 2;
                if (anchors[mid].refAddr < m.address) lo = mid + 1;
                else hi = mid;
            }
            const Anchor *below = (lo > 0)               ? &anchors[lo - 1] : nullptr;
            const Anchor *above = (lo < anchors.size())   ? &anchors[lo]     : nullptr;

            if (below && above) {
                if (std::abs(below->delta - above->delta) <= 0x100) {
                    // Neighbours agree — interpolate and promote to 60
                    localDelta = (below->delta + above->delta) / 2;
                    localConf  = 60;
                } else {
                    // Disagreement — use the closer neighbour's delta at 40
                    uint32_t dBelow = m.address - below->refAddr;
                    uint32_t dAbove = above->refAddr - m.address;
                    localDelta = (dBelow <= dAbove) ? below->delta : above->delta;
                    localConf  = 40;
                }
            } else if (below) {
                localDelta = below->delta;
                localConf  = 50;
            } else if (above) {
                localDelta = above->delta;
                localConf  = 50;
            }
        }

        int64_t guessAddr = (int64_t)m.address + localDelta;
        if (guessAddr >= 0 && guessAddr < (int64_t)targetRom.size()) {
            r.linkedAddress = (uint32_t)guessAddr;
            r.status        = MapLinkResult::Fuzzy;
            r.confidence    = localConf;
        }
    }

    // ── Finalise ─────────────────────────────────────────────────────────────
    session.results = results;
    for (const auto &r : results)
        if (r.status != MapLinkResult::NotFound) session.matchedCount++;

    emit_progress(QObject::tr("Done — %1/%2 maps linked (delta: %3%4), %5 axes located")
                  .arg(session.matchedCount)
                  .arg(session.totalCount)
                  .arg(dominantDelta >= 0 ? "+" : "")
                  .arg(dominantDelta)
                  .arg(session.axisOffsets.size()), 100);

    return session;
}

// ── Async wrapper ─────────────────────────────────────────────────────────────

void RomLinker::linkAsync(const QByteArray &refRom,
                          const QByteArray &targetRom,
                          const QVector<MapInfo> &maps,
                          const QString &label)
{
    // Capture by value for the lambda
    QByteArray ref    = refRom;
    QByteArray target = targetRom;
    QVector<MapInfo> mapsSnap = maps;
    QString lbl = label;

    auto *watcher = new QFutureWatcher<RomLinkSession>(this);
    connect(watcher, &QFutureWatcher<RomLinkSession>::finished, this, [this, watcher]() {
        emit finished(watcher->result());
        watcher->deleteLater();
    });

    // Progress relay — we can't directly emit from a worker thread,
    // but we approximate with periodic signals using a queued connection pattern.
    // For simplicity the progress callback captures 'this' via a thread-safe relay.
    QFuture<RomLinkSession> future = QtConcurrent::run([ref, target, mapsSnap, lbl, this]() {
        return RomLinker::link(ref, target, mapsSnap, lbl,
            [this](const QString &msg, int pct) {
                // QtConcurrent thread → queued signal
                QMetaObject::invokeMethod(this, [this, msg, pct]() {
                    emit progress(msg, pct);
                }, Qt::QueuedConnection);
            });
    });
    watcher->setFuture(future);
}
