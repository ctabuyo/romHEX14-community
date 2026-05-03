/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/WinOlsConfig.h"

#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include <QStandardPaths>

namespace winols {

namespace {

QString defaultDbRoot()
{
    return QStringLiteral("C:/ProgramData/Evc/WinOLS.64Bit/Local.db");
}

QString defaultIndexDb()
{
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (base.isEmpty()) base = QDir::homePath();
    return QDir(base).filePath(QStringLiteral("winols_index.db"));
}

}  // namespace

QStringList Config::dbRoots() const
{
    QSettings s;
    QStringList roots = s.value(QStringLiteral("winols/dbRoots"))
                            .toStringList();
    if (roots.isEmpty()) roots << defaultDbRoot();
    return roots;
}

void Config::setDbRoots(const QStringList &roots)
{
    QSettings().setValue(QStringLiteral("winols/dbRoots"), roots);
}

QHash<QString, QString> Config::fileRoots() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("winols/fileRoots"));
    QHash<QString, QString> out;
    for (const QString &k : s.childKeys())
        out.insert(k, s.value(k).toString());
    s.endGroup();
    return out;
}

void Config::setFileRoots(const QHash<QString, QString> &m)
{
    QSettings s;
    s.beginGroup(QStringLiteral("winols/fileRoots"));
    s.remove(QString());      // wipe group
    for (auto it = m.constBegin(); it != m.constEnd(); ++it)
        s.setValue(it.key(), it.value());
    s.endGroup();
}

void Config::setFileRoot(const QString &dbBasename, const QString &dirPath)
{
    QSettings s;
    s.setValue(QStringLiteral("winols/fileRoots/") + dbBasename, dirPath);
}

QStringList Config::scanFallback() const
{
    return QSettings().value(QStringLiteral("winols/scanFallback"))
                        .toStringList();
}

void Config::setScanFallback(const QStringList &dirs)
{
    QSettings().setValue(QStringLiteral("winols/scanFallback"), dirs);
}

QStringList Config::encodingChain() const
{
    QStringList chain = QSettings().value(QStringLiteral("winols/encodingChain"))
                            .toStringList();
    if (chain.isEmpty())
        chain = {QStringLiteral("UTF-8"),
                 QStringLiteral("Windows-1252"),
                 QStringLiteral("ISO-8859-1")};
    return chain;
}

void Config::setEncodingChain(const QStringList &chain)
{
    QSettings().setValue(QStringLiteral("winols/encodingChain"), chain);
}

QString Config::indexDbPath() const
{
    QString p = QSettings().value(QStringLiteral("winols/indexDbPath"))
                    .toString();
    return p.isEmpty() ? defaultIndexDb() : p;
}

void Config::setIndexDbPath(const QString &path)
{
    QSettings().setValue(QStringLiteral("winols/indexDbPath"), path);
}

QStringList Config::discoverCacheDbs() const
{
    QStringList out;
    for (const QString &root : dbRoots()) {
        QDir d(root);
        if (!d.exists()) continue;
        const auto entries = d.entryList(
            {QStringLiteral("Cache_*.db")}, QDir::Files | QDir::Readable);
        for (const QString &e : entries)
            out << d.absoluteFilePath(e);
    }
    return out;
}

}  // namespace winols
