/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rompatch.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

// ── CRC32 ─────────────────────────────────────────────────────────────────────

static uint32_t crc32Table[256];
static bool     crc32TableReady = false;

static void buildCrc32Table()
{
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32Table[i] = c;
    }
    crc32TableReady = true;
}

static uint32_t crc32(const uint8_t *data, int len)
{
    if (!crc32TableReady) buildCrc32Table();
    uint32_t crc = 0xFFFFFFFFu;
    for (int i = 0; i < len; ++i)
        crc = crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ── PatchApplyResult helpers ──────────────────────────────────────────────────

bool PatchApplyResult::hasFailures() const
{
    for (const auto &r : maps)
        if (r.status == MapApplyResult::Failed) return true;
    return false;
}

bool PatchApplyResult::hasWarnings() const
{
    for (const auto &r : maps)
        if (r.status == MapApplyResult::AppliedWithWarnings) return true;
    return false;
}

QStringList PatchApplyResult::summary() const
{
    QStringList out;
    for (const auto &r : maps) {
        QString prefix;
        switch (r.status) {
        case MapApplyResult::Applied:             prefix = QStringLiteral("[OK]    "); break;
        case MapApplyResult::AppliedWithWarnings: prefix = QStringLiteral("[WARN]  "); break;
        case MapApplyResult::Failed:              prefix = QStringLiteral("[FAIL]  "); break;
        }
        out << prefix + r.mapName + QStringLiteral(": ") + r.detail;
    }
    return out;
}

// ── Build from diff ───────────────────────────────────────────────────────────

RomPatch RomPatch::fromDiffs(const QVector<MapDiff> &diffs,
                              const QByteArray &refRom,
                              const QByteArray &cmpRom,
                              ByteOrder bo,
                              bool includeRawBytes,
                              const QString &srcLabel,
                              const QString &tgtLabel)
{
    RomPatch p;
    p.created     = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    p.label       = tgtLabel.isEmpty() ? QStringLiteral("Patch") : tgtLabel;
    p.sourceLabel = srcLabel;
    p.targetLabel = tgtLabel;

    // Track which byte offsets in refRom are covered by A2L maps
    // (used later to exclude them from raw byte diff scan)
    QVector<QPair<uint32_t,uint32_t>> mapRanges;   // [start, end)

    for (const MapDiff &md : diffs) {
        if (md.changedCells == 0) continue;

        MapPatch mp;
        mp.name        = md.map.name;
        mp.cols        = md.map.dimensions.x;
        mp.rows        = qMax(1, md.map.dimensions.y);
        mp.dataSize    = md.map.dataSize;
        mp.bigEndian   = (bo == ByteOrder::BigEndian);
        mp.columnMajor = md.map.columnMajor;
        if (mp.cols <= 0 || mp.rows <= 0 || mp.cols > 10000 || mp.rows > 10000) continue;
        mp.totalCells  = mp.cols * mp.rows;
        if (mp.totalCells > 1000000) continue;

        int dataOff = (int)md.map.mapDataOffset;
        int dataLen = mp.totalCells * mp.dataSize;
        if (dataLen <= 0 || mp.dataSize <= 0) continue;
        if ((int)(md.refOffset + dataOff + dataLen) > refRom.size()) continue;
        if ((int)(md.cmpOffset + dataOff + dataLen) > cmpRom.size()) continue;

        mp.sourceAddress = (uint32_t)(md.refOffset + dataOff);
        const uint8_t *srcBytes = (const uint8_t *)refRom.constData() + mp.sourceAddress;
        mp.dataCrc32     = crc32(srcBytes, dataLen);

        mapRanges.append({mp.sourceAddress, mp.sourceAddress + (uint32_t)dataLen});

        const uint8_t *rp = (const uint8_t *)refRom.constData() + md.refOffset + dataOff;
        const uint8_t *cp = (const uint8_t *)cmpRom.constData() + md.cmpOffset + dataOff;

        for (int r = 0; r < mp.rows; ++r) {
            for (int c = 0; c < mp.cols; ++c) {
                int memI = mp.columnMajor ? c * mp.rows + r : r * mp.cols + c;
                uint32_t rv = readRomValue(rp, dataLen, (uint32_t)(memI * mp.dataSize), mp.dataSize, bo);
                uint32_t cv = readRomValue(cp, dataLen, (uint32_t)(memI * mp.dataSize), mp.dataSize, bo);
                if (rv != cv)
                    mp.cells.append({memI, rv, cv});
            }
        }
        if (!mp.cells.isEmpty())
            p.maps.append(mp);
    }

    // ── Raw byte scan (checksums, CRC tables, non-map differences) ────────────
    if (includeRawBytes) {
        int scanLen = qMin(refRom.size(), cmpRom.size());
        const uint8_t *rp = (const uint8_t *)refRom.constData();
        const uint8_t *cp = (const uint8_t *)cmpRom.constData();

        auto inAnyMap = [&](uint32_t off) {
            for (const auto &range : mapRanges)
                if (off >= range.first && off < range.second) return true;
            return false;
        };

        for (int i = 0; i < scanLen; ++i) {
            if (rp[i] != cp[i] && !inAnyMap((uint32_t)i))
                p.rawBytes.append({(uint32_t)i, rp[i], cp[i]});
        }
    }

    return p;
}

// ── Serialise ─────────────────────────────────────────────────────────────────

QJsonDocument RomPatch::toJson() const
{
    QJsonObject root;
    root[QStringLiteral("format")]  = QStringLiteral("rxpatch");
    root[QStringLiteral("version")] = version;
    root[QStringLiteral("created")] = created;
    root[QStringLiteral("label")]   = label;
    root[QStringLiteral("source")]  = sourceLabel;
    root[QStringLiteral("target")]  = targetLabel;

    QJsonArray mapsArr;
    for (const MapPatch &mp : maps) {
        QJsonObject mo;
        mo[QStringLiteral("name")]        = mp.name;
        mo[QStringLiteral("cols")]        = mp.cols;
        mo[QStringLiteral("rows")]        = mp.rows;
        mo[QStringLiteral("dataSize")]    = mp.dataSize;
        mo[QStringLiteral("bigEndian")]   = mp.bigEndian;
        mo[QStringLiteral("columnMajor")] = mp.columnMajor;
        mo[QStringLiteral("totalCells")]  = mp.totalCells;
        mo[QStringLiteral("srcAddr")]     = QString("0x%1").arg(mp.sourceAddress, 0, 16);
        mo[QStringLiteral("dataCrc32")]   = QString("0x%1").arg(mp.dataCrc32, 8, 16, QLatin1Char('0'));

        QJsonArray cells;
        for (const CellPatch &cp : mp.cells) {
            QJsonObject co;
            co[QStringLiteral("i")]   = cp.memIdx;
            co[QStringLiteral("ref")] = (qint64)cp.refVal;
            co[QStringLiteral("val")] = (qint64)cp.newVal;
            cells.append(co);
        }
        mo[QStringLiteral("cells")] = cells;
        mapsArr.append(mo);
    }
    root[QStringLiteral("maps")] = mapsArr;

    // Raw bytes: stored as runs of consecutive bytes to keep JSON compact.
    // Each run: {"off": "0x...", "ref": "AABB...", "new": "CCDD..."}
    if (!rawBytes.isEmpty()) {
        QJsonArray runs;
        int i = 0;
        while (i < rawBytes.size()) {
            int j = i;
            // Extend run while offsets are consecutive
            while (j + 1 < rawBytes.size() &&
                   rawBytes[j+1].offset == rawBytes[j].offset + 1)
                ++j;

            QByteArray refBuf, newBuf;
            for (int k = i; k <= j; ++k) {
                refBuf.append((char)rawBytes[k].refVal);
                newBuf.append((char)rawBytes[k].newVal);
            }
            QJsonObject run;
            run[QStringLiteral("off")] = QString("0x%1").arg(rawBytes[i].offset, 0, 16);
            run[QStringLiteral("ref")] = QString::fromLatin1(refBuf.toHex());
            run[QStringLiteral("new")] = QString::fromLatin1(newBuf.toHex());
            runs.append(run);
            i = j + 1;
        }
        root[QStringLiteral("rawBytes")] = runs;
    }

    return QJsonDocument(root);
}

// ── Deserialise ───────────────────────────────────────────────────────────────

RomPatch RomPatch::fromJson(const QJsonDocument &doc, QString *error)
{
    RomPatch p;
    QJsonObject root = doc.object();
    if (root[QStringLiteral("format")].toString() != QLatin1String("rxpatch")) {
        if (error) *error = QStringLiteral("Not a valid .rxpatch file");
        return p;
    }
    p.version     = root[QStringLiteral("version")].toInt(1);
    p.created     = root[QStringLiteral("created")].toString();
    p.label       = root[QStringLiteral("label")].toString();
    p.sourceLabel = root[QStringLiteral("source")].toString();
    p.targetLabel = root[QStringLiteral("target")].toString();

    for (const QJsonValue &mv : root[QStringLiteral("maps")].toArray()) {
        QJsonObject mo = mv.toObject();
        MapPatch mp;
        mp.name          = mo[QStringLiteral("name")].toString();
        mp.cols          = mo[QStringLiteral("cols")].toInt(1);
        mp.rows          = mo[QStringLiteral("rows")].toInt(1);
        mp.dataSize      = mo[QStringLiteral("dataSize")].toInt(2);
        mp.bigEndian     = mo[QStringLiteral("bigEndian")].toBool(true);
        mp.columnMajor   = mo[QStringLiteral("columnMajor")].toBool(false);
        mp.totalCells    = mo[QStringLiteral("totalCells")].toInt(mp.cols * mp.rows);
        mp.sourceAddress = mo[QStringLiteral("srcAddr")].toString().toUInt(nullptr, 0);
        mp.dataCrc32     = mo[QStringLiteral("dataCrc32")].toString().toUInt(nullptr, 0);
        for (const QJsonValue &cv : mo[QStringLiteral("cells")].toArray()) {
            QJsonObject co = cv.toObject();
            mp.cells.append({co[QStringLiteral("i")].toInt(),
                             (uint32_t)co[QStringLiteral("ref")].toVariant().toULongLong(),
                             (uint32_t)co[QStringLiteral("val")].toVariant().toULongLong()});
        }
        p.maps.append(mp);
    }

    for (const QJsonValue &rv : root[QStringLiteral("rawBytes")].toArray()) {
        QJsonObject ro = rv.toObject();
        uint32_t off = ro[QStringLiteral("off")].toString().toUInt(nullptr, 0);
        QByteArray refBuf = QByteArray::fromHex(ro[QStringLiteral("ref")].toString().toLatin1());
        QByteArray newBuf = QByteArray::fromHex(ro[QStringLiteral("new")].toString().toLatin1());
        int len = qMin(refBuf.size(), newBuf.size());
        for (int i = 0; i < len; ++i)
            p.rawBytes.append({off + (uint32_t)i,
                               (uint8_t)refBuf[i],
                               (uint8_t)newBuf[i]});
    }

    return p;
}

bool RomPatch::save(const QString &path, QString *error) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(toJson().toJson(QJsonDocument::Indented));
    return true;
}

RomPatch RomPatch::load(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return {};
    }
    QJsonParseError pe;
    auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        if (error) *error = pe.errorString();
        return {};
    }
    return fromJson(doc, error);
}

// ── Apply ─────────────────────────────────────────────────────────────────────

PatchApplyResult RomPatch::apply(QByteArray &rom,
                                  const QVector<MapInfo> &projectMaps,
                                  ByteOrder /*bo*/,
                                  const QMap<QString, uint32_t> &mapOffsets) const
{
    PatchApplyResult result;
    uint8_t *rd     = reinterpret_cast<uint8_t *>(rom.data());
    int      romLen = rom.size();

    for (const MapPatch &mp : maps) {
        MapApplyResult mr;
        mr.mapName = mp.name;

        const ByteOrder mapBo = mp.bigEndian ? ByteOrder::BigEndian : ByteOrder::LittleEndian;

        // ── Locate map in target ROM ──────────────────────────────────────────
        const MapInfo *mi = nullptr;
        for (const MapInfo &m : projectMaps)
            if (m.name == mp.name) { mi = &m; break; }

        uint32_t base    = 0;
        int      dataOff = 0;
        int      dataLen = mp.totalCells * mp.dataSize;
        if (dataLen <= 0) dataLen = mp.cols * mp.rows * mp.dataSize;

        QStringList notes;
        bool useAddressFallback = false;

        if (mi) {
            base    = mapOffsets.contains(mp.name) ? mapOffsets[mp.name] : mi->address;
            dataOff = (int)mi->mapDataOffset;

            uint32_t expectedBase = mp.sourceAddress > (uint32_t)dataOff
                                    ? mp.sourceAddress - (uint32_t)dataOff : 0;
            if (mp.sourceAddress > 0 && base != expectedBase) {
                notes << QStringLiteral("address mismatch (project: 0x%1, patch: 0x%2) — using project address")
                            .arg(base, 0, 16).arg(expectedBase, 0, 16);
            }
        } else if (mp.sourceAddress > 0) {
            base              = 0;
            dataOff           = (int)mp.sourceAddress;
            useAddressFallback = true;
            notes << QStringLiteral("map not found by name, applying by stored address 0x%1")
                        .arg(mp.sourceAddress, 0, 16);
        } else {
            mr.status = MapApplyResult::Failed;
            mr.detail = QStringLiteral("map not found in project and no source address stored");
            result.maps.append(mr);
            continue;
        }

        // ── Bounds check ──────────────────────────────────────────────────────
        if ((int)(base + dataOff + dataLen) > romLen) {
            mr.status = MapApplyResult::Failed;
            mr.detail = QStringLiteral("out of ROM bounds (base=0x%1 off=%2 len=%3 romLen=%4)")
                        .arg(base, 0, 16).arg(dataOff).arg(dataLen).arg(romLen);
            result.maps.append(mr);
            continue;
        }

        uint8_t *dp = rd + base + dataOff;

        // ── CRC integrity check (warning only — never a hard failure) ────────
        // A CRC mismatch means the ROM region differs from the source ECU,
        // but we still apply using the stored address. The cell-level refVal
        // check below will flag any unexpected current values.
        if (mp.dataCrc32 != 0) {
            uint32_t actualCrc = crc32(dp, dataLen);
            if (actualCrc != mp.dataCrc32)
                notes << QStringLiteral("CRC mismatch — ROM region differs from source (applied anyway)");
        }

        // ── Apply cells ───────────────────────────────────────────────────────
        int mismatch = 0;
        for (const CellPatch &cp : mp.cells) {
            uint32_t cur = readRomValue(dp, dataLen,
                                       (uint32_t)(cp.memIdx * mp.dataSize),
                                       mp.dataSize, mapBo);
            if (cur != cp.refVal) ++mismatch;
            writeRomValue(dp, dataLen,
                          (uint32_t)(cp.memIdx * mp.dataSize),
                          mp.dataSize, mapBo, cp.newVal);
        }
        mr.applied    = mp.cells.size();
        mr.mismatches = mismatch;

        if (mismatch > 0)
            notes << QStringLiteral("%1 cell(s) had unexpected values (applied anyway)").arg(mismatch);

        if (notes.isEmpty()) {
            mr.status = MapApplyResult::Applied;
            mr.detail = QStringLiteral("%1 cell(s) applied").arg(mr.applied);
        } else {
            mr.status = MapApplyResult::AppliedWithWarnings;
            mr.detail = QStringLiteral("%1 cell(s) applied — ").arg(mr.applied) + notes.join("; ");
        }
        result.maps.append(mr);
    }

    // ── Apply raw bytes (checksums / non-map differences) ─────────────────────
    for (const RawBytePatch &rb : rawBytes) {
        if ((int)rb.offset >= romLen) continue;
        if (rd[rb.offset] != rb.refVal) ++result.rawBytesMismatched;
        rd[rb.offset] = rb.newVal;
        ++result.rawBytesApplied;
    }

    return result;
}
