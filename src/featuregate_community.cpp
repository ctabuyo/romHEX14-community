/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "featuregate.h"

namespace FeatureGate {

bool isAvailable(const QString &)
{
    // Community edition: premium server-side features (translation, DTC, AI)
    // are not available — they require the romHEX14 API backend.
    return false;
}

bool isLoggedIn()
{
    return false;
}

QString userEmail()
{
    return QStringLiteral("Community Edition");
}

} // namespace FeatureGate
