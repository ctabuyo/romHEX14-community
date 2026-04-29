/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "OlsHeader.h"
#include <QtEndian>
#include <cstring>

namespace ols {


static constexpr qsizetype kHeaderSize = 24;
static constexpr uint32_t  kExpectedMagic = 0x0000000Bu;
static const char          kSignature[] = "OLS File";
static constexpr int       kSignatureLen = 11;

OlsHeader OlsHeader::parse(const QByteArray &fileData)
{
    OlsHeader hdr;

    if (fileData.size() < kHeaderSize) {
        hdr.error = OlsHeader::tr("File too small for OLS header (%1 bytes, need %2)")
                        .arg(fileData.size())
                        .arg(kHeaderSize);
        return hdr;
    }

    const auto *data = reinterpret_cast<const uchar *>(fileData.constData());

    hdr.magic = qFromLittleEndian<uint32_t>(data + 0x00);
    if (hdr.magic != kExpectedMagic) {
        hdr.error = OlsHeader::tr("Magic mismatch: expected 0x%1, got 0x%2")
                        .arg(kExpectedMagic, 8, 16, QLatin1Char('0'))
                        .arg(hdr.magic, 8, 16, QLatin1Char('0'));
        return hdr;
    }

    if (std::memcmp(fileData.constData() + 0x04, kSignature, kSignatureLen) != 0) {
        hdr.error = OlsHeader::tr("Signature mismatch: expected 'OLS File'");
        return hdr;
    }

    if (static_cast<uint8_t>(fileData[0x0F]) != 0x00) {
        hdr.error = OlsHeader::tr("Missing NUL terminator after signature");
        return hdr;
    }

    hdr.formatVersion    = qFromLittleEndian<uint32_t>(data + 0x10);
    hdr.declaredFileSize = qFromLittleEndian<uint32_t>(data + 0x14);
    hdr.valid = true;


    return hdr;
}

}
