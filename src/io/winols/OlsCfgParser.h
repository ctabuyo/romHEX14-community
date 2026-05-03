/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Best-effort extractor for the directory list embedded in WinOLS'
 * `ols.cfg` (located at `%AppData%/Evc/WinOLS.64Bit/ols.cfg`).
 *
 * ols.cfg is a binary configuration blob with no published schema,
 * but the user-visible directory list ("Choose different path…") is
 * stored as plain ASCII embedded between binary fields.  We scan for
 * length-prefixed ASCII runs that look like absolute paths
 * (`[A-Z]:\…`), strip trailing binary noise, keep only entries that
 * resolve to existing directories on the local filesystem, and
 * dedupe.  The result is exactly what the user sees in WinOLS' Path
 * picker — perfect for seeding `WinOlsConfig::scanFallback`.
 *
 * Read-only.  We never write to ols.cfg.
 */

#pragma once

#include <QString>
#include <QStringList>

namespace winols {

class OlsCfgParser {
public:
    /// Default location: `%AppData%/Evc/WinOLS.64Bit/ols.cfg`.
    static QString defaultPath();

    /// Parse @p cfgPath (or default location if empty), return a
    /// deduplicated list of *existing* directory paths discovered
    /// inside.  Empty list on read failure or no candidates.
    /// Order preserves first-occurrence in the file.
    static QStringList extractScanRoots(const QString &cfgPath = {});
};

}  // namespace winols
