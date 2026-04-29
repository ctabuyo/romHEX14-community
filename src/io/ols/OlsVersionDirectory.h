/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace ols {

class CArchiveReader;

struct OlsVersionSlot {
    int      index = -1;
    qsizetype slotFileOffset = 0;
};

struct OlsVersionDirectory {
    uint32_t numVersions       = 0;
    uint32_t versionDataStart  = 0;
    uint32_t versionRecordSize = 0;

    QVector<OlsVersionSlot> versionSlots;

    qsizetype slotOffset(int i) const {
        return static_cast<qsizetype>(versionDataStart) + 4
             + static_cast<qsizetype>(versionRecordSize) * i;
    }

    static OlsVersionDirectory parse(const QByteArray &fileData,
                                     QStringList *warnings = nullptr);
};

}
