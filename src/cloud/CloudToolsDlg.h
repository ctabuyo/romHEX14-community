/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Cloud Tools dialog — replaces the in-process DtcWizardDlg.
 *
 * Two tabs:
 *   1. DTCs   — free tier; analyse + disable.  Visible to all users.
 *   2. Features — pro tier; only enabled when the user has configured a
 *                 token in Misc → Cloud configuration.
 *
 * The dialog drives a CloudClient and translates its async signals into
 * UI updates.  No detection / patching logic lives client-side.
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QString>

class CloudClient;
class Project;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QPlainTextEdit;
class QProgressBar;
class QCheckBox;

class CloudToolsDlg : public QDialog {
    Q_OBJECT
public:
    explicit CloudToolsDlg(Project *proj, QWidget *parent = nullptr);

private slots:
    void onAnalyseClicked();
    void onDisableDtcsClicked();
    void onApplyFeaturesClicked();
    void onConfigureClicked();
    void onDtcSearchChanged(const QString &text);
    void onDtcSelectAllOn();
    void onDtcSelectNone();
    void onDtcItemChanged(QTreeWidgetItem *item, int col);

    void onDtcAnalyzeFinished(bool ok, const QJsonObject &result);
    void onFeaturesDetectFinished(bool ok, const QJsonObject &result);
    void onDtcDisableFinished(bool ok, const QByteArray &patched,
                              const QJsonObject &meta);
    void onFeaturesApplyFinished(bool ok, const QByteArray &patched,
                                 const QJsonObject &meta);
    void onNetworkError(const QString &what, const QString &detail);

private:
    void buildUi();
    void refreshAuthBadge();
    QString familyHintFromProject() const;
    void    applyPatchedRom(const QByteArray &patched, const QJsonObject &meta);
    void    setBusy(bool busy, const QString &what = {});

    Project     *m_project = nullptr;
    CloudClient *m_client  = nullptr;

    // ── UI ─────────────────────────────────────────────────────────────
    QTabWidget   *m_tabs        = nullptr;

    // Header (shared)
    QLabel       *m_ecuLabel    = nullptr;
    QLabel       *m_authBadge   = nullptr;
    QPushButton  *m_btnConfig   = nullptr;
    QPushButton  *m_btnAnalyse  = nullptr;
    QProgressBar *m_progress    = nullptr;
    QLabel       *m_status      = nullptr;

    // DTC tab
    QTreeWidget  *m_dtcTree         = nullptr;     // table with toggles
    QLineEdit    *m_dtcSearch       = nullptr;     // search filter
    QPushButton  *m_btnSelectOn     = nullptr;     // bulk: check all currently-ON
    QPushButton  *m_btnSelectNone   = nullptr;     // bulk: uncheck all
    QPushButton  *m_btnDisableDtcs  = nullptr;
    QLabel       *m_dtcSummary      = nullptr;
    QLabel       *m_dtcSelCount     = nullptr;

    // Features tab
    QListWidget  *m_featuresList    = nullptr;
    QPushButton  *m_btnApplyFeats   = nullptr;
    QLabel       *m_featuresSummary = nullptr;
    QPlainTextEdit *m_featuresLog   = nullptr;

    // Detection cache (so apply doesn't re-upload + re-detect)
    QString  m_lastEcuName;
    QString  m_lastEcuMatch;
    QString  m_lastFid;
};
