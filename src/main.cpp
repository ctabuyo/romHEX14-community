/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDateTime>
#include <QSharedMemory>
#include <QRegularExpression>
#include <csignal>
#include <cstdio>
#include <cstring>
#include "mainwindow.h"
#include "logger.h"
#include "version.h"
#include "uiwidgets.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

// ── Qt message handler ───────────────────────────────────────────────────────
static void qtMessageHandler(QtMsgType type, const QMessageLogContext &ctx,
                              const QString &msg)
{
    Logger::Level level;
    switch (type) {
    case QtDebugMsg:    level = Logger::Debug;    break;
    case QtInfoMsg:     level = Logger::Info;     break;
    case QtWarningMsg:  level = Logger::Warning;  break;
    case QtCriticalMsg: level = Logger::Error;    break;
    case QtFatalMsg:    level = Logger::Critical; break;
    default:            level = Logger::Info;     break;
    }
    QString cat = ctx.category ? QString::fromLatin1(ctx.category) : QString();
    Logger::instance().log(level, cat, msg, ctx.file, ctx.line);

    if (type == QtFatalMsg) {
        Logger::instance().writeCrashLine("QtFatalMsg received — aborting");
        std::abort();
    }
}

// ── POSIX / Windows signal handler ───────────────────────────────────────────
static void crashHandler(int sig) noexcept
{
    char buf[128];
    const char *name = "UNKNOWN";
    if      (sig == SIGSEGV)  name = "SIGSEGV (segmentation fault)";
    else if (sig == SIGABRT)  name = "SIGABRT (abort)";
    else if (sig == SIGFPE)   name = "SIGFPE (floating-point exception)";
    else if (sig == SIGILL)   name = "SIGILL (illegal instruction)";

#ifdef _WIN32
    snprintf(buf, sizeof(buf), "Signal %d: %s", sig, name);
#else
    snprintf(buf, sizeof(buf), "Signal %d: %s", sig, name);
#endif

    Logger::instance().writeCrashLine(buf);
    Logger::instance().writeCrashLine("Stack trace not available on this platform.");
    Logger::instance().writeCrashLine("Check the log file for the last operations before the crash.");

    // Re-raise to get the default OS crash handler (generates minidump on Windows)
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

#ifdef Q_OS_WIN
// Windows Structured Exception Filter — catches access violations, stack overflows, etc.
static LONG WINAPI windowsExceptionFilter(EXCEPTION_POINTERS *ep)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Unhandled Windows exception code=0x%08lX at addr=%p",
             (unsigned long)ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress);
    Logger::instance().writeCrashLine(buf);
    Logger::instance().writeCrashLine(
        "The application encountered a fatal error. "
        "A crash report has been written to the log file.");
    return EXCEPTION_CONTINUE_SEARCH;  // let default handler run (generates minidump)
}
#endif


// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    // ── Logger ─────────────────────────────────────────────────────────────
    // Must be initialised before QApplication so it can catch early Qt messages.
    {
#ifdef RX14_DEBUG_RPC
        // Debug build: log to D:\rx14-debug so the dev workspace doesn't fill
        // up the C: drive.  The directory is created on first write.
        QString logPath = QStringLiteral("D:/rx14-debug/rx14.log");
#else
        QString logDir = QStandardPaths::writableLocation(
                             QStandardPaths::AppDataLocation);
        // QStandardPaths needs org/app set first — set them manually here
        // since QApplication isn't constructed yet.
        QString logPath = logDir + "/CT14/romHEX14/rx14.log";
        // Fallback: write next to exe if AppData not available
        if (logDir.isEmpty()) logPath = "rx14.log";
#endif
        Logger::instance().init(logPath);
    }

    // ── Signal / exception handlers ────────────────────────────────────────
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGFPE,  crashHandler);
    std::signal(SIGILL,  crashHandler);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(windowsExceptionFilter);
#endif

    // ── Qt message handler ─────────────────────────────────────────────────
    qInstallMessageHandler(qtMessageHandler);


    // ── Application ────────────────────────────────────────────────────────
    QApplication app(argc, argv);
    app.setApplicationName("romHEX14");
    app.setOrganizationName("CT14");
    app.setApplicationVersion(RX14_VERSION_STRING);

    // ── Single instance check ───────────────────────────────────────────
    QSharedMemory singleLock("romHEX14_SingleInstance_Lock");
    if (!singleLock.create(1)) {
        // Another instance is running — bring it to front on Windows
#ifdef Q_OS_WIN
        HWND existing = FindWindowW(nullptr, L"romHEX 14");
        if (existing) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
#endif
        return 0;
    }

    // Re-init logger now that QStandardPaths works properly
    {
#ifdef RX14_DEBUG_RPC
        // Debug build keeps the D:\rx14-debug path set above.
        QString logPath = QStringLiteral("D:/rx14-debug/rx14.log");
#else
        QString logPath = QStandardPaths::writableLocation(
                              QStandardPaths::AppDataLocation) + "/rx14.log";
#endif
        Logger::instance().init(logPath);
    }

    LOG_INFO(QString("romHEX14 v%1 starting on %2")
             .arg(app.applicationVersion())
             .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

    // ── Stylesheet ─────────────────────────────────────────────────────────
    QFile qss(":/style.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(qss.readAll());
        qss.close();
#ifdef Q_OS_MAC
        // On macOS, strip the custom QComboBox dark-theme styles so that
        // native Aqua rendering is used instead.  The markers in the QSS
        // delimit the block to remove.
        static const QRegularExpression comboRe(
            QStringLiteral("/\\* BEGIN_COMBOBOX_STYLE.*?END_COMBOBOX_STYLE \\*/"),
            QRegularExpression::DotMatchesEverythingOption);
        styleSheet.remove(comboRe);

        // ── macOS ergonomics pass ───────────────────────────────────────────
        // Qt's default metrics are tuned for Windows, where form controls
        // sit at ~20px tall. On macOS HiDPI that renders tiny, squished
        // inputs next to native chrome. This supplement is appended AFTER
        // the base sheet so it wins the cascade, and only applies on macOS.
        styleSheet += QStringLiteral(R"(
            QLineEdit {
                min-height: 26px;
                padding: 6px 10px;
                font-size: 11pt;
            }
            QPlainTextEdit, QTextEdit, QTextBrowser {
                padding: 6px 10px;
                font-size: 11pt;
            }
            QSpinBox, QDoubleSpinBox, QDateEdit, QDateTimeEdit, QTimeEdit {
                min-height: 26px;
                padding: 4px 8px;
                font-size: 11pt;
            }
            QComboBox {
                min-height: 24px;
                font-size: 11pt;
            }
            QCheckBox, QRadioButton {
                spacing: 8px;
                font-size: 11pt;
            }
            QCheckBox::indicator, QRadioButton::indicator {
                width: 18px;
                height: 18px;
            }
            QLabel { font-size: 11pt; }
            QDialog QLabel { padding: 2px 0; }
            QDialog QFormLayout { spacing: 10px; }
        )");
#endif
        // Append the design-system object styles so #primary / #destructive /
        // #flat buttons, [role="card"] frames, and [role="pill"] labels are
        // styled app-wide via a single source of truth (src/uiwidgets.cpp).
        styleSheet += Theme::objectStyles();
        app.setStyleSheet(styleSheet);
    }

    app.setWindowIcon(QIcon(":/icon.png"));

    // ── Main window ────────────────────────────────────────────────────────
    int exitCode = 0;
    try {
        MainWindow w;
        w.setWindowIcon(QIcon(":/icon.png"));
        w.setWindowTitle("romHEX 14 Community");
        w.resize(1400, 850);
        w.show();

        LOG_INFO("Main window shown — entering event loop");
        exitCode = app.exec();
        LOG_INFO(QString("Event loop exited with code %1").arg(exitCode));

    } catch (const std::exception &ex) {
        LOG_CRITICAL(QString("Unhandled std::exception in main: %1").arg(ex.what()));
        QMessageBox::critical(
            nullptr,
            "Fatal Error",
            QString("romHEX14 encountered an unrecoverable error:\n\n%1\n\n"
                    "A crash report has been saved to:\n%2")
                .arg(ex.what())
                .arg(Logger::instance().logPath()));
        exitCode = 1;
    } catch (...) {
        LOG_CRITICAL("Unhandled unknown exception in main");
        QMessageBox::critical(
            nullptr,
            "Fatal Error",
            "romHEX14 encountered an unknown fatal error.\n\n"
            "A crash report has been saved to:\n" + Logger::instance().logPath());
        exitCode = 1;
    }

    return exitCode;
}
