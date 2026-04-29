/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "OlsProjectMetadata.h"
#include "CArchiveReader.h"

namespace ols {

OlsProjectMetadata OlsProjectMetadata::parse(CArchiveReader &ar,
                                              QStringList *warnings)
{
    OlsProjectMetadata m;

    auto tryString = [&](QString &dst, const char *name) -> bool {
        try { dst = ar.cstring(); return true; }
        catch (...) {
            if (warnings)
                warnings->append(
                    QStringLiteral("failed at always-read '%1' (off=0x%2)")
                        .arg(QLatin1String(name))
                        .arg(ar.pos(), 0, 16));
            return false;
        }
    };

    if (!tryString(m.make,              "Make"))              return m;
    if (!tryString(m.model,             "Model"))             return m;
    if (!tryString(m.type,              "Type"))              return m;
    if (!tryString(m.year,              "Year"))              return m;
    if (!tryString(m.outputKwPs,        "Output_kW_PS"))      return m;
    if (!tryString(m.cylinders,         "Cylinders"))         return m;
    if (!tryString(m.country,           "Country"))           return m;
    if (!tryString(m.drivetrain,        "Drivetrain"))        return m;
    if (!tryString(m.memory,            "Memory"))            return m;
    if (!tryString(m.manufacturer,      "Manufacturer"))      return m;
    if (!tryString(m.ecuName,           "EcuName"))           return m;
    if (!tryString(m.hwNumber,          "HwNumber"))          return m;
    if (!tryString(m.swNumber,          "SwNumber"))          return m;
    if (!tryString(m.productionNo,      "ProductionNo"))      return m;
    if (!tryString(m.engineCode,        "EngineCode"))        return m;
    if (!tryString(m.transmission,      "Transmission"))      return m;

    try { m.lastWriteTime = ar.u64(); }
    catch (...) {
        if (warnings)
            warnings->append(
                QStringLiteral("failed at always-read 'LastWriteTime' (off=0x%1)")
                    .arg(ar.pos(), 0, 16));
        return m;
    }

    if (!tryString(m.originalFileName,    "OriginalFileName"))    return m;
    if (!tryString(m.olsVersionString, "OLSVersionString")) return m;
    if (!tryString(m.reserved2,           "Reserved2"))           return m;
    if (!tryString(m.revisionTag,         "RevisionTag"))         return m;
    if (!tryString(m.reserved3,           "Reserved3"))           return m;
    if (!tryString(m.reserved4,           "Reserved4"))           return m;
    if (!tryString(m.reserved5,           "Reserved5"))           return m;


    auto gatedString = [&](uint32_t minVer, QString &dst, const char *name) -> bool {
        if (!ar.hasField(minVer)) return true;
        if (ar.eof()) {
            if (warnings)
                warnings->append(
                    QStringLiteral("reached EOF before optional '%1' (minVer=0x%2)")
                        .arg(QLatin1String(name))
                        .arg(minVer, 0, 16));
            return false;
        }
        const auto saved = ar.pos();
        try { dst = ar.cstring(); return true; }
        catch (...) {
            if (warnings)
                warnings->append(
                    QStringLiteral("failed at gated '%1' (off=0x%2, minVer=0x%3)")
                        .arg(QLatin1String(name))
                        .arg(saved, 0, 16)
                        .arg(minVer, 0, 16));
            ar.seek(saved);
            return false;
        }
    };

    auto gatedU32 = [&](uint32_t minVer, uint32_t &dst, const char *name) -> bool {
        if (!ar.hasField(minVer)) return true;
        if (ar.eof()) {
            if (warnings)
                warnings->append(
                    QStringLiteral("reached EOF before optional '%1' (minVer=0x%2)")
                        .arg(QLatin1String(name))
                        .arg(minVer, 0, 16));
            return false;
        }
        const auto saved = ar.pos();
        try { dst = ar.u32(); return true; }
        catch (...) {
            if (warnings)
                warnings->append(
                    QStringLiteral("failed at gated '%1' (off=0x%2, minVer=0x%3)")
                        .arg(QLatin1String(name))
                        .arg(saved, 0, 16)
                        .arg(minVer, 0, 16));
            ar.seek(saved);
            return false;
        }
    };

    auto gatedU64 = [&](uint32_t minVer, uint64_t &dst, const char *name) -> bool {
        if (!ar.hasField(minVer)) return true;
        if (ar.eof()) {
            if (warnings)
                warnings->append(
                    QStringLiteral("reached EOF before optional '%1' (minVer=0x%2)")
                        .arg(QLatin1String(name))
                        .arg(minVer, 0, 16));
            return false;
        }
        const auto saved = ar.pos();
        try { dst = ar.u64(); return true; }
        catch (...) {
            if (warnings)
                warnings->append(
                    QStringLiteral("failed at gated '%1' (off=0x%2, minVer=0x%3)")
                        .arg(QLatin1String(name))
                        .arg(saved, 0, 16)
                        .arg(minVer, 0, 16));
            ar.seek(saved);
            return false;
        }
    };

    if (!gatedString(0x6E, m.baseAddressHex,  "BaseAddressHex"))  return m;
    if (!gatedU32   (0x30, m.buildNumber,     "BuildNumber"))     return m;
    if (!gatedU64   (0x34, m.checksum,        "Checksum"))        return m;
    if (!gatedU32   (0x38, m.flags,           "Flags"))           return m;
    if (!gatedString(0x3E, m.importComment,   "ImportComment"))   return m;
    if (!gatedU32   (0x3F, m.postCommentFlag, "PostCommentFlag")) return m;
    if (!gatedString(0x40, m.tag,             "Tag"))             return m;
    if (!gatedU32   (0x4F, m.regionCount,     "RegionCount"))     return m;
    if (!gatedString(0x55, m.notes,           "Notes"))           return m;

    return m;
}

}
