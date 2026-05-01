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

/// Result of automatic ECU identification on a ROM image.
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

/// Pointers to Project fields the caller wants populated.  Any may be null
/// to skip writing that field.
struct EcuMetadataFields {
    QString *producer    = nullptr;
    QString *ecuName     = nullptr;
    QString *hwNumber    = nullptr;
    QString *swNumber    = nullptr;
    QString *swVersion   = nullptr;
    QString *productionNo = nullptr;
    QString *engineCode  = nullptr;
};

/// String-based ECU detector built from a curated signature table.
///
/// Strategy (mirrors the Python reference at portal/dtc/app.py
/// `detect_ecu_universal()`):
///
///   1. Walk the first ~3 MiB of @p data, looking for each entry in the
///      signature table in priority order.  Order matters: more specific
///      signatures (e.g. MED17) precede less specific ones (e.g. EDC17)
///      because MED17 firmwares can contain the substring "EDC17".
///   2. If no signature matches, scan for "Hardware: " / "HW: " markers
///      and try to derive the family from the value string.
///
/// Returns @c ok==false when neither pass produces a hit.
class EcuAutoDetect {
public:
    static EcuDetectionResult detect(const QByteArray &data);

    /// Copy fields from @p result into @p fields.  Existing non-empty values
    /// are preserved unless @p overwrite is true.  Returns the number of
    /// pointers that were actually written.
    static int applyToFields(const EcuDetectionResult &result,
                              EcuMetadataFields &fields,
                              bool overwrite = false);

    /// No-op in community build (Pro can decode/decrypt vendor wrappers).
    static QByteArray decodeRom(const QByteArray &data,
                                 const QString &hint = {});

    /// Sorted, de-duplicated list of every producer registered in the
    /// signature table (Bosch, Continental, Delphi, Denso, Magneti
    /// Marelli, Hitachi, Siemens, AC-Delco, …).  Used by the Project
    /// Properties dialog to populate the Producer combo box.
    static QStringList knownProducers();

    /// Every ECU name we can detect — `family + variant` for each entry,
    /// de-duplicated and sorted.  Examples: "EDC17C46", "MED17",
    /// "MEVD17 2.6", "SIMOS 18", "DCM 6.2".  Used by the Project
    /// Properties dialog to populate the Build (ECU) combo box.
    static QStringList knownEcus();

    /// Subset of `knownEcus()` whose family maps to @p producer.  Empty /
    /// unknown producer returns the full list.  Used to cascade the Build
    /// dropdown after the user picks a Producer.
    static QStringList knownEcusForProducer(const QString &producer);

    /// Reverse of `knownEcus()`: parse a Project::ecuType string back into
    /// the (family, variant) pair the engines expect.  Accepts any of the
    /// formats `knownEcus()` emits ("EDC17C46", "MEVD17 2.6", "SIMOS 18",
    /// "DCM6.2", etc.) plus light variants (case-insensitive, ignores
    /// spaces / dots).  Returns true on a successful match.
    static bool parseFamilyModel(const QString &input,
                                  QString *outFamily,
                                  QString *outModel);
};

} // namespace ols
