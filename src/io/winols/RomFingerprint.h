/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Alignment-independent fuzzy fingerprint for ECU ROM files.
 * =================================================================
 *
 * Uses **one-permutation MinHash with bucketing** over byte-level
 * Karp-Rabin rolling-hash shingles:
 *
 *  1. Slide an n-byte shingle across every byte position of the ROM.
 *     The shingle hash is updated in O(1) per byte via the Karp-Rabin
 *     recurrence, so the whole file is hashed in O(N).
 *  2. For each shingle's hash h, compute bucket = h mod K.  In the
 *     bucket, keep only the SMALLEST hash seen so far.
 *  3. The fingerprint is the K-element vector of per-bucket minima.
 *
 * Why this beats the previous (bottom-K window) approach:
 *
 *   * **Position-independent.**  Two ROMs that share identical content
 *     but at different file offsets (e.g. raw `.bin` vs `.ols` inner
 *     ROM extracted with an 80-byte container header) produce
 *     overlapping shingles regardless of alignment.  Our previous
 *     fixed-stride 4 KB / 256 B-step windowing failed at this:
 *     a 76-byte shift dropped match% to zero.
 *
 *   * **Discriminative.**  Bucketing splits the hash space into K
 *     independent slots.  A bucket's minimum reflects content WITHIN
 *     that slot, not the globally-rarest hash.  Two unrelated ROMs
 *     from the same family no longer collide on shared rare patterns
 *     (vector tables, `0x00` padding) — they only match where they
 *     share specific byte content.
 *
 * Jaccard similarity is estimated as the fraction of buckets where
 * both fingerprints have an identical hash:
 *
 *     J(A, B)  ≈  count(i : A.sig[i] == B.sig[i] != 0)
 *                 ────────────────────────────────────────
 *                 count(i : A.sig[i] != 0 ∧ B.sig[i] != 0)
 *
 * For K = 128 the standard error on Jaccard is ≈ 1/√K ≈ 8 %.
 *
 * **Storage**: 4 + 8 + 4 + 4 + 128·8 + 128·8 ≈ 2 KB per file.  500k
 * fingerprints fit in ~1 GB on disk.
 *
 * **Note on `.ols` containers**: the indexer extracts the inner ROM
 * via OlsImporter before fingerprinting.  For multi-version `.ols`
 * (Original + Stage 1 + …), the stored fingerprint is the per-bucket
 * MIN over all versions — i.e. a sketch of the union of all versions'
 * shingles.  This makes the file match if ANY of its versions is
 * similar to the needle.
 *
 * **Stage 2 verification** (separate module): for top-N hits from this
 * sketch screen, the candidate is reopened, aligned to the needle by
 * byte search, and an exact byte-identity is computed over the
 * overlapping range — and over each map region declared in the .ols.
 * That gives WinOLS-grade "% Project" / "% Data area" precision.
 */

#pragma once

#include <QByteArrayView>
#include <QList>
#include <QString>
#include <QtTypes>

namespace winols {

/// Sketch size: K buckets in the one-permutation MinHash.  K = 128
/// gives ≈ 8 % standard error on Jaccard and ≈ 2 KB per file on disk.
/// MUST be a power of two (we use `hash & (K-1)` for bucket index).
constexpr int kMinHashK = 128;

/// Shingle (rolling-hash window) length in bytes.  Each shingle is the
/// content fingerprinted into one MinHash slot.  64 B is short enough
/// that small calibration patches don't destroy whole regions of
/// shingles, long enough to be content-distinguishing.
constexpr int kShingleSize = 64;

struct RomFingerprint {
    /// One-permutation MinHash sketch of byte-level shingles (size K,
    /// bucketed by hash mod K, each slot holds the smallest hash seen
    /// in that bucket; 0 = empty).  Position-independent.
    QList<quint64> wholeFile;
    /// (Reserved for future cal-only sketch — currently unused; the
    /// indexer leaves this empty.  WinOLS-grade "% Data area" comes
    /// from the Stage 2 verifier, not from this sketch.)
    QList<quint64> dataArea;
    /// Logical size after empty-padding strip.  Used for size-mismatch
    /// penalty in `similarity()`.
    qint64        bytesScanned = 0;

    bool isEmpty() const;

    /// Serialise to a binary blob, prefixed with magic "RFP3".
    /// Designed for SQLite BLOB storage in the similarity index.
    QByteArray toBlob() const;
    static RomFingerprint fromBlob(const QByteArray &blob);
};

struct SimilarityScore {
    double wholeFile = 0.0;     // 0..1, Jaccard on wholeFile sets
    double dataArea  = 0.0;     // 0..1, Jaccard on dataArea sets

    int  wholePct() const { return int(wholeFile * 100.0 + 0.5); }
    int  dataPct()  const { return int(dataArea  * 100.0 + 0.5); }
};

/// Compute a fingerprint over @p romBytes (raw ROM, not .ols container).
/// Cheap: ~30 ms on a 2 MB ROM.  Thread-safe — keep no global state.
RomFingerprint  fingerprint(QByteArrayView romBytes);

/// Jaccard-style comparison of two fingerprints.  Order-independent.
SimilarityScore similarity(const RomFingerprint &a,
                           const RomFingerprint &b);

}  // namespace winols
