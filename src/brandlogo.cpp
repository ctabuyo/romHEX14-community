/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "brandlogo.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QTimer>
#include <QPixmap>

namespace {

// Curated brand → repo-slug map. Used to construct the open-source car-logos
// dataset URL on GitHub raw:
//
//   https://raw.githubusercontent.com/filippofilip95/car-logos-dataset/
//       master/logos/optimized/<slug>.png
//
// (Originally we used Clearbit's free Logo API which was discontinued.)
//
// Keys MUST be normalized (lower-case, alnum only — see normalize()). Use any
// well-known spelling variant the user might enter (`mercedes`, `mercedes-benz`,
// `merc`, etc.) — they all resolve to the same slug.
const QHash<QString, QString> &kBrandSlugs()
{
    static const QHash<QString, QString> map = {
        // German
        {"porsche",        "porsche"},
        {"mercedes",       "mercedes-benz"},
        {"mercedesbenz",   "mercedes-benz"},
        {"merc",           "mercedes-benz"},
        {"benz",           "mercedes-benz"},
        {"bmw",            "bmw"},
        {"audi",           "audi"},
        {"volkswagen",     "volkswagen"},
        {"vw",             "volkswagen"},
        {"opel",           "opel"},
        {"smart",          "smart"},
        {"maybach",        "maybach"},
        // British
        {"bentley",        "bentley"},
        {"rollsroyce",     "rolls-royce"},
        {"astonmartin",    "aston-martin"},
        {"jaguar",         "jaguar"},
        {"landrover",      "land-rover"},
        {"rangerover",     "land-rover"},
        {"mini",           "mini"},
        {"vauxhall",       "vauxhall"},
        {"mclaren",        "mclaren"},
        {"lotus",          "lotus"},
        {"mg",             "mg"},
        // Italian
        {"ferrari",        "ferrari"},
        {"lamborghini",    "lamborghini"},
        {"maserati",       "maserati"},
        {"alfaromeo",      "alfa-romeo"},
        {"alfa",           "alfa-romeo"},
        {"fiat",           "fiat"},
        {"abarth",         "abarth"},
        {"bugatti",        "bugatti"},
        {"lancia",         "lancia"},
        {"pagani",         "pagani"},
        // French
        {"renault",        "renault"},
        {"peugeot",        "peugeot"},
        {"citroen",        "citroen"},
        {"ds",             "ds-automobiles"},
        {"dacia",          "dacia"},
        // Japanese
        {"toyota",         "toyota"},
        {"lexus",          "lexus"},
        {"honda",          "honda"},
        {"acura",          "acura"},
        {"nissan",         "nissan"},
        {"infiniti",       "infiniti"},
        {"mazda",          "mazda"},
        {"subaru",         "subaru"},
        {"mitsubishi",     "mitsubishi"},
        {"suzuki",         "suzuki"},
        {"isuzu",          "isuzu"},
        // Korean
        {"hyundai",        "hyundai"},
        {"kia",            "kia"},
        {"genesis",        "genesis"},
        {"daewoo",         "daewoo"},
        {"ssangyong",      "ssangyong"},
        // American
        {"ford",           "ford"},
        {"chevrolet",      "chevrolet"},
        {"chevy",          "chevrolet"},
        {"gmc",            "gmc"},
        {"cadillac",       "cadillac"},
        {"buick",          "buick"},
        {"chrysler",       "chrysler"},
        {"dodge",          "dodge"},
        {"jeep",           "jeep"},
        {"ram",            "ram"},
        {"lincoln",        "lincoln"},
        {"tesla",          "tesla"},
        {"hummer",         "hummer"},
        {"plymouth",       "plymouth"},
        {"pontiac",        "pontiac"},
        // Czech / Spanish
        {"skoda",          "skoda"},
        {"seat",           "seat"},
        // Swedish
        {"volvo",          "volvo"},
        {"saab",           "saab"},
        {"koenigsegg",     "koenigsegg"},
        // Chinese
        {"geely",          "geely"},
        {"greatwall",      "great-wall-motors"},
        {"chery",          "chery"},
        // Russian
        {"lada",           "lada"},
    };
    return map;
}

} // namespace

// ── BrandLogo ──────────────────────────────────────────────────────────────

BrandLogo &BrandLogo::instance()
{
    static BrandLogo inst;
    return inst;
}

BrandLogo::BrandLogo(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString BrandLogo::normalize(const QString &brand)
{
    QString s = brand.trimmed().toLower();
    // Collapse whitespace, drop hyphens, drop common punctuation so all
    // variants of "Mercedes-Benz" / "mercedes benz" / "Mercedes" resolve.
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        if (c.isLetterOrNumber()) out.append(c);
    }
    return out;
}

QString BrandLogo::brandToDomain(const QString &brand)
{
    // Method name kept for ABI/header stability — actually returns the slug
    // used in the GitHub car-logos-dataset URL.
    return kBrandSlugs().value(normalize(brand));
}

bool BrandLogo::isKnownBrand(const QString &brand)
{
    return !brandToDomain(brand).isEmpty();
}

QString BrandLogo::diskCachePath(const QString &brandKey)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + "/logos/" + brandKey + ".png";
}

void BrandLogo::requestLogo(const QString &brand, Callback cb)
{
    const QString key = normalize(brand);

    // Empty / unknown brand → null pixmap, synchronously.
    if (key.isEmpty() || !isKnownBrand(brand)) {
        QTimer::singleShot(0, this, [cb]() { cb(QPixmap()); });
        return;
    }

    // Memory cache hit
    auto memIt = m_memCache.constFind(key);
    if (memIt != m_memCache.constEnd()) {
        const QPixmap pm = memIt.value();
        QTimer::singleShot(0, this, [cb, pm]() { cb(pm); });
        return;
    }

    // Disk cache hit
    const QString diskPath = diskCachePath(key);
    if (QFile::exists(diskPath)) {
        QPixmap pm;
        if (pm.load(diskPath)) {
            m_memCache.insert(key, pm);
            QTimer::singleShot(0, this, [cb, pm]() { cb(pm); });
            return;
        }
    }

    // Coalesce concurrent requests for the same brand.
    if (m_pending.contains(key)) {
        m_pending[key].append(cb);
        return;
    }
    m_pending.insert(key, { cb });

    const QString slug = brandToDomain(brand);
    const QUrl url(QStringLiteral(
        "https://raw.githubusercontent.com/filippofilip95/car-logos-dataset/"
        "master/logos/optimized/%1.png").arg(slug));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QByteArray("romHEX14/1.0"));
    req.setTransferTimeout(8000);   // 8s — first-launch fetches; subsequent
                                    // launches are disk-cached and instant.
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
        onReplyFinished(reply, key);
    });
}

void BrandLogo::onReplyFinished(QNetworkReply *reply, const QString &brandKey)
{
    reply->deleteLater();

    QPixmap pm;
    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray bytes = reply->readAll();
        if (!bytes.isEmpty() && pm.loadFromData(bytes)) {
            // Persist to disk for next launch.
            const QString path = diskCachePath(brandKey);
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(bytes);
                f.close();
            }
            m_memCache.insert(brandKey, pm);
        }
    }
    // pm may be null on failure — callers fall back to a letter avatar.

    const auto callbacks = m_pending.take(brandKey);
    for (const auto &cb : callbacks) cb(pm);
}
