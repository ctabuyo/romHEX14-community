/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/BuildIndexProgressDlg.h"
#include "io/winols/SimilarityIndex.h"

#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace winols {

QString BuildIndexProgressDlg::humanDuration(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 s = ms / 1000;
    const qint64 h = s / 3600;
    const qint64 m = (s % 3600) / 60;
    const qint64 sec = s % 60;
    if (h > 0)
        return QStringLiteral("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'));
    if (m > 0)
        return QStringLiteral("%1m %2s").arg(m).arg(sec, 2, 10, QChar('0'));
    return QStringLiteral("%1s").arg(sec);
}

BuildIndexProgressDlg::BuildIndexProgressDlg(SimilarityIndex *idx,
                                             const QStringList &roots,
                                             QWidget *parent)
    : QDialog(parent), m_idx(idx)
{
    setWindowTitle(tr("Building similarity index"));
    setModal(true);
    setMinimumSize(720, 540);
    setStyleSheet(
        "QDialog { background:#0d1117; color:#e6edf3; }"
        "QLabel { color:#c9d1d9; }"
        "QLabel#header  { color:#58a6ff; font-size:14pt; font-weight:600; }"
        "QLabel#caption { color:#8b949e; font-size:9pt; "
        "  text-transform:uppercase; letter-spacing:1px; }"
        "QLabel#big     { color:#e6edf3; font-size:18pt; "
        "  font-family:'Consolas','Courier New',monospace; }"
        "QLabel#tiny    { color:#8b949e; font-size:8pt; }"
        "QPushButton { background:#21262d; color:#e6edf3;"
        "  border:1px solid #30363d; border-radius:4px;"
        "  padding:6px 18px; min-width:80px; }"
        "QPushButton:hover { background:#2d333b; }"
        "QPushButton:disabled { color:#484f58; border-color:#21262d; }"
        "QPushButton#pause { border-color:#d29922; color:#e3b341; }"
        "QPushButton#cancel { border-color:#da3633; color:#f85149; }"
        "QProgressBar {"
        "  background:#161b22; border:1px solid #30363d;"
        "  border-radius:4px; height:18px; text-align:center;"
        "  color:#e6edf3; font-weight:600;"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #1f6feb, stop:1 #58a6ff);"
        "  border-radius:3px;"
        "}"
        "QPlainTextEdit { background:#161b22; color:#8b949e;"
        "  border:1px solid #30363d; border-radius:4px;"
        "  font-family:'Consolas','Courier New',monospace; "
        "  font-size:8pt; }");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

    auto *header = new QLabel(tr("Indexing ROM fingerprints"));
    header->setObjectName("header");
    root->addWidget(header);

    m_subhead = new QLabel(tr(
        "Computing similarity fingerprints for every .ols / .kp / .bin "
        "below the configured WOLS catalog roots.  This is a one-time scan; "
        "subsequent searches will be instant."));
    m_subhead->setWordWrap(true);
    m_subhead->setStyleSheet("color:#8b949e;");
    root->addWidget(m_subhead);

    m_bar = new QProgressBar();
    m_bar->setRange(0, 100);
    m_bar->setMinimumHeight(22);
    m_bar->setFormat(QStringLiteral("%p%   ·   %v / %m"));
    root->addWidget(m_bar);

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(28);
    grid->setVerticalSpacing(2);
    auto addStat = [&](int col, const QString &cap, QLabel **value) {
        auto *c = new QLabel(cap); c->setObjectName("caption");
        *value  = new QLabel(QStringLiteral("—"));
        (*value)->setObjectName("big");
        grid->addWidget(c,      0, col);
        grid->addWidget(*value, 1, col);
    };
    addStat(0, tr("Files"),    &m_lblFiles);
    addStat(1, tr("Rate"),     &m_lblRate);
    addStat(2, tr("Elapsed"),  &m_lblElapsed);
    addStat(3, tr("ETA"),      &m_lblEta);
    root->addLayout(grid);

    auto *curCap = new QLabel(tr("Currently"));
    curCap->setObjectName("caption");
    root->addWidget(curCap);
    m_lblCurrent = new QLabel(tr("preparing scan…"));
    m_lblCurrent->setStyleSheet(
        "color:#e6edf3; font-family:Consolas,monospace; font-size:9pt;");
    m_lblCurrent->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_lblCurrent->setWordWrap(true);
    root->addWidget(m_lblCurrent);

    auto *logCap = new QLabel(tr("Recent activity"));
    logCap->setObjectName("caption");
    root->addWidget(logCap);
    m_log = new QPlainTextEdit();
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(200);
    m_log->setMinimumHeight(120);
    root->addWidget(m_log, 1);

    m_lblHint = new QLabel(tr(
        "Safe to leave running overnight.  Press Pause to suspend, "
        "Cancel to stop early — already-processed files are kept."));
    m_lblHint->setObjectName("tiny");
    m_lblHint->setWordWrap(true);
    root->addWidget(m_lblHint);

    auto *btnRow = new QHBoxLayout();
    m_pause  = new QPushButton(tr("Pause"));
    m_pause->setObjectName("pause");
    m_cancel = new QPushButton(tr("Cancel"));
    m_cancel->setObjectName("cancel");
    btnRow->addStretch();
    btnRow->addWidget(m_pause);
    btnRow->addWidget(m_cancel);
    root->addLayout(btnRow);

    connect(m_idx, &SimilarityIndex::progress,
            this,  &BuildIndexProgressDlg::onProgress,
            Qt::QueuedConnection);
    connect(m_idx, &SimilarityIndex::rebuildFinished,
            this,  &BuildIndexProgressDlg::onFinished,
            Qt::QueuedConnection);
    connect(m_pause, &QPushButton::clicked, this, [this]() {
        const bool nowPaused = !m_idx->isPaused();
        m_idx->setPaused(nowPaused);
        m_pause->setText(nowPaused ? tr("Resume") : tr("Pause"));
        m_lblHint->setText(nowPaused
            ? tr("Paused.  No files are being read.  Click Resume to continue.")
            : tr("Safe to leave running overnight.  "
                 "Press Pause to suspend, Cancel to stop."));
    });
    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        m_idx->requestCancel();
        m_cancel->setEnabled(false);
        m_cancel->setText(tr("Cancelling…"));
        m_lblHint->setText(tr(
            "Stopping after the current file finishes…"));
    });

    m_tickWall.start();
    m_tick = new QTimer(this);
    m_tick->setInterval(500);
    connect(m_tick, &QTimer::timeout, this, &BuildIndexProgressDlg::onTick);
    m_tick->start();

    m_watcher = new QFutureWatcher<void>(this);
    connect(m_watcher, &QFutureWatcher<void>::finished, this, [this]() {
        if (!m_done) accept();
    });
    QFuture<void> fut = QtConcurrent::run([idx, roots]() {
        idx->rebuild(roots);
    });
    m_watcher->setFuture(fut);
}

void BuildIndexProgressDlg::onTick()
{
    m_lblElapsed->setText(humanDuration(m_tickWall.elapsed()));
}

void BuildIndexProgressDlg::onProgress(int processed, int total,
                                       qint64 totalBytes, qint64 elapsedMs,
                                       const QString &currentPath)
{
    m_lastTotal = total;
    if (total > 0) m_bar->setRange(0, total);
    m_bar->setValue(processed);

    m_lblFiles->setText(QStringLiteral("%1 / %2")
        .arg(processed).arg(total > 0 ? QString::number(total)
                                      : QStringLiteral("?")));

    const double mbps = elapsedMs > 0
        ? (double(totalBytes) / (1024.0 * 1024.0))
          / (double(elapsedMs) / 1000.0)
        : 0.0;
    m_lblRate->setText(QStringLiteral("%1 MB/s").arg(mbps, 0, 'f', 1));
    m_lblElapsed->setText(humanDuration(elapsedMs));

    if (processed > 0 && total > processed) {
        const double pct = double(processed) / double(total);
        const qint64 etaMs = qint64(double(elapsedMs) / pct * (1.0 - pct));
        m_lblEta->setText(humanDuration(etaMs));
    } else if (total > 0 && processed >= total) {
        m_lblEta->setText(tr("done"));
    }

    if (!currentPath.isEmpty()) {
        const QString fn = QFileInfo(currentPath).fileName();
        m_lblCurrent->setText(fn);
        m_lblCurrent->setToolTip(currentPath);
        if (currentPath != m_lastPath) {
            m_log->appendPlainText(QStringLiteral("%1   %2")
                .arg(QTime::currentTime().toString("HH:mm:ss"), fn));
            m_lastPath = currentPath;
        }
    }
}

void BuildIndexProgressDlg::onFinished(int processed, bool cancelled)
{
    m_done = true;
    m_tick->stop();
    m_subhead->setText(cancelled
        ? tr("Cancelled by user.  %1 files indexed.").arg(processed)
        : tr("Done — %1 files indexed and ready to query.").arg(processed));
    m_subhead->setStyleSheet(cancelled ? "color:#f85149;" : "color:#3fb950;");
    m_lblCurrent->setText(QStringLiteral("—"));
    m_lblHint->setText(tr("You can close this dialog."));
    m_pause->setEnabled(false);
    m_cancel->setText(tr("Close"));
    m_cancel->setEnabled(true);
    disconnect(m_cancel, nullptr, nullptr, nullptr);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::accept);
    if (m_lastTotal > 0) m_bar->setValue(m_lastTotal);
}

}  // namespace winols
