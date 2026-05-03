/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/OlsCfgParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace winols {

QString OlsCfgParser::defaultPath()
{
    // %AppData%\Evc\WinOLS.64Bit\ols.cfg — Qt resolves AppData via
    // RoamingLocation on Windows; on macOS / Linux this gracefully
    // points to ~/.config or similar (where Wine WinOLS would also
    // land if installed).
    const QString appData = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    return QDir(appData).filePath(
        QStringLiteral("../Roaming/Evc/WinOLS.64Bit/ols.cfg"));
}

namespace {

// Treat byte as printable ASCII (the run terminator).
inline bool isPrintable(uint8_t b) { return b >= 0x20 && b <= 0x7e; }

// Recognise the start of an absolute Windows path (`X:\` where X is
// any uppercase ASCII letter).  This is what WinOLS embeds for the
// scanned-folder list.
inline bool looksLikeDriveStart(const uint8_t *p, qsizetype n)
{
    return n >= 3 && p[0] >= 'A' && p[0] <= 'Z'
                   && p[1] == ':' && p[2] == '\\';
}

}  // namespace

QStringList OlsCfgParser::extractScanRoots(const QString &cfgPathIn)
{
    const QString cfgPath = cfgPathIn.isEmpty() ? defaultPath() : cfgPathIn;
    QFile f(cfgPath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = f.readAll();
    f.close();
    const auto *p = reinterpret_cast<const uint8_t *>(data.constData());
    const qsizetype n = data.size();

    QStringList out;
    QSet<QString> seen;

    qsizetype i = 0;
    while (i < n - 3) {
        if (!looksLikeDriveStart(p + i, n - i)) { ++i; continue; }
        // Walk forward over printable ASCII bytes.
        qsizetype j = i;
        while (j < n && isPrintable(p[j])) ++j;
        QString s = QString::fromLatin1(reinterpret_cast<const char *>(p + i),
                                        int(j - i));
        i = j;

        // Some entries end with the next field's leading length byte
        // appended (e.g. `Database (1234).olsE`).  Truncate after the
        // last `.ols` if present, else strip any single trailing
        // non-alphanumeric / non-backslash glyph.
        const int dotOls = s.lastIndexOf(QStringLiteral(".ols"),
                                         -1, Qt::CaseInsensitive);
        if (dotOls >= 0) s = s.left(dotOls + 4);
        // Strip a known WinOLS suffix glyph that isn't a valid path
        // character (e.g. `\\…WinOLS\` followed by `0`).
        while (!s.isEmpty()) {
            const QChar c = s.back();
            if (c.isLetterOrNumber() || c == QChar('\\') || c == QChar('/')
                || c == QChar(' ')   || c == QChar('-')   || c == QChar('_')
                || c == QChar('.')   || c == QChar('(')   || c == QChar(')')
                || c == QChar('[')   || c == QChar(']')   || c == QChar('&')
                || c == QChar('\'')) break;
            s.chop(1);
        }
        if (s.size() < 4) continue;

        // Resolve to a directory:
        //   * strip filename if path points at an .ols
        //   * keep candidate if it resolves to an existing directory
        QFileInfo fi(s);
        if (!fi.exists()) {
            // Try as a file → take its parent dir.
            QFileInfo asFile(s);
            if (asFile.suffix().compare(QStringLiteral("ols"),
                                        Qt::CaseInsensitive) == 0) {
                QFileInfo parent(asFile.absolutePath());
                if (parent.exists() && parent.isDir())
                    fi = parent;
                else
                    continue;
            } else {
                continue;
            }
        }
        if (!fi.isDir()) {
            // Path is a file but exists; take its parent dir.
            QFileInfo parent(fi.absolutePath());
            if (!parent.exists() || !parent.isDir()) continue;
            fi = parent;
        }

        const QString abs = fi.absoluteFilePath();
        if (seen.contains(abs)) continue;
        seen.insert(abs);
        out << abs;
    }
    return out;
}

}  // namespace winols
