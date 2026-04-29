/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include "romdata.h"

// Result for a single map after linking
struct MapLinkResult {
    QString  mapName;
    uint32_t refAddress    = 0;   // offset in reference ROM
    uint32_t linkedAddress = 0;   // offset in target ROM (0 if not found)
    int      confidence    = 0;   // 0–100
    enum Status { Exact, Fuzzy, NotFound } status = NotFound;
};

// Full session result after linking a target ROM to a reference project
struct RomLinkSession {
    QString                label;
    QVector<MapLinkResult> results;
    int64_t                dominantDelta = 0;  // most common (linkedAddr - refAddr)
    int                    matchedCount  = 0;
    int                    totalCount    = 0;
    // Independent axis remapping: refAxisAddr → linkedAxisAddr
    // Populated by Phase 5 — used instead of the map delta for axis pointers.
    QMap<uint32_t, uint32_t> axisOffsets;
};

class RomLinker : public QObject {
    Q_OBJECT

public:
    explicit RomLinker(QObject *parent = nullptr);

    // Async version — emits progress() and finished()
    void linkAsync(const QByteArray &refRom,
                   const QByteArray &targetRom,
                   const QVector<MapInfo> &maps,
                   const QString &label);

    // Synchronous version (can be called from a thread)
    static RomLinkSession link(const QByteArray &refRom,
                               const QByteArray &targetRom,
                               const QVector<MapInfo> &maps,
                               const QString &label,
                               std::function<void(const QString &, int)> progressCb = {});

signals:
    void progress(const QString &msg, int pct);
    void finished(const RomLinkSession &session);
};
