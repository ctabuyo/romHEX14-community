/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Blake3.h"
#include <blake3.h>

namespace Blake3 {

QByteArray hash16(QByteArrayView data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.constData(), static_cast<size_t>(data.size()));

    uint8_t out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);

    return QByteArray(reinterpret_cast<const char *>(out), 16);
}

QByteArray hash32(QByteArrayView data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.constData(), static_cast<size_t>(data.size()));

    uint8_t out[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, out, BLAKE3_OUT_LEN);

    return QByteArray(reinterpret_cast<const char *>(out), BLAKE3_OUT_LEN);
}

} // namespace Blake3
