/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "logger.h"
#include <QMutexLocker>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <cstdio>

Logger &Logger::instance()
{
    static Logger s;
    return s;
}

void Logger::init(const QString &path)
{
    QMutexLocker lk(&m_mutex);
    m_path = path;

    QDir().mkpath(QFileInfo(path).absolutePath());

    // Rotate: truncate if > 5 MB
    QFileInfo fi(path);
    if (fi.exists() && fi.size() > 5 * 1024 * 1024) {
        QFile::remove(path + ".old");
        QFile::rename(path, path + ".old");
    }

    if (m_file.isOpen())
        m_file.close();
    m_file.setFileName(path);
    if (m_file.open(QIODevice::Append | QIODevice::Text)) {
        m_ready = true;
        QTextStream ts(&m_file);
        ts << "\n──────────────────────────────────────────────\n"
           << "Session started: "
           << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
           << "──────────────────────────────────────────────\n";
        m_file.flush();
    }
}

void Logger::log(Level level, const QString &msg, const char *file, int line)
{
    static const char *prefixes[] = { "DBG", "INF", "WRN", "ERR", "CRT" };
    const char *pfx = (level >= 0 && level <= Critical) ? prefixes[level] : "???";

    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString loc;
    if (file && line > 0) {
        // Strip full path down to filename only
        QString fname = QString::fromUtf8(file);
        int slash = qMax(fname.lastIndexOf('/'), fname.lastIndexOf('\\'));
        if (slash >= 0) fname = fname.mid(slash + 1);
        loc = QString(" [%1:%2]").arg(fname).arg(line);
    }

    QString line_str = QString("[%1][%2]%3  %4\n").arg(ts).arg(pfx).arg(loc).arg(msg);

    {
        QMutexLocker lk(&m_mutex);
        if (m_ready) {
            QTextStream ts_(&m_file);
            ts_ << line_str;
            m_file.flush();
        }
    }

    // Also echo to Qt debug output
    switch (level) {
    case Debug:    qDebug().noquote()    << line_str.trimmed(); break;
    case Info:     qInfo().noquote()     << line_str.trimmed(); break;
    case Warning:  qWarning().noquote()  << line_str.trimmed(); break;
    case Error:    qCritical().noquote() << line_str.trimmed(); break;
    case Critical: qCritical().noquote() << line_str.trimmed(); break;
    }
}

// Called from signal handler — no Qt, no dynamic allocation
void Logger::writeCrashLine(const char *msg) noexcept
{
    if (!m_ready) return;
    // Use low-level POSIX write to avoid heap operations
    const char *ts_pfx = "[CRASH] ";
    // We can't use QTextStream here; fall back to stdio FILE*
    FILE *f = fopen(m_path.toLocal8Bit().constData(), "a");
    if (f) {
        fputs(ts_pfx, f);
        fputs(msg, f);
        fputs("\n", f);
        fflush(f);
        fclose(f);
    }
    // Also write to stderr
    fputs(ts_pfx, stderr);
    fputs(msg, stderr);
    fputs("\n", stderr);
    fflush(stderr);
}
