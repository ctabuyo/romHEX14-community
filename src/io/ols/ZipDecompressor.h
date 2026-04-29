/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QString>

namespace ols {

class ZipDecompressor {
public:
    static QByteArray decompress(const QByteArray &compressed,
                                 QString *err = nullptr);

    static QByteArray decompress(const QByteArray &compressed,
                                 qsizetype expectedSize,
                                 QString *err = nullptr);
};

}
