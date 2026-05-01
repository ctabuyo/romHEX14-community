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

/// Build romHEX14 Project objects from a parsed OLS import.
///
/// Returns exactly one Project on success (or an empty vector if the import
/// has no Versions).  When the .ols contains multiple Versions (Original /
/// Modified / customer variants), Version 0 becomes the main ROM (currentData
/// / originalData / maps / olsSegments), and Versions 1..N-1 are appended to
/// the Project's versions[] list as ProjectVersion snapshots.  This matches
/// the contract documented at MainWindow::actImportOlsProject (mainwindow.cpp
/// "buildProjectsFromOlsImport returns exactly 1 Project").
///
/// @param result      Result from OlsImporter::importFromBytes().
/// @param sourcePath  Original .ols/.kp file path (for naming + display).
/// @param parent      QObject parent for the created Project (lifetime owner).
QVector<Project *> buildProjectsFromOlsImport(const OlsImportResult &result,
                                              const QString &sourcePath = {},
                                              QObject *parent = nullptr);

} // namespace ols
