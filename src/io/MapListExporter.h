/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Map list export  (Sprint D)
 * ============================
 *
 * One-shot dumps of the active project's map list:
 *
 *   * **CSV** — flat row-per-map summary (name, address, dims, axes,
 *     min/max/mean of current bytes, modified-cell count, user notes).
 *     Designed to drop into Excel / Google Sheets for diff workflows.
 *
 *   * **JSON** — full nested structure including each map's cell array,
 *     so a sister tool (e.g. dyno-overlay automation) can round-trip
 *     the data without re-parsing the .rx14proj container.
 *
 * Both functions return false + populate `*err` on failure (file-open
 * error, empty project, ...).  Success means the file was written.
 */

#pragma once

#include <QString>

class Project;

namespace MapListExporter {
    bool toCsv (const Project &p, const QString &path, QString *err);
    bool toJson(const Project &p, const QString &path, QString *err);
}
