/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/RomFingerprint.h"

#include <QDataStream>
#include <QIODevice>
#include <QtEndian>
#include <cstring>

#include <algorithm>

namespace winols {

namespace {

// Karp-Rabin rolling-hash parameters.
//   BASE: small prime.  31 is the canonical choice (used in Java's
//         String.hashCode() and many Rabin-Karp implementations).
//   MOD : a prime close to 2^63 to fit hash * BASE in a 128-bit
//         intermediate without overflow when computed via __uint128_t.
constexpr quint64 kBase     = 31ULL;
constexpr quint64 kMod      = (1ULL << 63) - 25;   // 9223372036854775783
constexpr quint64 kEmptySig = 0ULL;                 // sentinel: bucket unused

// 128-bit multiply-then-mod.  mingw13 / GCC provide __uint128_t.
inline quint64 mulmod(quint64 a, quint64 b)
{
    return quint64((__uint128_t(a) * b) % kMod);
}

// Walk every byte position of `bytes`, emitting the Karp-Rabin hash of
// each n-byte shingle.  For each emitted hash, slot it into the K-bucket
// MinHash sketch by keeping the minimum seen in `hash & (K-1)`.
//
// O(N) total work: one mulmod + one mod-correction per byte.
QList<quint64> oneShingleSketch(const uint8_t *raw, qsizetype N)
{
    QList<quint64> sig(kMinHashK, kEmptySig);
    if (N < kShingleSize) return sig;

    constexpr quint64 mask = kMinHashK - 1;
    static_assert((kMinHashK & (kMinHashK - 1)) == 0,
                  "kMinHashK must be a power of two");

    // Initial shingle: hash of bytes [0, kShingleSize).
    quint64 h = 0;
    for (int i = 0; i < kShingleSize; ++i)
        h = (mulmod(h, kBase) + raw[i]) % kMod;

    // pow_n = BASE^kShingleSize mod MOD — used to subtract the
    // outgoing byte's contribution as the window slides.
    quint64 powN = 1;
    for (int i = 0; i < kShingleSize; ++i) powN = mulmod(powN, kBase);

    auto offer = [&](quint64 hh) {
        if (hh == kEmptySig) hh = 1;     // never store the sentinel
        const quint64 b = hh & mask;
        if (sig[b] == kEmptySig || sig[b] > hh) sig[b] = hh;
    };
    offer(h);

    // Slide the shingle one byte at a time.  Each step: drop the
    // outgoing byte (raw[i - kShingleSize]) and add the incoming
    // byte (raw[i]).  All arithmetic mod kMod.
    for (qsizetype i = kShingleSize; i < N; ++i) {
        const quint64 outgoing = mulmod(raw[i - kShingleSize], powN);
        // h = h * BASE - outgoing + raw[i]   (mod MOD)
        // Add MOD before subtraction to avoid unsigned underflow.
        h = (mulmod(h, kBase) + kMod - outgoing + raw[i]) % kMod;
        offer(h);
    }
    return sig;
}

}  // namespace

bool RomFingerprint::isEmpty() const
{
    if (wholeFile.size() != kMinHashK) return true;
    for (quint64 h : wholeFile) if (h != kEmptySig) return false;
    return true;
}

RomFingerprint fingerprint(QByteArrayView romBytes)
{
    RomFingerprint fp;
    if (romBytes.size() < kShingleSize) return fp;

    const auto *raw = reinterpret_cast<const uint8_t *>(romBytes.constData());
    const qsizetype N = romBytes.size();

    // Strip leading / trailing 0x00/0xFF runs (linker padding).
    qsizetype lo = 0;
    while (lo < N && (raw[lo] == 0x00 || raw[lo] == 0xFF)) ++lo;
    qsizetype hi = N;
    while (hi > lo && (raw[hi - 1] == 0x00 || raw[hi - 1] == 0xFF)) --hi;
    fp.bytesScanned = hi - lo;
    if (fp.bytesScanned < kShingleSize) return fp;

    fp.wholeFile = oneShingleSketch(raw + lo, hi - lo);
    // dataArea is reserved; not populated by this path.
    fp.dataArea = QList<quint64>(kMinHashK, kEmptySig);
    return fp;
}

SimilarityScore similarity(const RomFingerprint &a, const RomFingerprint &b)
{
    SimilarityScore s;
    auto matchPct = [](const QList<quint64> &x, const QList<quint64> &y) -> double {
        if (x.size() != kMinHashK || y.size() != kMinHashK) return 0.0;
        int matched = 0, valid = 0;
        for (int i = 0; i < kMinHashK; ++i) {
            if (x[i] == kEmptySig || y[i] == kEmptySig) continue;
            ++valid;
            if (x[i] == y[i]) ++matched;
        }
        return valid > 0 ? double(matched) / double(valid) : 0.0;
    };
    s.wholeFile = matchPct(a.wholeFile, b.wholeFile);
    // dataArea sketch is reserved (currently empty) — Stage 2 fills
    // the precise data-area % at query time, not at indexing time.
    s.dataArea = matchPct(a.dataArea, b.dataArea);

    // Size-mismatch dampener: ROMs differing > 2× in scanned-byte
    // length probably belong to different ECU families.
    if (a.bytesScanned > 0 && b.bytesScanned > 0) {
        const double ratio = double(std::max(a.bytesScanned, b.bytesScanned))
                           / double(std::min(a.bytesScanned, b.bytesScanned));
        if (ratio > 2.0) {
            const double penalty = std::min(0.4, (ratio - 2.0) * 0.15);
            s.wholeFile *= (1.0 - penalty);
            s.dataArea  *= (1.0 - penalty);
        }
    }
    return s;
}

// ─── Serialisation (SQLite BLOB) ────────────────────────────────────────────
// Format (one-perm MinHash, version 3):
//   magic    "RFP3"           4 bytes
//   bytesScanned              8 bytes (little-endian qint64)
//   nWhole                    4 bytes  (== kMinHashK or 0)
//   nData                     4 bytes  (== kMinHashK or 0)
//   wholeFile[]               nWhole * 8 bytes  (per-bucket min, 0 = empty)
//   dataArea[]                nData  * 8 bytes
// Total: 20 + (nWhole + nData) * 8 ≤ 2068 bytes per fingerprint.
//
// RFP1 (unbounded set) and RFP2 (bottom-K sorted) are no longer accepted.
// Old indexes need a one-time `Build similarity index…` rebuild — the
// indexer purges non-RFP3 rows on startup so they get re-fingerprinted.

QByteArray RomFingerprint::toBlob() const
{
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RFP3", 4);
    s << qint64(bytesScanned)
      << qint32(wholeFile.size())
      << qint32(dataArea.size());
    for (quint64 h : wholeFile) s << h;
    for (quint64 h : dataArea)  s << h;
    return out;
}

RomFingerprint RomFingerprint::fromBlob(const QByteArray &blob)
{
    RomFingerprint fp;
    if (blob.size() < 20) return fp;
    QDataStream s(blob);
    s.setByteOrder(QDataStream::LittleEndian);
    char magic[4] = {0,0,0,0};
    s.readRawData(magic, 4);
    if (std::memcmp(magic, "RFP3", 4) != 0) return fp;
    qint64 scanned;
    qint32 nWhole, nData;
    s >> scanned >> nWhole >> nData;
    if (nWhole < 0 || nWhole > kMinHashK * 4) return fp;
    if (nData  < 0 || nData  > kMinHashK * 4) return fp;
    fp.bytesScanned = scanned;
    fp.wholeFile.reserve(nWhole);
    fp.dataArea.reserve(nData);
    quint64 h;
    for (qint32 i = 0; i < nWhole; ++i) { s >> h; fp.wholeFile.append(h); }
    for (qint32 i = 0; i < nData;  ++i) { s >> h; fp.dataArea.append(h);  }
    return fp;
}

}  // namespace winols
