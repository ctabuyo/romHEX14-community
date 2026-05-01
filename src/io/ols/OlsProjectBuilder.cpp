/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "OlsProjectBuilder.h"
#include "OlsImporter.h"
#include "../../project.h"

#include <QDateTime>
#include <QFileInfo>

namespace ols {

static void capturePrimarySegments(const QVector<OlsSegment> &segments,
                                    QVector<OlsSegmentSnapshot> &out)
{
    for (const auto &seg : segments) {
        if (!seg.isPrimary) continue;
        OlsSegmentSnapshot snap;
        snap.flashBase = seg.flashBase;
        snap.preamble  = seg.preamble;
        snap.rawBytes  = seg.data;
        out.append(snap);
    }
}

QVector<Project *> buildProjectsFromOlsImport(const OlsImportResult &result,
                                              const QString &sourcePath,
                                              QObject *parent)
{
    QVector<Project *> out;
    if (result.versions.isEmpty())
        return out;

    auto *p = new Project(parent);

    // ── Identity ─────────────────────────────────────────────────────────
    QString name = QFileInfo(sourcePath).completeBaseName();
    if (name.isEmpty())
        name = QFileInfo(result.metadata.originalFileName).completeBaseName();
    if (name.isEmpty())
        name = QStringLiteral("Imported OLS");
    p->name      = name;
    p->romPath   = sourcePath;
    p->createdAt = QDateTime::currentDateTime();
    p->createdBy = qEnvironmentVariable("USERNAME",
                       qEnvironmentVariable("USER", QStringLiteral("Unknown")));

    // ── Metadata mapping ─────────────────────────────────────────────────
    // OLS project metadata fields mirror the WinOLS "Project Properties"
    // dialog. Map them onto Project's WinOLS-style fields.  metadata.type
    // ("(DAMOS)" / "(EEPROM)") is OLS's storage-type tag, not Project's
    // vehicleType ("Passenger car" / "Truck") — we deliberately don't map
    // that one to avoid a category error.
    const auto &md = result.metadata;
    p->brand        = md.make;
    p->model        = md.model;
    p->transmission = md.transmission;
    p->engineCode   = md.engineCode;
    p->ecuType      = md.ecuName;
    p->ecuProducer  = md.manufacturer;
    p->ecuNrEcu     = md.hwNumber;
    p->ecuSwNumber  = md.swNumber;
    p->ecuNrProd    = md.productionNo;
    bool yearOk = false;
    const int yearN = md.year.toInt(&yearOk);
    if (yearOk) p->year = yearN;

    // OLS importComment is user-entered free text; notes is the long-form
    // notes field.  Prefer notes; fall back to importComment.
    if (!md.notes.isEmpty())              p->notes = md.notes;
    else if (!md.importComment.isEmpty()) p->notes = md.importComment;

    // ── Primary Version (index 0) → main ROM ─────────────────────────────
    const auto &v0 = result.versions[0];
    p->byteOrder    = v0.byteOrder;
    p->baseAddress  = v0.baseAddress;
    p->currentData  = v0.romData;
    p->originalData = v0.romData;
    p->maps         = v0.maps;
    capturePrimarySegments(v0.segments, p->olsSegments);

    // ── Extra Versions (1..N-1) as in-project ProjectVersion snapshots ──
    // mainwindow.cpp:3679-3690 reads primary->versions.size() to surface
    // "N extra version(s)" in the status bar, so the count here must
    // exclude Version 0 (the main ROM).
    for (int i = 1; i < result.versions.size(); ++i) {
        const auto &v = result.versions[i];
        ProjectVersion pv;
        pv.name = v.name.isEmpty()
                    ? QStringLiteral("Version %1").arg(i)
                    : v.name;
        pv.created = QDateTime::currentDateTime();
        pv.data    = v.romData;
        capturePrimarySegments(v.segments, pv.olsSegments);
        p->versions.append(pv);
    }

    out.append(p);
    return out;
}

} // namespace ols
