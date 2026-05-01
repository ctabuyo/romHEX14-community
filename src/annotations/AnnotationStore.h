/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Comments + Markers at ROM offsets  (Sprint C)
 * =============================================
 *
 * Lets the user pin a free-text note (or a pure marker — empty text)
 * to a ROM offset or byte range, then jump between them with F5 /
 * Shift+F5 and see them as glyphs in hex / waveform / 3D.
 *
 * Storage: sidecar JSON (`<project_basename>.comments.json`) next to
 * the .rx14proj, mirroring SavepointManager / AlignmentMap.  The
 * RX14 TLV project file itself is *not* touched.
 *
 * Distinct from `MapInfo::userNotes` which is *map-level*; an
 * Annotation is *byte-level* and can also span ranges that don't
 * line up with any defined map.
 */

#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>

class Project;
class QJsonObject;

struct Annotation {
    qint64    addr;        // start offset in the ROM
    qint64    length;      // 1 = point comment; >1 = ranged
    QString   text;        // empty == pure marker
    QDateTime createdAt;
    QString   author;      // optional — e.g. user name from QSettings

    bool isMarker() const { return text.isEmpty(); }
};

class AnnotationStore : public QObject {
    Q_OBJECT
public:
    explicit AnnotationStore(QObject *parent = nullptr);

    /// Bind to a project. Reads the sidecar file (if any). Subsequent
    /// mutations auto-save. Pass nullptr to detach.
    void attachTo(Project *project);
    Project *project() const { return m_project; }

    // ── Mutation ─────────────────────────────────────────────────────
    /// Add a new annotation. `length` defaults to 1 (point comment).
    /// Pure marker: pass an empty `text`.
    void add(qint64 addr, const QString &text,
             qint64 length = 1, const QString &author = QString());

    /// Remove all annotations exactly at @p addr.
    bool removeAt(qint64 addr);

    /// Replace the text of the annotation starting at @p addr (the
    /// first one if multiple exist). Returns false if none found.
    bool setText(qint64 addr, const QString &newText);

    void clear();

    // ── Inspection ───────────────────────────────────────────────────
    const QVector<Annotation> &all() const { return m_items; }
    /// Annotations whose [addr, addr+length) range covers @p offset.
    QVector<Annotation> at(qint64 offset) const;
    /// Strictly the next annotation start strictly after @p offset
    /// (wraps around if @p wrap is true). Returns -1 if none.
    qint64 nextAfter(qint64 offset, bool wrap = true) const;
    qint64 prevBefore(qint64 offset, bool wrap = true) const;

    // ── Persistence ─────────────────────────────────────────────────
    QString sidecarPath() const;
    bool save() const;
    bool load();

signals:
    void changed();

private:
    QJsonObject toJson() const;
    bool        fromJson(const QJsonObject &obj);

    Project             *m_project = nullptr;
    QVector<Annotation>  m_items;
};
