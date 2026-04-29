/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QByteArray>
#include <QByteArrayView>

namespace Blake3 {

/// Returns the first 16 bytes of the blake3 hash.
QByteArray hash16(QByteArrayView data);

/// Returns the full 32-byte blake3 hash.
QByteArray hash32(QByteArrayView data);

} // namespace Blake3
