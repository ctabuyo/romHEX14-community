/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "annotations/AnnotationStore.h"
#include "project.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

AnnotationStore::AnnotationStore(QObject *parent) : QObject(parent) {}

void AnnotationStore::attachTo(Project *project)
{
    m_items.clear();
    m_project = project;
    if (m_project) load();
    emit changed();
}

// ─── Mutation ───────────────────────────────────────────────────────────────

void AnnotationStore::add(qint64 addr, const QString &text,
                          qint64 length, const QString &author)
{
    if (addr < 0 || length < 1) return;
    Annotation a;
    a.addr      = addr;
    a.length    = length;
    a.text      = text;
    a.createdAt = QDateTime::currentDateTime();
    a.author    = author;
    m_items.append(a);
    // Keep list sorted by addr so nextAfter / prevBefore are linear scans.
    std::stable_sort(m_items.begin(), m_items.end(),
                     [](const Annotation &x, const Annotation &y) {
                         return x.addr < y.addr;
                     });
    save();
    emit changed();
}

bool AnnotationStore::removeAt(qint64 addr)
{
    int before = m_items.size();
    auto newEnd = std::remove_if(m_items.begin(), m_items.end(),
                                 [addr](const Annotation &a) {
                                     return a.addr == addr;
                                 });
    if (newEnd == m_items.end()) return false;
    m_items.erase(newEnd, m_items.end());
    if (m_items.size() != before) {
        save();
        emit changed();
        return true;
    }
    return false;
}

bool AnnotationStore::setText(qint64 addr, const QString &newText)
{
    for (Annotation &a : m_items) {
        if (a.addr == addr) {
            a.text = newText;
            save();
            emit changed();
            return true;
        }
    }
    return false;
}

void AnnotationStore::clear()
{
    if (m_items.isEmpty()) return;
    m_items.clear();
    save();
    emit changed();
}

// ─── Inspection ─────────────────────────────────────────────────────────────

QVector<Annotation> AnnotationStore::at(qint64 offset) const
{
    QVector<Annotation> out;
    for (const Annotation &a : m_items) {
        if (offset >= a.addr && offset < a.addr + a.length)
            out.append(a);
    }
    return out;
}

qint64 AnnotationStore::nextAfter(qint64 offset, bool wrap) const
{
    if (m_items.isEmpty()) return -1;
    for (const Annotation &a : m_items)
        if (a.addr > offset) return a.addr;
    return wrap ? m_items.first().addr : -1;
}

qint64 AnnotationStore::prevBefore(qint64 offset, bool wrap) const
{
    if (m_items.isEmpty()) return -1;
    qint64 best = -1;
    for (const Annotation &a : m_items) {
        if (a.addr < offset) best = a.addr;
        else break;     // sorted ascending — anything past this is later
    }
    if (best >= 0) return best;
    return wrap ? m_items.last().addr : -1;
}

// ─── Persistence ────────────────────────────────────────────────────────────

QString AnnotationStore::sidecarPath() const
{
    if (!m_project) return {};
    const QString path = m_project->filePath;
    if (path.isEmpty()) return {};
    QFileInfo fi(path);
    return fi.absolutePath() + "/" + fi.completeBaseName() + ".comments.json";
}

bool AnnotationStore::save() const
{
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(toJson());
    return f.write(doc.toJson(QJsonDocument::Indented)) > 0;
}

bool AnnotationStore::load()
{
    m_items.clear();
    const QString path = sidecarPath();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    return fromJson(doc.object());
}

QJsonObject AnnotationStore::toJson() const
{
    QJsonObject root;
    root.insert("version", 1);
    QJsonArray arr;
    for (const Annotation &a : m_items) {
        QJsonObject o;
        o.insert("addr",      static_cast<double>(a.addr));
        o.insert("length",    static_cast<double>(a.length));
        o.insert("text",      a.text);
        o.insert("createdAt", a.createdAt.toString(Qt::ISODateWithMs));
        if (!a.author.isEmpty()) o.insert("author", a.author);
        arr.append(o);
    }
    root.insert("items", arr);
    return root;
}

bool AnnotationStore::fromJson(const QJsonObject &obj)
{
    m_items.clear();
    const QJsonArray arr = obj.value("items").toArray();
    m_items.reserve(arr.size());
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        Annotation a;
        a.addr      = static_cast<qint64>(o.value("addr").toDouble());
        a.length    = static_cast<qint64>(o.value("length").toDouble(1));
        a.text      = o.value("text").toString();
        a.createdAt = QDateTime::fromString(o.value("createdAt").toString(),
                                            Qt::ISODateWithMs);
        a.author    = o.value("author").toString();
        if (a.length < 1) a.length = 1;
        if (a.addr >= 0) m_items.append(a);
    }
    std::stable_sort(m_items.begin(), m_items.end(),
                     [](const Annotation &x, const Annotation &y) {
                         return x.addr < y.addr;
                     });
    emit const_cast<AnnotationStore *>(this)->changed();
    return true;
}
