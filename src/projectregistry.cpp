/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "projectregistry.h"
#include "project.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

ProjectRegistry &ProjectRegistry::instance()
{
    static ProjectRegistry inst;
    return inst;
}

ProjectRegistry::ProjectRegistry()
{
    load();
}

QString ProjectRegistry::defaultProjectDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + "/projects";
}

QString ProjectRegistry::registryPath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + "/registry.json";
}

void ProjectRegistry::load()
{
    QFile f(registryPath());
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    m_entries.clear();
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        ProjectEntry e;
        e.path       = o["path"].toString();
        e.name       = o["name"].toString();
        e.brand      = o["brand"].toString();
        e.model      = o["model"].toString();
        e.ecuType    = o["ecuType"].toString();
        e.clientName = o["clientName"].toString();
        e.createdAt  = QDateTime::fromString(o["createdAt"].toString(), Qt::ISODate);
        e.changedAt  = QDateTime::fromString(o["changedAt"].toString(), Qt::ISODate);
        e.hasA2L     = o["hasA2L"].toBool();
        if (!e.path.isEmpty())
            m_entries.append(e);
    }
}

void ProjectRegistry::save() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);

    QFile f(registryPath());
    if (!f.open(QIODevice::WriteOnly)) return;

    QJsonArray arr;
    for (const auto &e : m_entries) {
        QJsonObject o;
        o["path"]       = e.path;
        o["name"]       = e.name;
        o["brand"]      = e.brand;
        o["model"]      = e.model;
        o["ecuType"]    = e.ecuType;
        o["clientName"] = e.clientName;
        o["createdAt"]  = e.createdAt.toString(Qt::ISODate);
        o["changedAt"]  = e.changedAt.toString(Qt::ISODate);
        o["hasA2L"]     = e.hasA2L;
        arr.append(o);
    }
    f.write(QJsonDocument(arr).toJson());
}

void ProjectRegistry::registerProject(const QString &path, const Project *proj)
{
    for (auto &e : m_entries) {
        if (e.path == path) {
            e.name       = proj->displayName();
            e.brand      = proj->brand;
            e.model      = proj->model;
            e.ecuType    = proj->ecuType;
            e.clientName = proj->clientName;
            e.createdAt  = proj->createdAt;
            e.changedAt  = proj->changedAt;
            e.hasA2L     = !proj->a2lContent.isEmpty();
            save();
            emit changed();
            return;
        }
    }
    // New entry — prepend so most-recent appears first
    ProjectEntry e;
    e.path       = path;
    e.name       = proj->displayName();
    e.brand      = proj->brand;
    e.model      = proj->model;
    e.ecuType    = proj->ecuType;
    e.clientName = proj->clientName;
    e.createdAt  = proj->createdAt;
    e.changedAt  = proj->changedAt;
    e.hasA2L     = !proj->a2lContent.isEmpty();
    m_entries.prepend(e);
    save();
    emit changed();
}

void ProjectRegistry::unregisterProject(const QString &path)
{
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
                       [&path](const ProjectEntry &e){ return e.path == path; }),
        m_entries.end());
    save();
    emit changed();
}

void ProjectRegistry::renameProject(const QString &path, const QString &newName)
{
    for (auto &e : m_entries) {
        if (e.path == path) {
            e.name = newName;
            save();
            emit changed();
            return;
        }
    }
}
