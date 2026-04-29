/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "OlsMagicScanner.h"
#include <cstring>

namespace ols {

static qint64 findMagic(const QByteArray &buf, qsizetype limit, uint32_t magic)
{
    if (limit > buf.size())
        limit = buf.size();
    if (limit < 4)
        return -1;

    uchar needle[4];
    qToLittleEndian<uint32_t>(magic, needle);

    const char *haystack = buf.constData();
    const qsizetype end = limit - 3;

    for (qsizetype i = 0; i < end; ++i) {
        if (std::memcmp(haystack + i, needle, 4) == 0)
            return static_cast<qint64>(i);
    }
    return -1;
}

MagicAnchors OlsMagicScanner::scan(const QByteArray &fileData, int /*preambleLen*/)
{
    const qsizetype limit = fileData.size();

    MagicAnchors anchors;
    anchors.m1 = findMagic(fileData, limit, MagicValues::M1);
    anchors.m2 = findMagic(fileData, limit, MagicValues::M2);
    anchors.m3 = findMagic(fileData, limit, MagicValues::M3);
    anchors.m4 = findMagic(fileData, limit, MagicValues::M4);
    anchors.m5 = findMagic(fileData, limit, MagicValues::M5);
    anchors.m6 = findMagic(fileData, limit, MagicValues::M6);
    anchors.m7 = findMagic(fileData, limit, MagicValues::M7);

    return anchors;
}

}
