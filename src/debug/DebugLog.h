/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QLoggingCategory>

// ── Subsystem logging categories ─────────────────────────────────────────────
//
// Use these with `qCDebug(catX) << "msg"`, `qCInfo(catX) << ...`, etc.  Output
// is routed through main.cpp's qInstallMessageHandler into the file-backed
// Logger (D:\rx14-debug\rx14.log when built with RX14_DEBUG_RPC).
//
// Filter at runtime via the QT_LOGGING_RULES env var, e.g.:
//     QT_LOGGING_RULES="rx14.*=true;rx14.hexsync.debug=false"
//
// Adding a new category? Declare it here and define it in DebugLog.cpp so the
// name string lives in exactly one place.

Q_DECLARE_LOGGING_CATEGORY(catHexSync)
Q_DECLARE_LOGGING_CATEGORY(catWaveSync)
Q_DECLARE_LOGGING_CATEGORY(catTree)
Q_DECLARE_LOGGING_CATEGORY(catMdi)
Q_DECLARE_LOGGING_CATEGORY(catDtc)
Q_DECLARE_LOGGING_CATEGORY(catRpc)
Q_DECLARE_LOGGING_CATEGORY(catOls)
Q_DECLARE_LOGGING_CATEGORY(catMap)
Q_DECLARE_LOGGING_CATEGORY(catFind)
