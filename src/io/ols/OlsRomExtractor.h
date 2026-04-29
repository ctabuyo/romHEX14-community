/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace ols {

struct OlsSegment {
    int       segmentIndex = 0;
    qsizetype projOffset   = 0;   // file offset of the project-marker string
    qsizetype dataStart    = 0;   // file offset of flash data (= projOffset - 26)
    uint32_t  flashBase    = 0;   // absolute flash address of segment start
    uint32_t  flashSize    = 0;   // number of flash bytes in this segment
    uint32_t  hash         = 0;   // per-segment content hash from descriptor
    uint32_t  framingBytes = 0;
    QByteArray data;              // raw flash bytes (length == flashSize)
    QByteArray preamble;
    bool      isPrimary = true;
};

struct OlsRomResult {
    int                 versionIndex = -1;
    QVector<OlsSegment> segments;
    QByteArray          assembledRom;  // all segments concatenated
    QString             error;
    QStringList         warnings;
};

class OlsRomExtractor {
    Q_DECLARE_TR_FUNCTIONS(ols::OlsRomExtractor)
public:
    static QVector<OlsRomResult> extractAll(const QByteArray &fileData);

    static qsizetype flashToFileOffset(const QVector<OlsSegment> &segments,
                                       uint32_t flashAddr);

    static QByteArray assemble(const QVector<OlsSegment> &segments);
};

} // namespace ols
