/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "OlsImporter.h"
#include "CArchiveReader.h"
#include "OlsHeader.h"
#include "OlsMagicScanner.h"
#include "OlsProjectMetadata.h"
#include "OlsVersionDirectory.h"
#include "OlsRomExtractor.h"
#include "OlsKennfeldParser.h"

#include <QtEndian>
#include <cstring>

namespace ols {

OlsImportResult OlsImporter::importFromBytes(const QByteArray &fileData)
{
    OlsImportResult result;

    result.header = OlsHeader::parse(fileData);
    if (!result.header.valid) {
        result.error = result.header.error;
        return result;
    }

    uint32_t fmtVer = result.header.formatVersion;

    if (fmtVer >= 200
        && result.header.declaredFileSize != static_cast<uint32_t>(fileData.size())) {
        result.warnings.append(
            OlsImporter::tr("Header declares size %1 but actual is %2")
                .arg(result.header.declaredFileSize)
                .arg(fileData.size()));
    }

    int schema = static_cast<int>(fmtVer);

    MagicAnchors anchors = OlsMagicScanner::scan(fileData, 1008);

    qsizetype metadataStart = (fmtVer >= 200) ? 0x18 : 0x14;
    try {
        CArchiveReader reader(fileData, metadataStart, fmtVer);
        result.metadata = OlsProjectMetadata::parse(reader, &result.warnings);
    } catch (const std::exception &e) {
        result.warnings.append(
            OlsImporter::tr("Project metadata parse failed: %1")
                .arg(QString::fromStdString(e.what())));
    }

    OlsVersionDirectory versionDir = OlsVersionDirectory::parse(
        fileData, &result.warnings);

    QVector<OlsRomResult> romVersions = OlsRomExtractor::extractAll(fileData);

    auto [mapRegionStart, mapRegionEnd] =
        OlsKennfeldParser::findMapRegion(fileData, schema);

    QVector<MapInfo> maps = OlsKennfeldParser::parseAll(
        fileData, mapRegionStart, mapRegionEnd, schema, &result.warnings);


    if (!romVersions.isEmpty() && !romVersions[0].error.isEmpty()
        && romVersions.size() == 1) {
        result.warnings.append(romVersions[0].error);
        OlsVersion ver;
        ver.name = OlsImporter::tr("Default");
        ver.maps = maps;
        result.versions.append(std::move(ver));
    } else {
        for (auto &rv : romVersions) {
            OlsVersion ver;
            ver.name = OlsImporter::tr("Version %1").arg(rv.versionIndex);
            ver.romData = rv.assembledRom;
            ver.segments = rv.segments;
            ver.byteOrder = ByteOrder::LittleEndian;

            uint32_t baseAddr = 0;
            if (!rv.segments.isEmpty()) {
                baseAddr = rv.segments[0].flashBase + rv.segments[0].framingBytes;
                for (const auto &seg : rv.segments) {
                    const uint32_t eff = seg.flashBase + seg.framingBytes;
                    if (eff < baseAddr)
                        baseAddr = eff;
                }
            }

            QVector<MapInfo> translatedMaps = maps;
            for (auto &m : translatedMaps) {
                m.rawAddress = m.address;
                m.address = (m.rawAddress >= baseAddr)
                    ? m.rawAddress - baseAddr : m.rawAddress;
                if (m.xAxis.hasPtsAddress && m.xAxis.ptsAddress >= baseAddr)
                    m.xAxis.ptsAddress -= baseAddr;
                if (m.yAxis.hasPtsAddress && m.yAxis.ptsAddress >= baseAddr)
                    m.yAxis.ptsAddress -= baseAddr;
            }
            for (auto &m : translatedMaps) {
                auto readAxisBreakpoints = [&](AxisInfo &ax, int count) {
                    if (!ax.hasPtsAddress || ax.ptsAddress == 0
                        || ax.ptsAddress == 0xFFFFFFFF || count <= 0)
                        return;

                    int cellBytes = ax.ptsDataSize;
                    if (cellBytes <= 0)
                        cellBytes = (m.dataSize > 0) ? m.dataSize : 2;
                    if (cellBytes != 1 && cellBytes != 2 && cellBytes != 4
                        && cellBytes != 8)
                        cellBytes = 2;

                    int startOff = static_cast<int>(ax.ptsAddress);
                    if (startOff < 0
                        || startOff + count * cellBytes > ver.romData.size())
                        return;
                    const auto *rom = reinterpret_cast<const uint8_t *>(
                        ver.romData.constData());

                    const uint32_t dt = ax.ptsDataType;
                    const bool sgn = ax.ptsSigned;
                    auto decodeOne = [&](int off) -> double {
                        switch (dt) {
                        case 2: {
                            uint16_t v = (uint16_t(rom[off]) << 8) | rom[off + 1];
                            return sgn ? double(int16_t(v)) : double(v);
                        }
                        case 3: {
                            uint16_t v = uint16_t(rom[off])
                                       | (uint16_t(rom[off + 1]) << 8);
                            return sgn ? double(int16_t(v)) : double(v);
                        }
                        case 4: {
                            uint32_t v = (uint32_t(rom[off]) << 24)
                                       | (uint32_t(rom[off + 1]) << 16)
                                       | (uint32_t(rom[off + 2]) << 8)
                                       |  uint32_t(rom[off + 3]);
                            return sgn ? double(int32_t(v)) : double(v);
                        }
                        case 5: {
                            uint32_t v = uint32_t(rom[off])
                                       | (uint32_t(rom[off + 1]) << 8)
                                       | (uint32_t(rom[off + 2]) << 16)
                                       | (uint32_t(rom[off + 3]) << 24);
                            return sgn ? double(int32_t(v)) : double(v);
                        }
                        case 6: {
                            uint32_t v = (uint32_t(rom[off]) << 24)
                                       | (uint32_t(rom[off + 1]) << 16)
                                       | (uint32_t(rom[off + 2]) << 8)
                                       |  uint32_t(rom[off + 3]);
                            float f; std::memcpy(&f, &v, 4); return f;
                        }
                        case 7: {
                            float f; std::memcpy(&f, rom + off, 4); return f;
                        }
                        case 11: {
                            uint64_t v = 0;
                            std::memcpy(&v, rom + off, 8);
                            return double(v);
                        }
                        case 13: {
                            double f; std::memcpy(&f, rom + off, 8); return f;
                        }
                        case 1: case 8: case 9: {
                            uint8_t v = rom[off];
                            return sgn ? double(int8_t(v)) : double(v);
                        }
                        default: {
                            switch (cellBytes) {
                            case 1: {
                                uint8_t v = rom[off];
                                return sgn ? double(int8_t(v)) : double(v);
                            }
                            case 2: {
                                uint16_t v = qFromLittleEndian<uint16_t>(rom + off);
                                return sgn ? double(int16_t(v)) : double(v);
                            }
                            case 4: {
                                uint32_t v = qFromLittleEndian<uint32_t>(rom + off);
                                return sgn ? double(int32_t(v)) : double(v);
                            }
                            default: {
                                uint8_t v = rom[off];
                                return sgn ? double(int8_t(v)) : double(v);
                            }
                            }
                        }
                        }
                    };

                    QVector<double> rawVals;
                    rawVals.reserve(count);
                    for (int i = 0; i < count; ++i)
                        rawVals.append(decodeOne(startOff + i * cellBytes));

                    ax.fixedValues = rawVals;
                    ax.ptsCount = count;
                    ax.ptsDataSize = cellBytes;
                };
                readAxisBreakpoints(m.xAxis, m.dimensions.x);
                readAxisBreakpoints(m.yAxis, m.dimensions.y);
            }

            ver.maps = translatedMaps;
            ver.baseAddress = baseAddr;

            for (const auto &w : rv.warnings)
                result.warnings.append(w);

            result.versions.append(std::move(ver));
        }
    }

    return result;
}

}
