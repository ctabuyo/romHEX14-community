/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Read-only adapter for WinOLS Cache_*.db SQLite catalogs.
 * ========================================================
 *
 * WinOLS keeps a per-folder project index in
 *   C:\ProgramData\Evc\WinOLS.64Bit\Local.db\Cache_<source>_<id>.db
 *
 * Each cache file is a plain SQLite database with one big table `dir`
 * holding columns `id, allvalues, col0..col82, colx1..colx6`.
 * Column meaning is positional and inferred from data — WinOLS never
 * names them.  This reader exposes a typed `Record` projection over the
 * known-useful subset.
 *
 * Hard guarantees
 * ---------------
 * 1. The catalog is opened with `QSQLITE_OPEN_READONLY` plus the URI
 *    flag `?immutable=1`.  We never issue INSERT / UPDATE / DELETE.
 *    `WinOLS` can keep the file open for writes from its own process —
 *    `immutable=1` lets SQLite skip locking entirely (we trade a tiny
 *    risk of stale reads against zero chance of contention).
 * 2. Text columns are decoded defensively.  WinOLS writes
 *    Windows-1252 / cp1250 (e.g. German "Binärdatei").  We try
 *    UTF-8 → CP1250 → Latin-1 and fall back to a replace-mode UTF-8
 *    decode on the bytes if all three fail.
 * 3. The reader sniffs which `colN` columns actually exist before
 *    issuing the projection — older WinOLS versions had a smaller
 *    schema.
 */

#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class QSqlDatabase;
class QSqlQuery;

namespace winols {

struct CatalogRecord {
    qint64    id            = 0;
    QString   filename;        // col30 — name of the .ols on disk
    QString   vehicleType;     // col1
    QString   make;            // col2
    QString   model;           // col3
    QString   engine;          // col4
    QString   year;            // col5
    QString   power;           // col11
    QString   module;          // col14
    QString   memoryType;      // col15
    QString   ecuMake;         // col16
    QString   ecuModel;        // col17
    QString   swNumber;        // col18
    QString   hwNumber;        // col19
    QString   winolsNumber;    // col20
    qint64    fileSize       = 0; // col22
    QDateTime createdAt;       // col28 (Unix epoch when sane)
    QDateTime modifiedAt;      // col29 (Unix epoch)
    QString   versionsInfo;    // col33 — multi-line, e.g. "1 (Original, ...)"
    QString   user;            // col38
    QString   fileType;        // col53 — "Komplette Binärdatei" / "Partiell"
    QString   language;        // col59
    QString   searchText;      // allvalues — full-text-friendly haystack

    bool isValid() const { return id > 0; }
};

class WinOlsCatalogReader {
public:
    WinOlsCatalogReader();
    ~WinOlsCatalogReader();

    /// Open a `Cache_*.db` read-only.  Returns false if the file does
    /// not exist, the SQLite driver is missing, or the schema does not
    /// look like a WinOLS catalog (no `dir` table).  Sets `*err`.
    bool open(const QString &dbPath, QString *err = nullptr);
    void close();
    bool isOpen() const { return m_open; }

    /// Source database file path passed to open().
    QString path() const { return m_path; }

    /// `LastMod2` from the auxiliary `nosql_int` table.  WinOLS bumps
    /// this whenever it rewrites the catalog.  Returns 0 if missing.
    qint64 lastMod() const { return m_lastMod; }

    /// Total number of records in the `dir` table.
    int rowCount() const { return m_rowCount; }

    /// Read every record.  Cheap on small DBs; on large DBs prefer
    /// `readRange()` + paging.
    QVector<CatalogRecord> readAll() const;

    /// Read records `[offset, offset+limit)` ordered by `id`.
    QVector<CatalogRecord> readRange(int offset, int limit) const;

    /// Lookup a single record by id.
    CatalogRecord readOne(qint64 id) const;

    /// Available `colN` column names actually present in this DB.
    /// Used to short-circuit unknown columns in older catalogs.
    const QStringList &availableColumns() const { return m_columns; }

    /// One-shot helper: open, list every record, close.  Returns
    /// empty on failure (and sets *err).
    static QVector<CatalogRecord> dumpAll(const QString &dbPath,
                                          QString *err = nullptr);

private:
    QString  m_connectionName;
    QString  m_path;
    bool     m_open = false;
    qint64   m_lastMod = 0;
    int      m_rowCount = 0;
    QStringList m_columns;

    /// Build SELECT projection that only references columns that exist.
    QString  buildSelect() const;
    /// Fill a CatalogRecord from a query row.  `colIndex` maps column
    /// name → index within the SELECT list.
    CatalogRecord rowToRecord(const QSqlQuery &q,
                              const QHash<QString, int> &colIndex) const;
};

}  // namespace winols
