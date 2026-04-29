/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QtEndian>
#include <cstdint>

namespace ols {

namespace MagicValues {
    constexpr uint32_t M1 = 0x42007899;
    constexpr uint32_t M2 = 0x11883377;
    constexpr uint32_t M3 = 0x98728833;
    constexpr uint32_t M4 = 0x08260064;
    constexpr uint32_t M5 = 0xCD23018A;
    constexpr uint32_t M6 = 0x88271283;
    constexpr uint32_t M7 = 0x84C0AD36;
}

struct MagicAnchors {
    qint64 m1 = -1;
    qint64 m2 = -1;
    qint64 m3 = -1;
    qint64 m4 = -1;
    qint64 m5 = -1;
    qint64 m6 = -1;
    qint64 m7 = -1;
};

class OlsMagicScanner {
public:
    static MagicAnchors scan(const QByteArray &fileData, int preambleLen = 1008);
};

}
