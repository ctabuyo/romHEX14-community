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

// A complete snapshot of one map's raw cell data plus display metadata.
struct MapPackEntry {
    // Identity / geometry
    QString    name;
    QString    description;       // human-readable / localised name from A2L
    int        cols          = 1;
    int        rows          = 1;
    int        dataSize      = 2;
    bool       bigEndian     = true;
    bool       columnMajor   = false;
    uint32_t   address       = 0;  // file offset in the target (reference) ROM
    uint32_t   mapDataOffset = 0;  // byte offset from address to actual cell data
    QByteArray data;               // raw cell bytes: cols * rows * dataSize

    // Axis display (pre-computed physical values so no A2L needed on import)
    QString         xAxisName;    // e.g. "n_mot"
    QString         xAxisUnit;    // e.g. "1/min"
    QVector<double> xAxisValues;  // one per column

    QString         yAxisName;
    QString         yAxisUnit;
    QVector<double> yAxisValues;  // one per row

    // Cell value scaling
    QString     zUnit;
    bool        hasScaling = false;
    CompuMethod scaling;
};

// A collection of map snapshots that can be applied to a target ROM.
// Serialised as JSON with extension .rxpack
struct MapPack {
    int     version = 1;
    QString created;
    QString label;
    QVector<MapPackEntry> maps;

    // Build from diff results — captures the compare ROM's map data.
    static MapPack fromDiffs(const QVector<MapDiff> &diffs,
                             const QByteArray &cmpRom,
                             ByteOrder bo,
                             const QString &label = {});

    // Build from an explicit selection of maps and a ROM.
    static MapPack fromMaps(const QByteArray &rom,
                            const QVector<MapInfo> &selectedMaps,
                            ByteOrder bo,
                            const QString &label = {});

    QJsonDocument toJson() const;
    static MapPack fromJson(const QJsonDocument &doc, QString *error = nullptr);

    bool save(const QString &path, QString *error = nullptr) const;
    static MapPack load(const QString &path, QString *error = nullptr);

    // Apply to a writable ROM buffer.
    QStringList apply(QByteArray &rom,
                      const QVector<MapInfo> &projectMaps,
                      ByteOrder bo,
                      const QMap<QString, uint32_t> &mapOffsets = {}) const;
};
