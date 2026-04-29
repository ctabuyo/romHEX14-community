/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <cstdint>

namespace ols {

struct OlsHeader {
    Q_DECLARE_TR_FUNCTIONS(ols::OlsHeader)
public:
    uint32_t magic           = 0;
    uint32_t formatVersion   = 0;
    uint32_t declaredFileSize = 0;
    bool     valid           = false;
    QString  error;

    static OlsHeader parse(const QByteArray &fileData);
};

}
