/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/WinOlsCatalogReader.h"

#include <QFileInfo>
#include <QTimeZone>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringConverter>
#include <QUuid>

namespace winols {

namespace {

// Defensive decode of a TEXT cell.  WinOLS writes Windows-1252 /
// CP1250.  Try UTF-8 first (fails fast on non-ASCII bytes that aren't
// valid UTF-8), then CP1250, then Latin-1 as a last-resort byte->char
// passthrough.  Never throws.
QString decodeText(const QVariant &v)
{
    if (v.isNull()) return {};

    // Fast path: Qt's SQLITE driver sometimes returns QString already.
    if (v.typeId() == QMetaType::QString) {
        const QString s = v.toString();
        // If the string round-trips cleanly and has no replacement
        // chars, accept it.  Otherwise re-decode from the underlying
        // bytes.
        if (!s.contains(QChar(0xFFFD))) return s;
    }

    QByteArray ba = v.toByteArray();
    if (ba.isEmpty()) return {};

    auto tryDecode = [&](QStringConverter::Encoding enc) -> QString {
        QStringDecoder d(enc, QStringConverter::Flag::Stateless);
        QString s = d.decode(ba);
        return d.hasError() ? QString() : s;
    };

    QString out = tryDecode(QStringConverter::Utf8);
    if (!out.isEmpty()) return out;

    // No CP1250 in QStringConverter — fall back to QTextCodec replacement
    // by manually mapping high bytes to Latin-1.  Good enough for the
    // diacritics WinOLS writes (Western + Polish overlap heavily on the
    // common range; for genuine cp1250 sequences we'd need ICU).
    QString latin = QString::fromLatin1(ba);
    return latin;
}

qint64 toInt64(const QVariant &v)
{
    if (v.isNull()) return 0;
    bool ok = false;
    qint64 n = v.toLongLong(&ok);
    return ok ? n : 0;
}

QDateTime toDateTime(const QVariant &v)
{
    qint64 ts = toInt64(v);
    if (ts <= 0) return {};
    // WinOLS stores Unix epoch in col29 (modified).  col28 sometimes
    // holds an offset-style small integer that is not a valid epoch
    // (e.g. 39600 seconds = 11h, looks like a day-of fragment).  Reject
    // anything before 2000-01-01 and after 2200-01-01 — both indicate
    // the field is not a real timestamp.
    constexpr qint64 kMin = 946684800;        // 2000-01-01 UTC
    constexpr qint64 kMax = 7258118400;       // ~2200-01-01 UTC
    if (ts < kMin || ts > kMax) return {};
    return QDateTime::fromSecsSinceEpoch(ts, QTimeZone::UTC);
}

}  // namespace

WinOlsCatalogReader::WinOlsCatalogReader()
    : m_connectionName(QStringLiteral("winols_cat_") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

WinOlsCatalogReader::~WinOlsCatalogReader()
{
    close();
}

bool WinOlsCatalogReader::open(const QString &dbPath, QString *err)
{
    close();

    QFileInfo fi(dbPath);
    if (!fi.exists() || !fi.isReadable()) {
        if (err) *err = QStringLiteral("file not readable: %1").arg(dbPath);
        return false;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (err) *err = QStringLiteral("Qt SQLITE driver missing");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                m_connectionName);
    // Read-only + immutable=1: skip locking, never write.  immutable=1
    // is a SQLite URI flag that tells the engine the file will not
    // change while we're open — we open it for short bursts so this
    // is safe even when WinOLS itself is running in the background.
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;"
                                        "QSQLITE_OPEN_URI"));
    db.setDatabaseName(QStringLiteral("file:%1?mode=ro&immutable=1")
                           .arg(fi.absoluteFilePath()));
    if (!db.open()) {
        if (err) *err = QStringLiteral("open failed: %1")
                            .arg(db.lastError().text());
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    // Verify schema: table `dir` must exist.
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT name FROM sqlite_master "
                              "WHERE type='table' AND name='dir'"));
        if (!q.next()) {
            if (err) *err = QStringLiteral("not a WinOLS catalog "
                                           "(no 'dir' table)");
            db.close();
            QSqlDatabase::removeDatabase(m_connectionName);
            return false;
        }
    }

    // Sniff available columns from PRAGMA table_info('dir').
    m_columns.clear();
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA table_info('dir')"));
        while (q.next()) {
            const QString name = q.value(1).toString();
            m_columns.append(name);
        }
    }

    // Row count.
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT COUNT(*) FROM dir")) && q.next())
            m_rowCount = q.value(0).toInt();
    }

    // LastMod2 from nosql_int (auxiliary refresh-tracker table).
    m_lastMod = 0;
    {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT value FROM nosql_int "
                                  "WHERE id='LastMod2'")) && q.next())
            m_lastMod = q.value(0).toLongLong();
    }

    m_path = dbPath;
    m_open = true;
    return true;
}

void WinOlsCatalogReader::close()
{
    if (m_open) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
        m_open = false;
        m_columns.clear();
        m_rowCount = 0;
        m_lastMod = 0;
        m_path.clear();
    }
}

QString WinOlsCatalogReader::buildSelect() const
{
    // Pick columns we know how to interpret, but only if they exist.
    static const QStringList kWanted = {
        QStringLiteral("id"),
        QStringLiteral("allvalues"),
        QStringLiteral("col1"),  QStringLiteral("col2"),
        QStringLiteral("col3"),  QStringLiteral("col4"),
        QStringLiteral("col5"),  QStringLiteral("col11"),
        QStringLiteral("col14"), QStringLiteral("col15"),
        QStringLiteral("col16"), QStringLiteral("col17"),
        QStringLiteral("col18"), QStringLiteral("col19"),
        QStringLiteral("col20"), QStringLiteral("col22"),
        QStringLiteral("col28"), QStringLiteral("col29"),
        QStringLiteral("col30"), QStringLiteral("col33"),
        QStringLiteral("col38"), QStringLiteral("col53"),
        QStringLiteral("col59"),
    };
    QStringList present;
    present.reserve(kWanted.size());
    for (const auto &c : kWanted)
        if (m_columns.contains(c)) present << c;
    return QStringLiteral("SELECT %1 FROM dir").arg(present.join(','));
}

CatalogRecord WinOlsCatalogReader::rowToRecord(
    const QSqlQuery &q, const QHash<QString, int> &idx) const
{
    CatalogRecord r;
    auto pick = [&](const char *name) -> QVariant {
        const auto it = idx.find(QString::fromLatin1(name));
        return it == idx.end() ? QVariant() : q.value(*it);
    };
    r.id           = toInt64(pick("id"));
    r.searchText   = decodeText(pick("allvalues"));
    r.vehicleType  = decodeText(pick("col1"));
    r.make         = decodeText(pick("col2"));
    r.model        = decodeText(pick("col3"));
    r.engine       = decodeText(pick("col4"));
    r.year         = decodeText(pick("col5"));
    r.power        = decodeText(pick("col11"));
    r.module       = decodeText(pick("col14"));
    r.memoryType   = decodeText(pick("col15"));
    r.ecuMake      = decodeText(pick("col16"));
    r.ecuModel     = decodeText(pick("col17"));
    r.swNumber     = decodeText(pick("col18"));
    r.hwNumber     = decodeText(pick("col19"));
    r.winolsNumber = decodeText(pick("col20"));
    r.fileSize     = toInt64(pick("col22"));
    r.createdAt    = toDateTime(pick("col28"));
    r.modifiedAt   = toDateTime(pick("col29"));
    r.filename     = decodeText(pick("col30"));
    r.versionsInfo = decodeText(pick("col33"));
    r.user         = decodeText(pick("col38"));
    r.fileType     = decodeText(pick("col53"));
    r.language     = decodeText(pick("col59"));
    return r;
}

QVector<CatalogRecord> WinOlsCatalogReader::readAll() const
{
    return readRange(0, -1);
}

QVector<CatalogRecord> WinOlsCatalogReader::readRange(int offset,
                                                      int limit) const
{
    QVector<CatalogRecord> out;
    if (!m_open) return out;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.setForwardOnly(true);

    QString sql = buildSelect() + QStringLiteral(" ORDER BY id");
    if (limit > 0)
        sql += QStringLiteral(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
    if (!q.exec(sql)) return out;

    // Build column-name → index map once for the whole result set.
    QHash<QString, int> idx;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i)
        idx.insert(rec.fieldName(i), i);

    out.reserve(limit > 0 ? limit : m_rowCount);
    while (q.next())
        out.append(rowToRecord(q, idx));
    return out;
}

CatalogRecord WinOlsCatalogReader::readOne(qint64 id) const
{
    if (!m_open) return {};
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.setForwardOnly(true);
    QString sql = buildSelect() + QStringLiteral(" WHERE id=:id LIMIT 1");
    q.prepare(sql);
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec() || !q.next()) return {};

    QHash<QString, int> idx;
    const QSqlRecord rec = q.record();
    for (int i = 0; i < rec.count(); ++i)
        idx.insert(rec.fieldName(i), i);

    return rowToRecord(q, idx);
}

QVector<CatalogRecord> WinOlsCatalogReader::dumpAll(const QString &dbPath,
                                                    QString *err)
{
    WinOlsCatalogReader r;
    if (!r.open(dbPath, err)) return {};
    return r.readAll();
}

}  // namespace winols
