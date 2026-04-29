/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include "romdata.h"

// Result of automatic ECU identification from a ROM binary
struct ECUDetection {
    QString family;      // e.g. "MED17", "ME7", "EDC17"
    QString identifier;  // full string found in ROM, e.g. "MED17.1.6"
    uint32_t baseAddr  = 0;
    ByteOrder byteOrder = ByteOrder::BigEndian;
    bool confident     = false;  // true when a specific version string was matched
};

// Scans romData for known ECU identifier strings and byte-order/base-address hints.
// Falls back to ROM-size heuristics when no string is found.
ECUDetection detectECU(const QByteArray &romData);
