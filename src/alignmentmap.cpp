/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "alignmentmap.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

AlignmentMap::AlignmentMap() = default;

// ─── Lookup ─────────────────────────────────────────────────────────────────

qint64 AlignmentMap::mapAtoB(qint64 addrA) const
{
    int idx = regionIndexForA(addrA);
    if (idx < 0) return -1;
    return addrA + m_regions[idx].deltaB;
}

qint64 AlignmentMap::mapAtoC(qint64 addrA) const
{
    int idx = regionIndexForA(addrA);
    if (idx < 0) return -1;
    return addrA + m_regions[idx].deltaC;
}

qint64 AlignmentMap::mapBtoA(qint64 addrB) const
{
    for (const auto &r : m_regions) {
        const qint64 a = addrB - r.deltaB;
        if (a >= r.rangeAStart && a < r.rangeAStart + r.length)
            return a;
    }
    return -1;
}

int AlignmentMap::regionIndexForA(qint64 addrA) const
{
    // Binary search — regions are kept sorted by rangeAStart and
    // non-overlapping by addRegion()'s invariant.
    int lo = 0, hi = m_regions.size() - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const auto &r = m_regions[mid];
        if (addrA < r.rangeAStart)         hi = mid - 1;
        else if (addrA >= r.rangeAStart + r.length) lo = mid + 1;
        else                               return mid;
    }
    return -1;
}

// ─── Mutation ───────────────────────────────────────────────────────────────

void AlignmentMap::clear()
{
    m_regions.clear();
}

void AlignmentMap::setGlobal(qint64 deltaB, qint64 deltaC, qint64 totalSize)
{
    AlignRegion r;
    r.rangeAStart = 0;
    r.length      = qMax<qint64>(totalSize, 0);
    r.deltaB      = deltaB;
    r.deltaC      = deltaC;
    r.confidence  = 100;
    r.source      = "manual";
    m_regions.clear();
    if (r.length > 0) m_regions.append(r);
}

QPair<qint64, qint64> AlignmentMap::nudgeGlobal(qint64 stepB, qint64 stepC)
{
    if (m_regions.size() != 1) return {0, 0};
    auto &r = m_regions[0];
    r.deltaB += stepB;
    r.deltaC += stepC;
    return {r.deltaB, r.deltaC};
}

int AlignmentMap::addRegion(const AlignRegion &r)
{
    if (r.length <= 0) return -1;

    // Trim any existing region that overlaps with the new one.  Strategy:
    // any pre-existing region that intersects [r.rangeAStart, r.endA) gets
    // shrunk on the overlap side, and if fully inside the new range it's
    // removed.  This keeps the non-overlapping invariant simple.
    QVector<AlignRegion> kept;
    kept.reserve(m_regions.size() + 1);

    const qint64 ns = r.rangeAStart;
    const qint64 ne = r.rangeAStart + r.length;

    for (const AlignRegion &e : m_regions) {
        const qint64 es = e.rangeAStart;
        const qint64 ee = e.rangeAStart + e.length;

        if (ee <= ns || es >= ne) {
            // disjoint
            kept.append(e);
            continue;
        }

        // Overlapping.  Possibly emit left fragment, possibly right fragment.
        if (es < ns) {
            AlignRegion left = e;
            left.length = ns - es;
            if (left.length > 0) kept.append(left);
        }
        if (ee > ne) {
            AlignRegion right = e;
            right.rangeAStart = ne;
            right.length      = ee - ne;
            if (right.length > 0) kept.append(right);
        }
        // else fully covered → drop
    }

    kept.append(r);
    std::sort(kept.begin(), kept.end(),
              [](const AlignRegion &a, const AlignRegion &b) {
                  return a.rangeAStart < b.rangeAStart;
              });
    m_regions = std::move(kept);

    // Locate inserted index after sort
    for (int i = 0; i < m_regions.size(); ++i)
        if (m_regions[i].rangeAStart == r.rangeAStart
            && m_regions[i].length == r.length)
            return i;
    return -1;
}

void AlignmentMap::removeRegion(int idx)
{
    if (idx < 0 || idx >= m_regions.size()) return;
    m_regions.remove(idx);
}

void AlignmentMap::replaceRegion(int idx, const AlignRegion &r)
{
    if (idx < 0 || idx >= m_regions.size()) return;
    m_regions[idx] = r;
}

// ─── Persistence ────────────────────────────────────────────────────────────

QString AlignmentMap::sidecarPath(const QString &projAPath,
                                  const QString &projBPath)
{
    QString a = QFileInfo(projAPath).completeBaseName();
    QString b = QFileInfo(projBPath).completeBaseName();
    if (a.isEmpty() || b.isEmpty()) return {};
    // Sort lexicographically so (A,B) and (B,A) yield the same file.
    if (a > b) std::swap(a, b);

    // Park sidecars next to the projects' directory by default — fall
    // back to the user's app-data dir if the projects come from
    // different folders.
    QFileInfo fa(projAPath);
    QFileInfo fb(projBPath);
    QString dir;
    if (fa.absolutePath() == fb.absolutePath() && fa.exists())
        dir = fa.absolutePath();
    else
        dir = QDir::homePath() + "/.romhex14/alignments";
    QDir().mkpath(dir);
    return dir + "/" + a + "~" + b + ".align.json";
}

QJsonObject AlignmentMap::toJson() const
{
    QJsonObject root;
    root.insert("version", 1);
    QJsonArray arr;
    for (const auto &r : m_regions) {
        QJsonObject o;
        o.insert("rangeAStart", static_cast<double>(r.rangeAStart));
        o.insert("length",      static_cast<double>(r.length));
        o.insert("deltaB",      static_cast<double>(r.deltaB));
        o.insert("deltaC",      static_cast<double>(r.deltaC));
        o.insert("confidence",  r.confidence);
        o.insert("source",      r.source);
        o.insert("note",        r.note);
        arr.append(o);
    }
    root.insert("regions", arr);
    return root;
}

bool AlignmentMap::fromJson(const QJsonObject &obj)
{
    m_regions.clear();
    if (obj.value("version").toInt() != 1) return false;
    for (const auto &v : obj.value("regions").toArray()) {
        QJsonObject o = v.toObject();
        AlignRegion r;
        r.rangeAStart = static_cast<qint64>(o.value("rangeAStart").toDouble());
        r.length      = static_cast<qint64>(o.value("length").toDouble());
        r.deltaB      = static_cast<qint64>(o.value("deltaB").toDouble());
        r.deltaC      = static_cast<qint64>(o.value("deltaC").toDouble());
        r.confidence  = o.value("confidence").toInt(100);
        r.source      = o.value("source").toString("manual");
        r.note        = o.value("note").toString();
        if (r.length > 0) m_regions.append(r);
    }
    std::sort(m_regions.begin(), m_regions.end(),
              [](const AlignRegion &a, const AlignRegion &b) {
                  return a.rangeAStart < b.rangeAStart;
              });
    return true;
}

bool AlignmentMap::saveTo(const QString &path) const
{
    if (path.isEmpty()) return false;
    QFile f(path);
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(toJson());
    return f.write(doc.toJson(QJsonDocument::Indented)) > 0;
}

bool AlignmentMap::loadFrom(const QString &path)
{
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    return fromJson(doc.object());
}
