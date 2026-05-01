/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class Project;

namespace ols {

struct OlsImportResult;

QVector<Project *> buildProjectsFromOlsImport(const OlsImportResult &result,
                                              const QString &sourcePath = {},
                                              QObject *parent = nullptr);

} // namespace ols
