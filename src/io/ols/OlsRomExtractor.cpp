/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "OlsRomExtractor.h"
#include "OlsVersionDirectory.h"
#include <QtEndian>
#include <cstring>
#include <limits>

namespace ols {

static constexpr uint32_t SENTINEL_A = 0xFADECAFE;   // at desc[5]
static constexpr uint32_t SENTINEL_B = 0xCAFEAFFE;   // at desc[6]

static constexpr int DESC_PRE_OFFSET = 20;
static constexpr int PROJ_SLOT_LEN = 18;
static constexpr int PROJ_TAG_LEN = 10;
static constexpr int DATA_START_PRE_OFFSET = 26;
static constexpr uint32_t MODERN_FRAMING_BYTES = 76;
static constexpr uint32_t MAX_SEG_SIZE = 16u * 1024u * 1024u;
static constexpr uint32_t MODERN_FMTVER_THRESHOLD = 200;

static constexpr uint32_t OLD_FMT_MAX_VERSIONS = 64;
static constexpr uint32_t OLD_FMT_MAX_REC_SIZE = 64u * 1024u * 1024u;


static inline uint32_t peekU32(const QByteArray &buf, qsizetype off)
{
    return qFromLittleEndian<uint32_t>(
        reinterpret_cast<const uchar *>(buf.constData() + off));
}

static inline bool isPrintable(uint8_t b) { return b >= 0x20 && b < 0x7F; }

static bool isValidProjSlot(const QByteArray &buf, qsizetype projOff)
{
    if (projOff + PROJ_TAG_LEN > buf.size())
        return false;
    const auto *p = reinterpret_cast<const uint8_t *>(buf.constData() + projOff);
    bool allAf = true, allPrint = true;
    for (int i = 0; i < PROJ_TAG_LEN; ++i) {
        if (p[i] != 0xAF) allAf = false;
        if (!isPrintable(p[i])) allPrint = false;
    }
    return allAf || allPrint;
}

static QString decodeProjTag(const QByteArray &buf, qsizetype projOff)
{
    if (projOff + PROJ_TAG_LEN > buf.size())
        return {};
    const auto *p = reinterpret_cast<const uint8_t *>(buf.constData() + projOff);
    QByteArray clean;
    clean.reserve(PROJ_TAG_LEN);
    for (int i = 0; i < PROJ_TAG_LEN; ++i) {
        if (isPrintable(p[i]) && p[i] != 0xAF)
            clean.append(static_cast<char>(p[i]));
    }
    return QString::fromLatin1(clean);
}


static bool parseDescriptorAt(const QByteArray &buf, qsizetype fadeOff,
                              OlsSegment &out)
{
    qsizetype descOff = fadeOff - DESC_PRE_OFFSET;
    if (descOff < PROJ_SLOT_LEN)
        return false;
    if (descOff + 32 > buf.size())
        return false;

    uint32_t d[8];
    for (int i = 0; i < 8; ++i)
        d[i] = peekU32(buf, descOff + i * 4);

    if (d[5] != SENTINEL_A || d[6] != SENTINEL_B)
        return false;

    uint32_t fb = d[3] & 0x7FFFFFFF;
    uint32_t fe = d[4] & 0x7FFFFFFF;
    if (fb >= fe)
        return false;
    uint32_t sz = fe - fb + 1;
    if (sz > MAX_SEG_SIZE)
        return false;

    qsizetype projOff = descOff - PROJ_SLOT_LEN;
    if (!isValidProjSlot(buf, projOff))
        return false;

    out.projOffset = projOff;
    out.dataStart  = projOff - DATA_START_PRE_OFFSET;
    out.flashBase  = fb;
    out.flashSize  = sz;
    out.hash       = d[1];
    out.framingBytes = (sz > MODERN_FRAMING_BYTES) ? MODERN_FRAMING_BYTES : 0;
    return true;
}


static QVector<OlsSegment> findAllSegments(const QByteArray &buf)
{
    QVector<OlsSegment> segs;
    static const QByteArray fadeLE = QByteArray::fromHex("FECADEFA");
    static const QByteArray cafeLE = QByteArray::fromHex("FEAFFECA");

    qsizetype pos = 0;
    while (true) {
        qsizetype j = buf.indexOf(fadeLE, pos);
        if (j < 0)
            break;
        if (j + 8 <= buf.size()
            && std::memcmp(buf.constData() + j + 4, cafeLE.constData(), 4) == 0) {
            OlsSegment sd;
            if (parseDescriptorAt(buf, j, sd))
                segs.append(sd);
        }
        pos = j + 1;
    }
    return segs;
}

static void classifyPrimariesAndCapturePreambles(QVector<OlsSegment> &segs,
                                                 const QByteArray &buf)
{
    uint32_t prevPrimaryEnd = 0;
    qsizetype prevPrimaryEndFile = 0;
    bool havePrev = false;
    for (auto &s : segs) {
        const uint32_t ds = static_cast<uint32_t>(s.dataStart);
        const bool primary = !havePrev || ds >= prevPrimaryEnd;
        s.isPrimary = primary;
        if (primary) {
            if (havePrev) {
                const qsizetype gapStart = prevPrimaryEndFile;
                const qsizetype gapEnd   = s.dataStart;
                if (gapEnd > gapStart && gapStart >= 0 && gapEnd <= buf.size())
                    s.preamble = buf.mid(gapStart, gapEnd - gapStart);
            }
            prevPrimaryEnd = ds + s.flashSize;
            prevPrimaryEndFile = s.dataStart + s.flashSize;
            havePrev = true;
        }
    }
}


static QVector<OlsRomResult> groupByVersion(const QVector<OlsSegment> &segs)
{
    QVector<OlsRomResult> versions;
    QVector<OlsSegment> cur;
    int prev = -1;

    for (const auto &s : segs) {
        if (static_cast<int>(s.flashBase) < prev) {
            OlsRomResult v;
            v.versionIndex = versions.size();
            v.segments = cur;
            versions.append(std::move(v));
            cur.clear();
        }
        cur.append(s);
        prev = static_cast<int>(s.flashBase);
    }
    if (!cur.isEmpty()) {
        OlsRomResult v;
        v.versionIndex = versions.size();
        v.segments = cur;
        versions.append(std::move(v));
    }
    return versions;
}


static QVector<OlsRomResult> oldFormatVersionsFromDirectory(
    const QByteArray &fileData)
{
    QStringList warnings;
    auto dir = OlsVersionDirectory::parse(fileData, &warnings);
    if (dir.numVersions == 0) {
        return {};
    }

    if (dir.numVersions > OLD_FMT_MAX_VERSIONS) return {};
    if (dir.versionRecordSize == 0
        || dir.versionRecordSize > OLD_FMT_MAX_REC_SIZE) {
        return {};
    }

    const qsizetype expectedEnd =
        static_cast<qsizetype>(dir.versionDataStart) + 4
        + static_cast<qsizetype>(dir.versionRecordSize) * dir.numVersions;
    if (expectedEnd > fileData.size()) {
        return {};
    }

    QVector<OlsRomResult> versions;
    versions.reserve(static_cast<int>(dir.numVersions));
    for (uint32_t i = 0; i < dir.numVersions; ++i) {
        const qsizetype payloadOff =
            static_cast<qsizetype>(dir.versionDataStart) + 4
            + static_cast<qsizetype>(dir.versionRecordSize) * i;
        if (payloadOff + dir.versionRecordSize > fileData.size())
            break;

        OlsSegment seg;
        seg.segmentIndex = 0;
        seg.projOffset   = payloadOff;
        seg.dataStart    = payloadOff;
        seg.flashBase    = static_cast<uint32_t>(payloadOff);
        seg.flashSize    = dir.versionRecordSize;
        seg.hash         = 0;

        OlsRomResult v;
        v.versionIndex = static_cast<int>(i);
        v.segments.append(seg);
        v.warnings = warnings;
        versions.append(std::move(v));
    }
    return versions;
}


QVector<OlsRomResult> OlsRomExtractor::extractAll(const QByteArray &fileData)
{
    uint32_t fmtVer = 0;
    if (fileData.size() >= 0x14)
        fmtVer = peekU32(fileData, 0x10);

    QVector<OlsSegment> rawSegs;

    if (fmtVer >= MODERN_FMTVER_THRESHOLD) {
        rawSegs = findAllSegments(fileData);
        classifyPrimariesAndCapturePreambles(rawSegs, fileData);
    }

    QVector<OlsRomResult> versions;

    if (!rawSegs.isEmpty()) {
        versions = groupByVersion(rawSegs);
        for (auto &v : versions) {
            if (!v.segments.isEmpty())
                v.segments.first().preamble.clear();
        }
    } else {
        versions = oldFormatVersionsFromDirectory(fileData);
        if (versions.isEmpty()) {
            OlsRomResult empty;
            empty.error = OlsRomExtractor::tr(
                "No ROM segments found (no FADECAFE sentinels and no "
                "valid M1-anchored per-Version directory header)");
            return { empty };
        }
    }

    for (auto &ver : versions) {
        for (int i = 0; i < ver.segments.size(); ++i) {
            auto &seg = ver.segments[i];
            seg.segmentIndex = i;
            if (seg.dataStart >= 0
                && seg.dataStart + static_cast<qsizetype>(seg.flashSize) <= fileData.size()) {
                seg.data = fileData.mid(seg.dataStart, seg.flashSize);
            } else {
                ver.warnings.append(
                    OlsRomExtractor::tr("Segment %1: data_start 0x%2 + flash_size 0x%3 "
                                        "exceeds file bounds (%4)")
                        .arg(i)
                        .arg(seg.dataStart, 0, 16)
                        .arg(seg.flashSize, 0, 16)
                        .arg(fileData.size()));
            }
        }
        ver.assembledRom = assemble(ver.segments);
    }

    return versions;
}

qsizetype OlsRomExtractor::flashToFileOffset(const QVector<OlsSegment> &segments,
                                              uint32_t flashAddr)
{
    for (const auto &seg : segments) {
        if (seg.flashBase <= flashAddr
            && flashAddr < seg.flashBase + seg.flashSize) {
            return seg.dataStart + (flashAddr - seg.flashBase);
        }
    }
    return -1;
}

QByteArray OlsRomExtractor::assemble(const QVector<OlsSegment> &segments)
{
    if (segments.isEmpty()) return {};

    uint32_t minBase = std::numeric_limits<uint32_t>::max();
    uint32_t maxEnd  = 0;
    for (const auto &s : segments) {
        if (s.flashSize <= s.framingBytes) continue;
        const uint32_t effBase = s.flashBase + s.framingBytes;
        const uint32_t effEnd  = s.flashBase + s.flashSize;
        if (effBase < minBase) minBase = effBase;
        if (effEnd  > maxEnd)  maxEnd  = effEnd;
    }

    if (maxEnd <= minBase) return {};

    QByteArray rom(static_cast<int>(maxEnd - minBase), '\xFF');
    for (const auto &s : segments) {
        if (s.flashSize <= s.framingBytes) continue;
        const uint32_t effBase = s.flashBase + s.framingBytes;
        const uint32_t effSize = s.flashSize - s.framingBytes;
        const uint32_t offset  = effBase - minBase;
        const qsizetype srcLen = s.data.size() > static_cast<qsizetype>(s.framingBytes)
            ? s.data.size() - static_cast<qsizetype>(s.framingBytes) : 0;
        const int copyLen = static_cast<int>(qMin(srcLen,
                                                  static_cast<qsizetype>(effSize)));
        if (copyLen > 0
            && offset + static_cast<uint32_t>(copyLen) <= static_cast<uint32_t>(rom.size())) {
            std::memcpy(rom.data() + offset,
                        s.data.constData() + s.framingBytes,
                        copyLen);
        }
    }

    return rom;
}

} // namespace ols
