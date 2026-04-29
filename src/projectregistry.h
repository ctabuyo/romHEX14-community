/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>

class Project;

struct ProjectEntry {
    QString   path;
    QString   name;
    QString   brand;
    QString   model;
    QString   ecuType;
    QString   clientName;
    QDateTime createdAt;
    QDateTime changedAt;
    bool      hasA2L = false;
};

// QObject so callers can `connect(&ProjectRegistry::instance(),
// &ProjectRegistry::changed, this, &MainWindow::refreshWelcomePage)` —
// the welcome page listens and rebuilds its "Recent Projects" list live.
class ProjectRegistry : public QObject {
    Q_OBJECT
public:
    static ProjectRegistry &instance();

    // Call after every successful save() / saveAs()
    void registerProject(const QString &path, const Project *proj);
    // Remove from list (does NOT delete the file)
    void unregisterProject(const QString &path);
    // Rename a project in the registry
    void renameProject(const QString &path, const QString &newName);

    QVector<ProjectEntry> entries() const { return m_entries; }

    // Suggested directory for new project files
    static QString defaultProjectDir();

signals:
    // Fired on register/unregister/rename so the welcome page can refresh.
    void changed();

private:
    ProjectRegistry();
    QString registryPath() const;
    void load();
    void save() const;

    QVector<ProjectEntry> m_entries;
};
