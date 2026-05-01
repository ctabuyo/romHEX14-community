/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/MapListExporter.h"
#include "project.h"
#include "romdata.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QtMath>
#include <algorithm>
#include <limits>

namespace {

QString csvEscape(const QString &s)
{
    if (s.isEmpty()) return s;
    bool needsQuotes = s.contains(',') || s.contains('"')
                       || s.contains('\n') || s.contains('\r');
    QString out = s;
    out.replace('"', "\"\"");
    return needsQuotes ? '"' + out + '"' : out;
}

struct Stats {
    double minVal  = 0.0;
    double maxVal  = 0.0;
    double meanVal = 0.0;
    int    cells   = 0;
    int    modCount = 0;
};

Stats statsForMap(const Project &p, const MapInfo &m)
{
    Stats st;
    const QByteArray &cur = p.currentData;
    const QByteArray &orig = p.originalData;
    if (cur.isEmpty() || m.length <= 0 || m.dataSize <= 0) return st;
    const auto bo = p.byteOrder;
    const auto *raw = reinterpret_cast<const uint8_t *>(cur.constData());
    const auto *origRaw = orig.size() == cur.size()
                              ? reinterpret_cast<const uint8_t *>(orig.constData())
                              : nullptr;
    const int n  = m.length;
    const int dz = m.dataSize;
    if (m.address + n > static_cast<uint32_t>(cur.size())) return st;

    double mn = std::numeric_limits<double>::max();
    double mx = std::numeric_limits<double>::lowest();
    double sum = 0.0;
    int cells = 0;
    int mods  = 0;

    for (int off = 0; off + dz <= n; off += dz) {
        uint32_t v = readRomValue(raw + m.address, n, off, dz, bo);
        double phys = static_cast<double>(v);
        if (m.dataSigned) {
            if (dz == 1) phys = static_cast<int8_t>(v);
            else if (dz == 2) phys = static_cast<int16_t>(v);
            else if (dz == 4) phys = static_cast<int32_t>(v);
        }
        if (m.hasScaling) phys = m.scaling.toPhysical(phys);
        mn = std::min(mn, phys);
        mx = std::max(mx, phys);
        sum += phys;
        ++cells;
        if (origRaw) {
            for (int b = 0; b < dz; ++b) {
                if (raw[m.address + off + b] != origRaw[m.address + off + b]) {
                    ++mods; break;
                }
            }
        }
    }
    if (cells > 0) {
        st.minVal  = mn;
        st.maxVal  = mx;
        st.meanVal = sum / cells;
        st.cells   = cells;
        st.modCount = mods;
    }
    return st;
}

QString byteOrderName(ByteOrder bo)
{
    return bo == ByteOrder::BigEndian ? QStringLiteral("BE") : QStringLiteral("LE");
}

}  // namespace

namespace MapListExporter {

bool toCsv(const Project &p, const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (err) *err = QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
        return false;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "Name,Address,Size (bytes),Type,Cols,Rows,DataSize,Signed,ByteOrder,"
        << "X-axis,Y-axis,MinValue,MaxValue,MeanValue,ModifiedCells,UserNotes\n";
    for (const MapInfo &m : p.maps) {
        const Stats st = statsForMap(p, m);
        out << csvEscape(m.name) << ','
            << QString("0x%1").arg(m.address, 8, 16, QChar('0')).toUpper() << ','
            << m.length << ','
            << csvEscape(m.type) << ','
            << m.dimensions.x << ','
            << m.dimensions.y << ','
            << m.dataSize << ','
            << (m.dataSigned ? "yes" : "no") << ','
            << byteOrderName(p.byteOrder) << ','
            << csvEscape(m.xAxis.inputName) << ','
            << csvEscape(m.yAxis.inputName) << ','
            << QString::number(st.minVal,  'g', 8) << ','
            << QString::number(st.maxVal,  'g', 8) << ','
            << QString::number(st.meanVal, 'g', 8) << ','
            << st.modCount << ','
            << csvEscape(m.userNotes)
            << '\n';
    }
    return true;
}

bool toJson(const Project &p, const QString &path, QString *err)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
        return false;
    }
    QJsonObject root;
    root.insert("project", p.displayName());
    root.insert("ecu",     p.ecuType);
    root.insert("brand",   p.brand);
    root.insert("model",   p.model);
    root.insert("byteOrder", byteOrderName(p.byteOrder));
    root.insert("baseAddress", static_cast<double>(p.baseAddress));
    root.insert("dataSize", p.currentData.size());

    QJsonArray maps;
    const QByteArray &cur = p.currentData;
    const auto bo = p.byteOrder;
    const auto *raw = reinterpret_cast<const uint8_t *>(cur.constData());

    for (const MapInfo &m : p.maps) {
        QJsonObject mo;
        mo.insert("name",        m.name);
        mo.insert("description", m.description);
        mo.insert("type",        m.type);
        mo.insert("address",     QString("0x%1")
                                       .arg(m.address, 8, 16, QChar('0')).toUpper());
        mo.insert("addressDec",  static_cast<double>(m.address));
        mo.insert("length",      m.length);
        mo.insert("cols",        m.dimensions.x);
        mo.insert("rows",        m.dimensions.y);
        mo.insert("dataSize",    m.dataSize);
        mo.insert("signed",      m.dataSigned);

        // X and Y axis names + units (best-effort)
        QJsonObject xaxis;
        xaxis.insert("name", m.xAxis.inputName);
        xaxis.insert("unit", m.xAxis.scaling.unit);
        mo.insert("xAxis", xaxis);
        QJsonObject yaxis;
        yaxis.insert("name", m.yAxis.inputName);
        yaxis.insert("unit", m.yAxis.scaling.unit);
        mo.insert("yAxis", yaxis);

        // Cell array — physical values if the map has scaling, else raw.
        QJsonArray cells;
        if (m.length > 0 && m.dataSize > 0
            && m.address + m.length <= static_cast<uint32_t>(cur.size())) {
            for (int off = 0; off + m.dataSize <= m.length; off += m.dataSize) {
                uint32_t v = readRomValue(raw + m.address, m.length, off,
                                          m.dataSize, bo);
                double phys = static_cast<double>(v);
                if (m.dataSigned) {
                    if (m.dataSize == 1) phys = static_cast<int8_t>(v);
                    else if (m.dataSize == 2) phys = static_cast<int16_t>(v);
                    else if (m.dataSize == 4) phys = static_cast<int32_t>(v);
                }
                if (m.hasScaling) phys = m.scaling.toPhysical(phys);
                cells.append(phys);
            }
        }
        mo.insert("cells", cells);

        const Stats st = statsForMap(p, m);
        mo.insert("min",  st.minVal);
        mo.insert("max",  st.maxVal);
        mo.insert("mean", st.meanVal);
        mo.insert("modifiedCells", st.modCount);

        if (!m.userNotes.isEmpty()) mo.insert("userNotes", m.userNotes);
        maps.append(mo);
    }
    root.insert("maps", maps);

    QJsonDocument doc(root);
    return f.write(doc.toJson(QJsonDocument::Indented)) > 0;
}

}  // namespace MapListExporter
