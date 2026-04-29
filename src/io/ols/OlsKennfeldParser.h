/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "../../romdata.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace ols {

class OlsKennfeldParser {
    Q_DECLARE_TR_FUNCTIONS(ols::OlsKennfeldParser)
public:
    static QVector<MapInfo> parseAll(const QByteArray &data,
                                     qsizetype regionStart,
                                     qsizetype regionEnd,
                                     int schema,
                                     QStringList *warnings = nullptr);

    static MapInfo parseOne(const QByteArray &data,
                            qsizetype commentOffset,
                            qsizetype hardLimit,
                            int schema);

    static QVector<MapInfo> parseIntern(const QByteArray &internPayload,
                                        int schema,
                                        QStringList *warnings = nullptr);

    static QPair<qsizetype, qsizetype> findMapRegion(const QByteArray &data,
                                                      int schema);


    struct PStr {
        QString text;
        qsizetype endOffset = -1;
        bool valid() const { return endOffset >= 0; }
    };
    static PStr readCString(const QByteArray &data, qsizetype offset,
                             int maxLen = 8192, int schema = 288);

    static uint32_t peekU32(const QByteArray &data, qsizetype off);
    static double   peekF64(const QByteArray &data, qsizetype off);

private:
    static bool isText(const char *data, int length);
    static bool isIdent(const QString &s);
};

}
