/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cloud/CloudClient.h"

#include "appconstants.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

namespace {
constexpr auto kSettingsBaseUrl = "cloud/baseUrl";
constexpr auto kSettingsToken   = "cloud/proToken";

// The endpoint URL is user-configured (Cloud Tools → Configure…).  The
// open-source build ships with no default — set this empty so a fresh
// install never phones home until the user opts in.  Distributors who
// run their own backend can drop a setting into QSettings or supply a
// build-time default via -DRX14_CLOUD_DEFAULT_URL="https://..." in
// CMakeLists.
#ifndef RX14_CLOUD_DEFAULT_URL
#  define RX14_CLOUD_DEFAULT_URL ""
#endif

constexpr auto kKindProperty  = "rx14_kind";   // dynamic property on QNetworkReply

// XOR obfuscation — same key as src/aiassistant.cpp:OBF_KEY so a future
// shared header can deduplicate.  Not real security: anyone with access
// to the binary can reverse the table in seconds.  Goal here is just to
// avoid leaving a Bearer token in the clear when an admin glances at
// regedit / Activity Monitor / the .ini file.
constexpr quint8 kObfKey[] = {0xA3,0x7F,0x1C,0xD2,0x56,0x8B,0x4E,0x93,
                              0xC1,0x2A,0xF7,0x65,0x3D,0xB8,0x0E,0x49};

QByteArray obfuscate(const QByteArray &data)
{
    QByteArray out = data;
    for (int i = 0; i < out.size(); ++i)
        out[i] = out[i] ^ kObfKey[i % sizeof(kObfKey)];
    return out.toBase64();
}

QByteArray deobfuscate(const QByteArray &data)
{
    QByteArray raw = QByteArray::fromBase64(data);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = raw[i] ^ kObfKey[i % sizeof(kObfKey)];
    return raw;
}
} // namespace

CloudClient::CloudClient(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);

    QSettings store = rx14::appSettings();
    m_baseUrl = store.value(kSettingsBaseUrl,
                            QString::fromUtf8(RX14_CLOUD_DEFAULT_URL))
                    .toString().trimmed();

    QByteArray stored = store.value(kSettingsToken).toByteArray();
    m_proToken = stored.isEmpty()
                    ? QString()
                    : QString::fromUtf8(deobfuscate(stored));
}

CloudClient::~CloudClient() = default;

void CloudClient::setBaseUrl(const QString &url)
{
    QString clean = url.trimmed();
    // Strip a stray trailing slash so callers don't end up with paths
    // like https://host//v1/health.
    while (clean.endsWith('/')) clean.chop(1);
    m_baseUrl = clean;
    QSettings store = rx14::appSettings();
    if (m_baseUrl.isEmpty())
        store.remove(kSettingsBaseUrl);
    else
        store.setValue(kSettingsBaseUrl, m_baseUrl);
}

void CloudClient::setProToken(const QString &token)
{
    m_proToken = token.trimmed();
    QSettings store = rx14::appSettings();
    if (m_proToken.isEmpty())
        store.remove(kSettingsToken);
    else
        store.setValue(kSettingsToken, obfuscate(m_proToken.toUtf8()));
}

QNetworkRequest CloudClient::buildRequest(const QString &path, bool needPro) const
{
    QString base = m_baseUrl;
    while (base.endsWith('/')) base.chop(1);
    QNetworkRequest req((QUrl(base + path)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("romHEX14-cloud/1.0"));
    if (needPro && !m_proToken.isEmpty()) {
        req.setRawHeader("Authorization",
                         ("Bearer " + m_proToken).toUtf8());
    }
    return req;
}

QNetworkReply *CloudClient::postMultipart(const QString &path,
                                          const QByteArray &rom,
                                          const QHash<QString, QString> &fields,
                                          bool needPro,
                                          bool replyIsBinary,
                                          const QString &kind)
{
    if (m_baseUrl.isEmpty()) {
        // No server configured.  Surface the same networkError() signal the
        // dialog already listens for, so the user sees a clean message
        // ("Cloud server not configured") instead of an opaque crash on a
        // QUrl with an empty host.  Defer via singleShot so the caller's
        // setBusy() runs first.
        QTimer::singleShot(0, this, [this, kind]() {
            emit networkError(kind, tr("Cloud server not configured. "
                                       "Open Configure… and set the URL."));
        });
        return nullptr;
    }
    auto *mp = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // ROM file part
    {
        QHttpPart p;
        p.setHeader(QNetworkRequest::ContentDispositionHeader,
                    QVariant("form-data; name=\"rom\"; filename=\"rom.bin\""));
        p.setHeader(QNetworkRequest::ContentTypeHeader,
                    QVariant("application/octet-stream"));
        p.setBody(rom);
        mp->append(p);
    }

    // Optional form fields (family hint, codes, features...)
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (it.value().isEmpty()) continue;
        QHttpPart p;
        p.setHeader(QNetworkRequest::ContentDispositionHeader,
                    QVariant(QString("form-data; name=\"%1\"").arg(it.key())));
        p.setBody(it.value().toUtf8());
        mp->append(p);
    }

    QNetworkReply *r = m_nam->post(buildRequest(path, needPro), mp);
    mp->setParent(r);                       // mp lives until reply done
    r->setProperty(kKindProperty, kind);
    if (replyIsBinary)
        connect(r, &QNetworkReply::finished, this, &CloudClient::onBinaryReplyDone);
    else
        connect(r, &QNetworkReply::finished, this, &CloudClient::onJsonReplyDone);
    return r;
}

// ─── Public request helpers ────────────────────────────────────────────────

void CloudClient::requestDtcAnalyze(const QByteArray &rom, const QString &familyHint)
{
    postMultipart("/v1/dtc/analyze", rom,
                  {{"family", familyHint}},
                  /*needPro*/false, /*binary*/false, "dtc.analyze");
}

void CloudClient::requestDtcDisable(const QByteArray &rom, const QString &codes,
                                    const QString &familyHint)
{
    postMultipart("/v1/dtc/disable", rom,
                  {{"family", familyHint}, {"codes", codes}},
                  /*needPro*/false, /*binary*/true, "dtc.disable");
}

void CloudClient::requestFeaturesDetect(const QByteArray &rom,
                                        const QString &familyHint)
{
    postMultipart("/v1/features/detect", rom,
                  {{"family", familyHint}},
                  /*needPro*/true, /*binary*/false, "features.detect");
}

void CloudClient::requestFeaturesApply(const QByteArray &rom,
                                       const QStringList &features,
                                       const QString &familyHint)
{
    postMultipart("/v1/features/apply", rom,
                  {{"family", familyHint},
                   {"features", features.join(",")}},
                  /*needPro*/true, /*binary*/true, "features.apply");
}

void CloudClient::requestHealth()
{
    if (m_baseUrl.isEmpty()) {
        QTimer::singleShot(0, this, [this]() {
            emit networkError("health", tr("Cloud server not configured."));
        });
        return;
    }
    QNetworkReply *r = m_nam->get(buildRequest("/v1/health", false));
    r->setProperty(kKindProperty, "health");
    connect(r, &QNetworkReply::finished, this, &CloudClient::onJsonReplyDone);
}

// ─── Reply handlers ────────────────────────────────────────────────────────

void CloudClient::onJsonReplyDone()
{
    auto *r = qobject_cast<QNetworkReply *>(sender());
    if (!r) return;
    r->deleteLater();

    const QString kind = r->property(kKindProperty).toString();
    const QByteArray body = r->readAll();
    const int httpCode = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (r->error() != QNetworkReply::NoError && httpCode == 0) {
        // True transport-level failure (no HTTP response received)
        emit networkError(kind, r->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
    const bool ok = (httpCode == 200) && obj.value("ok").toBool(true);

    // Surface server-side error code when present so the UI can show it.
    if (!ok && !obj.contains("http_code"))
        obj.insert("http_code", httpCode);

    if (kind == "dtc.analyze")           emit dtcAnalyzeFinished(ok, obj);
    else if (kind == "features.detect")  emit featuresDetectFinished(ok, obj);
    else if (kind == "health")           emit healthFinished(ok, obj);
}

void CloudClient::onBinaryReplyDone()
{
    auto *r = qobject_cast<QNetworkReply *>(sender());
    if (!r) return;
    r->deleteLater();

    const QString kind = r->property(kKindProperty).toString();
    const QByteArray body = r->readAll();
    const int httpCode = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString contentType = r->header(QNetworkRequest::ContentTypeHeader).toString();

    if (r->error() != QNetworkReply::NoError && httpCode == 0) {
        emit networkError(kind, r->errorString());
        return;
    }

    QJsonObject meta;
    meta.insert("http_code", httpCode);

    // 2xx + octet-stream → the patched ROM body; anything else → JSON error.
    if (httpCode == 200 && contentType.contains("octet-stream", Qt::CaseInsensitive)) {
        // Pass through any X-cloudfx-* headers as metadata so the UI can
        // surface "tier=free" vs "tier=pro" in the status bar etc.
        for (const auto &h : r->rawHeaderPairs()) {
            const QByteArray name = h.first.toLower();
            if (name.startsWith("x-cloudfx-"))
                meta.insert(QString::fromUtf8(h.first),
                            QString::fromUtf8(h.second));
        }
        if (kind == "dtc.disable")           emit dtcDisableFinished(true, body, meta);
        else if (kind == "features.apply")   emit featuresApplyFinished(true, body, meta);
        return;
    }

    // JSON body — error case
    QJsonDocument doc = QJsonDocument::fromJson(body);
    QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
    obj.insert("http_code", httpCode);
    if (kind == "dtc.disable")          emit dtcDisableFinished(false, {}, obj);
    else if (kind == "features.apply")  emit featuresApplyFinished(false, {}, obj);
}
