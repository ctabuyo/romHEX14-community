/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "EcuAutoDetect.h"

#include <QHash>
#include <QSet>
#include <QVector>

namespace ols {

namespace {


struct SigRow {
    const char *family;
    const char *model;
    QVector<QByteArray> patterns;
};

static const QVector<SigRow> &signatures()
{
    static const QVector<SigRow> kSigs = {
        {"MED17",   "",     {"MED17", "MED 17"}},
        {"MEDC17",  "",     {"MEDC17"}},
        {"MEV17",   "",     {"MEV17", "MEV 17"}},
        {"MEVD17",  "",     {"MEVD17"}},
        {"ME7",     "",     {"ME7.", "ME 7."}},
        {"MED9",    "",     {"MED9", "MED 9"}},
        {"ME9",     "",     {"ME9.", "ME 9."}},

        {"EDC17", "C46",  {"EDC17C46",  "EDC17_C46"}},
        {"EDC17", "C64",  {"EDC17C64",  "EDC17_C64"}},
        {"EDC17", "CP44", {"EDC17CP44", "EDC17_CP44"}},
        {"EDC17", "CP14", {"EDC17CP14", "EDC17_CP14"}},
        {"EDC17", "C54",  {"EDC17C54",  "EDC17_C54"}},
        {"EDC17", "CP04", {"EDC17CP04", "EDC17_CP04"}},
        {"EDC17", "CP20", {"EDC17CP20", "EDC17_CP20"}},
        {"EDC17", "CP24", {"EDC17CP24", "EDC17_CP24"}},
        {"EDC17", "C74",  {"EDC17C74",  "EDC17_C74"}},
        {"EDC17", "CP54", {"EDC17CP54", "EDC17_CP54"}},
        {"EDC17", "CP74", {"EDC17CP74", "EDC17_CP74"}},
        {"EDC17", "C06",  {"EDC17C06",  "EDC17_C06"}},
        {"EDC17", "C41",  {"EDC17C41",  "EDC17_C41"}},
        {"EDC17", "C50",  {"EDC17C50",  "EDC17_C50"}},
        {"EDC17", "CP02", {"EDC17CP02", "EDC17_CP02"}},
        {"EDC17", "CP09", {"EDC17CP09", "EDC17_CP09"}},
        {"EDC17", "CP45", {"EDC17CP45", "EDC17_CP45"}},
        {"EDC17", "C66",  {"EDC17C66",  "EDC17_C66"}},
        {"EDC17", "CP01", {"EDC17CP01", "EDC17_CP01"}},
        {"EDC17", "CP10", {"EDC17CP10", "EDC17_CP10"}},
        {"EDC17", "CP46", {"EDC17CP46", "EDC17_CP46"}},
        {"EDC17", "CP57", {"EDC17CP57", "EDC17_CP57"}},
        {"EDC17", "C10",  {"EDC17C10",  "EDC17_C10"}},
        {"EDC17", "C60",  {"EDC17C60",  "EDC17_C60"}},
        {"EDC17", "CP42", {"EDC17CP42", "EDC17_CP42"}},
        {"EDC17", "C70",  {"EDC17C70",  "EDC17_C70"}},
        {"EDC17", "C18",  {"EDC17C18",  "EDC17_C18"}},
        {"EDC17", "C19",  {"EDC17C19",  "EDC17_C19"}},
        {"EDC17", "C59",  {"EDC17C59",  "EDC17_C59"}},
        {"EDC17", "C08",  {"EDC17C08",  "EDC17_C08"}},
        {"EDC17", "C11",  {"EDC17C11",  "EDC17_C11"}},
        {"EDC17", "C42",  {"EDC17C42",  "EDC17_C42"}},
        {"EDC17", "C84",  {"EDC17C84",  "EDC17_C84"}},
        {"EDC17", "C49",  {"EDC17C49",  "EDC17_C49"}},
        {"EDC17", "C69",  {"EDC17C69",  "EDC17_C69"}},
        {"EDC17", "CP52", {"EDC17CP52", "EDC17_CP52"}},
        {"EDC17", "C79",  {"EDC17C79",  "EDC17_C79"}},
        {"EDC17", "CP27", {"EDC17CP27", "EDC17_CP27"}},
        {"EDC17", "CP07", {"EDC17CP07", "EDC17_CP07"}},
        {"EDC17", "CP06", {"EDC17CP06", "EDC17_CP06"}},
        {"EDC17", "CP16", {"EDC17CP16", "EDC17_CP16"}},
        {"EDC17", "CP11", {"EDC17CP11", "EDC17_CP11"}},
        {"EDC17", "CP22", {"EDC17CP22", "EDC17_CP22"}},
        {"EDC17", "CP48", {"EDC17CP48", "EDC17_CP48"}},
        {"EDC17", "CP55", {"EDC17CP55", "EDC17_CP55"}},
        {"EDC17", "CP65", {"EDC17CP65", "EDC17_CP65"}},
        {"EDC17", "CP05", {"EDC17CP05", "EDC17_CP05"}},
        {"EDC17", "CV41", {"EDC17CV41", "EDC17_CV41"}},
        {"EDC17", "CV42", {"EDC17CV42", "EDC17_CV42"}},
        {"EDC17", "CV52", {"EDC17CV52", "EDC17_CV52"}},
        {"EDC17", "U01",  {"EDC17U01",  "EDC17_U01", "EDC17U05", "EDC17_U05"}},
        {"EDC17", "C01",  {"EDC17C01",  "EDC17_C01"}},
        {"EDC17", "C09",  {"EDC17C09",  "EDC17_C09"}},
        {"EDC17", "C45",  {"EDC17C45",  "EDC17_C45"}},
        {"EDC17", "C58",  {"EDC17C58",  "EDC17_C58"}},
        {"EDC17", "CP18", {"EDC17CP18", "EDC17_CP18"}},
        {"EDC17", "CP50", {"EDC17CP50", "EDC17_CP50"}},
        {"EDC17", "",     {"EDC17", "EDC 17"}},

        {"EDC16", "C31",  {"EDC16C31", "EDC16_C31"}},
        {"EDC16", "C32",  {"EDC16C32", "EDC16_C32"}},
        {"EDC16", "C34",  {"EDC16C34", "EDC16_C34"}},
        {"EDC16", "C36",  {"EDC16C36", "EDC16_C36"}},
        {"EDC16", "C39",  {"EDC16C39", "EDC16_C39"}},
        {"EDC16", "CP31", {"EDC16CP31", "EDC16_CP31"}},
        {"EDC16", "CP34", {"EDC16CP34", "EDC16_CP34"}},
        {"EDC16", "U",    {"EDC16U", "EDC16_U"}},
        {"EDC16", "C0",   {"EDC16C0"}},
        {"EDC16", "C3",   {"EDC16C3"}},
        {"EDC16", "C7",   {"EDC16C7"}},
        {"EDC16", "C8",   {"EDC16C8"}},
        {"EDC16", "C9",   {"EDC16C9"}},
        {"EDC16", "CP39", {"EDC16CP39"}},
        {"EDC16", "",     {"EDC16", "EDC 16"}},

        {"MD1", "CS004",  {"MD1CS004"}},
        {"MD1", "CS005",  {"MD1CS005"}},
        {"MD1", "CP006",  {"MD1CP006"}},
        {"MD1", "",       {"MD1C", "MD1_C", "MD1CS", "MD1CP"}},
        {"MG1", "",       {"MG1C", "MG1_C", "MG1CS", "MG1CP"}},

        {"ME17", "3.0", {"ME17.3.0"}},
        {"ME17", "3.3", {"ME17.3.3"}},
        {"ME17", "",    {"ME17.", "ME 17."}},

        {"MS43",  "", {"MS43"}},
        {"MSS65", "", {"MSS65"}},

        {"EDC15", "C6", {"EDC15C6", "EDC15_C6"}},
        {"EDC15", "C4", {"EDC15C4", "EDC15_C4"}},
        {"EDC15", "",   {"EDC15", "EDC 15"}},
        {"EDC7",  "",   {"EDC7", "EDC 7"}},
        {"DCU",   "",   {"DCU17"}},
        {"PSG",   "",   {"PSG16"}},
        {"MT35",  "",   {"MT35E", "MT35"}},
        {"ME155", "",   {"ME1.5.5", "ME 1.5.5"}},

        {"D42",   "",   {"Sirius D42", "Sirius_D42", "Sirius"}},
        {"BPCM",  "",   {"BPCM", "BMS N82", "BMS_SBLD"}},
        {"INV",   "",   {"INV CON"}},

        {"SIMOS", "3",  {"SIMOS3", "SIMOS 3"}},
        {"SIMOS", "6",  {"SIMOS6", "SIMOS 6"}},
        {"SIMOS", "7",  {"SIMOS7", "SIMOS 7"}},
        {"SIMOS", "8",  {"SIMOS8", "SIMOS 8"}},
        {"SIMOS", "10", {"SIMOS10", "SIMOS 10"}},
        {"SIMOS", "11", {"SIMOS11", "SIMOS 11"}},
        {"SIMOS", "12", {"SIMOS12", "SIMOS 12"}},
        {"SIMOS", "16", {"SIMOS16", "SIMOS 16"}},
        {"SIMOS", "18", {"SIMOS18", "SIMOS 18"}},
        {"SIMOS", "19", {"SIMOS19", "SIMOS 19"}},
        {"SIMOS", "",   {"SIMOS", "Simos"}},

        {"PCR",   "", {"PCR2", "PCR 2"}},
        {"MSD80", "", {"MSD80", "MSD81", "MSD85"}},
        {"MSV",   "", {"MSV70", "MSV80", "MSV90"}},

        {"SID",       "807EVO", {"SID807 EVO", "SID807EVO"}},
        {"SID",       "807",    {"SID807"}},
        {"SID",       "301",    {"SID301", "SID 301"}},
        {"SID",       "803",    {"SID803"}},
        {"SID",       "310",    {"SID310", "SID307"}},
        {"FORD_SID",  "208",    {"_SID208_"}},
        {"PSA_SID",   "208",    {"PUMFRR72"}},
        {"SID",       "208",    {"SID208"}},
        {"SID",       "206",    {"SID206"}},
        {"SID",       "202",    {"SID202"}},
        {"SID",       "209",    {"SID209"}},
        {"SID",       "305",    {"SID305"}},
        {"SID",       "321",    {"SID321"}},
        {"SID",       "203",    {"SID203"}},
        {"SID",       "",       {"SID2", "SID3", "SID8"}},

        {"SDI",    "", {"SDI4", "SDI6", "SDI10", "SDI21"}},
        {"SIM271", "", {"SIM271"}},
        {"EMS",    "", {"EMS22"}},
        {"PPD",    "", {"PPD1"}},

        {"CRD", "3",   {"CRD3"}},
        {"CRD", "2",   {"CRD2"}},
        {"DCM", "3.5", {"DCM3.5", "DCM 3.5"}},
        {"DCM", "3.7", {"DCM3.7", "DCM 3.7"}},
        {"DCM", "6.2", {"DCM6.2", "DCM 6.2", "DCM62", "DCM6.2A"}},
        {"DCM", "6.1", {"DCM6.1", "DCM 6.1", "DCM61"}},
        {"DCM", "7.1", {"DCM7.1", "DCM 7.1", "DCM71"}},
        {"DCM", "3.3", {"DCM3.3", "DCM 3.3", "DCM33"}},
        {"DCM", "",    {"DCM3.", "DCM6.", "DCM7.", "Delphi DCM", "DELPHI"}},

        {"DELCO", "", {"Delco", "DELCO", "AC-Delco", "AC Delco"}},
        {"DENSO", "", {"DENSO", "Denso"}},

        {"IAW", "7GF", {"IAW7GF", "IAW 7GF", "IAW_7GF"}},
        {"MM9", "GF",  {"MM9GF", "MM9 GF", "MM9GV", "MM9 GV"}},
        {"MJD", "6",   {"MJD6"}},
        {"MJD", "8",   {"MJD8"}},
        {"MJD", "9",   {"MJD9"}},
        {"MJD", "",    {"Marelli", "MARELLI"}},

        {"NGC",     "", {"NGC4", "NGC5"}},
        {"PHOENIX", "", {"Phoenix"}},
        {"ME1730",  "", {"ME17.3.0"}},

        {"TOYOTA",  "",       {"TOYOTA", "Toyota", "LEXUS", "Lexus", "89661-"}},
        {"MAZDA",   "",       {"MAZDA", "Mazda"}},
        {"SUBARU",  "",       {"SUBARU", "Subaru"}},
        {"HITACHI", "SH7058", {"SH7058"}},
        {"HITACHI", "SH705x", {"SH705"}},
        {"NISSAN",  "",       {"NISSAN", "Nissan", "INFINITI"}},
        {"MITSUBISHI", "",    {"MITSUBISHI", "Mitsubishi"}},
        {"HONDA",   "",       {"HONDA", "Honda", "ACURA"}},
        {"ISUZU",   "",       {"ISUZU", "Isuzu"}},
        {"KIA",     "",       {"HYUNDAI", "Hyundai", "KIA", "Kia"}},
        {"SUZUKI",  "",       {"SUZUKI", "Suzuki"}},

        {"SIM266", "",  {"SIM266", "SIM 266"}},
        {"GPEC",   "2", {"GPEC2", "GPEC-2", "GPEG2"}},
        {"CPEGD",  "2", {"CPEGD2"}},
        {"CPEGD",  "3", {"CPEGD3"}},
        {"VALEO",  "",  {"VALEO", "Valeo"}},
        {"ESU",    "",  {"ESU4", "ESU1", "Visteon", "VISTEON"}},
    };
    return kSigs;
}


static QString producerForFamily(const QString &family)
{
    static const QHash<QString, QString> kMap = {
        {QStringLiteral("MED17"),   QStringLiteral("Bosch")},
        {QStringLiteral("MEDC17"),  QStringLiteral("Bosch")},
        {QStringLiteral("MEV17"),   QStringLiteral("Bosch")},
        {QStringLiteral("MEVD17"),  QStringLiteral("Bosch")},
        {QStringLiteral("ME7"),     QStringLiteral("Bosch")},
        {QStringLiteral("MED9"),    QStringLiteral("Bosch")},
        {QStringLiteral("ME9"),     QStringLiteral("Bosch")},
        {QStringLiteral("EDC17"),   QStringLiteral("Bosch")},
        {QStringLiteral("EDC16"),   QStringLiteral("Bosch")},
        {QStringLiteral("EDC15"),   QStringLiteral("Bosch")},
        {QStringLiteral("EDC7"),    QStringLiteral("Bosch")},
        {QStringLiteral("MD1"),     QStringLiteral("Bosch")},
        {QStringLiteral("MG1"),     QStringLiteral("Bosch")},
        {QStringLiteral("ME17"),    QStringLiteral("Bosch")},
        {QStringLiteral("ME1730"),  QStringLiteral("Bosch")},
        {QStringLiteral("ME155"),   QStringLiteral("Bosch")},
        {QStringLiteral("MS43"),    QStringLiteral("Bosch")},
        {QStringLiteral("MSS65"),   QStringLiteral("Bosch")},
        {QStringLiteral("DCU"),     QStringLiteral("Bosch")},
        {QStringLiteral("PSG"),     QStringLiteral("Bosch")},
        {QStringLiteral("MT35"),    QStringLiteral("Bosch")},
        {QStringLiteral("INV"),     QStringLiteral("Bosch")},
        {QStringLiteral("BPCM"),    QStringLiteral("Bosch")},
        {QStringLiteral("SIMOS"),   QStringLiteral("Continental")},
        {QStringLiteral("SIM266"),  QStringLiteral("Continental")},
        {QStringLiteral("SIM271"),  QStringLiteral("Continental")},
        {QStringLiteral("PCR"),     QStringLiteral("Continental")},
        {QStringLiteral("MSD80"),   QStringLiteral("Continental")},
        {QStringLiteral("MSV"),     QStringLiteral("Continental")},
        {QStringLiteral("SID"),     QStringLiteral("Continental")},
        {QStringLiteral("FORD_SID"), QStringLiteral("Continental")},
        {QStringLiteral("PSA_SID"), QStringLiteral("Continental")},
        {QStringLiteral("SDI"),     QStringLiteral("Continental")},
        {QStringLiteral("EMS"),     QStringLiteral("Continental")},
        {QStringLiteral("PPD"),     QStringLiteral("Continental")},
        {QStringLiteral("GPEC"),    QStringLiteral("Continental")},
        {QStringLiteral("CPEGD"),   QStringLiteral("Continental")},
        {QStringLiteral("D42"),     QStringLiteral("Siemens")},
        {QStringLiteral("NGC"),     QStringLiteral("Continental")},
        {QStringLiteral("DCM"),     QStringLiteral("Delphi")},
        {QStringLiteral("CRD"),     QStringLiteral("Delphi")},
        {QStringLiteral("IAW"),     QStringLiteral("Magneti Marelli")},
        {QStringLiteral("MM9"),     QStringLiteral("Magneti Marelli")},
        {QStringLiteral("MJD"),     QStringLiteral("Magneti Marelli")},
        {QStringLiteral("DENSO"),   QStringLiteral("Denso")},
        {QStringLiteral("TOYOTA"),  QStringLiteral("Denso")},
        {QStringLiteral("MAZDA"),   QStringLiteral("Denso")},
        {QStringLiteral("SUBARU"),  QStringLiteral("Denso")},
        {QStringLiteral("HITACHI"), QStringLiteral("Hitachi")},
        {QStringLiteral("NISSAN"),  QStringLiteral("Hitachi")},
        {QStringLiteral("DELCO"),   QStringLiteral("AC-Delco")},
        {QStringLiteral("MITSUBISHI"), QStringLiteral("Mitsubishi")},
        {QStringLiteral("HONDA"),   QStringLiteral("Honda")},
        {QStringLiteral("ISUZU"),   QStringLiteral("Isuzu")},
        {QStringLiteral("KIA"),     QStringLiteral("Hyundai/Kia")},
        {QStringLiteral("SUZUKI"),  QStringLiteral("Suzuki")},
        {QStringLiteral("PHOENIX"), QStringLiteral("John Deere")},
        {QStringLiteral("VALEO"),   QStringLiteral("Valeo")},
        {QStringLiteral("ESU"),     QStringLiteral("Visteon")},
    };
    return kMap.value(family);
}


static inline bool isAlnumByte(unsigned char c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z');
}

static QString findVwHwNumber(const QByteArray &data, qsizetype maxScan)
{
    const char *base = data.constData();
    const auto *u = reinterpret_cast<const unsigned char *>(base);
    for (qsizetype i = 0; i + 11 < maxScan; ++i) {
        if (i > 0 && isAlnumByte(u[i - 1])) continue;
        if (!isAlnumByte(u[i]) || !isAlnumByte(u[i + 1]) || !isAlnumByte(u[i + 2]))
            continue;
        if (base[i + 3] != '9') continue;
        bool digitsOk = true;
        for (int k = 4; k <= 9; ++k) {
            if (base[i + k] < '0' || base[i + k] > '9') { digitsOk = false; break; }
        }
        if (!digitsOk) continue;
        int len = 10;
        for (int k = 0; k < 3 && i + 10 + k < maxScan; ++k) {
            const char c = base[i + 10 + k];
            if (c >= 'A' && c <= 'Z') ++len;
            else break;
        }
        if (len == 10) continue;  // require revision suffix to avoid noise
        if (i + len < maxScan && isAlnumByte(u[i + len])) continue;
        return QString::fromLatin1(base + i, int(len));
    }
    return {};
}

static QString findBoschHwNumber(const QByteArray &data, qsizetype maxScan)
{
    const char *base = data.constData();
    const auto *u = reinterpret_cast<const unsigned char *>(base);
    for (qsizetype i = 0; i + 10 < maxScan; ++i) {
        if (i > 0 && isAlnumByte(u[i - 1])) continue;
        if (base[i] != '0' || base[i + 1] != '2') continue;
        if (base[i + 2] != '6' && base[i + 2] != '8') continue;
        if (base[i + 3] < '0' || base[i + 3] > '9') continue;
        bool ok = true;
        for (int k = 4; k <= 9; ++k) {
            const char c = base[i + k];
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) { ok = false; break; }
        }
        if (!ok) continue;
        int len = 10;
        if (i + 10 < maxScan) {
            const char c = base[i + 10];
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) ++len;
        }
        if (i + len < maxScan && isAlnumByte(u[i + len])) continue;
        return QString::fromLatin1(base + i, int(len));
    }
    return {};
}

static QString findBoschSwNumber(const QByteArray &data, qsizetype maxScan)
{
    const char *base = data.constData();
    const auto *u = reinterpret_cast<const unsigned char *>(base);
    for (qsizetype i = 0; i + 10 < maxScan; ++i) {
        if (i > 0 && isAlnumByte(u[i - 1])) continue;
        if (base[i] == '1' && base[i + 1] == '0'
            && base[i + 2] == '3' && base[i + 3] == '7') {
            bool ok = true;
            for (int k = 4; k <= 9; ++k) {
                if (base[i + k] < '0' || base[i + k] > '9') { ok = false; break; }
            }
            if (ok && (i + 10 == maxScan || !isAlnumByte(u[i + 10])))
                return QString::fromLatin1(base + i, 10);
        }
        if (base[i] == '0' && base[i + 1] == '2' && base[i + 2] == '6'
            && base[i + 3] >= '0' && base[i + 3] <= '9'
            && base[i + 4] == 'S') {
            bool ok = true;
            for (int k = 5; k <= 9; ++k) {
                if (base[i + k] < '0' || base[i + k] > '9') { ok = false; break; }
            }
            if (ok && (i + 10 == maxScan || !isAlnumByte(u[i + 10])))
                return QString::fromLatin1(base + i, 10);
        }
    }
    return {};
}

static QString findProductionNumber(const QByteArray &data, qsizetype maxScan)
{
    const char *base = data.constData();
    const auto *u = reinterpret_cast<const unsigned char *>(base);
    for (qsizetype i = 0; i + 6 < maxScan; ++i) {
        if (i > 0 && isAlnumByte(u[i - 1])) continue;
        bool ok = true;
        for (int k = 0; k < 6; ++k) {
            if (base[i + k] < '0' || base[i + k] > '9') { ok = false; break; }
        }
        if (!ok) continue;
        if (i + 6 < maxScan && isAlnumByte(u[i + 6])) continue;
        if (base[i] == '0') continue;          // require leading non-zero
        return QString::fromLatin1(base + i, 6);
    }
    return {};
}


static QString readMarkerValue(const QByteArray &data,
                                const QByteArrayList &markers,
                                qsizetype searchEnd)
{
    for (const auto &marker : markers) {
        const qsizetype pos = data.indexOf(marker, 0);
        if (pos < 0 || pos >= searchEnd) continue;
        const qsizetype valStart = pos + marker.size();
        const qsizetype maxEnd = std::min<qsizetype>(valStart + 32, data.size());
        QByteArray val;
        for (qsizetype i = valStart; i < maxEnd; ++i) {
            const unsigned char c = data.at(i);
            if (c == 0 || c == '\r' || c == '\n') break;
            val.append(static_cast<char>(c));
        }
        QString s = QString::fromLatin1(val).trimmed();
        if (s.size() >= 3) return s;
    }
    return {};
}

} // namespace


EcuDetectionResult EcuAutoDetect::detect(const QByteArray &data)
{
    EcuDetectionResult r;
    if (data.isEmpty()) return r;

    const qsizetype searchEnd = std::min<qsizetype>(data.size(), 0x300000);

    for (const auto &row : signatures()) {
        for (const auto &pat : row.patterns) {
            const qsizetype hit = data.indexOf(pat, 0);
            if (hit < 0 || hit >= searchEnd) continue;

            r.ok            = true;
            r.family        = QString::fromLatin1(row.family);
            r.ecuVariant    = QString::fromLatin1(row.model);
            r.ecuName       = r.family + r.ecuVariant;
            r.detector      = QStringLiteral("signature");
            r.detectorName  = QStringLiteral("Signature scan");
            r.idBlockOffset = hit;
            r.confidence    = row.model[0] ? 95 : 70;
            const qsizetype ctxStart = std::max<qsizetype>(0, hit - 16);
            const qsizetype ctxEnd   = std::min<qsizetype>(data.size(),
                                                            hit + pat.size() + 48);
            r.rawIdBlock = data.mid(ctxStart, ctxEnd - ctxStart);
            r.hwNumber = readMarkerValue(data,
                {"Hardware: ", "Hardware:", "HARDWARE: ", "HW: "}, searchEnd);
            r.swNumber = readMarkerValue(data,
                {"Software: ", "Software:", "SOFTWARE: ", "SW: "}, searchEnd);
            r.swVersion = readMarkerValue(data,
                {"Version: ", "Version:", "SW-Version: ", "Sw-Version: "}, searchEnd);
            if (r.hwNumber.isEmpty())
                r.hwNumber = findVwHwNumber(data, searchEnd);
            const QString boschHw = findBoschHwNumber(data, searchEnd);
            if (r.hwNumber.isEmpty())
                r.hwNumber = boschHw;
            else if (boschHw != r.hwNumber)
                r.hwAltNumber = boschHw;
            if (r.swNumber.isEmpty())
                r.swNumber = findBoschSwNumber(data, searchEnd);
            if (r.productionNo.isEmpty())
                r.productionNo = findProductionNumber(data, searchEnd);
            return r;
        }
    }

    const QString hwVal = readMarkerValue(data,
        {"Hardware: ", "Hardware:", "HARDWARE: ", "HW: "}, searchEnd);
    if (!hwVal.isEmpty()) {
        QString hwUpper = hwVal.toUpper();
        hwUpper.remove(' ').remove('_').remove('-');
        for (const auto &row : signatures()) {
            QString check = QString::fromLatin1(row.family)
                          + QString::fromLatin1(row.model);
            QString checkUpper = check.toUpper().remove(' ').remove('_');
            if (checkUpper.size() < 4) continue;
            if (!hwUpper.contains(checkUpper)) continue;

            r.ok           = true;
            r.family       = QString::fromLatin1(row.family);
            r.ecuVariant   = QString::fromLatin1(row.model);
            r.ecuName      = r.family + r.ecuVariant;
            r.detector     = QStringLiteral("hardware_string");
            r.detectorName = QStringLiteral("Hardware: marker");
            r.confidence   = 60;
            r.hwNumber     = hwVal;
            r.swNumber     = readMarkerValue(data,
                {"Software: ", "Software:", "SOFTWARE: ", "SW: "}, searchEnd);
            r.swVersion    = readMarkerValue(data,
                {"Version: ", "Version:", "SW-Version: "}, searchEnd);
            return r;
        }
    }

    return r;
}

int EcuAutoDetect::applyToFields(const EcuDetectionResult &result,
                                  EcuMetadataFields &fields,
                                  bool overwrite)
{
    if (!result.ok) return 0;
    int written = 0;
    auto put = [&](QString *dst, const QString &src) {
        if (!dst || src.isEmpty()) return;
        if (!overwrite && !dst->isEmpty()) return;
        *dst = src;
        ++written;
    };
    put(fields.producer,    producerForFamily(result.family));
    put(fields.ecuName,     result.ecuName);
    put(fields.hwNumber,    result.hwNumber);
    put(fields.swNumber,    result.swNumber);
    put(fields.swVersion,   result.swVersion);
    put(fields.productionNo, result.productionNo);
    put(fields.engineCode,  result.engineCode);
    return written;
}

QByteArray EcuAutoDetect::decodeRom(const QByteArray &data,
                                     const QString & /*hint*/)
{
    return data;
}


QStringList EcuAutoDetect::knownProducers()
{
    static const QStringList kCached = []() {
        QSet<QString> s;
        for (const auto &row : signatures()) {
            const QString p = producerForFamily(QString::fromLatin1(row.family));
            if (!p.isEmpty()) s.insert(p);
        }
        QStringList list(s.begin(), s.end());
        list.sort();
        return list;
    }();
    return kCached;
}

bool EcuAutoDetect::parseFamilyModel(const QString &input,
                                      QString *outFamily,
                                      QString *outModel)
{
    if (input.trimmed().isEmpty()) return false;
    auto norm = [](const QString &s) {
        QString o = s.toUpper();
        o.remove(QChar(' ')); o.remove(QChar('.'));
        o.remove(QChar('-')); o.remove(QChar('_'));
        return o;
    };
    const QString needle = norm(input);

    QString bestFamily, bestModel;
    int     bestScore = -1;
    for (const auto &row : signatures()) {
        const QString family = QString::fromLatin1(row.family);
        const QString model  = QString::fromLatin1(row.model);
        const QString combo  = norm(family + model);
        if (combo.isEmpty()) continue;
        if (combo == needle) {
            const int score = combo.size() * 2 + (model.isEmpty() ? 0 : 1);
            if (score > bestScore) {
                bestScore = score;
                bestFamily = family;
                bestModel  = model;
            }
        }
    }
    if (bestScore < 0) {
        for (const auto &row : signatures()) {
            const QString family = QString::fromLatin1(row.family);
            if (norm(family) == needle) {
                bestFamily = family;
                bestModel  = {};
                bestScore  = 1;
                break;
            }
        }
    }
    if (bestScore < 0) return false;
    if (outFamily) *outFamily = bestFamily;
    if (outModel)  *outModel  = bestModel;
    return true;
}

QStringList EcuAutoDetect::knownEcus()
{
    static const QStringList kCached = []() {
        QSet<QString> s;
        for (const auto &row : signatures()) {
            const QString family = QString::fromLatin1(row.family);
            const QString model  = QString::fromLatin1(row.model);
            QString name = family;
            if (!model.isEmpty()) {
                const bool needSpace = model.contains(QChar('.'))
                                       || (model.size() <= 3
                                           && !model.front().isLetter()
                                           && !model.startsWith(QChar('C')));
                if (needSpace) name += QChar(' ');
                name += model;
            }
            s.insert(name);
        }
        QStringList list(s.begin(), s.end());
        list.sort();
        return list;
    }();
    return kCached;
}

QStringList EcuAutoDetect::knownEcusForProducer(const QString &producer)
{
    if (producer.trimmed().isEmpty()) return knownEcus();
    QSet<QString> s;
    for (const auto &row : signatures()) {
        const QString family       = QString::fromLatin1(row.family);
        const QString rowProducer  = producerForFamily(family);
        if (rowProducer.compare(producer, Qt::CaseInsensitive) != 0)
            continue;
        const QString model = QString::fromLatin1(row.model);
        QString name = family;
        if (!model.isEmpty()) {
            const bool needSpace = model.contains(QChar('.'))
                                   || (model.size() <= 3
                                       && !model.front().isLetter()
                                       && !model.startsWith(QChar('C')));
            if (needSpace) name += QChar(' ');
            name += model;
        }
        s.insert(name);
    }
    QStringList list(s.begin(), s.end());
    list.sort();
    return list;
}

} // namespace ols
