/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "OlsVersionDirectory.h"
#include "CArchiveReader.h"
#include <QtEndian>

namespace ols {

static constexpr uint32_t kM1Magic = 0x42007899;

OlsVersionDirectory OlsVersionDirectory::parse(const QByteArray &fileData,
                                                QStringList *warnings)
{
    OlsVersionDirectory dir;

    const qsizetype scanLimit = qMin(fileData.size(),
                                     static_cast<qsizetype>(4096));
    const QByteArray needle(reinterpret_cast<const char *>(&kM1Magic), 4);
    const qsizetype anchorPos = fileData.indexOf(needle, 0);

    if (anchorPos < 0 || anchorPos >= scanLimit) {
        if (warnings)
            warnings->append(
                QStringLiteral("M1 anchor 0x42007899 not found in preamble"));
        return dir;
    }

    const qsizetype hdrOfs = anchorPos - 12;
    if (hdrOfs < 0 || hdrOfs + 16 > fileData.size()) {
        if (warnings)
            warnings->append(
                QStringLiteral("directory header at 0x%1 out of bounds")
                    .arg(hdrOfs, 0, 16));
        return dir;
    }

    const auto *raw = reinterpret_cast<const uchar *>(fileData.constData() + hdrOfs);
    const uint32_t nMinus1 = qFromLittleEndian<uint32_t>(raw);
    const uint32_t vStart  = qFromLittleEndian<uint32_t>(raw + 4);
    const uint32_t vRecSz  = qFromLittleEndian<uint32_t>(raw + 8);

    dir.numVersions       = nMinus1 + 1;
    dir.versionDataStart  = vStart;
    dir.versionRecordSize = vRecSz;

    const qsizetype expectedEnd =
        static_cast<qsizetype>(vStart) + 4
        + static_cast<qsizetype>(vRecSz) * dir.numVersions;

    if (expectedEnd != fileData.size()) {
        if (warnings)
            warnings->append(
                QStringLiteral("version-directory sanity: expected file size %1, got %2")
                    .arg(expectedEnd)
                    .arg(fileData.size()));
    }

    if (vRecSz == 0 || vStart >= static_cast<uint32_t>(fileData.size())) {
        if (warnings)
            warnings->append(
                QStringLiteral("versionRecordSize=%1 or versionDataStart=0x%2 invalid")
                    .arg(vRecSz)
                    .arg(vStart, 0, 16));
        dir.numVersions = 0;
        return dir;
    }

    dir.versionSlots.resize(static_cast<int>(dir.numVersions));
    for (uint32_t i = 0; i < dir.numVersions; ++i) {
        auto &slot = dir.versionSlots[static_cast<int>(i)];
        slot.index = static_cast<int>(i);
        slot.slotFileOffset = dir.slotOffset(static_cast<int>(i));
    }

    return dir;
}

}
