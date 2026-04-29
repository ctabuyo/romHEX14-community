/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

// Result of parsing a ROM file that may be in a text-based format.
struct ParsedROM {
    QByteArray  data;           // flat binary image (padded with 0xFF)
    uint32_t    baseAddress = 0;// lowest physical address found in the file
    bool        ok          = false;
    QString     error;
    QString     format;         // "Binary", "Intel HEX", "Motorola S-record"
};

// Auto-detects format and returns a flat binary image.
// Falls back to raw binary if the format is not recognised.
ParsedROM parseROMFile(const QString &filePath);
ParsedROM parseROMData(const QByteArray &raw);  // detect from in-memory bytes
