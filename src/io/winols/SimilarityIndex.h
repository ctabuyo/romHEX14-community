/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * On-disk index of `RomFingerprint`s for every `.ols` / `.kp` / `.bin`
 * found below the user-configured WinOLS roots.  Backed by a single
 * SQLite database in `<AppData>/CT14/romHEX14/similarity_index.db`.
 *
 * One row per file:
 *   path TEXT PRIMARY KEY
 *   size  INT
 *   mtime INT          -- last-modified timestamp, used for skip-on-rescan
 *   bytes_scanned INT  -- pulled from the fingerprint header for fast filter
 *   blob  BLOB         -- RomFingerprint::toBlob()
 *
 * Build is parallelised across CPU cores using `QtConcurrent::blockingMapped`
 * over chunks of 64 paths at a time:
 *   * worker pool reads file bytes + runs `winols::fingerprint()` in parallel
 *   * rebuild thread then upserts each result serially into SQLite
 *   * SQLite writes stay on the rebuild thread (Qt SQLITE driver is not
 *     thread-safe across connections; one writer is also fastest under WAL)
 *
 * Cancel/pause atomics are checked between chunks, so worst-case latency
 * before honouring a stop request is one chunk's worth of fingerprinting
 * (~2-3 s on a typical disk).  SQLite writes are transactional in batches
 * of 50, so even a forced kill leaves the index consistent up to the last
 * commit.  Throughput on a typical 8-core / SATA-SSD setup: ~18 files/s
 * (~7× the serial implementation).  With the bottom-K MinHash fingerprint
 * (~2 KB per row) the on-disk index stays roughly 1 GB even at half a
 * million files, three orders of magnitude smaller than the old "store
 * every window hash" representation.
 */

#pragma once

#include "io/winols/RomFingerprint.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <memory>

class QSqlDatabase;

namespace winols {

struct IndexedFile {
    QString  path;
    qint64   size = 0;
    qint64   mtime = 0;
    qint64   bytesScanned = 0;
    RomFingerprint fp;
};

struct SimilarityMatch {
    QString  path;
    qint64   size = 0;
    SimilarityScore score;
};

class SimilarityIndex : public QObject {
    Q_OBJECT
public:
    explicit SimilarityIndex(QObject *parent = nullptr);
    ~SimilarityIndex() override;

    /// Open (or create) the index database at the default path
    /// (`<AppData>/CT14/romHEX14/similarity_index.db`).  Returns
    /// false on failure with @p err populated.
    bool open(QString *err = nullptr);
    bool isOpen() const { return m_open; }
    void close();

    /// Path to the SQLite file actually in use.  Useful for
    /// "rebuild" / "show in folder" actions in Settings.
    QString dbPath() const { return m_dbPath; }

    /// Number of indexed files currently in the database.
    int  rowCount() const;

    /// Walk every file under @p roots whose extension matches
    /// `.ols / .kp / .bin / .ori / .rom`, compute its fingerprint
    /// (via OlsImporter for .ols/.kp, raw bytes otherwise) and write
    /// it to the database.  Skips files whose `(size, mtime)` already
    /// match a row in the index.  Honours `m_cancel` for early exit.
    /// Emits `progress(processed, total, currentPath, bytesPerSec)`.
    void rebuild(const QStringList &roots);

    /// Cancel an in-flight `rebuild()`.  Safe to call from any thread.
    void requestCancel() { m_cancel.store(true); }
    bool isCancelRequested() const { return m_cancel.load(); }

    /// Pause / resume the rebuild.  Pause blocks the worker pool until
    /// resumed.  Safe to call from any thread.
    void setPaused(bool p) { m_paused.store(p); }
    bool isPaused() const { return m_paused.load(); }

    /// Look up similar files from the index.  Returns up to @p limit
    /// matches whose `wholePct` is at least @p minPercent.  Sorted
    /// descending by combined score (3·whole + data).
    QVector<SimilarityMatch> findSimilar(const RomFingerprint &needle,
                                          int minPercent = 30,
                                          int limit      = 5000) const;

    /// Look up a single row by path.  Empty `IndexedFile` if missing.
    IndexedFile lookup(const QString &path) const;

signals:
    /// Emitted from the rebuild thread.  @p processed counts files
    /// touched (incl. skipped); @p totalBytes is cumulative file
    /// content read; @p elapsedMs is wall time since rebuild began.
    void progress(int processed, int total,
                  qint64 totalBytes, qint64 elapsedMs,
                  const QString &currentPath);
    void rebuildFinished(int processed, bool cancelled);

private:
    QString               m_connectionName;
    QString               m_dbPath;
    bool                  m_open = false;
    std::atomic<bool>     m_cancel{false};
    std::atomic<bool>     m_paused{false};

    bool createSchemaIfNeeded(QSqlDatabase &db, QString *err) const;
    void upsert(QSqlDatabase &db, const IndexedFile &f) const;
    static QStringList enumerate(const QStringList &roots);
};

}  // namespace winols
