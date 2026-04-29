/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "OlsHeader.h"
#include "OlsProjectMetadata.h"
#include "OlsVersionDirectory.h"
#include "OlsRomExtractor.h"
#include "../../romdata.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace ols {

struct OlsVersion {
    QString          name;
    QByteArray       romData;
    QVector<OlsSegment> segments;
    QVector<MapInfo> maps;
    ByteOrder        byteOrder = ByteOrder::BigEndian;
    uint32_t         baseAddress = 0;
};

struct OlsImportResult {
    OlsHeader              header;
    OlsProjectMetadata     metadata;
    QVector<OlsVersion>    versions;
    QString                error;
    QStringList            warnings;
};

class OlsImporter {
    Q_DECLARE_TR_FUNCTIONS(ols::OlsImporter)
public:
    static OlsImportResult importFromBytes(const QByteArray &fileData);
};

}
