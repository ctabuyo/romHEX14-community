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

struct MapAutoDetectOptions {
    bool tryBigEndianAxes        = false;  ///< also test u16 BE axes (rare; Mazda Denso)
    int  minScore2D              = 60;     ///< drop 2D candidates below this
    int  minScore1D              = 65;     ///< drop 1D curves below this
    int  maxCandidatesPerRegion  = 20000;
    int  maxAxesPerRegion        = 2048;   ///< cap axis-search work per cell-type
};

class MapAutoDetect {
    Q_DECLARE_TR_FUNCTIONS(ols::MapAutoDetect)
public:
    static QVector<MapCandidate> scan(const QByteArray &rom,
                                       quint32 baseAddress,
                                       const MapAutoDetectOptions &opts = {});

    static QVector<MapCandidate> scanProject(const Project &project,
                                              const MapAutoDetectOptions &opts = {});
};

} // namespace ols
