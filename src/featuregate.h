/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QString>

/// Feature gate abstraction.
/// Pro build: delegates to ApiClient (requires login + module entitlement).
/// Community build: all features unlocked unconditionally.
namespace FeatureGate {

/// Check if the user has access to a premium module.
/// Module IDs: "translation", "ai_functions", "dtc", "porsche_bcm"
bool isAvailable(const QString &module);

/// Whether the user is currently authenticated.
bool isLoggedIn();

/// Display email (or "Community Edition" in community builds).
QString userEmail();

} // namespace FeatureGate
