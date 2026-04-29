/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include "romdata.h"
#include "romcompare.h"

// One cell modification: memory-flat index, expected raw value, new raw value.
struct CellPatch {
    int      memIdx  = 0;
    uint32_t refVal  = 0;   // value in reference ROM (used for verification)
    uint32_t newVal  = 0;   // value to write
};

// A single raw byte change at an absolute ROM offset.
// Used to capture checksum/CRC bytes and other non-map differences.
struct RawBytePatch {
    uint32_t offset = 0;
    uint8_t  refVal = 0;
    uint8_t  newVal = 0;
};

// Per-map list of changed cells.
struct MapPatch {
    QString            name;
    int                cols          = 1;
    int                rows          = 1;
    int                dataSize      = 2;
    bool               bigEndian     = true;
    bool               columnMajor   = false;
    // Bulletproof fields (added in format v2):
    uint32_t           sourceAddress = 0;   // ROM file byte offset of map data start
    uint32_t           dataCrc32     = 0;   // CRC32 of source ROM bytes at that location
    int                totalCells    = 0;   // total cells (cols*rows) in source map
    QVector<CellPatch> cells;
};

// Per-map result of applying a patch.
struct MapApplyResult {
    enum Status { Applied, AppliedWithWarnings, Failed };
    Status  status   = Failed;
    QString mapName;
    QString detail;   // human-readable explanation
    int     applied  = 0;   // cells successfully written
    int     mismatches = 0; // cells whose refVal didn't match (still written)
};

struct PatchApplyResult {
    QVector<MapApplyResult> maps;
    int rawBytesApplied   = 0;
    int rawBytesMismatched = 0;   // refVal didn't match (still written)
    bool hasFailures()  const;
    bool hasWarnings()  const;
    QStringList summary() const;   // one line per map
};

// A patch script: a list of map patches that can be applied to a target ROM.
// Serialised as JSON with extension .rxpatch
struct RomPatch {
    int     version     = 3;
    QString created;
    QString label;
    QString sourceLabel;
    QString targetLabel;
    QVector<MapPatch>     maps;
    QVector<RawBytePatch> rawBytes;   // checksum/non-map byte changes

    // Build from diff results (reads raw values from refRom and cmpRom).
    // If includeRawBytes is true, all byte differences outside A2L map regions
    // are also captured (checksums, CRC tables, etc.) for exact reproduction.
    static RomPatch fromDiffs(const QVector<MapDiff> &diffs,
                              const QByteArray &refRom,
                              const QByteArray &cmpRom,
                              ByteOrder bo,
                              bool includeRawBytes,
                              const QString &srcLabel = {},
                              const QString &tgtLabel = {});

    QJsonDocument toJson() const;
    static RomPatch fromJson(const QJsonDocument &doc, QString *error = nullptr);

    bool save(const QString &path, QString *error = nullptr) const;
    static RomPatch load(const QString &path, QString *error = nullptr);

    // Apply to a writable ROM buffer.
    // projectMaps: map list from the project (for address lookup).
    // mapOffsets:  optional override addresses (e.g. from a RomLinkSession).
    // Fallback chain per map:
    //   1. Name match → address agree → CRC check → apply
    //   2. Name match → address mismatch → trust name, warn on CRC mismatch
    //   3. Name not found → address-only apply using sourceAddress → verify refVals
    PatchApplyResult apply(QByteArray &rom,
                           const QVector<MapInfo> &projectMaps,
                           ByteOrder bo,
                           const QMap<QString, uint32_t> &mapOffsets = {}) const;
};
