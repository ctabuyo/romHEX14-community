/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QVector>
#include <cstdint>

namespace rx14fmt {

// ── File header (64 bytes on disk) ──────────────────────────────────────────
struct FileHeader {
    uint32_t magic         = 0x52583134; // "RX14"
    uint32_t formatVersion = 1;
    uint64_t totalFileSize = 0;
    uint32_t tocOffset     = 0;
    uint32_t tocBlockCount = 0;
    QByteArray bodyChecksum;             // 16 bytes (blake3 truncated)
};

// ── TLV block header (32 bytes on disk) ─────────────────────────────────────
struct BlockHeader {
    uint32_t blockMagic    = 0;
    uint32_t blockSchema   = 0;
    uint64_t payloadSize   = 0;
    QByteArray blockChecksum;            // 16 bytes (blake3 of payload)
};

// ── TOC entry (20 bytes on disk) ────────────────────────────────────────────
struct TocEntry {
    uint32_t blockMagic  = 0;
    uint64_t blockOffset = 0;
    uint64_t blockSize   = 0;
};

// ── Known block magic constants ─────────────────────────────────────────────
constexpr uint32_t BLK_META = 0x4154454D; // "META"
constexpr uint32_t BLK_NOTS = 0x53544F4E; // "NOTS"
constexpr uint32_t BLK_ROM0 = 0x304D4F52; // "ROM0"
constexpr uint32_t BLK_ROMO = 0x4F4D4F52; // "ROMO"
constexpr uint32_t BLK_MAPS = 0x5350414D; // "MAPS"
constexpr uint32_t BLK_A2L0 = 0x304C3241; // "A2L0"
constexpr uint32_t BLK_GRPS = 0x53505247; // "GRPS"
constexpr uint32_t BLK_VERS = 0x53524556; // "VERS"
constexpr uint32_t BLK_LINK = 0x4B4E494C; // "LINK"
constexpr uint32_t BLK_STAR = 0x52415453; // "STAR"
constexpr uint32_t BLK_AMAP = 0x50414D41; // "AMAP" — auto-detected map overlay (persists across reloads so the overlay survives close→reopen without re-running the scan)
constexpr uint32_t TOC_MAGIC = 0x434F5400; // "TOC\0"

// ── On-disk sizes ───────────────────────────────────────────────────────────
constexpr int FILE_HEADER_SIZE  = 64;
constexpr int BLOCK_HEADER_SIZE = 32;
constexpr int TOC_ENTRY_SIZE    = 20;
constexpr int TOC_PREAMBLE_SIZE = 8;  // tocMagic(4) + reserved(4)

// ── Reader ──────────────────────────────────────────────────────────────────
FileHeader          readHeader(QIODevice *in, QString *err = nullptr);
QVector<TocEntry>   readToc(QIODevice *in, const FileHeader &hdr, QString *err = nullptr);
BlockHeader         readBlockHeader(QIODevice *in, QString *err = nullptr);

// ── Writer helpers ──────────────────────────────────────────────────────────
void writeHeader(QIODevice *out, const FileHeader &hdr);
void writeBlockHeader(QIODevice *out, const BlockHeader &hdr);
void writeTocEntry(QIODevice *out, const TocEntry &entry);
void writeTocMagic(QIODevice *out);

// ── Utility ─────────────────────────────────────────────────────────────────
/// Convert a 4-char ASCII tag to its uint32_t magic value.
constexpr uint32_t tagToMagic(const char tag[4])
{
    return static_cast<uint32_t>(tag[0])
         | (static_cast<uint32_t>(tag[1]) << 8)
         | (static_cast<uint32_t>(tag[2]) << 16)
         | (static_cast<uint32_t>(tag[3]) << 24);
}

/// Convert a uint32_t magic value to a 4-char ASCII string.
QString magicToTag(uint32_t magic);

} // namespace rx14fmt
