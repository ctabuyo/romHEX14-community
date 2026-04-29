/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CArchiveReader.h"
#include <QStringDecoder>
#include <stdexcept>

namespace ols {


void CArchiveReader::ensureAvailable(qsizetype n) const
{
    if (m_off + n > m_buf.size()) {
        throw std::runtime_error(
            QStringLiteral("CArchiveReader: read of %1 bytes at offset 0x%2 "
                           "exceeds buffer size 0x%3")
                .arg(n)
                .arg(m_off, 0, 16)
                .arg(m_buf.size(), 0, 16)
                .toStdString());
    }
}


uint8_t CArchiveReader::u8()
{
    ensureAvailable(1);
    uint8_t v = static_cast<uint8_t>(m_buf[m_off]);
    m_off += 1;
    return v;
}

bool CArchiveReader::boolean()
{
    return u8() != 0;
}


uint32_t CArchiveReader::u32()
{
    ensureAvailable(4);
    uint32_t v = qFromLittleEndian<uint32_t>(
        reinterpret_cast<const uchar *>(m_buf.constData() + m_off));
    m_off += 4;
    return v;
}

int32_t CArchiveReader::i32()
{
    ensureAvailable(4);
    int32_t v = qFromLittleEndian<int32_t>(
        reinterpret_cast<const uchar *>(m_buf.constData() + m_off));
    m_off += 4;
    return v;
}


uint64_t CArchiveReader::u64()
{
    ensureAvailable(8);
    uint64_t v = qFromLittleEndian<uint64_t>(
        reinterpret_cast<const uchar *>(m_buf.constData() + m_off));
    m_off += 8;
    return v;
}

double CArchiveReader::f64()
{
    ensureAvailable(8);
    uint64_t bits = qFromLittleEndian<uint64_t>(
        reinterpret_cast<const uchar *>(m_buf.constData() + m_off));
    m_off += 8;
    double v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}


QByteArray CArchiveReader::cstringBytes()
{
    ensureAvailable(4);
    int32_t length = qFromLittleEndian<int32_t>(
        reinterpret_cast<const uchar *>(m_buf.constData() + m_off));
    m_off += 4;

    if (m_formatVersion >= 439) {
        if (length == -1) return QByteArrayLiteral("-");
        if (length == -2) return QByteArrayLiteral("?");
        if (length == -3) return QByteArrayLiteral("%");
        if (length <= 0)  return QByteArray();
        ensureAvailable(static_cast<qsizetype>(length));
        QByteArray body(m_buf.constData() + m_off, static_cast<qsizetype>(length));
        m_off += static_cast<qsizetype>(length);
        return body;
    }

    if (length <= 0)
        return QByteArray();

    ensureAvailable(static_cast<qsizetype>(length) + 1);

    QByteArray body(m_buf.constData() + m_off, static_cast<qsizetype>(length));
    m_off += static_cast<qsizetype>(length);

    uint8_t nul = static_cast<uint8_t>(m_buf[m_off]);
    m_off += 1;
    if (nul != 0x00) {
        throw std::runtime_error(
            QStringLiteral("CString NUL terminator mismatch at 0x%1: got 0x%2")
                .arg(m_off - 1, 0, 16)
                .arg(nul, 2, 16, QLatin1Char('0'))
                .toStdString());
    }

    return body;
}

static QString decodeOlsString(const QByteArray &raw)
{
    if (raw.isEmpty()) return QString();

    bool allAscii = true;
    for (char c : raw) {
        if (static_cast<uint8_t>(c) >= 0x80) { allAscii = false; break; }
    }
    if (allAscii)
        return QString::fromLatin1(raw);

    QStringDecoder utf8(QStringDecoder::Utf8);
    QString asUtf8 = utf8(raw);
    if (!utf8.hasError() && !asUtf8.contains(QChar::ReplacementCharacter))
        return asUtf8;

    QStringDecoder cp1252("Windows-1252");
    if (cp1252.isValid())
        return cp1252(raw);

    return QString::fromLatin1(raw);
}

QString CArchiveReader::cstring()
{
    QByteArray raw = cstringBytes();
    return decodeOlsString(raw);
}


QByteArray CArchiveReader::byteArray()
{
    uint32_t length = u32();
    if (length == 0)
        return QByteArray();
    if (length > 0x06400000u) {
        throw std::runtime_error(
            QStringLiteral("byteArray length 0x%1 out of range at 0x%2")
                .arg(length, 0, 16)
                .arg(m_off - 4, 0, 16)
                .toStdString());
    }
    ensureAvailable(static_cast<qsizetype>(length));
    QByteArray out(m_buf.constData() + m_off, static_cast<qsizetype>(length));
    m_off += static_cast<qsizetype>(length);
    return out;
}


QVector<uint64_t> CArchiveReader::u64Array()
{
    uint32_t count = u32();
    if (count > 0x00100000u) {
        throw std::runtime_error(
            QStringLiteral("u64Array count 0x%1 out of range at 0x%2")
                .arg(count, 0, 16)
                .arg(m_off - 4, 0, 16)
                .toStdString());
    }
    QVector<uint64_t> result;
    result.reserve(static_cast<int>(count));
    for (uint32_t i = 0; i < count; ++i)
        result.append(u64());
    return result;
}


QByteArray CArchiveReader::bulk(qsizetype n)
{
    ensureAvailable(n);
    QByteArray out(m_buf.constData() + m_off, n);
    m_off += n;
    return out;
}


bool CArchiveReader::verifyMagic(uint32_t expected, bool strict)
{
    uint32_t got = u32();
    if (got == expected)
        return true;
    if (strict) {
        throw std::runtime_error(
            QStringLiteral("magic mismatch at 0x%1: got 0x%2, expected 0x%3")
                .arg(m_off - 4, 0, 16)
                .arg(got, 8, 16, QLatin1Char('0'))
                .arg(expected, 8, 16, QLatin1Char('0'))
                .toStdString());
    }
    return false;
}


QString CArchiveReader::errorContext() const
{
    return QStringLiteral("@0x%1").arg(m_off, 0, 16);
}

}
