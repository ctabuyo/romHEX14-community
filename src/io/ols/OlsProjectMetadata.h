/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>
#include <cstdint>

namespace ols {

class CArchiveReader;

struct OlsProjectMetadata {
    QString make;
    QString model;
    QString type;
    QString year;
    QString outputKwPs;
    QString cylinders;
    QString country;
    QString drivetrain;
    QString memory;
    QString manufacturer;
    QString ecuName;
    QString hwNumber;
    QString swNumber;
    QString productionNo;
    QString engineCode;
    QString transmission;
    uint64_t lastWriteTime = 0;
    QString originalFileName;
    QString olsVersionString;
    QString reserved2;
    QString revisionTag;
    QString reserved3;
    QString reserved4;
    QString reserved5;

    QString  baseAddressHex;
    uint32_t buildNumber = 0;
    uint64_t checksum = 0;
    uint32_t flags = 0;
    QString  importComment;
    uint32_t postCommentFlag = 0;
    QString  tag;
    uint32_t regionCount = 0;
    QString  notes;

    static OlsProjectMetadata parse(CArchiveReader &reader,
                                    QStringList *warnings = nullptr);
};

}
