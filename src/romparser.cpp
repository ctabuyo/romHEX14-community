/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "romparser.h"
#include <QFile>
#include <QMap>

// ── Helpers ────────────────────────────────────────────────────────────────

static inline uint8_t hexByte(const QStringView &s, int pos)
{
    return (uint8_t)s.mid(pos, 2).toString().toUInt(nullptr, 16);
}
static inline uint16_t hexWord(const QStringView &s, int pos)
{
    return (uint16_t)s.mid(pos, 4).toString().toUInt(nullptr, 16);
}
static inline uint32_t hexDword(const QStringView &s, int pos)
{
    return s.mid(pos, 8).toString().toUInt(nullptr, 16);
}

// ── Intel HEX parser ───────────────────────────────────────────────────────
//
// Supports all standard record types:
//   00 = data
//   01 = end of file
//   02 = extended segment address  (base = SSSS << 4)
//   03 = start segment address     (ignored)
//   04 = extended linear address   (base = AAAA << 16)
//   05 = start linear address      (ignored)

static ParsedROM parseIntelHex(const QString &text)
{
    // Collect (physicalAddress → byte) in a sorted map so we can later
    // flatten into a contiguous buffer without worrying about record order.
    QMap<uint32_t, uint8_t> memory;
    uint32_t upperBase = 0;   // bits 31:16 from record type 04
    uint32_t segBase   = 0;   // bits 19:4  from record type 02

    for (const QString &rawLine : text.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;
        if (line[0] != ':') continue;       // not a valid record

        QStringView sv(line);
        if (sv.size() < 11) continue;       // minimum: :LLAAAATT CC

        uint8_t  byteCount = hexByte(sv, 1);
        uint16_t addr16    = hexWord(sv, 3);
        uint8_t  recType   = hexByte(sv, 7);

        // Verify minimum length
        if ((int)sv.size() < 11 + byteCount * 2) continue;

        switch (recType) {
        case 0x00: { // Data
            uint32_t physAddr = upperBase | segBase | addr16;
            for (int i = 0; i < byteCount; i++)
                memory[physAddr + i] = hexByte(sv, 9 + i * 2);
            break;
        }
        case 0x01:   // End of file
            goto done;
        case 0x02:   // Extended segment address
            segBase   = (uint32_t)hexWord(sv, 9) << 4;
            upperBase = 0;
            break;
        case 0x04:   // Extended linear address
            upperBase = (uint32_t)hexWord(sv, 9) << 16;
            segBase   = 0;
            break;
        default:
            break;
        }
    }
done:
    if (memory.isEmpty()) {
        ParsedROM r; r.error = "No data records found in Intel HEX file"; return r;
    }

    uint32_t minAddr = memory.firstKey();
    uint32_t maxAddr = memory.lastKey();
    uint32_t size    = maxAddr - minAddr + 1;

    QByteArray data(size, (char)0xFF);
    for (auto it = memory.cbegin(); it != memory.cend(); ++it)
        data[it.key() - minAddr] = (char)it.value();

    ParsedROM r;
    r.data        = data;
    r.baseAddress = minAddr;
    r.ok          = true;
    r.format      = "Intel HEX";
    return r;
}

// ── Motorola S-record parser ───────────────────────────────────────────────
//
// Supports:
//   S0 = header  (ignored)
//   S1 = data with 16-bit address
//   S2 = data with 24-bit address
//   S3 = data with 32-bit address
//   S5 = record count (ignored)
//   S7/S8/S9 = end of file

static ParsedROM parseSRecord(const QString &text)
{
    QMap<uint32_t, uint8_t> memory;

    for (const QString &rawLine : text.split('\n')) {
        QString line = rawLine.trimmed();
        if (line.size() < 4) continue;
        if (line[0] != 'S') continue;

        QChar   rtype = line[1];
        QStringView sv(line);

        // byte count starts at position 2
        uint8_t byteCount = hexByte(sv, 2);

        int addrLen = 0; // in bytes
        if      (rtype == '1') addrLen = 2;
        else if (rtype == '2') addrLen = 3;
        else if (rtype == '3') addrLen = 4;
        else if (rtype == '7' || rtype == '8' || rtype == '9') break; // EOF
        else continue;  // S0, S5, etc.

        // Address
        uint32_t addr = 0;
        for (int i = 0; i < addrLen; i++)
            addr = (addr << 8) | hexByte(sv, 4 + i * 2);

        // Data: byteCount - addrLen - 1 (checksum) bytes
        int dataBytes = byteCount - addrLen - 1;
        int dataStart = 4 + addrLen * 2;
        if (dataBytes < 0 || (int)sv.size() < dataStart + dataBytes * 2) continue;

        for (int i = 0; i < dataBytes; i++)
            memory[addr + i] = hexByte(sv, dataStart + i * 2);
    }

    if (memory.isEmpty()) {
        ParsedROM r; r.error = "No data records found in S-record file"; return r;
    }

    uint32_t minAddr = memory.firstKey();
    uint32_t maxAddr = memory.lastKey();
    uint32_t size    = maxAddr - minAddr + 1;

    QByteArray data(size, (char)0xFF);
    for (auto it = memory.cbegin(); it != memory.cend(); ++it)
        data[it.key() - minAddr] = (char)it.value();

    ParsedROM r;
    r.data        = data;
    r.baseAddress = minAddr;
    r.ok          = true;
    r.format      = "Motorola S-record";
    return r;
}

// ── Public API ─────────────────────────────────────────────────────────────

ParsedROM parseROMData(const QByteArray &raw)
{
    // Detect format from content, not file extension.
    // Scan the first few non-empty lines.
    bool looksLikeHex = false, looksLikeSrec = false;
    int lineCount = 0;
    for (int i = 0; i < raw.size() && lineCount < 8; i++) {
        if (raw[i] != '\n') continue;
        lineCount++;
    }
    // Quick test: look at first non-whitespace char
    for (int i = 0; i < qMin(raw.size(), 4096); i++) {
        char c = raw[i];
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        if (c == ':')  { looksLikeHex  = true; break; }
        if (c == 'S')  { looksLikeSrec = true; break; }
        break;  // starts with something else → binary
    }

    if (looksLikeHex) {
        QString text = QString::fromLatin1(raw);
        return parseIntelHex(text);
    }
    if (looksLikeSrec) {
        QString text = QString::fromLatin1(raw);
        return parseSRecord(text);
    }

    // Raw binary fallback
    ParsedROM r;
    r.data        = raw;
    r.baseAddress = 0;
    r.ok          = true;
    r.format      = "Binary";
    return r;
}

ParsedROM parseROMFile(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        ParsedROM r; r.error = "Cannot open file: " + filePath; return r;
    }
    return parseROMData(f.readAll());
}
