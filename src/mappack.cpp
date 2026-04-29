/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mappack.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Extract pre-computed physical axis values from an AxisInfo.
// Falls back to ROM data in `rom` if the axis values are stored there.
static QVector<double> extractAxisValues(const AxisInfo &ax,
                                          const QByteArray &rom,
                                          ByteOrder bo)
{
    if (!ax.fixedValues.isEmpty())
        return ax.fixedValues;

    QVector<double> vals;
    if (!ax.hasPtsAddress || ax.ptsCount <= 0)
        return vals;

    const auto *raw = reinterpret_cast<const uint8_t *>(rom.constData());
    const int   dlen = rom.size();
    for (int i = 0; i < ax.ptsCount; ++i) {
        uint32_t off = ax.ptsAddress + uint32_t(i) * ax.ptsDataSize;
        double   sv  = readRomValueAsDouble(raw, dlen, off, ax.ptsDataSize, bo, ax.ptsSigned);
        double   phys = (ax.hasScaling && ax.scaling.type != CompuMethod::Type::Identical)
                        ? ax.scaling.toPhysical(sv) : sv;
        vals.append(phys);
    }
    return vals;
}

// Populate the display-metadata fields of e from a MapInfo.
static void fillMetadata(MapPackEntry &e, const MapInfo &mi,
                         const QByteArray &rom, ByteOrder bo)
{
    e.description = mi.description;

    e.xAxisName   = mi.xAxis.inputName;
    e.xAxisUnit   = mi.xAxis.hasScaling ? mi.xAxis.scaling.unit : QString();
    e.xAxisValues = extractAxisValues(mi.xAxis, rom, bo);

    e.yAxisName   = mi.yAxis.inputName;
    e.yAxisUnit   = mi.yAxis.hasScaling ? mi.yAxis.scaling.unit : QString();
    e.yAxisValues = extractAxisValues(mi.yAxis, rom, bo);

    e.hasScaling  = mi.hasScaling;
    e.scaling     = mi.scaling;
    e.zUnit       = mi.hasScaling ? mi.scaling.unit : QString();
}

// ── Build helpers ─────────────────────────────────────────────────────────────

MapPack MapPack::fromDiffs(const QVector<MapDiff> &diffs,
                            const QByteArray &cmpRom,
                            ByteOrder bo,
                            const QString &label)
{
    MapPack pk;
    pk.created = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    pk.label   = label.isEmpty() ? QStringLiteral("Map Pack") : label;

    for (const MapDiff &md : diffs) {
        int cols    = md.map.dimensions.x;
        int rows    = qMax(1, md.map.dimensions.y);
        if (cols <= 0 || rows <= 0 || cols > 10000 || rows > 10000) continue;
        int cells   = cols * rows;
        if (cells > 1000000) continue;
        int dataOff = (int)md.map.mapDataOffset;
        int dataLen = cells * md.map.dataSize;
        if (dataLen <= 0 || md.map.dataSize <= 0) continue;

        if ((int)(md.cmpOffset + dataOff + dataLen) > cmpRom.size()) continue;

        MapPackEntry e;
        e.name          = md.map.name;
        e.cols          = cols;
        e.rows          = rows;
        e.dataSize      = md.map.dataSize;
        e.bigEndian     = (bo == ByteOrder::BigEndian);
        e.columnMajor   = md.map.columnMajor;
        // Store the reference (ORI) address — this is where we write on import.
        // cmpOffset is only used here to READ the Stage3 data.
        e.address       = md.map.address;
        e.mapDataOffset = md.map.mapDataOffset;
        e.data          = cmpRom.mid((int)(md.cmpOffset + dataOff), dataLen);
        fillMetadata(e, md.map, cmpRom, bo);
        pk.maps.append(e);
    }
    return pk;
}

MapPack MapPack::fromMaps(const QByteArray &rom,
                           const QVector<MapInfo> &selectedMaps,
                           ByteOrder bo,
                           const QString &label)
{
    MapPack pk;
    pk.created = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    pk.label   = label.isEmpty() ? QStringLiteral("Map Pack") : label;

    for (const MapInfo &mi : selectedMaps) {
        int cols    = mi.dimensions.x;
        int rows    = qMax(1, mi.dimensions.y);
        if (cols <= 0 || rows <= 0 || cols > 10000 || rows > 10000) continue;
        int cells   = cols * rows;
        if (cells > 1000000) continue;
        int dataOff = (int)mi.mapDataOffset;
        int dataLen = cells * mi.dataSize;
        if (dataLen <= 0 || mi.dataSize <= 0) continue;
        if ((int)(mi.address + dataOff + dataLen) > rom.size()) continue;

        MapPackEntry e;
        e.name          = mi.name;
        e.cols          = cols;
        e.rows          = rows;
        e.dataSize      = mi.dataSize;
        e.bigEndian     = (bo == ByteOrder::BigEndian);
        e.columnMajor   = mi.columnMajor;
        e.address       = mi.address;
        e.mapDataOffset = mi.mapDataOffset;
        e.data          = rom.mid((int)(mi.address + dataOff), dataLen);
        fillMetadata(e, mi, rom, bo);
        pk.maps.append(e);
    }
    return pk;
}

// ── Serialise ─────────────────────────────────────────────────────────────────

QJsonDocument MapPack::toJson() const
{
    QJsonObject root;
    root["format"]  = QStringLiteral("rxpack");
    root["version"] = version;
    root["created"] = created;
    root["label"]   = label;

    auto toJsonArray = [](const QVector<double> &v) {
        QJsonArray a;
        for (double d : v) a.append(d);
        return a;
    };
    auto scalingToJson = [](const CompuMethod &cm) {
        QJsonObject s;
        s["type"]   = (int)cm.type;
        s["unit"]   = cm.unit;
        s["format"] = cm.format;
        s["linA"]   = cm.linA;
        s["linB"]   = cm.linB;
        s["rfA"]    = cm.rfA; s["rfB"] = cm.rfB; s["rfC"] = cm.rfC;
        s["rfD"]    = cm.rfD; s["rfE"] = cm.rfE; s["rfF"] = cm.rfF;
        return s;
    };

    QJsonArray arr;
    for (const MapPackEntry &e : maps) {
        QJsonObject mo;
        mo["name"]          = e.name;
        mo["description"]   = e.description;
        mo["cols"]          = e.cols;
        mo["rows"]          = e.rows;
        mo["dataSize"]      = e.dataSize;
        mo["bigEndian"]     = e.bigEndian;
        mo["columnMajor"]   = e.columnMajor;
        mo["address"]       = (qint64)e.address;
        mo["mapDataOffset"] = (qint64)e.mapDataOffset;
        mo["data"]          = QString::fromLatin1(e.data.toBase64());
        mo["xAxisName"]     = e.xAxisName;
        mo["xAxisUnit"]     = e.xAxisUnit;
        mo["xAxisValues"]   = toJsonArray(e.xAxisValues);
        mo["yAxisName"]     = e.yAxisName;
        mo["yAxisUnit"]     = e.yAxisUnit;
        mo["yAxisValues"]   = toJsonArray(e.yAxisValues);
        mo["zUnit"]         = e.zUnit;
        mo["hasScaling"]    = e.hasScaling;
        if (e.hasScaling)
            mo["scaling"]   = scalingToJson(e.scaling);
        arr.append(mo);
    }
    root["maps"] = arr;
    return QJsonDocument(root);
}

// ── Deserialise ───────────────────────────────────────────────────────────────

MapPack MapPack::fromJson(const QJsonDocument &doc, QString *error)
{
    MapPack pk;
    QJsonObject root = doc.object();
    if (root["format"].toString() != QLatin1String("rxpack")) {
        if (error) *error = QStringLiteral("Not a valid .rxpack file");
        return pk;
    }
    pk.version = root["version"].toInt(1);
    pk.created = root["created"].toString();
    pk.label   = root["label"].toString();

    auto fromJsonArray = [](const QJsonValue &val) {
        QVector<double> v;
        for (const QJsonValue &d : val.toArray()) v.append(d.toDouble());
        return v;
    };
    auto scalingFromJson = [](const QJsonObject &s) {
        CompuMethod cm;
        cm.type   = (CompuMethod::Type)s["type"].toInt(0);
        cm.unit   = s["unit"].toString();
        cm.format = s["format"].toString();
        cm.linA   = s["linA"].toDouble(1.0);
        cm.linB   = s["linB"].toDouble(0.0);
        cm.rfA    = s["rfA"].toDouble(); cm.rfB = s["rfB"].toDouble(); cm.rfC = s["rfC"].toDouble();
        cm.rfD    = s["rfD"].toDouble(); cm.rfE = s["rfE"].toDouble(1.0); cm.rfF = s["rfF"].toDouble();
        return cm;
    };

    for (const QJsonValue &v : root["maps"].toArray()) {
        QJsonObject mo = v.toObject();
        MapPackEntry e;
        e.name          = mo["name"].toString();
        e.description   = mo["description"].toString();
        e.cols          = mo["cols"].toInt(1);
        e.rows          = mo["rows"].toInt(1);
        e.dataSize      = mo["dataSize"].toInt(2);
        e.bigEndian     = mo["bigEndian"].toBool(true);
        e.columnMajor   = mo["columnMajor"].toBool(false);
        e.address       = (uint32_t)mo["address"].toInteger(0);
        e.mapDataOffset = (uint32_t)mo["mapDataOffset"].toInteger(0);
        e.data          = QByteArray::fromBase64(mo["data"].toString().toLatin1());
        e.xAxisName     = mo["xAxisName"].toString();
        e.xAxisUnit     = mo["xAxisUnit"].toString();
        e.xAxisValues   = fromJsonArray(mo["xAxisValues"]);
        e.yAxisName     = mo["yAxisName"].toString();
        e.yAxisUnit     = mo["yAxisUnit"].toString();
        e.yAxisValues   = fromJsonArray(mo["yAxisValues"]);
        e.zUnit         = mo["zUnit"].toString();
        e.hasScaling    = mo["hasScaling"].toBool(false);
        if (e.hasScaling)
            e.scaling   = scalingFromJson(mo["scaling"].toObject());
        pk.maps.append(e);
    }
    return pk;
}

bool MapPack::save(const QString &path, QString *error) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(toJson().toJson(QJsonDocument::Indented));
    return true;
}

MapPack MapPack::load(const QString &path, QString *error)
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

QStringList MapPack::apply(QByteArray &rom,
                            const QVector<MapInfo> &projectMaps,
                            ByteOrder bo,
                            const QMap<QString, uint32_t> &mapOffsets) const
{
    QStringList warnings;
    int romLen = rom.size();

    for (const MapPackEntry &e : maps) {
        const MapInfo *mi = nullptr;
        for (const MapInfo &m : projectMaps)
            if (m.name == e.name) { mi = &m; break; }

        // Resolve base address and data offset.
        // Priority: explicit offset override → project map → self-contained address in pack entry.
        uint32_t base;
        int      dataOff;
        if (mapOffsets.contains(e.name)) {
            base    = mapOffsets[e.name];
            dataOff = mi ? (int)mi->mapDataOffset : (int)e.mapDataOffset;
        } else if (mi) {
            base    = mi->address;
            dataOff = (int)mi->mapDataOffset;
        } else if (e.address != 0 || e.mapDataOffset != 0) {
            // No project map found — use the address embedded in the pack itself
            base    = e.address;
            dataOff = (int)e.mapDataOffset;
        } else {
            warnings << QStringLiteral("Map not found and no address in pack: %1").arg(e.name);
            continue;
        }

        if (e.cols <= 0 || e.rows <= 0 || e.cols > 10000 || e.rows > 10000 || e.dataSize <= 0) {
            warnings << QStringLiteral("Invalid map dimensions: %1").arg(e.name);
            continue;
        }
        int dataLen = e.cols * e.rows * e.dataSize;
        if (dataLen > 10000000 || dataLen <= 0) {
            warnings << QStringLiteral("Map data length out of bounds: %1").arg(e.name);
            continue;
        }

        if (e.data.size() < dataLen) {
            warnings << QStringLiteral("Map pack data too short for: %1").arg(e.name);
            continue;
        }
        if ((int)(base + dataOff + dataLen) > romLen) {
            warnings << QStringLiteral("Map out of ROM bounds: %1").arg(e.name);
            continue;
        }

        // Dimension mismatch check (only possible when we have a project map to compare against)
        if (mi && (e.cols != mi->dimensions.x || e.rows != qMax(1, mi->dimensions.y))) {
            warnings << QStringLiteral("Map %1: dimension mismatch (pack %2×%3, project %4×%5) — skipped")
                        .arg(e.name).arg(e.cols).arg(e.rows)
                        .arg(mi->dimensions.x).arg(qMax(1, mi->dimensions.y));
            continue;
        }

        // Byte-order conversion if pack was stored with different endianness
        bool packBE    = e.bigEndian;
        bool projectBE = (bo == ByteOrder::BigEndian);

        if (packBE == projectBE || e.dataSize == 1) {
            // Same byte order — direct copy
            memcpy(rom.data() + base + dataOff, e.data.constData(), dataLen);
        } else {
            // Byte-swap each cell
            const uint8_t *src = (const uint8_t *)e.data.constData();
            uint8_t       *dst = (uint8_t *)rom.data() + base + dataOff;
            ByteOrder srcBO = packBE ? ByteOrder::BigEndian : ByteOrder::LittleEndian;
            for (int i = 0; i < e.cols * e.rows; ++i) {
                uint32_t val = readRomValue(src, dataLen,
                                           (uint32_t)(i * e.dataSize),
                                           e.dataSize, srcBO);
                writeRomValue(dst, dataLen,
                              (uint32_t)(i * e.dataSize),
                              e.dataSize, bo, val);
            }
        }
    }
    return warnings;
}
