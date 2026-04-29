/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>

// ── Logger ───────────────────────────────────────────────────────────────────
// Thread-safe file + stderr logger.
// Usage: Logger::instance().log(Logger::Info, "msg");
//        LOG_INFO("something happened");
// ─────────────────────────────────────────────────────────────────────────────

class Logger {
public:
    enum Level { Debug, Info, Warning, Error, Critical };

    static Logger &instance();

    void init(const QString &path);          // call once at startup
    void log(Level level, const QString &msg,
             const char *file = nullptr, int line = 0);

    // Low-level: safe to call from signal handler (no Qt, no malloc)
    void writeCrashLine(const char *msg) noexcept;

    QString logPath() const { return m_path; }

private:
    Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    QMutex  m_mutex;
    QFile   m_file;
    QString m_path;
    bool    m_ready = false;
};

// ── Convenience macros ───────────────────────────────────────────────────────
#define LOG_DEBUG(msg)    Logger::instance().log(Logger::Debug,    (msg), __FILE__, __LINE__)
#define LOG_INFO(msg)     Logger::instance().log(Logger::Info,     (msg), __FILE__, __LINE__)
#define LOG_WARN(msg)     Logger::instance().log(Logger::Warning,  (msg), __FILE__, __LINE__)
#define LOG_ERROR(msg)    Logger::instance().log(Logger::Error,    (msg), __FILE__, __LINE__)
#define LOG_CRITICAL(msg) Logger::instance().log(Logger::Critical, (msg), __FILE__, __LINE__)
