/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QtEndian>
#include <cstdint>

namespace ols {

class CArchiveReader {
public:
    CArchiveReader(const QByteArray &buf, qsizetype off, uint32_t formatVersion)
        : m_buf(buf), m_off(off), m_formatVersion(formatVersion) {}

    qsizetype pos()  const { return m_off; }
    qsizetype size() const { return m_buf.size(); }
    bool      eof()  const { return m_off >= m_buf.size(); }
    void      seek(qsizetype p) { m_off = p; }
    void      skip(qsizetype n) { m_off += n; }

    uint32_t formatVersion() const { return m_formatVersion; }
    bool     hasField(uint32_t minVersion) const { return m_formatVersion >= minVersion; }

    uint8_t  u8();
    bool     boolean();
    uint32_t u32();
    int32_t  i32();
    uint64_t u64();
    double   f64();

    QString    cstring();
    QByteArray cstringBytes();
    QByteArray byteArray();
    QVector<uint64_t> u64Array();
    QByteArray bulk(qsizetype n);

    bool verifyMagic(uint32_t expected, bool strict = true);

    QString errorContext() const;

private:
    void ensureAvailable(qsizetype n) const;

    QByteArray m_buf;
    qsizetype  m_off = 0;
    uint32_t   m_formatVersion = 0;
};

}
