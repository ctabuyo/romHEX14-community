/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "XdfIo.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QStringDecoder>
#include <QHash>
#include <QMap>
#include <cmath>

namespace xdf {

// ── MATH equation <-> linear scaling ─────────────────────────────────────────

namespace {

// Tiny recursive-descent evaluator over the TunerPro MATH grammar restricted
// to expressions that are linear in X.  Every sub-expression is carried as
// the pair (a, b) meaning a*X + b; a product needs at least one constant
// factor and a quotient needs a constant divisor, anything else (X*X, 1/X,
// functions) is rejected so the caller leaves the value unscaled.
struct Lin { double a = 0.0, b = 0.0; };

class LinearEqParser {
public:
    explicit LinearEqParser(const QString &src) : m_s(src) {}
    bool parse(Lin *out) {
        m_pos = 0; m_ok = true;
        Lin v = expr();
        if (!m_ok || m_pos != m_s.size()) return false;
        *out = v;
        return true;
    }
private:
    QString m_s;
    int  m_pos = 0;
    bool m_ok  = true;

    QChar peek() const { return m_pos < m_s.size() ? m_s.at(m_pos) : QChar(); }
    bool eat(QChar c) { if (peek() == c) { ++m_pos; return true; } return false; }

    Lin expr() {
        Lin v = term();
        while (m_ok) {
            if (eat(QChar('+')))      { Lin r = term(); v.a += r.a; v.b += r.b; }
            else if (eat(QChar('-'))) { Lin r = term(); v.a -= r.a; v.b -= r.b; }
            else break;
        }
        return v;
    }
    Lin term() {
        Lin v = unary();
        while (m_ok) {
            if (eat(QChar('*'))) {
                Lin r = unary();
                if (v.a != 0.0 && r.a != 0.0) { m_ok = false; return {}; }
                if (v.a == 0.0) { v = { v.b * r.a, v.b * r.b }; }
                else            { v = { v.a * r.b, v.b * r.b }; }
            } else if (eat(QChar('/'))) {
                Lin r = unary();
                if (r.a != 0.0 || r.b == 0.0) { m_ok = false; return {}; }
                v = { v.a / r.b, v.b / r.b };
            } else break;
        }
        return v;
    }
    Lin unary() {
        if (eat(QChar('-'))) { Lin v = unary(); return { -v.a, -v.b }; }
        if (eat(QChar('+'))) return unary();
        return primary();
    }
    Lin primary() {
        if (eat(QChar('('))) {
            Lin v = expr();
            if (!eat(QChar(')'))) { m_ok = false; return {}; }
            return v;
        }
        if (peek() == QChar('X')) {
            ++m_pos;
            if (peek().isLetterOrNumber()) { m_ok = false; return {}; }   // e.g. "XA"
            return { 1.0, 0.0 };
        }
        // number: digits [. digits] [e[+-]digits]
        const int begin = m_pos;
        while (peek().isDigit() || peek() == QChar('.')) ++m_pos;
        if (peek() == QChar('e') || peek() == QChar('E')) {
            const int save = m_pos;
            ++m_pos;
            if (peek() == QChar('+') || peek() == QChar('-')) ++m_pos;
            if (!peek().isDigit()) m_pos = save;
            else while (peek().isDigit()) ++m_pos;
        }
        if (m_pos == begin) { m_ok = false; return {}; }
        bool ok = false;
        const double d = m_s.mid(begin, m_pos - begin).toDouble(&ok);
        if (!ok) { m_ok = false; return {}; }
        return { 0.0, d };
    }
};

} // namespace

bool parseLinearEquation(const QString &equationIn, double *a, double *b)
{
    QString eq = equationIn;
    eq.remove(QChar(' ')).remove(QChar('\t'));
    if (eq.isEmpty()) return false;
    // Uppercase the variable so "x" and "X" both work.
    eq.replace(QChar('x'), QChar('X'));

    Lin v;
    if (!LinearEqParser(eq).parse(&v)) return false;
    if (!std::isfinite(v.a) || !std::isfinite(v.b)) return false;
    *a = v.a; *b = v.b;
    return true;
}

QString buildLinearEquation(double a, double b)
{
    // Trim trailing zeros for readability without losing precision.
    auto fmt = [](double v) {
        QString s = QString::number(v, 'g', 12);
        return s;
    };
    if (a == 1.0 && b == 0.0) return QStringLiteral("X");
    QString s = QStringLiteral("X*") + fmt(a);
    if (b != 0.0)
        s += (b > 0 ? QStringLiteral("+") : QStringLiteral("-")) + fmt(std::abs(b));
    return s;
}

// ── Import ───────────────────────────────────────────────────────────────────

namespace {

struct Embedded {
    bool     hasAddress = false;
    uint32_t address    = 0;
    int      sizeBits   = 8;
    int      rows       = 0;   // 0 = attribute absent
    int      cols       = 0;
    bool     hasTypeFlags = false;
    uint32_t typeFlags  = 0;   // mmedtypeflags
};

// mmedtypeflags bits (TunerPro XDF 1.x):
//   0x01 signed, 0x02 LSB first (little-endian), 0x10000 IEEE float.
constexpr uint32_t kTypeSigned   = 0x01;
constexpr uint32_t kTypeLsbFirst = 0x02;
constexpr uint32_t kTypeFloat    = 0x10000;

long long parseNum(const QString &v)
{
    if (v.startsWith(QLatin1String("0x")) || v.startsWith(QLatin1String("0X")))
        return v.mid(2).toLongLong(nullptr, 16);
    return v.toLongLong(nullptr, 0);
}

Embedded readEmbedded(const QXmlStreamAttributes &at)
{
    Embedded e;
    auto num = [&](const QString &k, bool *ok = nullptr) -> long long {
        const QString v = at.value(k).toString();
        if (v.isEmpty()) { if (ok) *ok = false; return 0; }
        if (ok) *ok = true;
        return parseNum(v);
    };
    bool ok = false;
    const long long addr = num(QStringLiteral("mmedaddress"), &ok);
    if (ok) { e.hasAddress = true; e.address = uint32_t(addr); }
    if (at.hasAttribute(QStringLiteral("mmedelementsizebits")))
        e.sizeBits = int(num(QStringLiteral("mmedelementsizebits")));
    if (at.hasAttribute(QStringLiteral("mmedrowcount")))
        e.rows = int(num(QStringLiteral("mmedrowcount")));
    if (at.hasAttribute(QStringLiteral("mmedcolcount")))
        e.cols = int(num(QStringLiteral("mmedcolcount")));
    if (at.hasAttribute(QStringLiteral("mmedtypeflags"))) {
        e.hasTypeFlags = true;
        e.typeFlags = uint32_t(num(QStringLiteral("mmedtypeflags")));
    }
    return e;
}

int bytesFromBits(int bits) { return bits <= 8 ? 1 : bits <= 16 ? 2 : 4; }

// OLS-style data-type code used by MapInfo::cellDataType / AxisInfo::ptsDataType
// (1 = u8, 2/3 = u16 BE/LE, 4/5 = u32 BE/LE, 6/7 = float BE/LE).  Setting it
// makes the editor honour the definition's byte order instead of the
// project-wide default, which matters for little-endian ECUs (MS43, EDC15).
uint32_t dataTypeCode(int bytes, bool bigEndian, bool isFloat)
{
    if (bytes == 1) return 1;
    if (isFloat && bytes == 4) return bigEndian ? 6 : 7;
    if (bytes == 2) return bigEndian ? 2 : 3;
    return bigEndian ? 4 : 5;
}

// XDF files exported by ECU tools are frequently Latin-1/Windows-1252 with no
// <?xml encoding> declaration (e.g. a "°C" unit byte), which a UTF-8 parser
// rejects. Decode to a QString ourselves — honor a declared encoding, else use
// UTF-8 when the bytes are valid UTF-8, otherwise fall back to Latin-1.
QString decodeXml(const QByteArray &xml)
{
    // Look for an explicit encoding in the XML declaration (ASCII-safe scan).
    const QByteArray head = xml.left(200).toLower();
    if (head.contains("encoding=")) {
        if (head.contains("iso-8859-1") || head.contains("latin1")
            || head.contains("windows-1252") || head.contains("cp1252"))
            return QString::fromLatin1(xml);
        if (head.contains("utf-8"))
            return QString::fromUtf8(xml);
    }
    QStringDecoder dec(QStringConverter::Utf8);
    QString s = dec.decode(xml);
    if (dec.hasError())
        return QString::fromLatin1(xml);   // not valid UTF-8 -> treat as Latin-1
    return s;
}

} // namespace

ImportResult importFromXml(const QByteArray &xml)
{
    ImportResult res;
    const QString text = decodeXml(xml);
    QXmlStreamReader r(text);

    QMap<int, QString> categories;   // index -> name
    bool defaultsSigned = false;
    bool defaultsBigEndian = true;   // lsbfirst=0 -> big-endian
    bool defaultsFloat = false;
    bool baseSubtract = false;       // BASEOFFSET subtract="1"

    // BASEOFFSET: TunerPro adds the offset to every mmedaddress to reach the
    // file position (subtract="0"), or subtracts it (subtract="1").  Split
    // 512K/64K definitions such as the MS43X ones rely on the additive form.
    auto toFileOffset = [&](uint32_t addr) -> uint32_t {
        if (baseSubtract)
            return addr >= res.baseOffset ? addr - res.baseOffset : addr;
        return addr + res.baseOffset;
    };
    auto sizeBitsToBytes = [](int bits) { return bytesFromBits(bits); };

    // Two passes are awkward with a streaming reader, so parse structurally:
    // dispatch on start elements, accumulating the current object.
    while (!r.atEnd()) {
        const auto tok = r.readNext();
        if (tok != QXmlStreamReader::StartElement) continue;
        const QStringView name = r.name();

        if (name == QLatin1String("BASEOFFSET")) {
            const auto at = r.attributes();
            res.baseOffset = uint32_t(parseNum(at.value(QStringLiteral("offset")).toString()));
            baseSubtract = at.value(QStringLiteral("subtract")).toString() == QLatin1String("1");
        } else if (name == QLatin1String("DEFAULTS")) {
            const auto at = r.attributes();
            defaultsSigned    = at.value(QStringLiteral("signed")).toString() == QLatin1String("1");
            defaultsBigEndian = at.value(QStringLiteral("lsbfirst")).toString() != QLatin1String("1");
            defaultsFloat     = at.value(QStringLiteral("float")).toString() == QLatin1String("1");
        } else if (name == QLatin1String("REGION")) {
            const auto at = r.attributes();
            res.romSize = uint32_t(parseNum(at.value(QStringLiteral("size")).toString()));
        } else if (name == QLatin1String("CATEGORY")) {
            const auto at = r.attributes();
            const int idx = int(parseNum(at.value(QStringLiteral("index")).toString()));
            categories.insert(idx, at.value(QStringLiteral("name")).toString());
        } else if (name == QLatin1String("XDFCONSTANT") || name == QLatin1String("XDFTABLE")) {
            const bool isTable = name == QLatin1String("XDFTABLE");
            const QString endTag = isTable ? QStringLiteral("XDFTABLE")
                                           : QStringLiteral("XDFCONSTANT");
            MapInfo m;
            m.linkConfidence = 100;
            m.columnMajor    = false;
            int categoryIdx  = -1;

            Embedded zData;                // constant body or table z-axis
            bool haveZ = false;
            QString zUnits;
            double  zEqA = 1.0, zEqB = 0.0; bool zEqLinear = false;
            int xIndexCount = 0, yIndexCount = 0;

            QString curAxis;               // "x" / "y" / "z" while inside XDFAXIS
            Embedded axisData;
            bool axisHasEmbedded = false;
            int  axisIndexCount = 0;
            QString pendingUnits;
            double  eqA = 1.0, eqB = 0.0; bool eqLinear = false;
            QMap<int, double> axisLabels;  // LABEL index -> value (fixed axes)

            auto resolveType = [&](const Embedded &e, bool *isSigned, bool *bigEndian, bool *isFloat) {
                if (e.hasTypeFlags) {
                    *isSigned  = (e.typeFlags & kTypeSigned) != 0;
                    *bigEndian = (e.typeFlags & kTypeLsbFirst) == 0;
                    *isFloat   = (e.typeFlags & kTypeFloat) != 0;
                } else {
                    *isSigned = defaultsSigned; *bigEndian = defaultsBigEndian; *isFloat = defaultsFloat;
                }
            };

            auto finishAxis = [&]() {
                if (curAxis.isEmpty()) return;
                if (curAxis == QLatin1String("z")) {
                    zData = axisData; haveZ = axisHasEmbedded;
                    zUnits = pendingUnits;
                    zEqLinear = eqLinear; zEqA = eqA; zEqB = eqB;
                } else {
                    const bool isX = curAxis == QLatin1String("x");
                    AxisInfo &ax = isX ? m.xAxis : m.yAxis;
                    (isX ? xIndexCount : yIndexCount) = axisIndexCount;
                    if (axisHasEmbedded && axisData.hasAddress) {
                        bool sg = false, be = true, fl = false;
                        resolveType(axisData, &sg, &be, &fl);
                        ax.ptsAddress    = toFileOffset(axisData.address);
                        ax.hasPtsAddress = true;
                        ax.ptsDataSize   = sizeBitsToBytes(axisData.sizeBits);
                        ax.ptsSigned     = sg;
                        ax.ptsBigEndian  = be;
                        ax.ptsDataType   = dataTypeCode(ax.ptsDataSize, be, fl);
                        int n = qMax(axisData.cols, axisData.rows);
                        if (n <= 1 && axisIndexCount > 0) n = axisIndexCount;
                        ax.ptsCount = qMax(1, n);
                    } else if (!axisLabels.isEmpty()) {
                        // Fixed axis given as LABEL entries (no ROM bytes).
                        const int n = axisIndexCount > 0 ? axisIndexCount
                                                         : axisLabels.lastKey() + 1;
                        ax.fixedValues.resize(n);
                        for (int i = 0; i < n; i++)
                            ax.fixedValues[i] = axisLabels.value(i, double(i));
                    }
                    if (eqLinear) {
                        ax.hasScaling   = true;
                        ax.scaling.type = CompuMethod::Type::Linear;
                        ax.scaling.linA = eqA; ax.scaling.linB = eqB;
                        ax.scaling.unit = pendingUnits;
                    }
                    if (!pendingUnits.isEmpty() && pendingUnits != QLatin1String("-"))
                        ax.inputName = pendingUnits;
                }
                curAxis.clear(); axisHasEmbedded = false; pendingUnits.clear();
                axisIndexCount = 0; axisLabels.clear();
                eqLinear = false; eqA = 1.0; eqB = 0.0;
            };

            // Walk this object's subtree.
            while (!r.atEnd()) {
                const auto t2 = r.readNext();
                if (t2 == QXmlStreamReader::EndElement) {
                    if (r.name() == QLatin1String("XDFAXIS")) finishAxis();
                    else if (r.name() == endTag) break;
                    continue;
                }
                if (t2 != QXmlStreamReader::StartElement) continue;
                const QStringView n2 = r.name();
                if (n2 == QLatin1String("title")) {
                    m.name = r.readElementText().trimmed();
                    m.description = m.name;
                } else if (n2 == QLatin1String("description")) {
                    const QString d = r.readElementText().trimmed();
                    if (!d.isEmpty()) m.description = d;
                } else if (n2 == QLatin1String("CATEGORYMEM")) {
                    // category attr is 1-based (0 = none); header index is 0-based.
                    const int c = int(parseNum(r.attributes().value(QStringLiteral("category")).toString()));
                    if (c > 0 && categoryIdx < 0) categoryIdx = c - 1;
                } else if (n2 == QLatin1String("XDFAXIS")) {
                    curAxis = r.attributes().value(QStringLiteral("id")).toString().toLower();
                } else if (n2 == QLatin1String("EMBEDDEDDATA")) {
                    const Embedded e = readEmbedded(r.attributes());
                    if (isTable && !curAxis.isEmpty()) { axisData = e; axisHasEmbedded = true; }
                    else { zData = e; haveZ = true; }        // constant body
                } else if (n2 == QLatin1String("indexcount")) {
                    axisIndexCount = r.readElementText().trimmed().toInt();
                } else if (n2 == QLatin1String("LABEL")) {
                    const auto at = r.attributes();
                    const int idx = int(parseNum(at.value(QStringLiteral("index")).toString()));
                    bool ok = false;
                    const double v = at.value(QStringLiteral("value")).toString().toDouble(&ok);
                    if (ok && idx >= 0 && idx < 4096) axisLabels.insert(idx, v);
                } else if (n2 == QLatin1String("units")) {
                    const QString u = r.readElementText().trimmed();
                    if (!curAxis.isEmpty() || !isTable) pendingUnits = u;
                } else if (n2 == QLatin1String("MATH")) {
                    const QString eq = r.attributes().value(QStringLiteral("equation")).toString();
                    double a, b;
                    if (parseLinearEquation(eq, &a, &b)) { eqLinear = true; eqA = a; eqB = b; }
                    else if (!isTable || !curAxis.isEmpty()) {
                        res.warnings.append(QStringLiteral("'%1': non-linear equation '%2' ignored")
                                                .arg(m.name, eq));
                    }
                }
            }
            if (!isTable) {                // constant: units/MATH sit at object level
                zUnits = pendingUnits;
                zEqLinear = eqLinear; zEqA = eqA; zEqB = eqB;
            }

            if (!haveZ || !zData.hasAddress) {
                res.warnings.append(QStringLiteral("Skipped '%1': no address").arg(m.name));
                continue;
            }

            bool zSigned = false, zBigEndian = true, zFloat = false;
            resolveType(zData, &zSigned, &zBigEndian, &zFloat);

            m.rawAddress    = zData.address;
            m.address       = toFileOffset(zData.address);
            m.dataSize      = sizeBitsToBytes(zData.sizeBits);
            m.dataSigned    = zSigned;
            m.cellBigEndian = zBigEndian;
            m.cellDataType  = dataTypeCode(m.dataSize, zBigEndian, zFloat);
            int cols = zData.cols, rows = zData.rows;
            if (cols <= 0) cols = (isTable && xIndexCount > 0) ? xIndexCount : 1;
            if (rows <= 0) rows = (isTable && yIndexCount > 0) ? yIndexCount : 1;
            m.dimensions = { cols, rows };
            m.length     = cols * rows * m.dataSize;
            if (zEqLinear) {
                m.hasScaling   = true;
                m.scaling.type = CompuMethod::Type::Linear;
                m.scaling.linA = zEqA; m.scaling.linB = zEqB;
            }
            if (!zUnits.isEmpty() && zUnits != QLatin1String("-"))
                m.scaling.unit = zUnits;
            if (!isTable) {
                m.type = QStringLiteral("VALUE");
            } else {
                m.type = (cols > 1 && rows > 1) ? QStringLiteral("MAP")
                       : (cols > 1 || rows > 1) ? QStringLiteral("CURVE")
                                                : QStringLiteral("VALUE");
            }
            if (categoryIdx >= 0 && categories.contains(categoryIdx))
                m.folderPath = categories.value(categoryIdx);

            res.maps.append(m);
        }
    }

    if (r.hasError()) {
        res.error = QStringLiteral("XDF parse error: %1 (line %2)")
                        .arg(r.errorString()).arg(r.lineNumber());
        res.maps.clear();
    }
    return res;
}

// ── Export ───────────────────────────────────────────────────────────────────

QByteArray exportToXml(const QVector<MapInfo> &maps, const ExportOptions &opt)
{
    // Collect folder paths -> category indices (stable, insertion order).
    QVector<QString> catNames;
    QHash<QString, int> catIndex;
    for (const auto &m : maps) {
        if (m.folderPath.isEmpty()) continue;
        if (!catIndex.contains(m.folderPath)) {
            catIndex.insert(m.folderPath, catNames.size());
            catNames.append(m.folderPath);
        }
    }

    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("XDFFORMAT"));
    w.writeAttribute(QStringLiteral("version"), QStringLiteral("1.60"));

    w.writeStartElement(QStringLiteral("XDFHEADER"));
    w.writeTextElement(QStringLiteral("description"), opt.description);
    w.writeStartElement(QStringLiteral("BASEOFFSET"));
    w.writeAttribute(QStringLiteral("offset"), QString::number(opt.baseOffset));
    w.writeAttribute(QStringLiteral("subtract"), QStringLiteral("0"));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("DEFAULTS"));
    w.writeAttribute(QStringLiteral("datasizeinbits"), QStringLiteral("8"));
    w.writeAttribute(QStringLiteral("sigdigits"), QStringLiteral("2"));
    w.writeAttribute(QStringLiteral("outputtype"), QStringLiteral("1"));
    w.writeAttribute(QStringLiteral("signed"), QStringLiteral("0"));
    w.writeAttribute(QStringLiteral("lsbfirst"), opt.bigEndian ? QStringLiteral("0")
                                                               : QStringLiteral("1"));
    w.writeAttribute(QStringLiteral("float"), QStringLiteral("0"));
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("REGION"));
    w.writeAttribute(QStringLiteral("type"), QStringLiteral("0xFFFFFFFF"));
    w.writeAttribute(QStringLiteral("startaddress"), QStringLiteral("0x0"));
    w.writeAttribute(QStringLiteral("size"),
        QStringLiteral("0x%1").arg(opt.romSize ? opt.romSize : 0x400000u, 0, 16));
    w.writeAttribute(QStringLiteral("regionflags"), QStringLiteral("0x0"));
    w.writeAttribute(QStringLiteral("name"), QStringLiteral("Binary File"));
    w.writeAttribute(QStringLiteral("desc"), QStringLiteral("Binary edited by this XDF"));
    w.writeEndElement();
    for (int i = 0; i < catNames.size(); i++) {
        w.writeStartElement(QStringLiteral("CATEGORY"));
        w.writeAttribute(QStringLiteral("index"), QStringLiteral("0x%1").arg(i, 0, 16));
        w.writeAttribute(QStringLiteral("name"), catNames[i]);
        w.writeEndElement();
    }
    w.writeEndElement(); // XDFHEADER

    auto writeMath = [&](const CompuMethod &cm, bool hasScaling) {
        if (!hasScaling || cm.type != CompuMethod::Type::Linear) return;
        w.writeStartElement(QStringLiteral("MATH"));
        w.writeAttribute(QStringLiteral("equation"), buildLinearEquation(cm.linA, cm.linB));
        w.writeStartElement(QStringLiteral("VAR"));
        w.writeAttribute(QStringLiteral("id"), QStringLiteral("X"));
        w.writeEndElement();
        w.writeEndElement();
    };
    auto writeEmbedded = [&](uint32_t addr, bool hasAddr, int dataSize, int rows, int cols,
                             bool isSigned, bool bigEndian) {
        w.writeStartElement(QStringLiteral("EMBEDDEDDATA"));
        const uint32_t flags = (isSigned ? kTypeSigned : 0u) | (bigEndian ? 0u : kTypeLsbFirst);
        w.writeAttribute(QStringLiteral("mmedtypeflags"), QStringLiteral("0x%1").arg(flags, 2, 16, QChar('0')));
        if (hasAddr)
            w.writeAttribute(QStringLiteral("mmedaddress"),
                             QStringLiteral("0x%1").arg(addr, 0, 16));
        w.writeAttribute(QStringLiteral("mmedelementsizebits"), QString::number(dataSize * 8));
        if (rows > 1) w.writeAttribute(QStringLiteral("mmedrowcount"), QString::number(rows));
        w.writeAttribute(QStringLiteral("mmedcolcount"), QString::number(cols));
        w.writeEndElement();
    };

    int uid = 1;
    for (const auto &m : maps) {
        const int cols = qMax(1, m.dimensions.x);
        const int rows = qMax(1, m.dimensions.y);
        const bool scalar = (cols == 1 && rows == 1);
        // BASEOFFSET is written with subtract="0", so TunerPro adds it back to
        // every mmedaddress: emit addresses relative to it, never rawAddress
        // (which may be an ECU-space address from A2L/OLS imports).
        auto rel = [&](uint32_t fileOff) {
            return fileOff >= opt.baseOffset ? fileOff - opt.baseOffset : fileOff;
        };
        const uint32_t addr = rel(m.address);

        if (scalar) {
            w.writeStartElement(QStringLiteral("XDFCONSTANT"));
            w.writeAttribute(QStringLiteral("uniqueid"), QStringLiteral("0x%1").arg(uid++, 0, 16));
            w.writeTextElement(QStringLiteral("title"), m.name);
            if (!m.folderPath.isEmpty()) {
                w.writeStartElement(QStringLiteral("CATEGORYMEM"));
                w.writeAttribute(QStringLiteral("index"), QStringLiteral("0"));
                w.writeAttribute(QStringLiteral("category"),
                                 QString::number(catIndex.value(m.folderPath) + 1));
                w.writeEndElement();
            }
            writeEmbedded(addr, true, m.dataSize, 1, 1, m.dataSigned,
                          m.cellDataType ? m.cellBigEndian : opt.bigEndian);
            if (!m.scaling.unit.isEmpty())
                w.writeTextElement(QStringLiteral("units"), m.scaling.unit);
            writeMath(m.scaling, m.hasScaling);
            w.writeEndElement();
            continue;
        }

        w.writeStartElement(QStringLiteral("XDFTABLE"));
        w.writeAttribute(QStringLiteral("uniqueid"), QStringLiteral("0x%1").arg(uid++, 0, 16));
        w.writeTextElement(QStringLiteral("title"), m.name);
        if (!m.folderPath.isEmpty()) {
            w.writeStartElement(QStringLiteral("CATEGORYMEM"));
            w.writeAttribute(QStringLiteral("index"), QStringLiteral("0"));
            w.writeAttribute(QStringLiteral("category"),
                             QString::number(catIndex.value(m.folderPath) + 1));
            w.writeEndElement();
        }
        auto writeTableAxis = [&](const QString &id, const AxisInfo &ax, int count) {
            w.writeStartElement(QStringLiteral("XDFAXIS"));
            w.writeAttribute(QStringLiteral("id"), id);
            writeEmbedded(rel(ax.ptsAddress), ax.hasPtsAddress,
                          ax.ptsDataSize > 0 ? ax.ptsDataSize : 2, 1, count,
                          ax.ptsSigned, ax.ptsDataType ? ax.ptsBigEndian : opt.bigEndian);
            w.writeTextElement(QStringLiteral("indexcount"), QString::number(count));
            for (int i = 0; i < ax.fixedValues.size() && i < count; i++) {
                w.writeStartElement(QStringLiteral("LABEL"));
                w.writeAttribute(QStringLiteral("index"), QString::number(i));
                w.writeAttribute(QStringLiteral("value"), QString::number(ax.fixedValues[i], 'g', 10));
                w.writeEndElement();
            }
            if (ax.hasScaling && !ax.scaling.unit.isEmpty())
                w.writeTextElement(QStringLiteral("units"), ax.scaling.unit);
            writeMath(ax.scaling, ax.hasScaling);
            w.writeEndElement();
        };
        writeTableAxis(QStringLiteral("x"), m.xAxis, cols);
        writeTableAxis(QStringLiteral("y"), m.yAxis, rows);
        // z axis = the actual table data
        w.writeStartElement(QStringLiteral("XDFAXIS"));
        w.writeAttribute(QStringLiteral("id"), QStringLiteral("z"));
        writeEmbedded(addr, true, m.dataSize, rows, cols, m.dataSigned,
                      m.cellDataType ? m.cellBigEndian : opt.bigEndian);
        if (!m.scaling.unit.isEmpty())
            w.writeTextElement(QStringLiteral("units"), m.scaling.unit);
        writeMath(m.scaling, m.hasScaling);
        w.writeEndElement();
        w.writeEndElement(); // XDFTABLE
    }

    w.writeEndElement(); // XDFFORMAT
    w.writeEndDocument();
    return out;
}

} // namespace xdf
