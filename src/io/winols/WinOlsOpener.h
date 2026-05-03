/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Resolve a WinOLS catalog record's filename (`col30`) to a real
 * `.ols` / `.kp` path on disk, using the user-configured roots.
 *
 * The catalog stores only the basename (e.g. `Heinz_101.OLS`).
 * The directory for that file is implicit:
 *   1. mapped explicitly through `Config::fileRoots()` (preferred)
 *   2. searched recursively through `Config::scanFallback()` dirs
 * On failure returns an empty path and (optionally) a diagnostic.
 */

#pragma once

#include <QString>

namespace winols {

class Config;

class Opener {
public:
    /// @p dbBasename — name of the source `Cache_*.db` (used as key
    ///                 in `Config::fileRoots`).
    /// @p filename   — the value of the record's col30, e.g. `Heinz_101.OLS`.
    /// Returns an absolute path to the .ols / .kp on disk, or empty if
    /// not found.  Sets @p err with a human-readable explanation when
    /// resolution fails.
    static QString resolve(const Config &cfg,
                           const QString &dbBasename,
                           const QString &filename,
                           QString *err = nullptr);
};

}  // namespace winols
