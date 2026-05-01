/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "savepoints/SavepointManager.h"

#include "project.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

SavepointManager::SavepointManager(QObject *parent)
    : QObject(parent) {}

void SavepointManager::attachTo(Project *project)
{
    if (m_project == project) return;
    // Persist any pending changes against the previous project before
    // detaching — protects against the "open new project, forgot to
    // save savepoints on the old one" failure mode.
    if (m_project)
        save();

    m_project = project;
    m_savepoints.clear();
    m_currentId.clear();

    if (m_project)
        load();
    emit savepointsChanged();
    emit currentChanged(m_currentId);
}

// ─── Mutation ───────────────────────────────────────────────────────────────

QString SavepointManager::create(const QString &label, const QString &note)
{
    if (!m_project) return {};
    Savepoint sp;
    sp.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sp.label     = label.trimmed().isEmpty()
                       ? tr("untitled #%1").arg(m_savepoints.size() + 1)
                       : label.trimmed();
    sp.note      = note;
    sp.createdAt = QDateTime::currentDateTimeUtc();
    sp.edits     = computeEditsAgainstOriginal();
    m_savepoints.append(sp);
    m_currentId = sp.id;
    save();
    emit savepointsChanged();
    emit currentChanged(m_currentId);
    return sp.id;
}

bool SavepointManager::switchTo(const QString &id)
{
    if (!m_project) return false;
    int idx = indexOf(id);
    if (idx < 0) return false;
    const Savepoint &sp = m_savepoints[idx];

    // Reconstruct: original data → apply target's edits.
    QByteArray restored = applyEdits(m_project->originalData, sp.edits);
    if (restored.size() != m_project->currentData.size()
        && !m_project->currentData.isEmpty()) {
        // Size mismatch — savepoint was made against a different ROM.
        // Refuse rather than corrupt.
        return false;
    }
    m_project->currentData = restored;
    m_project->modified    = true;
    emit m_project->dataChanged();

    m_currentId = id;
    save();
    emit currentChanged(m_currentId);
    return true;
}

bool SavepointManager::deleteSavepoint(const QString &id)
{
    int idx = indexOf(id);
    if (idx < 0) return false;
    m_savepoints.remove(idx);

    if (m_currentId == id) {
        // Pick the most recent remaining savepoint as the new current —
        // tuner's mental model is "I just deleted the active branch,
        // I'm now on whatever was last".
        m_currentId = m_savepoints.isEmpty()
                          ? QString()
                          : m_savepoints.last().id;
        emit currentChanged(m_currentId);
    }
    save();
    emit savepointsChanged();
    return true;
}

bool SavepointManager::rename(const QString &id, const QString &newLabel,
                              const QString &newNote)
{
    int idx = indexOf(id);
    if (idx < 0) return false;
    Savepoint &sp = m_savepoints[idx];
    if (!newLabel.isEmpty()) sp.label = newLabel.trimmed();
    if (!newNote.isNull())   sp.note  = newNote;
    save();
    emit savepointsChanged();
    return true;
}

// ─── Inspection ─────────────────────────────────────────────────────────────

int SavepointManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_savepoints.size(); ++i)
        if (m_savepoints[i].id == id) return i;
    return -1;
}

Savepoint SavepointManager::find(const QString &id) const
{
    int i = indexOf(id);
    return i < 0 ? Savepoint{} : m_savepoints[i];
}

// ─── Persistence ────────────────────────────────────────────────────────────

QString SavepointManager::sidecarPath() const
{
    if (!m_project) return {};
    QString path = m_project->filePath;
    if (path.isEmpty()) return {};
    QFileInfo fi(path);
    return fi.absolutePath() + "/" + fi.completeBaseName() + ".savepoints.json";
}

bool SavepointManager::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(toJson());
    return f.write(doc.toJson(QJsonDocument::Indented)) > 0;
}

bool SavepointManager::load()
{
    m_savepoints.clear();
    m_currentId.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    return fromJson(doc.object());
}

// ─── Helpers ────────────────────────────────────────────────────────────────

QVector<ByteEdit> SavepointManager::computeEditsAgainstOriginal() const
{
    QVector<ByteEdit> edits;
    if (!m_project) return edits;
    const QByteArray &orig = m_project->originalData;
    const QByteArray &curr = m_project->currentData;
    const qint64 n = qMin<qint64>(orig.size(), curr.size());
    edits.reserve(2048);
    for (qint64 i = 0; i < n; ++i) {
        const quint8 o = static_cast<quint8>(orig[i]);
        const quint8 c = static_cast<quint8>(curr[i]);
        if (o != c) edits.append({i, o, c});
    }
    return edits;
}

QByteArray SavepointManager::applyEdits(const QByteArray &original,
                                        const QVector<ByteEdit> &edits) const
{
    QByteArray out = original;
    for (const ByteEdit &e : edits) {
        if (e.addr < 0 || e.addr >= out.size()) continue;
        out[e.addr] = static_cast<char>(e.now);
    }
    return out;
}

QJsonObject SavepointManager::toJson() const
{
    QJsonObject root;
    root.insert("version", 1);
    root.insert("currentId", m_currentId);

    QJsonArray arr;
    for (const Savepoint &sp : m_savepoints) {
        QJsonObject o;
        o.insert("id",        sp.id);
        o.insert("label",     sp.label);
        o.insert("note",      sp.note);
        o.insert("createdAt", sp.createdAt.toString(Qt::ISODateWithMs));

        QJsonArray editArr;
        for (const ByteEdit &e : sp.edits) {
            QJsonArray triplet;
            triplet.append(static_cast<double>(e.addr));
            triplet.append(static_cast<int>(e.was));
            triplet.append(static_cast<int>(e.now));
            editArr.append(triplet);
        }
        o.insert("edits", editArr);
        arr.append(o);
    }
    root.insert("savepoints", arr);
    return root;
}

bool SavepointManager::fromJson(const QJsonObject &obj)
{
    if (obj.value("version").toInt() != 1) return false;
    m_currentId = obj.value("currentId").toString();
    for (const auto &v : obj.value("savepoints").toArray()) {
        QJsonObject o = v.toObject();
        Savepoint sp;
        sp.id        = o.value("id").toString();
        sp.label     = o.value("label").toString();
        sp.note      = o.value("note").toString();
        sp.createdAt = QDateTime::fromString(o.value("createdAt").toString(),
                                             Qt::ISODateWithMs);
        for (const auto &ev : o.value("edits").toArray()) {
            QJsonArray t = ev.toArray();
            if (t.size() < 3) continue;
            ByteEdit e;
            e.addr = static_cast<qint64>(t[0].toDouble());
            e.was  = static_cast<quint8>(t[1].toInt());
            e.now  = static_cast<quint8>(t[2].toInt());
            sp.edits.append(e);
        }
        if (!sp.id.isEmpty())
            m_savepoints.append(sp);
    }
    emit savepointsChanged();
    emit currentChanged(m_currentId);
    return true;
}
