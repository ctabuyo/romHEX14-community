/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Named Savepoints / Tuning Branches
 * ===================================
 *
 * Lets the user mark labelled snapshots of the current ROM state — "stock
 * baseline", "boost +0.2 trial", "lambda safer" — and switch between them
 * instantly.  Same conceptual workflow as named git stashes, but rendered
 * for a tuner: Switch / Compare / Apply, no command-line.
 *
 * Storage strategy
 * ----------------
 * Each savepoint is encoded as the delta against `Project::originalData`,
 * not as a full byte copy.  Tuners modify a few KiB at most, so a 50-entry
 * branch list fits in <1 MiB of sidecar JSON instead of 100 MiB of full
 * snapshots.  Persisted as `<project_basename>.savepoints.json` next to
 * the .rx14proj file (loaded automatically when the project opens; saved
 * on every mutation).
 *
 * Note: this is a flat model — each savepoint is delta-against-original,
 * not delta-against-parent.  Hierarchical "branch" trees (Stage1 → Stage2
 * → Stage2_v2) are a future enhancement; today every savepoint is rooted
 * at the project's original data.
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class Project;
class QJsonObject;

struct ByteEdit {
    qint64 addr;     // offset in ROM
    quint8 was;      // byte value in originalData
    quint8 now;      // byte value at savepoint creation time
};

struct Savepoint {
    QString    id;          // UUID, stable across sessions
    QString    label;       // user-supplied, "stock baseline" etc.
    QString    note;        // optional longer description
    QDateTime  createdAt;
    QVector<ByteEdit> edits;

    bool isEmpty() const { return edits.isEmpty(); }
    int  byteCount() const { return edits.size(); }
};

class SavepointManager : public QObject {
    Q_OBJECT
public:
    explicit SavepointManager(QObject *parent = nullptr);

    /// Bind to a project.  Reads the sidecar file (if any) and caches
    /// state.  Subsequent mutations are auto-saved.  Pass nullptr to
    /// detach (e.g. when the project closes).
    void attachTo(Project *project);
    Project *project() const { return m_project; }

    // ── Mutation ─────────────────────────────────────────────────────
    /// Create a savepoint from the project's current data (relative to
    /// originalData).  Becomes the new "current" id.  Returns the new
    /// savepoint id, or empty string on failure (e.g. no project bound).
    QString create(const QString &label, const QString &note = {});

    /// Restore the project's currentData to the bytes captured by the
    /// savepoint with @p id.  Updates current id, emits Project's
    /// dataChanged so all views refresh.
    bool switchTo(const QString &id);

    /// Drop a savepoint.  If it was the current one, falls back to the
    /// previous one in the list (or "no current" when the list is empty).
    bool deleteSavepoint(const QString &id);

    /// Rename / re-note an existing savepoint.  Returns false if the id
    /// is unknown.
    bool rename(const QString &id, const QString &newLabel,
                const QString &newNote = QString());

    // ── Inspection ───────────────────────────────────────────────────
    const QVector<Savepoint> &all() const { return m_savepoints; }
    QString  currentId() const { return m_currentId; }
    int      indexOf(const QString &id) const;
    Savepoint find(const QString &id) const;

    // ── Persistence ─────────────────────────────────────────────────
    /// Path of the sidecar JSON file for the bound project, or empty.
    QString sidecarPath() const;

    bool save() const;
    bool load();

signals:
    /// List membership changed (create/delete/rename).
    void savepointsChanged();

    /// Active savepoint changed (switchTo, or auto-set after create).
    void currentChanged(const QString &newId);

private:
    QVector<ByteEdit> computeEditsAgainstOriginal() const;
    QByteArray        applyEdits(const QByteArray &original,
                                 const QVector<ByteEdit> &edits) const;
    QJsonObject       toJson() const;
    bool              fromJson(const QJsonObject &obj);

    Project           *m_project = nullptr;
    QVector<Savepoint> m_savepoints;
    QString            m_currentId;
};
