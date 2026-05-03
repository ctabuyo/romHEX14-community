/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * QSettings-backed config for the WinOLS catalog importer.
 *
 * Stored under [winols] section, scope per-user.  All paths are
 * configurable so the importer works on:
 *   * a stock Windows install (default `C:/ProgramData/Evc/...`)
 *   * non-default WinOLS installs (custom Local.db location)
 *   * Linux / macOS users running WinOLS in Wine / a VM
 *   * external NAS-hosted .ols collections
 */

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace winols {

class Config {
public:
    /// One or more directories that contain `Cache_*.db` files.
    /// Default: `C:/ProgramData/Evc/WinOLS.64Bit/Local.db`.
    QStringList dbRoots() const;
    void        setDbRoots(const QStringList &roots);

    /// Per-cache mapping: a Cache_X.db filename → the on-disk
    /// directory whose `.ols` files it indexes.  WinOLS keeps this
    /// mapping in `ols.cfg` (binary), but it is brittle to read, so
    /// we expose it for explicit user setup.
    /// Key: Cache_*.db basename only (e.g. "Cache_Heinz_8.db").
    /// Value: absolute directory path.
    QHash<QString, QString> fileRoots() const;
    void                    setFileRoots(const QHash<QString, QString> &);
    void                    setFileRoot(const QString &dbBasename,
                                        const QString &dirPath);

    /// Extra directories to scan recursively if a record's filename
    /// is not found under its mapped fileRoot.  Last-resort lookup.
    QStringList scanFallback() const;
    void        setScanFallback(const QStringList &dirs);

    /// Encoding fallback chain for defensive text decoding.  Default:
    /// ["UTF-8", "Windows-1252", "ISO-8859-1"].  Honoured by the
    /// catalog reader.
    QStringList encodingChain() const;
    void        setEncodingChain(const QStringList &);

    /// Where to keep the local mirror index DB.  Default:
    /// `<AppDataLocation>/CT14/romHEX14/winols_index.db`.
    QString indexDbPath() const;
    void    setIndexDbPath(const QString &path);

    /// Discover all `Cache_*.db` paths beneath every dbRoot.
    QStringList discoverCacheDbs() const;
};

}  // namespace winols
