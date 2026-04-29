/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "a2lparser.h"
#include <QRegularExpression>

A2LParser::A2LParser(QObject *parent)
    : QObject(parent)
{
}

// Extract blocks between /begin TAG ... /end TAG without regex (safe for huge files)
static QVector<QStringView> extractBlocks(const QString &text, const QString &tag)
{
    QVector<QStringView> blocks;
    const QString beginTag = "/begin " + tag;
    const QString endTag   = "/end "   + tag;
    int pos = 0;
    while (pos < text.size()) {
        int start = text.indexOf(beginTag, pos, Qt::CaseInsensitive);
        if (start < 0) break;
        int bodyStart = start + beginTag.size();
        int end = text.indexOf(endTag, bodyStart, Qt::CaseInsensitive);
        if (end < 0) break;
        blocks.append(QStringView(text).mid(bodyStart, end - bodyStart));
        pos = end + endTag.size();
    }
    return blocks;
}

void A2LParser::parse(const QString &text, int romSize, uint32_t knownBase)
{
    m_characteristics.clear();
    m_groups.clear();
    m_compuMethods.clear();
    m_axisPts.clear();
    m_baseAddress = 0;
    m_byteOrder = ByteOrder::BigEndian;

    emit progress("Parsing MOD_COMMON...", 5);

    // Parse MOD_COMMON for byte order (small block, safe with indexOf)
    auto modBlocks = extractBlocks(text, "MOD_COMMON");
    if (!modBlocks.isEmpty()) {
        if (modBlocks[0].contains(QLatin1String("MSB_LAST")))
            m_byteOrder = ByteOrder::LittleEndian;
    }

    emit progress("Parsing RECORD_LAYOUTs...", 8);
    parseRecordLayouts(text);

    emit progress("Parsing characteristics...", 10);
    parseCharacteristics(text);

    emit progress("Parsing axis points...", 75);
    parseAxisPts(text);

    emit progress("Parsing COMPU_METHODs...", 85);
    parseCompuMethods(text);

    emit progress("Parsing GROUPs...", 88);
    parseGroups(text);
    generateAutoGroups();

    emit progress("Detecting base address...", 90);
    if (knownBase != 0) {
        // Authoritative base from HEX/SREC parser — trust it directly.
        m_baseAddress = knownBase;
    } else {
        // Use actual ROM size if known, otherwise allow up to 256 MB.
        detectBaseAddress(romSize > 0 ? romSize : 0x10000000);
    }

    emit progress("Building map list...", 95);
}

void A2LParser::parseCharacteristics(const QString &text)
{
    auto blocks = extractBlocks(text, "CHARACTERISTIC");
    int count = 0;

    for (const auto &blockView : blocks) {
        QString block = blockView.trimmed().toString();

        // Header: name "description" type address recordLayout ...
        QRegularExpression nameRe(R"re(^(\S+)\s+"([^"]*)"\s+(\S+)\s+(0x[0-9A-Fa-f]+|\d+))re");
        auto nameMatch = nameRe.match(block);
        if (!nameMatch.hasMatch()) continue;

        A2LCharacteristic ch;
        ch.name = nameMatch.captured(1);
        ch.description = nameMatch.captured(2);
        ch.type = nameMatch.captured(3);
        ch.rawAddress = nameMatch.captured(4).toUInt(nullptr, 0);

        // AXIS_DESCR blocks — extract dimension, compu method, axis pts ref, fixed values
        auto axisBlocks = extractBlocks(block, "AXIS_DESCR");
        QVector<A2LAxisDesc> axisDescs;
        for (const auto &axisBlockView : axisBlocks) {
            QString axisBlock = axisBlockView.trimmed().toString();

            A2LAxisDesc ad;
            // ASAP2 AXIS_DESCR header: axisType  input_quantity  conversion  max_axis_points
            QRegularExpression axisInfoRe(R"(^(\S+)\s+(\S+)\s+(\S+)\s+(\d+))");
            auto ai = axisInfoRe.match(axisBlock);
            if (ai.hasMatch()) {
                ad.axisType   = ai.captured(1).toUpper();
                ad.inputName  = ai.captured(2);   // input_quantity (e.g. nmot_uw)
                ad.maxCount   = ai.captured(4).toInt();
                const QString cmRef = ai.captured(3);  // conversion (e.g. nmot_uw_q0p25)
                if (cmRef != "NO_COMPU_METHOD" && !cmRef.startsWith('/'))
                    ad.compuMethodRef = cmRef;
            }

            // AXIS_PTS_REF — reference to shared AXIS_PTS object in ROM
            QRegularExpression ptsRefRe(R"(AXIS_PTS_REF\s+(\S+))");
            auto ptsRef = ptsRefRe.match(axisBlock);
            if (ptsRef.hasMatch())
                ad.axisPtsRef = ptsRef.captured(1);

            // FIX_AXIS_PAR_DIST offset distance numPoints
            QRegularExpression fadRe(R"(FIX_AXIS_PAR_DIST\s+(\S+)\s+(\S+)\s+(\d+))");
            auto fad = fadRe.match(axisBlock);
            if (fad.hasMatch()) {
                double off  = fad.captured(1).toDouble();
                double dist = fad.captured(2).toDouble();
                int    n    = fad.captured(3).toInt();
                ad.fixedValues.reserve(n);
                for (int i = 0; i < n; i++)
                    ad.fixedValues.append(off + dist * i);
            }

            // FIX_AXIS_PAR offset shift numPoints (only if not already filled)
            if (ad.fixedValues.isEmpty()) {
                QRegularExpression fapRe(R"(FIX_AXIS_PAR\s+(\S+)\s+(\S+)\s+(\d+))");
                auto fap = fapRe.match(axisBlock);
                if (fap.hasMatch()) {
                    double off   = fap.captured(1).toDouble();
                    double shift = fap.captured(2).toDouble();
                    int    n     = fap.captured(3).toInt();
                    ad.fixedValues.reserve(n);
                    for (int i = 0; i < n; i++)
                        ad.fixedValues.append(off + shift * i);
                }
            }

            axisDescs.append(ad);
        }
        if (axisDescs.size() >= 1) {
            ch.dimensions.x = axisDescs[0].maxCount > 0 ? axisDescs[0].maxCount : ch.dimensions.x;
            ch.xAxisDesc = axisDescs[0];
        }
        if (axisDescs.size() >= 2) {
            ch.dimensions.y = axisDescs[1].maxCount > 0 ? axisDescs[1].maxCount : ch.dimensions.y;
            ch.yAxisDesc = axisDescs[1];
        }

        // FIX_AXIS_PAR fallback
        QRegularExpression fixRe(R"(FIX_AXIS_PAR\s+\S+\s+\S+\s+(\d+))");
        auto fixIt = fixRe.globalMatch(block);
        int fixIdx = 0;
        while (fixIt.hasNext()) {
            auto fm = fixIt.next();
            int cnt = fm.captured(1).toInt();
            if (fixIdx == 0 && cnt > 0) ch.dimensions.x = cnt;
            if (fixIdx == 1 && cnt > 0) ch.dimensions.y = cnt;
            fixIdx++;
        }

        // MATRIX_DIM
        QRegularExpression matRe(R"(MATRIX_DIM\s+(\d+)(?:\s+(\d+))?)");
        auto matMatch = matRe.match(block);
        if (matMatch.hasMatch()) {
            ch.dimensions.x = matMatch.captured(1).toInt();
            if (!matMatch.captured(2).isEmpty())
                ch.dimensions.y = matMatch.captured(2).toInt();
        }

        // NUMBER keyword
        QRegularExpression numRe(R"(NUMBER\s+(\d+))");
        auto numMatch = numRe.match(block);
        if (numMatch.hasMatch()) {
            ch.dimensions.x = numMatch.captured(1).toInt();
        }

        // Record layout → data size and layout ref (5th token on first line)
        QRegularExpression layoutRe(R"(^(\S+)\s+"[^"]*"\s+(\S+)\s+\S+\s+(\S+))");
        auto layoutMatch = layoutRe.match(block);
        if (layoutMatch.hasMatch()) {
            ch.recordLayoutRef = layoutMatch.captured(3);
            // Use actual FNC_VALUES datatype from RECORD_LAYOUT when available
            if (m_recordLayoutDataSize.contains(ch.recordLayoutRef)) {
                ch.dataSize   = m_recordLayoutDataSize[ch.recordLayoutRef];
                ch.dataSigned = m_recordLayoutSigned.value(ch.recordLayoutRef, false);
            } else {
                // Fallback: heuristic from layout name
                QString layout = ch.recordLayoutRef.toUpper();
                if (layout.contains("BYTE") || layout.contains("8BIT"))
                    ch.dataSize = 1;
                else if (layout.contains("LONG") || layout.contains("32BIT") || layout.contains("FLOAT"))
                    ch.dataSize = 4;
                else
                    ch.dataSize = 2;
            }
        }
        if (ch.dataSize == 0) ch.dataSize = 2;

        // ASAP2 CHARACTERISTIC header:
        // name "desc" type address recordLayout max_diff conversion lower_limit upper_limit
        // Capture 7th token (conversion = compuMethodRef), skipping max_diff at position 6
        QRegularExpression cmRefRe(R"(^(?:\S+)\s+"[^"]*"\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+))");
        auto cmRefMatch = cmRefRe.match(block);
        if (cmRefMatch.hasMatch()) {
            QString ref = cmRefMatch.captured(1);
            if (ref != "NO_COMPU_METHOD" && !ref.startsWith('/'))
                ch.compuMethodRef = ref;
        }

        ch.dataLength = estimateLength(ch);
        m_characteristics.append(ch);
        count++;

        if (count % 200 == 0) {
            int pct = 10 + (int)((double)count / blocks.size() * 60);
            emit progress(QString("Parsing characteristics... (%1)").arg(count), qMin(pct, 70));
        }
    }
}

void A2LParser::parseAxisPts(const QString &text)
{
    auto blocks = extractBlocks(text, "AXIS_PTS");
    QRegularExpression hdrRe(R"re(^(\S+)\s+"[^"]*"\s+(0x[0-9A-Fa-f]+|\d+)\s+(\S+)\s+(\S+)\s+\S+\s+(\S+)\s+(\d+))re");

    for (const auto &blockView : blocks) {
        QString block = blockView.trimmed().toString();

        auto hm = hdrRe.match(block);
        if (!hm.hasMatch()) continue;

        A2LAxisPts ap;
        ap.name           = hm.captured(1);
        ap.rawAddress     = hm.captured(2).toUInt(nullptr, 0);
        // captured(3) = inputQuantity, captured(4) = deposit/recordLayout
        ap.maxCount       = hm.captured(6).toInt();
        const QString cmr = hm.captured(5);
        if (cmr != "NO_COMPU_METHOD" && !cmr.startsWith('/'))
            ap.compuMethodRef = cmr;

        // Store record layout reference (deposit = 4th capture)
        ap.recordLayoutRef = hm.captured(4);
        const QString layoutName = ap.recordLayoutRef;
        if (m_recordLayoutDataSize.contains(layoutName)) {
            ap.dataSize = m_recordLayoutDataSize[layoutName];
        } else {
            // Fallback: heuristic from layout name
            const QString layout = layoutName.toUpper();
            if (layout.contains("BYTE") || layout.contains("8BIT"))
                ap.dataSize = 1;
            else if (layout.contains("LONG") || layout.contains("32BIT") || layout.contains("FLOAT"))
                ap.dataSize = 4;
            else
                ap.dataSize = 2;
        }

        m_axisPts[ap.name] = ap;
    }
}

void A2LParser::parseCompuMethods(const QString &text)
{
    auto blocks = extractBlocks(text, "COMPU_METHOD");
    for (const auto &blockView : blocks) {
        QString block = blockView.trimmed().toString();

        // Header: name "long_ident" conversion_type "format" "unit"
        QRegularExpression headerRe(R"re(^(\S+)\s+"[^"]*"\s+(\S+)\s+"([^"]*)"\s+"([^"]*)")re");
        auto hm = headerRe.match(block);
        if (!hm.hasMatch()) continue;

        CompuMethod cm;
        const QString name     = hm.captured(1);
        const QString convType = hm.captured(2).toUpper();
        cm.format = hm.captured(3);
        cm.unit   = hm.captured(4);

        if (convType == "LINEAR") {
            cm.type = CompuMethod::Type::Linear;
            QRegularExpression linRe(R"(COEFFS_LINEAR\s+(\S+)\s+(\S+))");
            auto lm = linRe.match(block);
            if (lm.hasMatch()) {
                cm.linA = lm.captured(1).toDouble();
                cm.linB = lm.captured(2).toDouble();
            }
        } else if (convType == "RAT_FUNC") {
            cm.type = CompuMethod::Type::RationalFunction;
            // COEFFS a b c d e f  →  physical = (a*x²+b*x+c)/(d*x²+e*x+f)
            QRegularExpression rfRe(R"(COEFFS\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+))");
            auto rm = rfRe.match(block);
            if (rm.hasMatch()) {
                cm.rfA = rm.captured(1).toDouble();
                cm.rfB = rm.captured(2).toDouble();
                cm.rfC = rm.captured(3).toDouble();
                cm.rfD = rm.captured(4).toDouble();
                cm.rfE = rm.captured(5).toDouble();
                cm.rfF = rm.captured(6).toDouble();
            }
        } else {
            cm.type = CompuMethod::Type::Identical;
        }

        m_compuMethods[name] = cm;
    }
}

void A2LParser::parseGroups(const QString &text)
{
    auto blocks = extractBlocks(text, "GROUP");

    for (const auto &blockView : blocks) {
        QString block = blockView.trimmed().toString();

        // First token = name, then optional "description"
        QRegularExpression headerRe(R"re(^(\S+)(?:\s+"([^"]*)")?)re");
        auto hm = headerRe.match(block);
        if (!hm.hasMatch()) continue;

        A2LGroup group;
        group.name        = hm.captured(1);
        group.description = hm.captured(2);

        // REF_CHARACTERISTIC block → list of characteristic names
        auto refBlocks = extractBlocks(block, "REF_CHARACTERISTIC");
        if (!refBlocks.isEmpty()) {
            const QString refs = refBlocks[0].toString();
            for (const QString &tok : refs.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts))
                group.characteristics.append(tok);
        }

        // SUB_GROUP block → list of sub-group names
        auto subBlocks = extractBlocks(block, "SUB_GROUP");
        if (!subBlocks.isEmpty()) {
            const QString subs = subBlocks[0].toString();
            for (const QString &tok : subs.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts))
                group.subGroups.append(tok);
        }

        m_groups.append(group);
    }
}

void A2LParser::generateAutoGroups()
{
    if (!m_groups.isEmpty()) return;

    QMap<QString, QStringList> prefixMap;
    for (const auto &ch : m_characteristics) {
        int us = ch.name.indexOf('_');
        QString prefix = (us > 0) ? ch.name.left(us) : ch.type;
        prefixMap[prefix].append(ch.name);
    }

    for (auto it = prefixMap.begin(); it != prefixMap.end(); ++it) {
        A2LGroup g;
        g.name = it.key();
        g.characteristics = it.value();
        m_groups.append(g);
    }
}

// Convert ASAP2 datatype keyword to byte size
static int datatypeToSize(const QString &dt)
{
    const QString u = dt.toUpper();
    if (u == "UBYTE"   || u == "SBYTE"   || u == "A_UINT8"  || u == "A_INT8")   return 1;
    if (u == "UWORD"   || u == "SWORD"   || u == "A_UINT16" || u == "A_INT16")  return 2;
    if (u == "ULONG"   || u == "SLONG"   || u == "A_UINT32" || u == "A_INT32")  return 4;
    if (u == "FLOAT32_IEEE" || u == "FLOAT32" || u == "A_FLOAT32")              return 4;
    if (u == "FLOAT64_IEEE" || u == "FLOAT64" || u == "A_FLOAT64")              return 8;
    return 0; // unknown
}

// Check if ASAP2 datatype is signed
static bool datatypeIsSigned(const QString &dt)
{
    const QString u = dt.toUpper();
    return u == "SBYTE" || u == "SWORD" || u == "SLONG"
        || u == "A_INT8" || u == "A_INT16" || u == "A_INT32"
        || u.startsWith("FLOAT");
}

void A2LParser::parseRecordLayouts(const QString &text)
{
    m_recordLayoutColMajor.clear();
    m_recordLayoutDataSize.clear();
    m_recordLayoutSigned.clear();
    m_recordLayoutAxisSkip.clear();
    m_recordLayoutAxisSkipY.clear();
    m_recordLayoutAxisSize.clear();
    m_recordLayoutAxisSizeY.clear();
    auto rlBlocks = extractBlocks(text, "RECORD_LAYOUT");
    for (const auto &blockView : rlBlocks) {
        QString block = blockView.trimmed().toString();
        QString name = block.section(QRegularExpression(R"(\s+)"), 0, 0);
        if (name.isEmpty()) continue;
        // FNC_VALUES <position> <datatype> <index_mode>
        QRegularExpression fncRe(R"(FNC_VALUES\s+\S+\s+(\S+)\s+(\S+))");
        auto fm = fncRe.match(block);
        if (fm.hasMatch()) {
            m_recordLayoutColMajor[name] = fm.captured(2).toUpper() == "COLUMN_DIR";
            int sz = datatypeToSize(fm.captured(1));
            if (sz > 0) {
                m_recordLayoutDataSize[name] = sz;
                m_recordLayoutSigned[name] = datatypeIsSigned(fm.captured(1));
            }
        }
        // NO_AXIS_PTS_X/Y → count header sizes
        QRegularExpression naxRe(R"(\bNO_AXIS_PTS_X\s+\d+\s+(\S+))");
        auto naxm = naxRe.match(block);
        if (naxm.hasMatch()) {
            int skip = datatypeToSize(naxm.captured(1));
            if (skip > 0) m_recordLayoutAxisSkip[name] = skip;
        }
        QRegularExpression nayRe(R"(\bNO_AXIS_PTS_Y\s+\d+\s+(\S+))");
        auto naym = nayRe.match(block);
        if (naym.hasMatch()) {
            int skip = datatypeToSize(naym.captured(1));
            if (skip > 0) m_recordLayoutAxisSkipY[name] = skip;
        }
        // AXIS_PTS_X/Y → datatype for axis values (lookbehind to avoid NO_ prefix)
        QRegularExpression apxRe(R"((?<![A-Z_])AXIS_PTS_X\s+\d+\s+(\S+))");
        auto apxm = apxRe.match(block);
        if (apxm.hasMatch()) {
            int sz = datatypeToSize(apxm.captured(1));
            if (sz > 0) m_recordLayoutAxisSize[name] = sz;
        }
        QRegularExpression apyRe(R"((?<![A-Z_])AXIS_PTS_Y\s+\d+\s+(\S+))");
        auto apym = apyRe.match(block);
        if (apym.hasMatch()) {
            int sz = datatypeToSize(apym.captured(1));
            if (sz > 0) m_recordLayoutAxisSizeY[name] = sz;
        }
    }
}

int A2LParser::estimateLength(const A2LCharacteristic &ch) const
{
    int elemSize = ch.dataSize > 0 ? ch.dataSize : 2;
    if (ch.type == "MAP")
        return ch.dimensions.x * ch.dimensions.y * elemSize;
    if (ch.type == "CURVE")
        return ch.dimensions.x * elemSize;
    if (ch.type == "VALUE")
        return elemSize;
    if (ch.type == "VAL_BLK")
        return ch.dimensions.x * qMax(1, ch.dimensions.y) * elemSize;
    if (ch.type == "ASCII")
        return ch.dimensions.x;
    return elemSize;
}

uint32_t A2LParser::detectBaseAddress(int romSize)
{
    if (m_characteristics.isEmpty()) return 0;

    QVector<uint32_t> addrs;
    for (const auto &c : m_characteristics) {
        if (c.rawAddress > 0) addrs.append(c.rawAddress);
    }
    if (addrs.isEmpty()) return 0;

    uint32_t minAddr = addrs[0], maxAddr = addrs[0];
    for (auto a : addrs) {
        if (a < minAddr) minAddr = a;
        if (a > maxAddr) maxAddr = a;
    }

    // Common ECU base addresses (24-bit and 32-bit)
    const uint32_t candidates[] = {
        // 32-bit TriCore (Bosch MED17.x / EDC17.x, Infineon TC179x)
        0x80000000, 0x80040000, 0x80080000, 0x80100000,
        // 32-bit Continental / Simos (Infineon TC1xxx)
        0xA0000000, 0xA0040000, 0xA0080000, 0xA0800000, 0xA0820000,
        // 32-bit Renesas RH850 / SH-series
        0x00000000, 0x01000000, 0xFE000000, 0xFF000000,
        // 24-bit: Bosch ME7.x, MED9 (Motorola HC12 / ST10)
        0x800000, 0x810000, 0x820000, 0x808000, 0x818000,
        // 24-bit: Bosch EDC15/16
        0x400000, 0x410000, 0x420000,
        // 24-bit: Siemens / Continental
        0xA00000, 0xA08000, 0xA10000,
        // 24-bit: Direct-mapped
        0x000000, 0x010000, 0x020000,
        // 24-bit: Denso
        0xC00000, 0xD00000, 0xE00000,
        // 24-bit: Marelli
        0x200000, 0x300000,
        // 24-bit: Delco / Delphi
        0x060000, 0x070000,
    };

    for (auto base : candidates) {
        if (minAddr >= base && (maxAddr - base) < (uint32_t)romSize) {
            m_baseAddress = base;
            return base;
        }
    }

    // Try aligning to progressively coarser boundaries
    const uint32_t masks[] = {
        0xFFFF0000, 0xFFF00000, 0xFF000000, 0xF0000000
    };
    for (uint32_t mask : masks) {
        uint32_t guessBase = minAddr & mask;
        if ((maxAddr - guessBase) < (uint32_t)romSize) {
            m_baseAddress = guessBase;
            return guessBase;
        }
    }

    if (maxAddr < (uint32_t)romSize) {
        m_baseAddress = 0;
        return 0;
    }

    m_baseAddress = minAddr & 0xFFFF0000;
    return m_baseAddress;
}

// Helper: fill an AxisInfo from an A2LAxisDesc (called for xAxis / yAxis)
static AxisInfo buildAxisInfo(const A2LAxisDesc &ad,
                               const QMap<QString, CompuMethod>  &compuMethods,
                               const QMap<QString, A2LAxisPts>   &axisPts,
                               const QMap<QString, int>          &axisSkipMap,
                               const QMap<QString, int>          &axisSizeMap,
                               uint32_t baseAddress)
{
    AxisInfo ai;
    ai.inputName = ad.inputName;

    // Attach scaling / unit
    if (!ad.compuMethodRef.isEmpty() && compuMethods.contains(ad.compuMethodRef)) {
        ai.scaling    = compuMethods[ad.compuMethodRef];
        ai.hasScaling = true;
    }

    // Fixed values from A2L (FIX_AXIS_PAR / FIX_AXIS_PAR_DIST)
    if (!ad.fixedValues.isEmpty()) {
        ai.fixedValues = ad.fixedValues;
    }

    // AXIS_PTS reference → read later from ROM
    if (!ad.axisPtsRef.isEmpty() && axisPts.contains(ad.axisPtsRef)) {
        const A2LAxisPts &ap = axisPts[ad.axisPtsRef];
        // Skip count header (NO_AXIS_PTS_X) if present in the AXIS_PTS RECORD_LAYOUT
        uint32_t skip = 0;
        int axisValSize = ap.dataSize; // default from name heuristic
        if (!ap.recordLayoutRef.isEmpty()) {
            skip = (uint32_t)axisSkipMap.value(ap.recordLayoutRef, 0);
            // Use AXIS_PTS_X datatype for correct axis value size (overrides heuristic)
            if (axisSizeMap.contains(ap.recordLayoutRef))
                axisValSize = axisSizeMap[ap.recordLayoutRef];
        }
        ai.ptsAddress   = (ap.rawAddress - baseAddress) + skip;
        ai.ptsCount     = ap.maxCount;
        ai.ptsDataSize  = axisValSize;
        ai.hasPtsAddress = true;
        // Use the axis pts compu method if the axis desc didn't have one
        if (!ai.hasScaling && !ap.compuMethodRef.isEmpty()
                && compuMethods.contains(ap.compuMethodRef)) {
            ai.scaling    = compuMethods[ap.compuMethodRef];
            ai.hasScaling = true;
        }
    }
    return ai;
}

QVector<MapInfo> A2LParser::getMapList() const
{
    QVector<MapInfo> result;
    for (const auto &c : m_characteristics) {
        if (c.type != "MAP" && c.type != "CURVE" && c.type != "VAL_BLK" && c.type != "VALUE")
            continue;

        MapInfo m;
        m.name = c.name;
        m.description = c.description;
        m.type = c.type;
        m.rawAddress = c.rawAddress;
        m.address = (c.rawAddress >= m_baseAddress) ? (c.rawAddress - m_baseAddress) : 0;
        m.length = c.dataLength;
        m.dimensions = c.dimensions;
        m.dataSize = c.dataSize > 0 ? c.dataSize : 2;
        m.dataSigned = c.dataSigned;
        if (!c.compuMethodRef.isEmpty() && m_compuMethods.contains(c.compuMethodRef)) {
            m.scaling = m_compuMethods[c.compuMethodRef];
            m.hasScaling = true;
        }
        // Cell storage order from RECORD_LAYOUT FNC_VALUES index_mode
        if (!c.recordLayoutRef.isEmpty())
            m.columnMajor = m_recordLayoutColMajor.value(c.recordLayoutRef, false);

        // Axis info
        m.xAxis = buildAxisInfo(c.xAxisDesc, m_compuMethods, m_axisPts, m_recordLayoutAxisSkip, m_recordLayoutAxisSize, m_baseAddress);
        m.yAxis = buildAxisInfo(c.yAxisDesc, m_compuMethods, m_axisPts, m_recordLayoutAxisSkip, m_recordLayoutAxisSize, m_baseAddress);

        // STD_AXIS: axis values stored inline in ROM at the characteristic address.
        // Layout: [count_x][count_y (MAP)][x-axis values][y-axis values (MAP)][map data]
        // Sizes come from RECORD_LAYOUT (NO_AXIS_PTS_X/Y, AXIS_PTS_X/Y, FNC_VALUES).
        const bool xStd = (c.xAxisDesc.axisType == "STD_AXIS");
        const bool yStd = (c.yAxisDesc.axisType == "STD_AXIS");
        if (xStd || yStd) {
            const int ds  = m.dataSize;
            const int nx  = m.dimensions.x;
            const int ny  = m.dimensions.y;
            const QString &rl = c.recordLayoutRef;

            // Get actual sizes from RECORD_LAYOUT (fallback: 2 bytes for counts, ds for axes)
            int cntXSz = m_recordLayoutAxisSkip.value(rl, 2);   // NO_AXIS_PTS_X size
            int cntYSz = m_recordLayoutAxisSkipY.value(rl, 2);  // NO_AXIS_PTS_Y size
            int axXSz  = m_recordLayoutAxisSize.value(rl, ds);  // AXIS_PTS_X value size
            int axYSz  = m_recordLayoutAxisSizeY.value(rl, ds); // AXIS_PTS_Y value size

            if (m.type == "CURVE" && xStd) {
                int headerSz = cntXSz;
                m.xAxis.ptsAddress   = m.address + (uint32_t)headerSz;
                m.xAxis.ptsCount     = nx;
                m.xAxis.ptsDataSize  = axXSz;
                m.xAxis.hasPtsAddress = true;
                m.mapDataOffset = (uint32_t)(headerSz + nx * axXSz);

            } else if (m.type == "MAP") {
                int headerSz = cntXSz + cntYSz;
                if (xStd) {
                    m.xAxis.ptsAddress   = m.address + (uint32_t)headerSz;
                    m.xAxis.ptsCount     = nx;
                    m.xAxis.ptsDataSize  = axXSz;
                    m.xAxis.hasPtsAddress = true;
                }
                if (yStd) {
                    m.yAxis.ptsAddress   = m.address + (uint32_t)(headerSz + nx * axXSz);
                    m.yAxis.ptsCount     = ny;
                    m.yAxis.ptsDataSize  = axYSz;
                    m.yAxis.hasPtsAddress = true;
                }
                m.mapDataOffset = (uint32_t)(headerSz + nx * axXSz + ny * axYSz);
            }
        }

        result.append(m);
    }

    std::sort(result.begin(), result.end(), [](const MapInfo &a, const MapInfo &b) {
        return a.address < b.address;
    });

    return result;
}
