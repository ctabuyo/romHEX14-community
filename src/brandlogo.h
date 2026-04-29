/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// brandlogo.h — Async vehicle-brand logo fetcher with disk cache.
//
// Pulls logos from Clearbit's free Logo API (logo.clearbit.com/<domain>) using
// a curated brand → domain map. Logos are cached on disk under
// AppDataLocation/logos/<brand>.png so subsequent app launches are offline-fast.
// ─────────────────────────────────────────────────────────────────────────────

#include <QObject>
#include <QString>
#include <QPixmap>
#include <QHash>
#include <QList>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

class BrandLogo : public QObject {
    Q_OBJECT

public:
    static BrandLogo &instance();

    using Callback = std::function<void(const QPixmap &)>;

    // Resolve or fetch the logo for `brand`. Behavior:
    //   • Brand string is normalized (lowercase, trimmed, hyphens collapsed).
    //   • If cached in memory:  callback fires synchronously.
    //   • If cached on disk:    pixmap is loaded and callback fires synchronously.
    //   • Otherwise:            HTTP fetch is started, callback fires on finish.
    //   • Brand with no domain mapping: callback fires synchronously with null
    //     pixmap so the caller can fall back to a letter avatar or similar.
    //
    // The returned pixmap is the original-resolution PNG; the caller should
    // scale to its display size.
    void requestLogo(const QString &brand, Callback cb);

    // Returns true if we have a known domain mapping for this brand.
    static bool isKnownBrand(const QString &brand);

private:
    explicit BrandLogo(QObject *parent = nullptr);

    static QString normalize(const QString &brand);
    static QString brandToDomain(const QString &brand);   // "" if unknown
    static QString diskCachePath(const QString &brandKey);

    void onReplyFinished(QNetworkReply *reply, const QString &brandKey);

    QNetworkAccessManager      *m_nam = nullptr;
    QHash<QString, QPixmap>     m_memCache;     // brandKey → loaded pixmap
    QHash<QString, QList<Callback>> m_pending;  // brandKey → waiting callbacks
};
