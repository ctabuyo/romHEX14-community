/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Single source of truth for hard-coded strings the rest of the codebase
 * needs across many call sites.  Splitting them out of the dialogs they
 * happen to be used in:
 *   * lets a future rename touch one file instead of fifteen,
 *   * keeps QSettings() reads / writes pointing at one registry key
 *     (we used to drift between CT14/RX14 and CT14/romHEX14, splitting
 *     user settings across two stores),
 *   * gives reviewers a tractable place to audit what we persist,
 *     phone home to, etc.
 *
 * Add only widely-shared constants here.  Single-call-site identifiers
 * stay as `constexpr auto` next to where they're used — pulling those in
 * makes the header a junk drawer and inflicts unnecessary recompiles.
 */

#pragma once

#include <QSettings>
#include <QString>

namespace rx14 {

// Organisation + application names used by QApplication and any QSettings
// constructor that needs to be explicit (worker threads, factories, etc).
// QApplication is initialised with these in main.cpp; the no-arg
// QSettings() ctor uses them automatically thereafter, so prefer
// appSettings() below over reaching for the literals directly.
inline constexpr auto kOrgName = "CT14";
inline constexpr auto kAppName = "romHEX14";

// Legacy app-name; pre-2026 builds wrote to this store.  Read-only at
// this point — see settingsMigrateLegacy() in main.cpp, which copies
// CT14/<kLegacyAppName> -> CT14/<kAppName> on first boot.
inline constexpr auto kLegacyAppName = "RX14";

// Returns a QSettings bound to the canonical (kOrgName, kAppName) store.
// Identical to a no-arg QSettings() once QApplication has been
// configured, but reads more clearly at the call site and stays correct
// from worker threads / static factories that QApplication hasn't set
// up yet.
inline QSettings appSettings()
{
    return QSettings(QString::fromUtf8(kOrgName),
                     QString::fromUtf8(kAppName));
}

}  // namespace rx14
