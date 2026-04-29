/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "rx14container.h"
#include "util/Blake3.h"

#include <QtEndian>
#include <QDebug>
#include <cstring>

namespace rx14fmt {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool readExact(QIODevice *in, char *buf, qint64 n, QString *err, const char *what)
{
    const qint64 got = in->read(buf, n);
    if (got != n) {
        if (err)
            *err = QString("Failed to read %1: expected %2 bytes, got %3")
                       .arg(what).arg(n).arg(got);
        return false;
    }
    return true;
}

static void writeRaw(QIODevice *out, const char *buf, qint64 n)
{
    out->write(buf, n);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reader
// ─────────────────────────────────────────────────────────────────────────────

FileHeader readHeader(QIODevice *in, QString *err)
{
    FileHeader hdr;
    char raw[FILE_HEADER_SIZE];
    if (!readExact(in, raw, FILE_HEADER_SIZE, err, "file header"))
        return hdr;

    hdr.magic         = qFromLittleEndian<uint32_t>(raw + 0x00);
    hdr.formatVersion = qFromLittleEndian<uint32_t>(raw + 0x04);
    hdr.totalFileSize = qFromLittleEndian<uint64_t>(raw + 0x08);
    hdr.tocOffset     = qFromLittleEndian<uint32_t>(raw + 0x10);
    hdr.tocBlockCount = qFromLittleEndian<uint32_t>(raw + 0x14);
    hdr.bodyChecksum  = QByteArray(raw + 0x18, 16);

    if (hdr.magic != 0x52583134) {
        if (err)
            *err = QString("Bad magic: 0x%1 (expected 0x52583134)")
                       .arg(hdr.magic, 8, 16, QChar('0'));
    }
    return hdr;
}

QVector<TocEntry> readToc(QIODevice *in, const FileHeader &hdr, QString *err)
{
    QVector<TocEntry> entries;

    if (!in->seek(hdr.tocOffset)) {
        if (err) *err = QStringLiteral("Failed to seek to TOC");
        return entries;
    }

    // Read TOC preamble
    char preamble[TOC_PREAMBLE_SIZE];
    if (!readExact(in, preamble, TOC_PREAMBLE_SIZE, err, "TOC preamble"))
        return entries;

    const uint32_t tocMagic = qFromLittleEndian<uint32_t>(preamble + 0);
    if (tocMagic != TOC_MAGIC) {
        if (err)
            *err = QString("Bad TOC magic: 0x%1").arg(tocMagic, 8, 16, QChar('0'));
        return entries;
    }

    entries.reserve(static_cast<int>(hdr.tocBlockCount));
    for (uint32_t i = 0; i < hdr.tocBlockCount; ++i) {
        char raw[TOC_ENTRY_SIZE];
        if (!readExact(in, raw, TOC_ENTRY_SIZE, err, "TOC entry"))
            return entries;

        TocEntry e;
        e.blockMagic  = qFromLittleEndian<uint32_t>(raw + 0);
        e.blockOffset = qFromLittleEndian<uint64_t>(raw + 4);
        e.blockSize   = qFromLittleEndian<uint64_t>(raw + 12);
        entries.append(e);
    }
    return entries;
}

BlockHeader readBlockHeader(QIODevice *in, QString *err)
{
    BlockHeader bh;
    char raw[BLOCK_HEADER_SIZE];
    if (!readExact(in, raw, BLOCK_HEADER_SIZE, err, "block header"))
        return bh;

    bh.blockMagic    = qFromLittleEndian<uint32_t>(raw + 0x00);
    bh.blockSchema   = qFromLittleEndian<uint32_t>(raw + 0x04);
    bh.payloadSize   = qFromLittleEndian<uint64_t>(raw + 0x08);
    bh.blockChecksum = QByteArray(raw + 0x10, 16);
    return bh;
}

// ─────────────────────────────────────────────────────────────────────────────
// Writer
// ─────────────────────────────────────────────────────────────────────────────

void writeHeader(QIODevice *out, const FileHeader &hdr)
{
    char raw[FILE_HEADER_SIZE];
    std::memset(raw, 0, FILE_HEADER_SIZE);

    qToLittleEndian<uint32_t>(hdr.magic,         raw + 0x00);
    qToLittleEndian<uint32_t>(hdr.formatVersion,  raw + 0x04);
    qToLittleEndian<uint64_t>(hdr.totalFileSize,  raw + 0x08);
    qToLittleEndian<uint32_t>(hdr.tocOffset,      raw + 0x10);
    qToLittleEndian<uint32_t>(hdr.tocBlockCount,  raw + 0x14);

    if (hdr.bodyChecksum.size() >= 16)
        std::memcpy(raw + 0x18, hdr.bodyChecksum.constData(), 16);

    // 0x28..0x3F reserved (already zeroed)
    writeRaw(out, raw, FILE_HEADER_SIZE);
}

void writeBlockHeader(QIODevice *out, const BlockHeader &hdr)
{
    char raw[BLOCK_HEADER_SIZE];
    std::memset(raw, 0, BLOCK_HEADER_SIZE);

    qToLittleEndian<uint32_t>(hdr.blockMagic,  raw + 0x00);
    qToLittleEndian<uint32_t>(hdr.blockSchema, raw + 0x04);
    qToLittleEndian<uint64_t>(hdr.payloadSize, raw + 0x08);

    if (hdr.blockChecksum.size() >= 16)
        std::memcpy(raw + 0x10, hdr.blockChecksum.constData(), 16);

    writeRaw(out, raw, BLOCK_HEADER_SIZE);
}

void writeTocEntry(QIODevice *out, const TocEntry &entry)
{
    char raw[TOC_ENTRY_SIZE];
    qToLittleEndian<uint32_t>(entry.blockMagic,  raw + 0);
    qToLittleEndian<uint64_t>(entry.blockOffset, raw + 4);
    qToLittleEndian<uint64_t>(entry.blockSize,   raw + 12);
    writeRaw(out, raw, TOC_ENTRY_SIZE);
}

void writeTocMagic(QIODevice *out)
{
    char raw[TOC_PREAMBLE_SIZE];
    std::memset(raw, 0, TOC_PREAMBLE_SIZE);
    qToLittleEndian<uint32_t>(TOC_MAGIC, raw + 0);
    // raw+4..raw+7 reserved = 0
    writeRaw(out, raw, TOC_PREAMBLE_SIZE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility
// ─────────────────────────────────────────────────────────────────────────────

QString magicToTag(uint32_t magic)
{
    char tag[5];
    tag[0] = static_cast<char>((magic >>  0) & 0xFF);
    tag[1] = static_cast<char>((magic >>  8) & 0xFF);
    tag[2] = static_cast<char>((magic >> 16) & 0xFF);
    tag[3] = static_cast<char>((magic >> 24) & 0xFF);
    tag[4] = '\0';
    return QString::fromLatin1(tag, 4);
}

} // namespace rx14fmt
