/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace ols {

struct EcuDetectionResult {
    QString family;            ///< e.g. "EDC17", "MED17", "SIMOS"
    QString ecuName;           ///< full name (family+model), e.g. "EDC17C46"
    QString ecuVariant;        ///< sub-model (often == model), free-form
    QString detectorName;      ///< human label of which detector fired
    QString detector;          ///< machine id ("signature" / "hardware_string" / "filename")

    QString hwNumber;          ///< hardware part number (Bosch HW)
    QString swNumber;          ///< software part number / production
    QString swVersion;
    QString productionNo;
    QString engineCode;

    QString hwAltNumber;       ///< alternative HW string from a secondary marker

    int          confidence    = 0;     ///< 0-100
    bool         ok            = false; ///< true if a positive match was made
    qint64       idBlockOffset = -1;    ///< offset where the matched signature was found
    QStringList  dataAreas;             ///< informational tags
    QByteArray   rawIdBlock;            ///< raw bytes around the match (for diagnostics)
};

struct EcuMetadataFields {
    QString *producer    = nullptr;
    QString *ecuName     = nullptr;
    QString *hwNumber    = nullptr;
    QString *swNumber    = nullptr;
    QString *swVersion   = nullptr;
    QString *productionNo = nullptr;
    QString *engineCode  = nullptr;
};

class EcuAutoDetect {
public:
    static EcuDetectionResult detect(const QByteArray &data);

    static int applyToFields(const EcuDetectionResult &result,
                              EcuMetadataFields &fields,
                              bool overwrite = false);

    static QByteArray decodeRom(const QByteArray &data,
                                 const QString &hint = {});

    static QStringList knownProducers();

    static QStringList knownEcus();

    static QStringList knownEcusForProducer(const QString &producer);

    static bool parseFamilyModel(const QString &input,
                                  QString *outFamily,
                                  QString *outModel);
};

} // namespace ols
