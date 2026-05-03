/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/WinOlsOpener.h"
#include "io/winols/WinOlsConfig.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace winols {

QString Opener::resolve(const Config &cfg,
                        const QString &dbBasename,
                        const QString &filename,
                        QString *err)
{
    if (filename.isEmpty()) {
        if (err) *err = QStringLiteral("empty filename");
        return {};
    }

    // 1. Direct mapping for this Cache_*.db.
    const auto roots = cfg.fileRoots();
    auto it = roots.constFind(dbBasename);
    if (it != roots.constEnd()) {
        const QString candidate =
            QDir(it.value()).filePath(filename);
        if (QFileInfo::exists(candidate)) return candidate;
        // Mapped but not found at the top level — try one level down.
        QDirIterator scan(it.value(), {filename},
                          QDir::Files,
                          QDirIterator::Subdirectories);
        if (scan.hasNext()) return scan.next();
    }

    // 2. Scan fallback dirs (recursive).
    for (const QString &fallback : cfg.scanFallback()) {
        QDirIterator scan(fallback, {filename},
                          QDir::Files,
                          QDirIterator::Subdirectories);
        if (scan.hasNext()) return scan.next();
    }

    if (err) {
        *err = QStringLiteral(
            "%1 not found.  Map %2 to its source folder in WinOLS "
            "settings, or add the folder to scanFallback.")
            .arg(filename, dbBasename);
    }
    return {};
}

}  // namespace winols
