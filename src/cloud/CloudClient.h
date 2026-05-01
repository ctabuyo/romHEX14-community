/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Thin Qt6 Network client for the cloudfx API at https://ogorek.ftp.sh.
 *
 * Two tiers:
 *   - Free: /v1/dtc/analyze + /v1/dtc/disable (IP rate-limited, no auth)
 *   - Pro:  /v1/features/detect + /v1/features/apply (Bearer token)
 *
 * The client never sees the detection / patching logic — only the
 * results.  An optional `family` hint (e.g. "EDC17", "DENSO") can be
 * passed to override auto-detection on the server, mirroring the
 * Producer/Build dropdowns in Project Properties.
 *
 * All requests are asynchronous; results arrive via signals.  Callers
 * connect once at dialog construction and react to whichever signal
 * fires first.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class CloudClient : public QObject {
    Q_OBJECT
public:
    explicit CloudClient(QObject *parent = nullptr);
    ~CloudClient() override;

    /// Persistent settings.  Read on construction, written by setters that
    /// also flush to QSettings under "cloud/baseUrl" and "cloud/proToken".
    void    setBaseUrl(const QString &url);
    void    setProToken(const QString &token);
    QString baseUrl()  const { return m_baseUrl; }
    QString proToken() const { return m_proToken; }
    bool    hasProToken() const { return !m_proToken.isEmpty(); }

    // ── Free tier — DTCs ────────────────────────────────────────────
    /// POST /v1/dtc/analyze.  @p familyHint is the canonical family
    /// (e.g. "EDC17", "DENSO") chosen by the user in Project Properties;
    /// empty string falls back to auto-detect on the server.
    void requestDtcAnalyze(const QByteArray &rom, const QString &familyHint = {});

    /// POST /v1/dtc/disable.  @p codes contains the DTC indices/codes
    /// the user selected to disable (server-defined format —
    /// comma-separated string).
    void requestDtcDisable(const QByteArray &rom, const QString &codes,
                           const QString &familyHint = {});

    // ── Pro tier — Features (Bearer token required) ─────────────────
    void requestFeaturesDetect(const QByteArray &rom,
                               const QString &familyHint = {});
    void requestFeaturesApply(const QByteArray &rom, const QStringList &features,
                              const QString &familyHint = {});

    // ── Health ──────────────────────────────────────────────────────
    /// GET /v1/health.  Useful to verify configuration without uploading
    /// a ROM.  Result delivered via @ref healthFinished.
    void requestHealth();

signals:
    /// Each *Finished signal fires exactly once per request.
    /// @p ok = true when the server returned 2xx and a parseable body;
    /// false otherwise (see @p result["error"] for detail).
    void dtcAnalyzeFinished(bool ok, const QJsonObject &result);
    void featuresDetectFinished(bool ok, const QJsonObject &result);

    /// Apply endpoints return either binary (the patched ROM) or a JSON
    /// error.  When @p ok is true, @p patchedRom contains the new bytes
    /// and @p log carries the server-provided summary if any.
    void dtcDisableFinished(bool ok, const QByteArray &patchedRom,
                            const QJsonObject &meta);
    void featuresApplyFinished(bool ok, const QByteArray &patchedRom,
                               const QJsonObject &meta);

    void healthFinished(bool ok, const QJsonObject &result);

    /// Low-level transport error (network down, DNS fail, TLS fail, …).
    /// Distinct from server-returned 4xx/5xx — those are surfaced via
    /// the matching *Finished signal with ok=false.
    void networkError(const QString &what, const QString &detail);

private slots:
    void onJsonReplyDone();
    void onBinaryReplyDone();

private:
    QNetworkRequest buildRequest(const QString &path, bool needPro) const;
    QNetworkReply  *postMultipart(const QString &path,
                                  const QByteArray &rom,
                                  const QHash<QString, QString> &fields,
                                  bool needPro,
                                  bool replyIsBinary,
                                  const QString &kind);

    QNetworkAccessManager *m_nam = nullptr;
    QString m_baseUrl;
    QString m_proToken;
};
