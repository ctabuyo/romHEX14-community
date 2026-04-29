/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kpparser.h"
#include <QtEndian>
#include <QDebug>
#include <QMap>
#include <cmath>
#if __has_include(<zlib.h>)
#  include <zlib.h>
#else
#  include <QtZlib/zlib.h>
#endif

KPParser::KPParser(QObject *parent)
    : QObject(parent)
{
}

// ── Header parsing ────────────────────────────────────────────────────────────
// KP header layout:
//   [0x00] uint32 LE: length of "WinOLS File\0" string (= 12)
//   [0x04] "WinOLS File\0"  (12 bytes)
//   [0x10] ~80 bytes of binary fields
//   somewhere after 0x10: uint32 len + semicolon-delimited vehicle info string
bool KPParser::parseHeader(const QByteArray &data)
{
    const char *d = data.constData();
    const int sz  = data.size();

    // Scan from offset 0x10 forward for a length-prefixed string that
    // contains semicolons (the vehicle info record).
    QString vehicleStr;
    for (int p = 0x10; p < qMin(sz - 4, 0x400); p++) {
        uint32_t slen = qFromLittleEndian<uint32_t>(d + p);
        if (slen < 20 || slen > 512 || p + 4 + (int)slen > sz) continue;

        // Verify the candidate string is printable ASCII and contains ";"
        bool valid = true;
        int semicolons = 0;
        for (int i = 0; i < (int)slen; i++) {
            char c = d[p + 4 + i];
            if (c == '\0') break;
            if (c < 32 || c > 126) { valid = false; break; }
            if (c == ';') semicolons++;
        }
        if (!valid || semicolons < 5) continue;

        vehicleStr = QString::fromLatin1(data.mid(p + 4, slen)).trimmed();
        while (vehicleStr.endsWith('\0')) vehicleStr.chop(1);
        break;
    }

    if (vehicleStr.isEmpty()) {
        // Non-fatal: just log and continue without vehicle info
        qDebug() << "KP: vehicle info string not found in header";
        return true;
    }

    // Split by ";" and assign fields:
    // [0]=manufacturer [1]=model [2]=? [3]=variant [4]=? [5]=year [6]=power
    // [7]=ecuBrand [8]=? [9]=partNumber [10]=swVersion [11]=serial [12]=?
    // [13]=cs1 [14]=cs2 [15]=romSizeHex [16]=cstype [17]=ext
    QStringList fields = vehicleStr.split(';');

    auto field = [&](int idx) -> QString {
        return (idx < fields.size()) ? fields[idx].trimmed() : QString();
    };

    m_info.manufacturer = field(0);
    m_info.model        = field(1);
    // field(2) = unused
    m_info.variant      = field(3);
    // field(4) = unused
    m_info.year         = field(5);
    m_info.power        = field(6);
    m_info.ecuBrand     = field(7);
    // field(8) = unused
    m_info.partNumber   = field(9);
    m_info.swVersion    = field(10);
    // fields 11-14 = serial, unknown, checksum fields
    QString romSizeHex  = field(15);

    bool ok = false;
    m_info.romWordCount = romSizeHex.toUInt(&ok, 16);
    if (ok && m_info.romWordCount > 0) {
        m_info.romByteSize = m_info.romWordCount * 2;
        qDebug() << "KP: vehicle info parsed. ROM word count ="
                 << Qt::hex << m_info.romWordCount
                 << "=> byte size =" << m_info.romByteSize;
    } else {
        qDebug() << "KP: could not parse romSizeHex field:" << romSizeHex;
        m_info.romWordCount = 0;
        m_info.romByteSize  = 0;
    }

    return true;
}

// ── ZIP extraction ────────────────────────────────────────────────────────────
// Locate the "PK\x03\x04" local file header, parse it, and inflate the
// "intern" entry using raw deflate (zlib windowBits = -15).
QByteArray KPParser::extractIntern(const QByteArray &data)
{
    const int sz = data.size();
    int pkPos = data.indexOf("PK\x03\x04");
    if (pkPos < 0) {
        m_error = "No ZIP local file header found in KP file";
        return {};
    }

    // ZIP local file header layout (offsets relative to pkPos):
    //   0  PK\x03\x04 (4)
    //   4  version needed (2, ignore)
    //   6  flags (2)
    //   8  compression method (2): 0=store, 8=deflate
    //  10  mod time (2, ignore)
    //  12  mod date (2, ignore)
    //  14  CRC-32 (4, ignore)
    //  18  compressed size (4 LE)
    //  22  uncompressed size (4 LE)
    //  26  filename length (2 LE)
    //  28  extra field length (2 LE)
    //  30  filename (filenameLen bytes)
    //  30+filenameLen+extraLen: compressed data

    if (pkPos + 30 > sz) {
        m_error = "Truncated ZIP local file header";
        return {};
    }

    const char *d = data.constData() + pkPos;

    uint16_t method          = qFromLittleEndian<uint16_t>(d + 8);
    uint32_t compressedSize  = qFromLittleEndian<uint32_t>(d + 18);
    uint32_t uncompressedSize= qFromLittleEndian<uint32_t>(d + 22);
    uint16_t filenameLen     = qFromLittleEndian<uint16_t>(d + 26);
    uint16_t extraLen        = qFromLittleEndian<uint16_t>(d + 28);

    int dataOffset = pkPos + 30 + filenameLen + extraLen;
    if (dataOffset + (int)compressedSize > sz) {
        m_error = "KP ZIP entry extends beyond end of file";
        return {};
    }

    QByteArray compressed = data.mid(dataOffset, compressedSize);

    if (method == 0) {
        // Store — no compression
        return compressed;
    }

    if (method != 8) {
        m_error = QString("Unsupported ZIP compression method: %1").arg(method);
        return {};
    }

    // Inflate using raw deflate (windowBits = -15)
    z_stream strm = {};
    if (inflateInit2(&strm, -15) != Z_OK) {
        m_error = "zlib inflateInit2 failed";
        return {};
    }

    strm.avail_in  = compressedSize;
    strm.next_in   = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));

    QByteArray out(uncompressedSize, '\0');
    strm.avail_out = uncompressedSize;
    strm.next_out  = reinterpret_cast<Bytef *>(out.data());

    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK) {
        m_error = QString("zlib inflate failed (code %1)").arg(ret);
        return {};
    }

    qDebug() << "KP: inflated" << compressedSize << "bytes ->" << out.size() << "bytes";
    return out;
}

// ── Map record parsing v2 ─────────────────────────────────────────────────────
// Newer KP format (OLS 5.x+): fixed-count length-prefixed records.
// Intern header: [0x00][uint8 mapCount][00 00 00 00][FF FF FF FF][00 00 00 00 00]
// Each record: [uint32 nameLen][name bytes][0x00][variable-length body]
//   body+0x00 = type_marker (uint32)   body+0x04 = dataSize
//   body+0x08 = dimX                   body+0x0C = dimY
//   body+0x10 = groupId
// Physical start address is found by scanning the body for the pattern:
//   [a0][a1][...][a0_repeat] where a0 is a plausible ROM byte address,
//   a1 > a0, and a0 reappears within 40 bytes. This is more reliable than
//   a fixed offset because body size varies with axis label sub-records
//   (different files place the address at +0x87, +0x89, +0x57, etc.).
void KPParser::parseMapRecordsV2(const QByteArray &internData)
{
    const char *d = internData.constData();
    const int   sz = internData.size();

    uint8_t mapCount = (uint8_t)d[1];
    qDebug() << "KP v2: mapCount =" << mapCount;

    // Helper: returns true if pos looks like a top-level map record start.
    // Sets *bodyStart_out to the position right after name+null.
    auto isTopLevel = [&](int pos, int *bodyStart_out) -> bool {
        if (pos + 5 > sz) return false;
        uint32_t nl = qFromLittleEndian<uint32_t>(d + pos);
        if (nl < 1 || nl > 200) return false;
        if (pos + 4 + (int)nl + 1 > sz) return false;
        // Latin-1 printable: >= 0x20 and != 0x7F (DEL)
        for (int j = 0; j < (int)nl; j++) {
            uint8_t c = (uint8_t)d[pos + 4 + j];
            if (c < 0x20 || c == 0x7F) return false;
        }
        if ((uint8_t)d[pos + 4 + nl] != 0x00) return false;
        int bs = pos + 4 + (int)nl + 1;
        if (bs + 20 > sz) return false;
        uint32_t type_m = qFromLittleEndian<uint32_t>(d + bs + 0);
        uint32_t ds     = qFromLittleEndian<uint32_t>(d + bs + 4);
        uint32_t dx     = qFromLittleEndian<uint32_t>(d + bs + 8);
        uint32_t dy     = qFromLittleEndian<uint32_t>(d + bs + 12);
        uint32_t gid    = qFromLittleEndian<uint32_t>(d + bs + 16);
        if (type_m < 1 || type_m > 10) return false;
        if (ds != 0 && ds != 1 && ds != 2 && ds != 4) return false;  // 0 = scalar/auto
        if (dx < 1 || dx > 999) return false;
        if (dy < 1 || dy > 999) return false;
        if (gid > 9999) return false;
        if (bodyStart_out) *bodyStart_out = bs;
        return true;
    };

    // --- Pass 1: collect records by dynamic scanning ---
    struct RawRec { QString name; int bodyStart; };
    QVector<RawRec> recs;

    {
        // Header is 15 bytes; some files have additional zero-padding before records.
        // Scan byte-by-byte past any trailing zeros to find the first record.
        int firstPos = 15;
        while (firstPos < sz && (uint8_t)d[firstPos] == 0x00) firstPos++;

        int bs = 0;
        if (!isTopLevel(firstPos, &bs)) {
            qDebug() << "KP v2: first record at offset" << firstPos << "is not valid";
            return;
        }
        uint32_t nl = qFromLittleEndian<uint32_t>(d + firstPos);
        recs.append({ QString::fromLatin1(d + firstPos + 4, nl), bs });
    }

    for (int i = 1; i < (int)mapCount; i++) {
        int prevBs = recs.last().bodyStart;
        bool found = false;
        for (int s = prevBs + 20; s < sz - 8; s++) {
            int bs = 0;
            if (isTopLevel(s, &bs)) {
                uint32_t nl = qFromLittleEndian<uint32_t>(d + s);
                recs.append({ QString::fromLatin1(d + s + 4, nl), bs });
                found = true;
                break;
            }
        }
        if (!found) {
            qDebug() << "KP v2: found only" << recs.size() << "of" << mapCount << "records";
            break;
        }
    }

    if (recs.isEmpty()) {
        qDebug() << "KP v2: no records parsed";
        return;
    }
    qDebug() << "KP v2: parsed" << recs.size() << "records";

    // --- Scan each record body for the physical address ---
    // Pattern: [a0][a1][...][a0_repeat] where a0 is a plausible ROM byte
    // address and a0 reappears within 40 bytes. The exact offset varies per
    // file (bodies with axis sub-records shift the address location), so a
    // scan is required instead of a fixed offset.
    struct AddrInfo { uint32_t a0; uint32_t a1; uint32_t a2; };
    auto scanBodyAddr = [&](int bodyStart) -> AddrInfo {
        const int scanEnd = qMin(bodyStart + 600, sz - 16);
        for (int s = bodyStart + 20; s < scanEnd; s++) {
            uint32_t a0 = qFromLittleEndian<uint32_t>(d + s);
            // Plausible ECU byte address: above 4 KB (avoid small sentinel values)
            // and below 16 MB (avoids false positives where the first byte of a
            // real address lands in the upper byte of a misaligned uint32 read,
            // producing values like 0x4C000000 which pass the 0xFFFF0000 check).
            if (a0 < 0x1000u || a0 >= 0x01000000u) continue;
            uint32_t a1 = qFromLittleEndian<uint32_t>(d + s + 4);
            if (a1 <= a0 || a1 - a0 > 4u * 1024u * 1024u) continue;
            // a0 must appear again within 40 bytes — this is the key
            // discriminator that confirms it's an address entry and not
            // coincidental data (observed consistently across all KP variants).
            bool repeat = false;
            for (int gap = 12; gap <= 40; gap += 4) {
                if (s + gap + 4 > sz) break;
                if (qFromLittleEndian<uint32_t>(d + s + gap) == a0) {
                    repeat = true; break;
                }
            }
            if (!repeat) continue;
            uint32_t a2 = qFromLittleEndian<uint32_t>(d + s + 8);
            return { a0, a1, a2 };
        }
        return { 0, 0, 0 };
    };

    // --- Determine ROM byte size ---
    // Try header first; fall back to the most common plausible a2 value across
    // all records (a2 is the ROM size marker in most KP variants).
    uint32_t romByteSize = m_info.romByteSize;
    if (romByteSize == 0) {
        QMap<uint32_t, int> a2Counts;
        for (const auto &r : recs) {
            auto ai = scanBodyAddr(r.bodyStart);
            if (ai.a0 > 0 && ai.a2 >= 0x10000u && ai.a2 <= 0x10000000u
                    && (ai.a2 & 0xFFFFu) == 0)     // must be 64KB-aligned
                a2Counts[ai.a2]++;
        }
        int bestCnt = 0;
        for (auto it = a2Counts.cbegin(); it != a2Counts.cend(); ++it) {
            if (it.value() > bestCnt) { bestCnt = it.value(); romByteSize = it.key(); }
        }
        if (romByteSize > 0) {
            m_info.romByteSize  = romByteSize;
            m_info.romWordCount = romByteSize / 2;
            qDebug() << "KP v2: detected ROM byte size =" << Qt::hex << romByteSize
                     << "from a2 majority vote (" << bestCnt << "records)";
        }
    }

    // --- Determine base address ---
    // If minAddr < romByteSize the addresses are direct file offsets (base=0).
    // If minAddr >= romByteSize the ROM is mapped at a physical base address
    // aligned to the ROM size (e.g. 0x000C0000 for a 768 KB ROM).
    uint32_t baseAddr = 0;
    {
        uint32_t minAddr = 0xFFFFFFFFu;
        for (const auto &r : recs) {
            auto ai = scanBodyAddr(r.bodyStart);
            if (ai.a0 > 0x1000u && ai.a0 < 0xFFFF0000u)
                minAddr = qMin(minAddr, ai.a0);
        }
        if (minAddr < 0xFFFF0000u && romByteSize > 0 && minAddr >= romByteSize) {
            baseAddr = (minAddr / romByteSize) * romByteSize;
        }
        qDebug() << "KP v2: min physical addr =" << Qt::hex << minAddr
                 << "romByteSize =" << romByteSize << "=> base =" << baseAddr;
    }

    // --- Pass 2: build MapInfo ---
    for (const auto &r : recs) {
        uint32_t dataSize = qFromLittleEndian<uint32_t>(d + r.bodyStart + 4);
        if (dataSize == 0) dataSize = 1;       // 0 = scalar/auto → treat as 1 byte
        else if (dataSize > 4) dataSize = 2;
        uint32_t dimX = qFromLittleEndian<uint32_t>(d + r.bodyStart + 8);
        uint32_t dimY = qFromLittleEndian<uint32_t>(d + r.bodyStart + 12);
        if (dimX < 1 || dimX > 999) dimX = 1;
        if (dimY < 1 || dimY > 999) dimY = 1;

        auto ai = scanBodyAddr(r.bodyStart);
        uint32_t fileOff = 0;
        if (ai.a0 > 0 && ai.a0 >= baseAddr && ai.a0 < 0xFFFF0000u)
            fileOff = ai.a0 - baseAddr;

        int byteLen = (int)(dimX * dimY * dataSize);

        MapInfo m;
        m.name        = r.name.trimmed();
        m.description = r.name.trimmed();
        m.address     = fileOff;
        m.rawAddress  = fileOff;
        m.length      = byteLen;
        m.dataSize    = (int)dataSize;
        m.dimensions  = { (int)dimX, (int)dimY };
        m.type        = (dimX > 1 && dimY > 1) ? "MAP" :
                        (dimX > 1 || dimY > 1) ? "CURVE" : "VALUE";
        m.linkConfidence = 100;
        m.columnMajor = true;
        m_maps.append(m);
    }

    int nAddr = 0;
    for (const auto &m : m_maps) if (m.address > 0) nAddr++;
    qDebug() << "KP v2 result:" << m_maps.size() << "maps," << nAddr << "with addresses";
}

// ── Map record parsing ────────────────────────────────────────────────────────
// Same binary record format as OLS files (intern = OLS-style map records).
// Record marker: [0x0A 0x00 0x00 0x00][uint32 groupId][uint32 nameLen][name]
void KPParser::parseMapRecords(const QByteArray &internData)
{
    const char *d   = internData.constData();
    const int   sz  = internData.size();

    // --- Format detection ---
    // v2 format: intern[0]==0x00, intern[1]==mapCount (small), intern[6..9]==0xFFFFFFFF
    if (sz >= 15
        && (uint8_t)d[0] == 0x00
        && (uint8_t)d[1] >= 1 && (uint8_t)d[1] <= 100
        && (uint8_t)d[6] == 0xFF && (uint8_t)d[7] == 0xFF
        && (uint8_t)d[8] == 0xFF && (uint8_t)d[9] == 0xFF)
    {
        qDebug() << "KP intern: detected v2 (length-prefixed) format";
        parseMapRecordsV2(internData);
        return;
    }

    // If romWordCount was not found in the header, auto-detect from address
    // patterns using the same [A][B][C][A] frequency counting as OLSParser.
    uint32_t romSize = m_info.romByteSize;

    if (romSize == 0) {
        emit progress(tr("Detecting ROM size..."), 55);

        // Collect name-end positions first (same anchor approach as OLSParser)
        QVector<int> namePositions;
        for (int p = 0; p < sz - 20 && namePositions.size() < 500; p++) {
            if (qFromLittleEndian<uint32_t>(d + p) != 10) continue;
            uint32_t nl = qFromLittleEndian<uint32_t>(d + p + 8);
            if (nl < 3 || nl > 60 || p + 12 + (int)nl > sz) continue;
            bool valid = true;
            for (int i = 0; i < (int)nl; i++) {
                char c = d[p + 12 + i];
                if (c == '\0') break;
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '_')) { valid = false; break; }
            }
            if (!valid || !QChar::fromLatin1(d[p + 12]).isLetter()) continue;
            namePositions.append(p + 12 + nl);
        }

        QMap<uint32_t, int> markerCounts;
        for (int ne : namePositions) {
            for (int s = ne; s < qMin(ne + 500, sz - 16); s++) {
                uint32_t a0 = qFromLittleEndian<uint32_t>(d + s);
                uint32_t a1 = qFromLittleEndian<uint32_t>(d + s + 4);
                uint32_t a2 = qFromLittleEndian<uint32_t>(d + s + 8);
                if (a0 < 0x100 || a0 > 0x1000000) continue;
                if (a1 <= a0 || a1 > 0x1000000) continue;
                if (a2 < 0x10000 || a2 > 0x1000000) continue;
                bool found = false;
                for (int gap = 12; gap <= 28; gap += 4) {
                    if (s + gap + 4 > sz) break;
                    if (qFromLittleEndian<uint32_t>(d + s + gap) == a0) {
                        markerCounts[a2]++;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        uint32_t bestMarker = 0;
        int bestCount = 0;
        for (auto it = markerCounts.begin(); it != markerCounts.end(); ++it) {
            if (it.value() > bestCount) {
                bestCount = it.value();
                bestMarker = it.key();
            }
        }

        if (bestCount >= 3 && bestMarker > 0) {
            romSize = bestMarker;
            m_info.romByteSize  = romSize;
            m_info.romWordCount = romSize / 2;
            qDebug() << "KP: Auto-detected ROM size =" << Qt::hex << romSize
                     << "from" << bestCount << "patterns";
        } else {
            qDebug() << "KP: Failed to auto-detect ROM size (best count" << bestCount << ")";
        }
    }

    qDebug() << "KP: parseMapRecords, intern size =" << sz
             << "ROM byte size =" << Qt::hex << romSize;

    // In KP files, addresses in map records are word addresses (16-bit word
    // offsets into the ROM). Convert to byte offsets by multiplying by 2.
    // The romSize field in the intern triplet equals romWordCount (the word
    // count), so when comparing a2 against the ROM marker we compare against
    // m_info.romWordCount, then convert a0/a1 to byte addresses.
    uint32_t romWordCount = m_info.romWordCount; // used as triplet marker

    QSet<QString> seen;

    for (int pos = 0; pos < sz - 20; pos++) {
        if (qFromLittleEndian<uint32_t>(d + pos) != 10) continue;

        uint32_t groupId = qFromLittleEndian<uint32_t>(d + pos + 4);
        if (groupId > 50000) continue;

        uint32_t nameLen = qFromLittleEndian<uint32_t>(d + pos + 8);
        if (nameLen < 2 || nameLen > 60) continue;
        if (pos + 12 + (int)nameLen > sz) continue;

        QByteArray raw = internData.mid(pos + 12, nameLen);
        bool valid = true;
        for (int i = 0; i < raw.size(); i++) {
            char c = raw[i];
            if (c == '\0') { raw.truncate(i); break; }
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '[' || c == ']'))
                { valid = false; break; }
        }
        if (!valid || raw.size() < 2) continue;
        if (!QChar::fromLatin1(raw[0]).isLetter()) continue;

        QString name = QString::fromLatin1(raw);
        if (seen.contains(name)) continue;
        seen.insert(name);

        int nameEnd = pos + 12 + nameLen;

        MapInfo m;
        m.name           = name;
        m.linkConfidence = 100;
        m.columnMajor    = true; // OLS/KP intern stores data column-major

        // ── Description: scan backward from nameMarkerPos (up to 400 bytes)
        // for first length-prefixed string (uint32 len 8-200, text with spaces, len > 5)
        for (int dscan = pos - 4; dscan > qMax(0, pos - 400); dscan--) {
            uint32_t dlen = qFromLittleEndian<uint32_t>(d + dscan);
            if (dlen < 8 || dlen > 200 || dscan + 4 + (int)dlen > pos) continue;
            QString desc = QString::fromLatin1(internData.mid(dscan + 4, dlen)).trimmed();
            while (desc.endsWith('\0')) desc.chop(1);
            if (desc.contains(' ') && desc.size() > 5) {
                m.description = desc;
                break;
            }
        }

        // ── Type and data size: read from fields before the 0x0A marker
        // nameMarkerPos - 16 = recType (2=VALUE, 3=CURVE, 4/5=MAP)
        // nameMarkerPos - 12 = recDS (data size in bytes: 1, 2, 4)
        // nameMarkerPos -  8 = rows
        // nameMarkerPos -  4 = cols
        uint32_t recType = (pos >= 16) ? qFromLittleEndian<uint32_t>(d + pos - 16) : 0;
        uint32_t recDS   = (pos >= 12) ? qFromLittleEndian<uint32_t>(d + pos - 12) : 2;
        if (recDS == 0 || recDS > 4) recDS = 2;
        m.dataSize = (int)recDS;

        // ── Address triplet: [word_addr][word_end][romWordCount] with word_addr
        // repeated within 28 bytes after romWordCount.
        // KP word addresses * 2 = byte addresses.
        bool foundAddr = false;
        {
            int scanEnd = qMin(nameEnd + 500, sz - 16);
            for (int s = nameEnd; s < scanEnd; s++) {
                uint32_t a0 = qFromLittleEndian<uint32_t>(d + s);
                uint32_t a1 = qFromLittleEndian<uint32_t>(d + s + 4);
                uint32_t a2 = qFromLittleEndian<uint32_t>(d + s + 8);

                // a2 must equal the known ROM word count (or, if unknown, be a
                // plausible word-count value: power-of-two-ish in range)
                bool markerOk = false;
                if (romWordCount > 0) {
                    markerOk = (a2 == romWordCount);
                } else {
                    // plausibility check: a2 looks like a ROM word count
                    markerOk = (a2 >= 0x8000 && a2 <= 0x800000 && (a2 & 0x0FFF) == 0);
                }
                if (!markerOk) continue;

                // word addresses: a0 < a1, both within range
                if (a0 < 0x80 || a0 >= a2) continue;
                if (a1 <= a0 || a1 > a2 + 0x80) continue;

                // a0 must be repeated within 28 bytes after a2
                bool repeat = false;
                for (int gap = 12; gap <= 28; gap += 4) {
                    if (s + gap + 4 > sz) break;
                    if (qFromLittleEndian<uint32_t>(d + s + gap) == a0) { repeat = true; break; }
                }
                if (!repeat) continue;

                // Convert word addresses to byte addresses
                m.rawAddress = a0 * 2;
                m.address    = a0 * 2;
                m.length     = (int)((a1 - a0) * 2);

                // If romWordCount was previously unknown, lock it in from this triplet
                if (romWordCount == 0) {
                    romWordCount          = a2;
                    m_info.romWordCount   = a2;
                    m_info.romByteSize    = a2 * 2;
                    romSize               = m_info.romByteSize;
                }

                // ── Dimensions
                int byteLen = m.length;
                if (recType == 2 || byteLen <= (int)recDS) {
                    // VALUE: single cell
                    m.dimensions = {1, 1};
                    m.dataSize   = byteLen > 0 ? byteLen : (int)recDS;
                } else if (recType == 3) {
                    // CURVE (1D)
                    int cells = (m.dataSize > 0) ? byteLen / m.dataSize : 1;
                    if (cells < 1) cells = 1;
                    m.dimensions = {cells, 1};
                } else if (recType == 4 || recType == 5) {
                    // MAP (2D): search for uint16 pair [cols][rows] in nameEnd+0..+0x50
                    int cells = (m.dataSize > 0) ? byteLen / m.dataSize : 1;
                    bool foundDims = false;

                    QVector<QPair<int,int>> candidates;
                    for (int doff = 0x00; doff < 0x50; doff += 2) {
                        int dp = nameEnd + doff;
                        if (dp + 2 > sz) break;
                        uint16_t v = qFromLittleEndian<uint16_t>(d + dp);
                        if (v >= 2 && v <= 999 && cells > 0 && cells % v == 0) {
                            int other = cells / v;
                            if (other >= 2 && other <= 999)
                                candidates.append({v, doff});
                        }
                    }

                    // Prefer a candidate where rows appears 4 bytes after cols
                    for (const auto &[cols, off] : candidates) {
                        int rows = cells / cols;
                        int dp2  = nameEnd + off + 4;
                        if (dp2 + 2 <= sz) {
                            uint16_t v2 = qFromLittleEndian<uint16_t>(d + dp2);
                            if ((int)v2 == rows) {
                                m.dimensions = {cols, rows};
                                foundDims = true;
                                break;
                            }
                        }
                    }

                    if (!foundDims && !candidates.isEmpty()) {
                        int cols = candidates.first().first;
                        m.dimensions = {cols, cells / cols};
                        foundDims = true;
                    }

                    if (!foundDims && cells > 0) {
                        // Last resort: square-ish shape
                        for (int c = (int)sqrt((double)cells); c >= 2; c--) {
                            if (cells % c == 0) {
                                m.dimensions = {c, cells / c};
                                foundDims = true;
                                break;
                            }
                        }
                        if (!foundDims)
                            m.dimensions = {cells, 1};
                    }
                } else {
                    // Unknown recType: treat as 1D
                    int cells = (m.dataSize > 0) ? byteLen / m.dataSize : 1;
                    m.dimensions = {cells > 0 ? cells : 1, 1};
                }

                // ── Scaling factor: stored as double at addrTripleOffset - 16
                {
                    int addrOff    = s - nameEnd;
                    int scalingPos = nameEnd + addrOff - 16;
                    if (scalingPos >= 0 && scalingPos + 8 <= sz) {
                        union { uint64_t u; double dv; } conv;
                        conv.u = qFromLittleEndian<uint64_t>(d + scalingPos);
                        double factor = conv.dv;
                        if (std::isfinite(factor) && factor != 0.0
                            && std::abs(factor) > 1e-20 && std::abs(factor) < 1e10) {
                            m.hasScaling      = true;
                            m.scaling.type    = CompuMethod::Type::Linear;
                            m.scaling.linA    = factor;
                            m.scaling.linB    = 0.0;
                        }
                    }
                }

                foundAddr = true;
                break;
            }
        }
        Q_UNUSED(foundAddr)

        // ── Determine type string
        if (m.dimensions.x > 1 && m.dimensions.y > 1)
            m.type = "MAP";
        else if (m.dimensions.x > 1 || m.dimensions.y > 1)
            m.type = "CURVE";
        else
            m.type = "VALUE";

        m_maps.append(m);

        if (m_maps.size() % 500 == 0)
            emit progress(tr("Parsing map records... (%1)").arg(m_maps.size()),
                          50 + qMin(40, m_maps.size() / 100));
    }

    // Summary
    int nVal = 0, nCur = 0, nMap = 0, nAddr = 0;
    for (const auto &mp : m_maps) {
        if (mp.type == "MAP")        nMap++;
        else if (mp.type == "CURVE") nCur++;
        else                         nVal++;
        if (mp.address > 0)          nAddr++;
    }
    qDebug() << "KP result:" << m_maps.size() << "total,"
             << nVal << "VALUEs," << nCur << "CURVEs," << nMap << "MAPs,"
             << nAddr << "with addresses";
}

// ── Public entry point ────────────────────────────────────────────────────────
bool KPParser::parse(const QByteArray &fileData)
{
    m_maps.clear();
    m_error.clear();

    if (fileData.size() < 0x100) {
        m_error = "File too small";
        return false;
    }
    if (fileData.mid(4, 11) != "WinOLS File") {
        m_error = "Not a valid KP file (bad magic)";
        return false;
    }

    emit progress(tr("Parsing KP header..."), 10);
    if (!parseHeader(fileData)) return false;

    emit progress(tr("Extracting map data..."), 30);
    QByteArray internData = extractIntern(fileData);
    if (internData.isEmpty()) {
        if (m_error.isEmpty())
            m_error = "Failed to extract map data from KP ZIP";
        return false;
    }

    emit progress(tr("Parsing map records..."), 50);
    parseMapRecords(internData);

    emit progress(tr("Done"), 100);
    return true;
}
