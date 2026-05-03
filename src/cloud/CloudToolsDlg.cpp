/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cloud/CloudToolsDlg.h"

#include "cloud/CloudClient.h"
#include "project.h"

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

CloudToolsDlg::CloudToolsDlg(Project *proj, QWidget *parent)
    : QDialog(parent), m_project(proj)
{
    setWindowTitle(tr("Cloud Tools — DTC & Features"));
    resize(720, 560);

    m_client = new CloudClient(this);
    connect(m_client, &CloudClient::dtcAnalyzeFinished,
            this, &CloudToolsDlg::onDtcAnalyzeFinished);
    connect(m_client, &CloudClient::featuresDetectFinished,
            this, &CloudToolsDlg::onFeaturesDetectFinished);
    connect(m_client, &CloudClient::dtcDisableFinished,
            this, &CloudToolsDlg::onDtcDisableFinished);
    connect(m_client, &CloudClient::featuresApplyFinished,
            this, &CloudToolsDlg::onFeaturesApplyFinished);
    connect(m_client, &CloudClient::networkError,
            this, &CloudToolsDlg::onNetworkError);

    buildUi();
    refreshAuthBadge();
}

QString CloudToolsDlg::familyHintFromProject() const
{
    if (!m_project) return {};
    // Project Properties' Producer dropdown maps to the cloud "family"
    // hint values directly (see EcuAutoDetect::knownProducers()).  When
    // the user picked one in the dialog, forward it as-is so the server
    // forces detection rather than auto-guessing.
    QString p = m_project->ecuProducer.trimmed();
    if (p.isEmpty()) return {};

    // Map a few common producers to canonical family names the cloudfx
    // whitelist accepts.  Unknown values pass through (server normalises
    // them or falls back to auto-detect).
    static const QHash<QString, QString> kMap = {
        {"Bosch",            "EDC17"},   // covers EDC15/16/17, MED17, ME7
        {"Continental",      "SID"},     // SID, PCR, SIMOS
        {"Delphi",           "EDC17"},
        {"Denso",            "DENSO"},
        {"Hitachi",          "EDC17"},
        {"Magneti Marelli",  "EDC17"},
        {"Siemens",          "MED17"},
    };
    auto it = kMap.constFind(p);
    return it != kMap.constEnd() ? it.value() : p;
}

// ─── UI ─────────────────────────────────────────────────────────────────────

void CloudToolsDlg::buildUi()
{
    auto *root = new QVBoxLayout(this);

    // Header — ECU label + auth badge + Configure button + Analyse button
    {
        auto *h = new QHBoxLayout();
        m_ecuLabel = new QLabel(tr("ECU: (not detected yet)"));
        m_ecuLabel->setStyleSheet("font-weight:600; color:#e7eefc;");
        h->addWidget(m_ecuLabel, 1);

        m_authBadge = new QLabel();
        m_authBadge->setStyleSheet("padding:2px 8px; border-radius:8px;"
                                   "color:white; font-size:9pt;");
        h->addWidget(m_authBadge);

        m_btnConfig = new QPushButton(tr("Configure…"));
        connect(m_btnConfig, &QPushButton::clicked,
                this, &CloudToolsDlg::onConfigureClicked);
        h->addWidget(m_btnConfig);

        m_btnAnalyse = new QPushButton(tr("Analyse ROM"));
        m_btnAnalyse->setDefault(true);
        connect(m_btnAnalyse, &QPushButton::clicked,
                this, &CloudToolsDlg::onAnalyseClicked);
        h->addWidget(m_btnAnalyse);
        root->addLayout(h);
    }

    // Hint info
    {
        QString hint = familyHintFromProject();
        auto *info = new QLabel(hint.isEmpty()
            ? tr("Hint: (none — auto-detect on server)")
            : tr("Hint from Project Properties → Producer: <b>%1</b>").arg(hint));
        info->setStyleSheet("color:#8b949e; font-size:9pt;");
        root->addWidget(info);
    }

    m_tabs = new QTabWidget();
    root->addWidget(m_tabs, 1);

    // ── Tab: DTCs ─────────────────────────────────────────────────────
    {
        auto *page = new QWidget();
        auto *v = new QVBoxLayout(page);

        m_dtcSummary = new QLabel(tr("Click Analyse to fetch DTC list."));
        m_dtcSummary->setStyleSheet("color:#8b949e;");
        v->addWidget(m_dtcSummary);

        // Search + bulk-select toolbar above the table
        auto *bar = new QHBoxLayout();
        m_dtcSearch = new QLineEdit();
        m_dtcSearch->setPlaceholderText(
            tr("Search code / description (P0420, EGR, lambda, …)"));
        m_dtcSearch->setClearButtonEnabled(true);
        connect(m_dtcSearch, &QLineEdit::textChanged,
                this, &CloudToolsDlg::onDtcSearchChanged);
        bar->addWidget(m_dtcSearch, 1);

        m_btnSelectOn = new QPushButton(tr("Select all currently-ON"));
        m_btnSelectOn->setEnabled(false);
        connect(m_btnSelectOn, &QPushButton::clicked,
                this, &CloudToolsDlg::onDtcSelectAllOn);
        bar->addWidget(m_btnSelectOn);

        m_btnSelectNone = new QPushButton(tr("Clear"));
        m_btnSelectNone->setEnabled(false);
        connect(m_btnSelectNone, &QPushButton::clicked,
                this, &CloudToolsDlg::onDtcSelectNone);
        bar->addWidget(m_btnSelectNone);
        v->addLayout(bar);

        // Tree-table: checkbox | code | manufacturer code | status | description
        m_dtcTree = new QTreeWidget();
        m_dtcTree->setColumnCount(5);
        m_dtcTree->setHeaderLabels(
            QStringList() << "" << tr("Code") << tr("Mfr") << tr("Status") << tr("Description"));
        m_dtcTree->setRootIsDecorated(false);
        m_dtcTree->setUniformRowHeights(true);
        m_dtcTree->setAlternatingRowColors(true);
        m_dtcTree->header()->setStretchLastSection(true);
        m_dtcTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_dtcTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_dtcTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_dtcTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_dtcTree->setSortingEnabled(true);
        connect(m_dtcTree, &QTreeWidget::itemChanged,
                this, &CloudToolsDlg::onDtcItemChanged);
        v->addWidget(m_dtcTree, 1);

        // Footer — selection count + Disable button
        auto *foot = new QHBoxLayout();
        m_dtcSelCount = new QLabel(tr("0 selected"));
        m_dtcSelCount->setStyleSheet("color:#8b949e; font-size:9pt;");
        foot->addWidget(m_dtcSelCount);
        foot->addStretch();
        m_btnDisableDtcs = new QPushButton(tr("Disable selected → save patched ROM"));
        m_btnDisableDtcs->setEnabled(false);
        connect(m_btnDisableDtcs, &QPushButton::clicked,
                this, &CloudToolsDlg::onDisableDtcsClicked);
        foot->addWidget(m_btnDisableDtcs);
        v->addLayout(foot);

        m_tabs->addTab(page, tr("DTCs (free)"));
    }

    // ── Tab: Features ─────────────────────────────────────────────────
    {
        auto *page = new QWidget();
        auto *v = new QVBoxLayout(page);

        m_featuresSummary = new QLabel(tr("Pro tier — requires API token."));
        m_featuresSummary->setStyleSheet("color:#8b949e;");
        v->addWidget(m_featuresSummary);

        m_featuresList = new QListWidget();
        m_featuresList->setSelectionMode(QAbstractItemView::ExtendedSelection);
        v->addWidget(m_featuresList, 2);

        m_featuresLog = new QPlainTextEdit();
        m_featuresLog->setReadOnly(true);
        m_featuresLog->setPlaceholderText(tr("Apply log will appear here."));
        m_featuresLog->setMaximumHeight(120);
        v->addWidget(m_featuresLog, 1);

        auto *h = new QHBoxLayout();
        h->addStretch();
        m_btnApplyFeats = new QPushButton(tr("Apply selected → save patched ROM"));
        m_btnApplyFeats->setEnabled(false);
        connect(m_btnApplyFeats, &QPushButton::clicked,
                this, &CloudToolsDlg::onApplyFeaturesClicked);
        h->addWidget(m_btnApplyFeats);
        v->addLayout(h);

        m_tabs->addTab(page, tr("Features (pro)"));
    }

    // Footer — progress + status + Close
    {
        auto *h = new QHBoxLayout();
        m_progress = new QProgressBar();
        m_progress->setRange(0, 0);          // indeterminate spinner
        m_progress->setVisible(false);
        m_progress->setMaximumWidth(140);
        h->addWidget(m_progress);

        m_status = new QLabel();
        m_status->setStyleSheet("color:#8b949e; font-size:9pt;");
        h->addWidget(m_status, 1);

        auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(bb, &QDialogButtonBox::rejected, this, &CloudToolsDlg::reject);
        h->addWidget(bb);
        root->addLayout(h);
    }
}

void CloudToolsDlg::refreshAuthBadge()
{
    bool pro = m_client->hasProToken();
    if (pro) {
        m_authBadge->setText(tr("PRO"));
        m_authBadge->setStyleSheet(
            "padding:2px 8px; border-radius:8px;"
            "color:white; background:#2da44e; font-size:9pt; font-weight:bold;");
    } else {
        m_authBadge->setText(tr("FREE"));
        m_authBadge->setStyleSheet(
            "padding:2px 8px; border-radius:8px;"
            "color:white; background:#8b949e; font-size:9pt; font-weight:bold;");
    }
    if (m_featuresSummary)
        m_featuresSummary->setText(pro
            ? tr("Pro tier — token configured.  Click Analyse to fetch features.")
            : tr("Pro tier — requires API token.  Click Configure… to set one."));
    if (m_btnApplyFeats && !pro)
        m_btnApplyFeats->setEnabled(false);
}

void CloudToolsDlg::setBusy(bool busy, const QString &what)
{
    m_progress->setVisible(busy);
    m_status->setText(what);
    m_btnAnalyse->setEnabled(!busy);
    // Disable button enable state is owned by onDtcItemChanged (driven by
    // the user's checkbox selections); we only force it OFF while busy.
    if (m_btnDisableDtcs && busy)
        m_btnDisableDtcs->setEnabled(false);
    if (m_btnApplyFeats && busy)
        m_btnApplyFeats->setEnabled(false);
    else if (m_btnApplyFeats && m_featuresList && m_featuresList->count() > 0
             && m_client->hasProToken())
        m_btnApplyFeats->setEnabled(true);
}

// ─── Slots ──────────────────────────────────────────────────────────────────

void CloudToolsDlg::onConfigureClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Cloud configuration"));
    dlg.setMinimumWidth(440);

    auto *form = new QVBoxLayout(&dlg);

    auto *blurb = new QLabel(tr(
        "romHEX14 does not ship with a default cloud server.  Point this "
        "client at any compatible backend — the protocol and a reference "
        "implementation are documented under <code>server/</code> in the "
        "source tree."), &dlg);
    blurb->setWordWrap(true);
    blurb->setStyleSheet("color:#8b949e; font-size:9pt;");
    form->addWidget(blurb);

    auto *urlLbl = new QLabel(tr("Server URL"), &dlg);
    auto *urlEdit = new QLineEdit(m_client->baseUrl(), &dlg);
    urlEdit->setPlaceholderText(QStringLiteral("https://your-cloud.example.com"));
    form->addWidget(urlLbl);
    form->addWidget(urlEdit);

    auto *tokLbl = new QLabel(tr("API token  (optional — required for Pro features)"),
                              &dlg);
    auto *tokEdit = new QLineEdit(m_client->proToken(), &dlg);
    tokEdit->setEchoMode(QLineEdit::Password);
    tokEdit->setPlaceholderText(tr("leave empty to use the free DTC tier"));
    form->addWidget(tokLbl);
    form->addWidget(tokEdit);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                    &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addWidget(bb);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString url   = urlEdit->text().trimmed();
    const QString token = tokEdit->text();

    // Light validation: refuse obviously broken URLs but don't try to
    // probe network here — that's what Health does after Save.
    if (!url.isEmpty() && !url.startsWith("http://") && !url.startsWith("https://")) {
        QMessageBox::warning(this, tr("Cloud configuration"),
            tr("Server URL must start with http:// or https://"));
        return;
    }

    m_client->setBaseUrl(url);
    m_client->setProToken(token);

    refreshAuthBadge();
    m_status->setText(url.isEmpty()
        ? tr("Cloud disabled — set a server URL to enable.")
        : token.isEmpty()
            ? tr("Server saved (free tier — no token).")
            : tr("Server + token saved (Pro tier active)."));
}

void CloudToolsDlg::onAnalyseClicked()
{
    if (!m_project || m_project->currentData.isEmpty()) {
        QMessageBox::information(this, tr("Cloud Tools"),
            tr("No ROM data available in the current project."));
        return;
    }
    setBusy(true, tr("Analysing on server…"));
    const QString hint = familyHintFromProject();
    m_client->requestDtcAnalyze(m_project->currentData, hint);
    if (m_client->hasProToken())
        m_client->requestFeaturesDetect(m_project->currentData, hint);
}

void CloudToolsDlg::onDtcAnalyzeFinished(bool ok, const QJsonObject &result)
{
    if (!ok) {
        const QString detail = result.value("detail").toString();
        const QString err    = result.value("error").toString(tr("server error"));
        m_dtcSummary->setText(tr("Analyse failed: %1%2")
            .arg(err)
            .arg(detail.isEmpty() ? "" : QString(" — %1").arg(detail)));
        setBusy(false);
        return;
    }
    const QJsonObject ecu = result.value("ecu").toObject();
    m_lastEcuName  = ecu.value("name").toString();
    m_lastEcuMatch = ecu.value("match").toString();
    m_lastFid      = result.value("fid").toString();
    m_ecuLabel->setText(tr("ECU: %1  (%2)")
        .arg(m_lastEcuName.isEmpty() ? tr("unknown") : m_lastEcuName)
        .arg(m_lastEcuMatch.isEmpty() ? tr("no match") : m_lastEcuMatch));

    const int total = result.value("dtcs_total").toInt();
    const int off   = result.value("dtcs_off").toInt();
    const int on    = result.value("dtcs_on").toInt();
    m_dtcSummary->setText(tr("Found %1 DTCs  —  %2 currently ON, %3 OFF")
        .arg(total).arg(on).arg(off));

    // Populate per-DTC tree from the `dtcs` array.  Block itemChanged
    // during bulk insert so the selection counter doesn't recompute on
    // every row (much faster on 3 000-entry C64 lists).
    m_dtcTree->blockSignals(true);
    m_dtcTree->setSortingEnabled(false);
    m_dtcTree->clear();

    const QJsonArray rows = result.value("dtcs").toArray();
    for (const QJsonValue &v : rows) {
        const QJsonObject d = v.toObject();
        const QString code   = d.value("code").toString();
        const QString obd    = d.value("obd").toString();
        const QString man    = d.value("man").toString();
        const QString status = d.value("status").toString();
        const QString info   = d.value("info").toString();
        const int     idx    = d.value("idx").toInt(-1);

        // Show OBD code if present, otherwise the IDX_n placeholder.
        const QString display = obd.isEmpty() ? code : obd;
        auto *itm = new QTreeWidgetItem(m_dtcTree);
        itm->setFlags(itm->flags() | Qt::ItemIsUserCheckable);
        itm->setCheckState(0, Qt::Unchecked);
        itm->setText(1, display);
        itm->setText(2, man);
        itm->setText(3, status);
        itm->setText(4, info);
        // Server matches by `obd` / `man` / `dtc` field on the apply call,
        // so prefer the most-specific identifier we have.  Fall back to
        // the index-string when nothing else is set (those rows can't
        // really be selected meaningfully — the OFF guard skips them
        // anyway).
        const QString token = !obd.isEmpty() ? obd
                            : !code.isEmpty() ? code
                                              : QString::number(idx);
        itm->setData(0, Qt::UserRole, token);
        Q_UNUSED(idx);
        // Visual hint: dim rows that are already OFF (nothing to do).
        if (status == "OFF") {
            for (int c = 1; c <= 4; ++c) {
                QFont f = itm->font(c);
                f.setItalic(true);
                itm->setFont(c, f);
                itm->setForeground(c, QColor(0x8b, 0x94, 0x9e));
            }
        }
    }

    m_dtcTree->setSortingEnabled(true);
    m_dtcTree->sortItems(1, Qt::AscendingOrder);
    m_dtcTree->blockSignals(false);

    const bool hasRows = m_dtcTree->topLevelItemCount() > 0;
    m_btnSelectOn->setEnabled(hasRows && on > 0);
    m_btnSelectNone->setEnabled(hasRows);
    m_dtcSelCount->setText(tr("0 selected"));
    setBusy(false);
}

void CloudToolsDlg::onFeaturesDetectFinished(bool ok, const QJsonObject &result)
{
    m_featuresList->clear();
    if (!ok) {
        const QString err = result.value("error").toString(tr("not authorised"));
        m_featuresSummary->setText(tr("Features detect failed: %1").arg(err));
        return;
    }
    const QJsonObject feats = result.value("available").toObject();
    if (feats.isEmpty()) {
        m_featuresSummary->setText(tr("No features available for this ECU."));
        return;
    }
    m_featuresSummary->setText(tr("%1 features available — select to apply.")
        .arg(feats.size()));

    for (auto it = feats.constBegin(); it != feats.constEnd(); ++it) {
        const QString id = it.key();
        const QJsonObject info = it.value().toObject();
        const QString label = info.value("label").toString(id.toUpper());
        QString hints;
        if (info.value("has_map_mods").toBool()) hints += " maps";
        if (info.value("has_patches").toBool())  hints += " patches";
        const int dtcs = info.value("dtc_count").toInt();
        QString display = label;
        if (dtcs > 0) display += tr("  (%1 DTCs)").arg(dtcs);
        if (!hints.isEmpty()) display += "  ·" + hints;
        auto *itm = new QListWidgetItem(display);
        itm->setData(Qt::UserRole, id);
        m_featuresList->addItem(itm);
    }
    if (m_client->hasProToken() && m_featuresList->count() > 0)
        m_btnApplyFeats->setEnabled(true);
}

void CloudToolsDlg::onDtcSearchChanged(const QString &text)
{
    // Visibility-only filter — preserves checkbox state.  Match against
    // code, manufacturer code, and description (all case-insensitive).
    const QString needle = text.trimmed().toLower();
    for (int i = 0; i < m_dtcTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm = m_dtcTree->topLevelItem(i);
        bool show = needle.isEmpty();
        if (!show) {
            for (int c = 1; c <= 4; ++c) {
                if (itm->text(c).toLower().contains(needle)) {
                    show = true;
                    break;
                }
            }
        }
        itm->setHidden(!show);
    }
}

void CloudToolsDlg::onDtcSelectAllOn()
{
    m_dtcTree->blockSignals(true);
    for (int i = 0; i < m_dtcTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm = m_dtcTree->topLevelItem(i);
        if (itm->isHidden()) continue;          // respect search filter
        if (itm->text(3) != "ON") continue;
        itm->setCheckState(0, Qt::Checked);
    }
    m_dtcTree->blockSignals(false);
    onDtcItemChanged(nullptr, 0);               // refresh count + button
}

void CloudToolsDlg::onDtcSelectNone()
{
    m_dtcTree->blockSignals(true);
    for (int i = 0; i < m_dtcTree->topLevelItemCount(); ++i)
        m_dtcTree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
    m_dtcTree->blockSignals(false);
    onDtcItemChanged(nullptr, 0);
}

void CloudToolsDlg::onDtcItemChanged(QTreeWidgetItem * /*item*/, int /*col*/)
{
    int n = 0;
    for (int i = 0; i < m_dtcTree->topLevelItemCount(); ++i) {
        if (m_dtcTree->topLevelItem(i)->checkState(0) == Qt::Checked)
            ++n;
    }
    m_dtcSelCount->setText(tr("%1 selected").arg(n));
    m_btnDisableDtcs->setEnabled(n > 0);
}

void CloudToolsDlg::onDisableDtcsClicked()
{
    if (!m_project) return;

    // Collect the OBD code (or fallback identifier) of every checked
    // row.  Server matches against `obd`/`man`/`dtc` fields, so any of
    // those formats works (P0420 / U1112 / IDX_n).
    QStringList codes;
    for (int i = 0; i < m_dtcTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm = m_dtcTree->topLevelItem(i);
        if (itm->checkState(0) != Qt::Checked) continue;
        const QString token = itm->data(0, Qt::UserRole).toString();
        if (!token.isEmpty()) codes.append(token);
    }
    if (codes.isEmpty()) {
        QMessageBox::information(this, tr("Cloud Tools"),
            tr("No DTCs selected."));
        return;
    }
    setBusy(true, tr("Disabling %1 DTC(s)…").arg(codes.size()));
    m_client->requestDtcDisable(m_project->currentData, codes.join(","),
                                familyHintFromProject());
}

void CloudToolsDlg::onApplyFeaturesClicked()
{
    if (!m_project) return;
    QStringList chosen;
    for (auto *itm : m_featuresList->selectedItems())
        chosen.append(itm->data(Qt::UserRole).toString());
    if (chosen.isEmpty()) {
        QMessageBox::information(this, tr("Cloud Tools"),
            tr("Select at least one feature to apply."));
        return;
    }
    setBusy(true, tr("Applying %1 feature(s)…").arg(chosen.size()));
    m_client->requestFeaturesApply(m_project->currentData, chosen,
                                   familyHintFromProject());
}

void CloudToolsDlg::applyPatchedRom(const QByteArray &patched,
                                    const QJsonObject &meta)
{
    Q_UNUSED(meta);
    // Replace the project's currentData and notify the rest of the app.
    m_project->currentData = patched;
    m_project->modified = true;
    emit m_project->dataChanged();
    QMessageBox::information(this, tr("Cloud Tools"),
        tr("Patched ROM applied to the active project.  Save to persist."));
}

void CloudToolsDlg::onDtcDisableFinished(bool ok, const QByteArray &patched,
                                         const QJsonObject &meta)
{
    setBusy(false);
    if (!ok) {
        QMessageBox::warning(this, tr("Cloud Tools"),
            tr("Disable DTCs failed: %1")
                .arg(meta.value("error").toString(tr("server error"))));
        return;
    }
    applyPatchedRom(patched, meta);
}

void CloudToolsDlg::onFeaturesApplyFinished(bool ok, const QByteArray &patched,
                                            const QJsonObject &meta)
{
    setBusy(false);
    if (!ok) {
        QMessageBox::warning(this, tr("Cloud Tools"),
            tr("Apply features failed: %1")
                .arg(meta.value("error").toString(tr("server error"))));
        return;
    }
    if (m_featuresLog) {
        m_featuresLog->appendPlainText(
            tr("[%1]  Patched ROM received: %2 bytes")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                .arg(patched.size()));
    }
    applyPatchedRom(patched, meta);
}

void CloudToolsDlg::onNetworkError(const QString &what, const QString &detail)
{
    setBusy(false);
    m_status->setText(tr("Network error (%1): %2").arg(what, detail));
}
