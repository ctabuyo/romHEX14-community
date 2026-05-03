/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/SimilarityIndex.h"
#include "io/winols/WinOlsConfig.h"
#include "io/ols/OlsImporter.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>
#include <QtConcurrent>

#include <algorithm>

namespace winols {

namespace {

// Read the file and produce a RomFingerprint.  For .ols / .kp containers
// we MUST unwrap to the inner ROM bytes — the same ROM lives in many
// different .ols files (each with its own header, project name, version
// list, map TLVs).  Hashing the raw container bytes makes two .ols files
// containing the SAME ROM look completely different, which defeats the
// whole point of similarity search.
//
// For multi-version .ols (Original + Stage 1 + …), we score against ALL
// versions and store the per-bucket MIN MinHash slot across them — this
// is the sketch of the union of all versions' shingles, and it lets the
// file match if ANY of its versions is similar to the needle.  (The
// alternative — pick versions[0] only — would miss matches against
// tuned variants when the .ols's "Original" version differs from the
// needle.)
//
// Fallback: if the OLS parse fails, hash the raw bytes (better than
// dropping the file entirely; a corrupted .ols still has *some* signal).
RomFingerprint hashOne(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    constexpr qint64 kMaxRead = 64 * 1024 * 1024;     // 64 MB cap (.ols can grow)
    QByteArray fileData = f.read(kMaxRead);
    f.close();
    if (fileData.size() < 4096) return {};

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("ols") || ext == QStringLiteral("kp")) {
        const ols::OlsImportResult res = ols::OlsImporter::importFromBytes(fileData);
        if (res.error.isEmpty() && !res.versions.isEmpty()) {
            // Compute fingerprint for each version and merge bucket-by-bucket
            // (per-bucket min — sketch of union of all versions' shingles).
            RomFingerprint merged;
            merged.wholeFile = QList<quint64>(kMinHashK, 0);
            merged.dataArea  = QList<quint64>(kMinHashK, 0);
            qint64 maxScanned = 0;
            int versionsScored = 0;
            for (const auto &v : res.versions) {
                if (v.romData.size() < kShingleSize) continue;
                const RomFingerprint vfp = fingerprint(QByteArrayView(v.romData));
                if (vfp.isEmpty()) continue;
                ++versionsScored;
                maxScanned = std::max(maxScanned, vfp.bytesScanned);
                for (int i = 0; i < kMinHashK; ++i) {
                    if (i < vfp.wholeFile.size()) {
                        const quint64 hh = vfp.wholeFile[i];
                        if (hh != 0 && (merged.wholeFile[i] == 0 ||
                                        hh < merged.wholeFile[i]))
                            merged.wholeFile[i] = hh;
                    }
                    if (i < vfp.dataArea.size()) {
                        const quint64 hh = vfp.dataArea[i];
                        if (hh != 0 && (merged.dataArea[i] == 0 ||
                                        hh < merged.dataArea[i]))
                            merged.dataArea[i] = hh;
                    }
                }
            }
            if (versionsScored > 0) {
                merged.bytesScanned = maxScanned;
                return merged;
            }
        }
        // Parse failed — fall through to raw-bytes fingerprint.
    }
    return fingerprint(QByteArrayView(fileData));
}

}  // namespace

SimilarityIndex::SimilarityIndex(QObject *parent)
    : QObject(parent),
      m_connectionName(QStringLiteral("similarity_idx_") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

SimilarityIndex::~SimilarityIndex() { close(); }

bool SimilarityIndex::open(QString *err)
{
    close();
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (err) *err = QStringLiteral("Qt SQLITE driver missing");
        return false;
    }
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath();
    QDir().mkpath(base);
    m_dbPath = QDir(base).filePath(QStringLiteral("similarity_index.db"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        if (err) *err = QStringLiteral("open failed: %1")
                            .arg(db.lastError().text());
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    if (!createSchemaIfNeeded(db, err)) {
        db.close();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    m_open = true;
    return true;
}

void SimilarityIndex::close()
{
    if (!m_open) return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) db.close();
    }  // db copy goes out of scope before removeDatabase
    QSqlDatabase::removeDatabase(m_connectionName);
    m_open = false;
}

bool SimilarityIndex::createSchemaIfNeeded(QSqlDatabase &db, QString *err) const
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS files ("
            "  path          TEXT PRIMARY KEY,"
            "  size          INTEGER,"
            "  mtime         INTEGER,"
            "  bytes_scanned INTEGER,"
            "  blob          BLOB)"))) {
        if (err) *err = q.lastError().text();
        return false;
    }
    // Speed-up & journal mode for batched writes.
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_size "
                          "ON files(size)"));
    return true;
}

int SimilarityIndex::rowCount() const
{
    if (!m_open) return 0;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM files")) && q.next())
        return q.value(0).toInt();
    return 0;
}

void SimilarityIndex::upsert(QSqlDatabase &db, const IndexedFile &f) const
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO files(path,size,mtime,bytes_scanned,blob) "
        "VALUES(:p,:s,:m,:b,:blob) "
        "ON CONFLICT(path) DO UPDATE SET "
        "  size=:s, mtime=:m, bytes_scanned=:b, blob=:blob"));
    q.bindValue(QStringLiteral(":p"),    f.path);
    q.bindValue(QStringLiteral(":s"),    f.size);
    q.bindValue(QStringLiteral(":m"),    f.mtime);
    q.bindValue(QStringLiteral(":b"),    f.bytesScanned);
    q.bindValue(QStringLiteral(":blob"), f.fp.toBlob());
    q.exec();
}

QStringList SimilarityIndex::enumerate(const QStringList &roots)
{
    static const QStringList kSuffixes = {
        QStringLiteral("*.ols"), QStringLiteral("*.kp"),
        QStringLiteral("*.bin"), QStringLiteral("*.rom"),
        QStringLiteral("*.ori"),
    };
    QStringList out;
    for (const QString &r : roots) {
        QDir d(r);
        if (!d.exists()) continue;
        QDirIterator it(r, kSuffixes, QDir::Files | QDir::Readable,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) out << it.next();
    }
    return out;
}

void SimilarityIndex::rebuild(const QStringList &roots)
{
    if (!m_open) {
        emit rebuildFinished(0, false);
        return;
    }
    m_cancel.store(false);
    m_paused.store(false);

    QElapsedTimer wall;
    wall.start();
    QStringList paths = enumerate(roots);
    const int total = paths.size();
    if (total == 0) {
        emit rebuildFinished(0, false);
        return;
    }

    // Qt's SQLITE driver requires that a connection is used only on
    // the thread that opened it.  rebuild() runs on a worker thread
    // (QtConcurrent::run from BuildIndexProgressDlg), so the
    // connection opened on the main thread (`m_connectionName`) is
    // off-limits here.  Open a private connection scoped to this call.
    const QString workerConn = m_connectionName +
                               QStringLiteral("_worker_") +
                               QString::number(qint64(QThread::currentThreadId()));
    {
        QSqlDatabase wdb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                     workerConn);
        wdb.setDatabaseName(m_dbPath);
        if (!wdb.open()) {
            QSqlDatabase::removeDatabase(workerConn);
            emit rebuildFinished(0, false);
            return;
        }
        QSqlQuery q(wdb);
        q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    }
    QSqlDatabase db = QSqlDatabase::database(workerConn);

    // Purge rows whose blob is NOT the current fingerprint format.
    // When the algorithm changes (RFP1 → RFP2 → RFP3 …) old rows would
    // otherwise be skipped by the (size,mtime) pre-filter below and
    // never re-fingerprinted.  Run-once cost; rows with current magic
    // survive untouched so incremental rebuilds stay cheap.
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "DELETE FROM files WHERE SUBSTR(blob,1,4) != 'RFP3'"))) {
            const int purged = q.numRowsAffected();
            if (purged > 0)
                qWarning("SimilarityIndex: purged %d rows in old fingerprint "
                         "format — they will be re-fingerprinted now.", purged);
        }
    }

    // Pre-load existing path → (size,mtime) into memory for O(1)
    // skip lookup.  ~16 bytes per row, fits 100k+ in <2 MB.
    QHash<QString, QPair<qint64, qint64>> known;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT path,size,mtime FROM files"))) {
            while (q.next())
                known.insert(q.value(0).toString(),
                             {q.value(1).toLongLong(), q.value(2).toLongLong()});
        }
    }

    int processed = 0;
    qint64 totalBytes = 0;
    // Smaller batches mean the user sees `rowCount` climbing in real
    // time (visible in DB tools, find-similar query results, etc.)
    // instead of waiting for the next 200-file commit.  Throughput
    // cost is negligible — SQLite WAL handles small TX cheaply.
    constexpr int kCommitEvery = 50;

    // Pre-filter: split paths into "skip" (already indexed, identical
    // size+mtime) and "fresh" (need fingerprinting).  Doing this up
    // front lets the parallel pool spend 100 % of its time on real
    // work instead of stat+hashmap-lookup serialised through one core.
    QStringList fresh;
    fresh.reserve(paths.size());
    for (const QString &path : paths) {
        QFileInfo fi(path);
        const qint64 size  = fi.size();
        const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
        auto it = known.find(path);
        if (it != known.end()
            && it.value().first == size
            && it.value().second == mtime) {
            ++processed;
            continue;
        }
        fresh.append(path);
    }
    if (processed > 0)
        emit progress(processed, total, totalBytes, wall.elapsed(), QString());

    // Multi-threaded fingerprinting.  hashOne() is pure (no shared state)
    // and dominated by file I/O + Blake3 hashing — both scale almost
    // linearly with cores until disk bandwidth saturates.  We process
    // in chunks so cancel/pause stay responsive (≤ chunk worth of work
    // before checking flags) and so memory stays bounded (chunk × ~16
    // KB per fingerprint, plus the file bytes briefly held by hashOne).
    constexpr int kChunkSize = 64;

    db.transaction();
    int batchInTx = 0;

    for (int i = 0; i < fresh.size(); i += kChunkSize) {
        while (m_paused.load()) QThread::msleep(50);
        if (m_cancel.load()) break;

        QStringList chunk = fresh.mid(i, kChunkSize);

        // Fingerprint this chunk in parallel across the global thread
        // pool.  Returns once every result is ready.
        QList<IndexedFile> results =
            QtConcurrent::blockingMapped<QList<IndexedFile>>(
                chunk,
                [](const QString &path) -> IndexedFile {
                    IndexedFile f;
                    f.path  = path;
                    QFileInfo fi(path);
                    f.size  = fi.size();
                    f.mtime = fi.lastModified().toSecsSinceEpoch();
                    f.fp    = hashOne(path);
                    f.bytesScanned = f.fp.bytesScanned;
                    return f;
                });

        // Serial SQLite write.  Per-file commits would dominate, so we
        // keep one open transaction and commit every kCommitEvery
        // upserts to bound recovery on crash.
        for (const IndexedFile &f : results) {
            if (!f.fp.isEmpty()) {
                upsert(db, f);
                ++batchInTx;
            }
            ++processed;
            totalBytes += f.size;
            if (batchInTx >= kCommitEvery) {
                db.commit();
                db.transaction();
                batchInTx = 0;
            }
            emit progress(processed, total, totalBytes, wall.elapsed(), f.path);
        }
    }
    db.commit();
    emit progress(processed, total, totalBytes, wall.elapsed(), QString());
    // Close + drop the worker-thread connection before returning so
    // the next rebuild() call can re-create it cleanly.
    db.close();
    QSqlDatabase::removeDatabase(workerConn);
    emit rebuildFinished(processed, m_cancel.load());
}

QVector<SimilarityMatch> SimilarityIndex::findSimilar(
    const RomFingerprint &needle, int minPercent, int limit) const
{
    QVector<SimilarityMatch> out;
    if (!m_open || needle.isEmpty()) return out;

    const bool onWorker = (QThread::currentThread() != qApp->thread());
    const QString connName = onWorker
        ? m_connectionName + QStringLiteral("_query_") +
          QString::number(reinterpret_cast<quintptr>(QThread::currentThread()), 16)
        : m_connectionName;

    {
        QSqlDatabase db;
        if (onWorker) {
            db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(m_dbPath);
            if (!db.open()) {
                QSqlDatabase::removeDatabase(connName);
                return out;
            }
        } else {
            db = QSqlDatabase::database(m_connectionName);
        }

        QSqlQuery q(db);
        q.setForwardOnly(true);
        q.exec(QStringLiteral("SELECT path,size,blob FROM files"));

        const double threshold = double(minPercent) / 100.0;
        while (q.next()) {
            const QByteArray blob = q.value(2).toByteArray();
            if (blob.isEmpty()) continue;
            const RomFingerprint fp = RomFingerprint::fromBlob(blob);
            const SimilarityScore s = winols::similarity(needle, fp);
            if (s.wholeFile < threshold) continue;
            SimilarityMatch m;
            m.path  = q.value(0).toString();
            m.size  = q.value(1).toLongLong();
            m.score = s;
            out.append(m);
        }
        if (onWorker) db.close();
    }

    if (onWorker)
        QSqlDatabase::removeDatabase(connName);

    std::sort(out.begin(), out.end(),
              [](const SimilarityMatch &a, const SimilarityMatch &b) {
                  const double ra = 3 * a.score.wholeFile + a.score.dataArea;
                  const double rb = 3 * b.score.wholeFile + b.score.dataArea;
                  return ra > rb;
              });
    if (out.size() > limit) out.resize(limit);
    return out;
}

IndexedFile SimilarityIndex::lookup(const QString &path) const
{
    IndexedFile f;
    if (!m_open) return f;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT path,size,mtime,bytes_scanned,blob FROM files "
        "WHERE path=:p"));
    q.bindValue(QStringLiteral(":p"), path);
    if (q.exec() && q.next()) {
        f.path  = q.value(0).toString();
        f.size  = q.value(1).toLongLong();
        f.mtime = q.value(2).toLongLong();
        f.bytesScanned = q.value(3).toLongLong();
        f.fp    = RomFingerprint::fromBlob(q.value(4).toByteArray());
    }
    return f;
}

}  // namespace winols
