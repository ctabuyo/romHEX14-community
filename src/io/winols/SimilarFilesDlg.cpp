/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "io/winols/SimilarFilesDlg.h"
#include "io/winols/BuildIndexProgressDlg.h"
#include "io/winols/SimilarityIndex.h"
#include "io/winols/WinOlsConfig.h"
#include "io/winols/WinOlsCatalogReader.h"
#include "io/winols/RomFingerprint.h"

#include "debug/DebugLog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QTime>
#include <QTimer>
#include <QSettings>
#include <QSlider>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace winols {

// ─── BuildIndexProgressDlg ──────────────────────────────────────────────────
// Moved to its own header (BuildIndexProgressDlg.h) so MainWindow can
// also instantiate it for the explicit "Index WinOLS catalog now" entry.
//
// Old inline version retained below as a no-op compile guard.
#if 0
namespace {
class BuildIndexProgressDlg : public QDialog {
public:
    BuildIndexProgressDlg(SimilarityIndex *idx, const QStringList &roots,
                          QWidget *parent)
        : QDialog(parent), m_idx(idx)
    {
        setWindowTitle(QObject::tr("Building similarity index"));
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

        // ── Header ──
        auto *header = new QLabel(QObject::tr("Indexing ROM fingerprints"));
        header->setObjectName("header");
        root->addWidget(header);

        m_subhead = new QLabel(QObject::tr(
            "Computing similarity fingerprints for every .ols / .kp / .bin "
            "below the configured WinOLS roots.  This is a one-time scan; "
            "subsequent searches will be instant."));
        m_subhead->setWordWrap(true);
        m_subhead->setStyleSheet("color:#8b949e;");
        root->addWidget(m_subhead);

        // ── Big progress bar ──
        m_bar = new QProgressBar();
        m_bar->setRange(0, 100);
        m_bar->setMinimumHeight(22);
        m_bar->setFormat(QStringLiteral("%p%   ·   %v / %m"));
        root->addWidget(m_bar);

        // ── Stats grid (4 columns × 2 rows of caption/value pairs) ──
        auto *grid = new QGridLayout();
        grid->setHorizontalSpacing(28);
        grid->setVerticalSpacing(2);

        auto addStat = [&](int col, const QString &cap,
                           QLabel **value) -> void {
            auto *c = new QLabel(cap); c->setObjectName("caption");
            *value  = new QLabel(QStringLiteral("—"));
            (*value)->setObjectName("big");
            grid->addWidget(c,      0, col);
            grid->addWidget(*value, 1, col);
        };
        addStat(0, QObject::tr("Files"),    &m_lblFiles);
        addStat(1, QObject::tr("Rate"),     &m_lblRate);
        addStat(2, QObject::tr("Elapsed"),  &m_lblElapsed);
        addStat(3, QObject::tr("ETA"),      &m_lblEta);
        root->addLayout(grid);

        // ── Current file ──
        auto *curCap = new QLabel(QObject::tr("Currently"));
        curCap->setObjectName("caption");
        root->addWidget(curCap);
        m_lblCurrent = new QLabel(QObject::tr("preparing scan…"));
        m_lblCurrent->setStyleSheet(
            "color:#e6edf3; font-family:Consolas,monospace; font-size:9pt;");
        m_lblCurrent->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_lblCurrent->setWordWrap(true);
        root->addWidget(m_lblCurrent);

        // ── Live log (last ~12 files) ──
        auto *logCap = new QLabel(QObject::tr("Recent activity"));
        logCap->setObjectName("caption");
        root->addWidget(logCap);
        m_log = new QPlainTextEdit();
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(200);
        m_log->setMinimumHeight(120);
        root->addWidget(m_log, 1);

        m_lblHint = new QLabel(QObject::tr(
            "Safe to leave running overnight.  Press Pause to suspend, "
            "Cancel to stop early — already-processed files are kept."));
        m_lblHint->setObjectName("tiny");
        m_lblHint->setWordWrap(true);
        root->addWidget(m_lblHint);

        // ── Buttons ──
        auto *btnRow = new QHBoxLayout();
        m_pause  = new QPushButton(QObject::tr("Pause"));
        m_pause->setObjectName("pause");
        m_cancel = new QPushButton(QObject::tr("Cancel"));
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
            m_pause->setText(nowPaused ? QObject::tr("Resume")
                                       : QObject::tr("Pause"));
            m_lblHint->setText(nowPaused
                ? QObject::tr("Paused.  No files are being read.  "
                              "Click Resume to continue.")
                : QObject::tr("Safe to leave running overnight.  "
                              "Press Pause to suspend, Cancel to stop."));
        });
        connect(m_cancel, &QPushButton::clicked, this, [this]() {
            m_idx->requestCancel();
            m_cancel->setEnabled(false);
            m_cancel->setText(QObject::tr("Cancelling…"));
            m_lblHint->setText(QObject::tr(
                "Stopping after the current file finishes…"));
        });

        // ── Tick timer keeps Elapsed counter ticking even between
        //    progress() events (which only fire every ~10 files).
        m_tickWall.start();
        m_tick = new QTimer(this);
        m_tick->setInterval(500);
        connect(m_tick, &QTimer::timeout, this, &BuildIndexProgressDlg::tick);
        m_tick->start();

        // Worker
        m_watcher = new QFutureWatcher<void>(this);
        connect(m_watcher, &QFutureWatcher<void>::finished, this, [this]() {
            if (!m_done) accept();
        });
        QFuture<void> fut = QtConcurrent::run([idx, roots]() {
            idx->rebuild(roots);
        });
        m_watcher->setFuture(fut);
    }

private:
    static QString humanDuration(qint64 ms)
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

    void tick()
    {
        m_lblElapsed->setText(humanDuration(m_tickWall.elapsed()));
    }

    void onProgress(int processed, int total, qint64 totalBytes,
                    qint64 elapsedMs, const QString &currentPath)
    {
        m_lastProcessed = processed;
        m_lastTotal     = total;
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
            m_lblEta->setText(QObject::tr("done"));
        }

        if (!currentPath.isEmpty()) {
            const QString fn = QFileInfo(currentPath).fileName();
            m_lblCurrent->setText(fn);
            m_lblCurrent->setToolTip(currentPath);
            // Append to log only when the file changes — avoids
            // hundreds of duplicate lines when a slow file takes
            // multiple progress() ticks.
            if (currentPath != m_lastPath) {
                m_log->appendPlainText(QStringLiteral("%1   %2")
                    .arg(QTime::currentTime().toString("HH:mm:ss"), fn));
                m_lastPath = currentPath;
            }
        }
    }

    void onFinished(int processed, bool cancelled)
    {
        m_done = true;
        m_tick->stop();
        m_subhead->setText(cancelled
            ? QObject::tr("Cancelled by user.  %1 files indexed.")
                  .arg(processed)
            : QObject::tr("Done — %1 files indexed and ready to query.")
                  .arg(processed));
        m_subhead->setStyleSheet(cancelled
            ? "color:#f85149;" : "color:#3fb950;");
        m_lblCurrent->setText(QStringLiteral("—"));
        m_lblHint->setText(QObject::tr("You can close this dialog."));
        m_pause->setEnabled(false);
        m_cancel->setText(QObject::tr("Close"));
        m_cancel->setEnabled(true);
        disconnect(m_cancel, nullptr, nullptr, nullptr);
        connect(m_cancel, &QPushButton::clicked, this, &QDialog::accept);
        // Force the bar to its final state so it doesn't look stuck.
        if (m_lastTotal > 0) m_bar->setValue(m_lastTotal);
    }

    SimilarityIndex *m_idx;
    QFutureWatcher<void> *m_watcher = nullptr;
    QLabel *m_subhead = nullptr, *m_lblFiles = nullptr, *m_lblRate = nullptr;
    QLabel *m_lblElapsed = nullptr, *m_lblEta = nullptr;
    QLabel *m_lblCurrent = nullptr, *m_lblHint = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_pause = nullptr, *m_cancel = nullptr;
    QTimer *m_tick = nullptr;
    QElapsedTimer m_tickWall;
    QString m_lastPath;
    int m_lastProcessed = 0, m_lastTotal = 0;
    bool m_done = false;
};

}  // namespace
#endif  // moved to BuildIndexProgressDlg.h

// ─── SimilarFilesDlg ────────────────────────────────────────────────────────

SimilarFilesDlg::SimilarFilesDlg(const QString &sourcePath,
                                 const QByteArray &romBytes,
                                 QWidget *parent)
    : QDialog(parent),
      m_sourcePath(sourcePath),
      m_romBytes(romBytes),
      m_cfg(new Config()),
      m_index(new SimilarityIndex(this))
{
    qCInfo(catFind) << "ctor sourcePath=" << sourcePath
                    << "romBytes=" << romBytes.size() << "B";
    setWindowTitle(tr("Find similar files / projects"));
    setMinimumSize(880, 520);
    buildUi();
    QString err;
    if (!m_index->open(&err)) {
        qCWarning(catFind) << "index open failed:" << err;
        QMessageBox::warning(this, tr("Find similar"),
            tr("Could not open similarity index:\n%1").arg(err));
    } else {
        qCInfo(catFind) << "index opened at" << m_index->dbPath();
    }
    const int rows = m_index->rowCount();
    qCInfo(catFind) << "index rowCount=" << rows;
    m_status->setText(tr("Index: %1 files cached").arg(rows));
    if (rows == 0) {
        qCInfo(catFind) << "empty index — scheduling onRebuildIndex";
        QTimer::singleShot(0, this, &SimilarFilesDlg::onRebuildIndex);
    } else {
        runQuery();
    }
}

SimilarFilesDlg::~SimilarFilesDlg()
{
    qCInfo(catFind) << "dtor — chosen=" << m_chosen;
    delete m_needle;
    delete m_cfg;
}

void SimilarFilesDlg::buildUi()
{
    setStyleSheet(
        "QDialog { background:#1c2128; color:#e6edf3; }"
        "QTreeWidget { background:#0d1117; color:#e6edf3; "
        "  alternate-background-color:#111820; border:1px solid #30363d; "
        "  selection-background-color:#1f6feb; }"
        "QHeaderView::section { background:#161b22; color:#8b949e; "
        "  border:none; border-right:1px solid #30363d; "
        "  border-bottom:1px solid #30363d; padding:4px 8px; }"
        "QPushButton { background:#21262d; color:#e6edf3; "
        "  border:1px solid #30363d; border-radius:4px; padding:5px 14px; }"
        "QPushButton:hover { background:#2d333b; }"
        "QPushButton#openBtn { border-color:#238636; color:#3fb950; }"
        "QLabel { color:#8b949e; }"
        "QSlider::groove:horizontal { height:6px; background:#30363d; "
        "  border-radius:3px; }"
        "QSlider::handle:horizontal { background:#58a6ff; width:12px; "
        "  margin:-4px 0; border-radius:6px; }");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *srcRow = new QHBoxLayout();
    auto *srcLbl = new QLabel(tr("Source:"));
    auto *srcVal = new QLabel(QFileInfo(m_sourcePath).fileName().isEmpty()
        ? tr("<active project>") : m_sourcePath);
    srcVal->setStyleSheet("color:#e6edf3;");
    srcVal->setToolTip(m_sourcePath);
    srcRow->addWidget(srcLbl);
    srcRow->addWidget(srcVal, 1);
    root->addLayout(srcRow);

    auto *minRow = new QHBoxLayout();
    auto *minTxt = new QLabel(tr("Min similarity:"));
    m_minSlide = new QSlider(Qt::Horizontal);
    m_minSlide->setRange(5, 99);
    m_minSlide->setValue(QSettings()
        .value(QStringLiteral("similarFiles/minPct"), 30).toInt());
    m_minLabel = new QLabel(QString::number(m_minSlide->value()) + "%");
    m_minLabel->setMinimumWidth(40);
    minRow->addWidget(minTxt);
    minRow->addWidget(m_minSlide, 1);
    minRow->addWidget(m_minLabel);
    root->addLayout(minRow);

    m_tree = new QTreeWidget();
    m_tree->setColumnCount(7);
    m_tree->setHeaderLabels({
        tr("% Match"), tr("Source"),
        tr("Make"), tr("Model"), tr("ECU"), tr("SW"), tr("File")});
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setStretchLastSection(true);
    root->addWidget(m_tree, 1);

    m_busyBar = new QProgressBar();
    m_busyBar->setRange(0, 0);
    m_busyBar->setMaximumHeight(3);
    m_busyBar->setTextVisible(false);
    m_busyBar->hide();
    root->addWidget(m_busyBar);

    m_status = new QLabel();
    root->addWidget(m_status);

    auto *btnRow = new QHBoxLayout();
    m_rebuild = new QPushButton(tr("Rebuild index"));
    m_openBtn = new QPushButton(tr("Open"));
    m_openBtn->setObjectName("openBtn");
    m_openBtn->setEnabled(false);
    m_cmpBtn  = new QPushButton(tr("Open as comparison"));
    m_cmpBtn->setEnabled(false);
    auto *closeBtn = new QPushButton(tr("Close"));
    btnRow->addWidget(m_rebuild);
    btnRow->addStretch();
    btnRow->addWidget(m_openBtn);
    btnRow->addWidget(m_cmpBtn);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    connect(m_minSlide, &QSlider::valueChanged,
            this, &SimilarFilesDlg::onMinChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &SimilarFilesDlg::onRowActivated);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        const bool any = !m_tree->selectedItems().isEmpty();
        m_openBtn->setEnabled(any);
        m_cmpBtn->setEnabled(any);
    });
    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        const auto sel = m_tree->selectedItems();
        if (!sel.isEmpty()) onRowActivated(sel.first(), 0);
    });
    connect(m_cmpBtn, &QPushButton::clicked, this, [this]() {
        const auto sel = m_tree->selectedItems();
        if (sel.isEmpty()) return;
        const QString p = sel.first()->data(0, Qt::UserRole).toString();
        if (!p.isEmpty()) {
            emit compareWithRequested(p);
            accept();
        }
    });
    connect(m_rebuild, &QPushButton::clicked,
            this, &SimilarFilesDlg::onRebuildIndex);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void SimilarFilesDlg::runQuery()
{
    qCInfo(catFind) << "runQuery start";
    if (!m_index || !m_index->isOpen()) {
        qCWarning(catFind) << "runQuery aborted: index not open";
        return;
    }
    if (!m_needle) {
        try {
            m_needle = new RomFingerprint(fingerprint(QByteArrayView(m_romBytes)));
            qCInfo(catFind) << "needle fingerprint computed; empty="
                            << m_needle->isEmpty();
        } catch (const std::exception &e) {
            qCCritical(catFind) << "fingerprint() threw:" << e.what();
            m_status->setText(tr("Could not fingerprint source ROM: %1")
                                  .arg(QString::fromUtf8(e.what())));
            return;
        } catch (...) {
            qCCritical(catFind) << "fingerprint() threw unknown exception";
            m_status->setText(tr("Could not fingerprint source ROM"));
            return;
        }
    }
    if (m_needle->isEmpty()) {
        qCWarning(catFind) << "needle empty — aborting query";
        m_status->setText(tr("Source ROM is empty or all-padding — nothing to compare."));
        return;
    }
    if (m_queryRunning.load()) {
        qCInfo(catFind) << "runQuery skipped: already running";
        return;
    }
    m_queryRunning.store(true);
    m_busyBar->show();
    m_minSlide->setEnabled(false);
    m_status->setText(tr("Searching %1 files…").arg(m_index->rowCount()));

    const RomFingerprint needle = *m_needle;
    const int minPct = m_minSlide->value();
    SimilarityIndex *idx = m_index;
    qCInfo(catFind) << "dispatching worker query: minPct=" << minPct
                    << " indexRows=" << idx->rowCount();

    auto *watcher = new QFutureWatcher<QPair<QVector<SimilarityMatch>, qint64>>(this);
    connect(watcher, &QFutureWatcher<QPair<QVector<SimilarityMatch>, qint64>>::finished,
            this, [this, watcher]() {
        try {
            auto result = watcher->result();
            qCInfo(catFind) << "worker finished: matches="
                            << result.first.size()
                            << " ms=" << result.second;
            onQueryFinished(result.first, result.second);
        } catch (const std::exception &e) {
            qCCritical(catFind) << "worker threw:" << e.what();
            m_queryRunning.store(false);
            m_busyBar->hide();
            m_minSlide->setEnabled(true);
            m_status->setText(tr("Search failed: %1")
                                  .arg(QString::fromUtf8(e.what())));
        } catch (...) {
            qCCritical(catFind) << "worker threw unknown exception";
            m_queryRunning.store(false);
            m_busyBar->hide();
            m_minSlide->setEnabled(true);
            m_status->setText(tr("Search failed"));
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([idx, needle, minPct]() {
        QElapsedTimer t; t.start();
        auto matches = idx->findSimilar(needle, minPct, 5000);
        return qMakePair(matches, t.elapsed());
    }));
}

void SimilarFilesDlg::onQueryFinished(const QVector<SimilarityMatch> &matches,
                                       qint64 ms)
{
    qCInfo(catFind) << "onQueryFinished: matches=" << matches.size()
                    << " ms=" << ms;
    m_queryRunning.store(false);
    m_busyBar->hide();
    m_minSlide->setEnabled(true);
    populateTable(matches);
    m_status->setText(tr("%1 matches · %2 ms · index size %3")
                          .arg(matches.size())
                          .arg(ms)
                          .arg(m_index->rowCount()));
    try {
        enrichFromCatalog();
    } catch (const std::exception &e) {
        qCCritical(catFind) << "enrichFromCatalog threw:" << e.what();
    } catch (...) {
        qCCritical(catFind) << "enrichFromCatalog threw unknown exception";
    }
    qCInfo(catFind) << "onQueryFinished done";
}

void SimilarFilesDlg::populateTable(const QVector<SimilarityMatch> &matches)
{
    qCInfo(catFind) << "populateTable: " << matches.size() << " matches";
    m_tree->setSortingEnabled(false);
    m_tree->clear();
    for (const auto &m : matches) {
        auto *it = new QTreeWidgetItem({
            QString("%1%").arg(m.score.wholePct(), 3),
            tr("(file)"),
            QString(), QString(), QString(), QString(),
            QFileInfo(m.path).fileName()
        });
        it->setData(0, Qt::UserRole, m.path);
        it->setData(0, Qt::UserRole + 1, double(m.score.wholeFile));
        it->setToolTip(6, m.path);
        m_tree->addTopLevelItem(it);
    }
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(0, Qt::DescendingOrder);
}

namespace {

// Process-wide WOLS catalog cache.  Built lazily on first Find-Similar-Files
// open (parallel SQLite reads of every Cache_*.db) and reused thereafter so
// repeated dialog opens within one rx14 session are instant.  Stale-after-edit
// is acceptable because the user can always restart rx14 (or hit "Rebuild
// index", which the dialog also exposes for the similarity index proper).
QMutex                          sCatalogMutex;
QHash<QString, CatalogRecord>   sCatalogCache;
std::atomic<bool>               sCatalogLoaded{false};

QStringList parseVersionNames(const QString &versionsInfo)
{
    if (versionsInfo.isEmpty()) return {};
    int parenOpen = versionsInfo.indexOf(QLatin1Char('('));
    int parenClose = versionsInfo.lastIndexOf(QLatin1Char(')'));
    if (parenOpen < 0 || parenClose <= parenOpen) return {};

    QString inner = versionsInfo.mid(parenOpen + 1, parenClose - parenOpen - 1);
    QStringList entries = inner.split(QStringLiteral(",\t"), Qt::SkipEmptyParts);
    if (entries.isEmpty())
        entries = inner.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QStringList names;
    names.reserve(entries.size());
    for (const QString &e : entries) {
        QString s = e.trimmed();
        int colon = s.indexOf(QStringLiteral(":\t"));
        if (colon < 0) colon = s.indexOf(QLatin1Char(':'));
        if (colon > 0) s = s.left(colon).trimmed();
        if (!s.isEmpty()) names.append(s);
    }
    return names;
}

}  // anon namespace

void SimilarFilesDlg::enrichFromCatalog()
{
    if (!m_cfg) {
        qCWarning(catFind) << "enrichFromCatalog aborted: no Config";
        return;
    }
    // Fast path: process-wide cache already populated from an earlier dialog
    // instance.  No SQLite reads, no worker dispatch — just paint.
    if (sCatalogLoaded.load(std::memory_order_acquire)) {
        qCInfo(catFind) << "enrichFromCatalog: using process-wide cache ("
                        << sCatalogCache.size() << "entries)";
        applyCatalogToRows();
        return;
    }
    if (m_catalogLoading.load()) {
        qCInfo(catFind) << "enrichFromCatalog: load already in flight";
        return;
    }
    qCInfo(catFind) << "enrichFromCatalog start (async)";
    m_catalogLoading.store(true);

    Config *cfg = m_cfg;
    auto *watcher =
        new QFutureWatcher<QHash<QString, CatalogRecord>>(this);
    connect(watcher, &QFutureWatcher<QHash<QString, CatalogRecord>>::finished,
            this, [this, watcher]() {
        m_catalogLoading.store(false);
        try {
            auto map = watcher->result();
            qCInfo(catFind) << "catalog worker finished:" << map.size()
                            << "entries";
            // Publish to the process-wide cache so the next dialog opens
            // instantly.
            {
                QMutexLocker lock(&sCatalogMutex);
                sCatalogCache = std::move(map);
                sCatalogLoaded.store(true, std::memory_order_release);
            }
            applyCatalogToRows();
        } catch (const std::exception &e) {
            qCCritical(catFind) << "catalog worker threw:" << e.what();
        } catch (...) {
            qCCritical(catFind) << "catalog worker threw unknown exception";
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([cfg]() {
        QElapsedTimer t; t.start();
        const auto dbs = cfg->discoverCacheDbs();
        qCInfo(catFind) << "[worker] discovered" << dbs.size() << "Cache_*.db";
        // Read DBs in parallel — each WinOlsCatalogReader uses a UUID-named
        // SQLite connection so concurrent reads don't conflict.  29 DBs
        // sequentially = ~7s; parallel across 8 cores = ~1-2s.
        QVector<QVector<CatalogRecord>> perDb =
            QtConcurrent::blockingMapped(dbs, [](const QString &dbPath) {
                return WinOlsCatalogReader::dumpAll(dbPath);
            });
        qCInfo(catFind) << "[worker] parallel reads done in" << t.elapsed() << "ms";
        // Merge serially on this thread (QHash is not thread-safe).
        QHash<QString, CatalogRecord> byName;
        int totalRecs = 0;
        for (const auto &recs : perDb) totalRecs += recs.size();
        byName.reserve(totalRecs);
        for (const auto &recs : perDb) {
            for (const auto &r : recs) {
                if (!r.filename.isEmpty())
                    byName.insert(r.filename, r);
            }
        }
        qCInfo(catFind) << "[worker] catalog map:" << byName.size()
                        << "entries; total" << t.elapsed() << "ms";
        return byName;
    }));
}

void SimilarFilesDlg::applyCatalogToRows()
{
    if (!sCatalogLoaded.load(std::memory_order_acquire)) return;
    QElapsedTimer t; t.start();
    const int N = m_tree->topLevelItemCount();
    qCInfo(catFind) << "applyCatalogToRows: enriching" << N << "rows";

    // Batching: disable sorting + updates while mutating, otherwise every
    // setText triggers a re-sort + repaint.
    const bool wasSorting = m_tree->isSortingEnabled();
    m_tree->setSortingEnabled(false);
    m_tree->setUpdatesEnabled(false);

    QMutexLocker lock(&sCatalogMutex);
    const auto &byName = sCatalogCache;
    int matched = 0;
    for (int i = 0; i < N; ++i) {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        const QString fn = it->text(6);
        auto found = byName.constFind(fn);
        if (found == byName.constEnd()) continue;

        ++matched;
        const QString path = it->data(0, Qt::UserRole).toString();
        const auto &r = found.value();
        it->setText(1, QFileInfo(path).dir().dirName());
        it->setText(2, r.make);
        it->setText(3, r.model);
        it->setText(4, r.ecuModel);
        it->setText(5, r.swNumber);

        // populateTable() already set a path-only tooltip on column 6;
        // upgrade it with versions when we have them.  Setting tooltip on
        // every column of every row was the prior bottleneck (5000 × 7
        // setToolTip calls = ~2 s).  Hovering the File column to read the
        // tooltip is the natural interaction.
        const QStringList vers = parseVersionNames(r.versionsInfo);
        if (!vers.isEmpty()) {
            it->setToolTip(6, path + QStringLiteral("\nVersions: ")
                              + vers.join(QStringLiteral(", ")));
        }
    }

    m_tree->setUpdatesEnabled(true);
    m_tree->setSortingEnabled(wasSorting);
    qCInfo(catFind) << "applyCatalogToRows done in" << t.elapsed()
                    << "ms (" << matched << "matched of" << N << ")";
}

void SimilarFilesDlg::onMinChanged(int v)
{
    m_minLabel->setText(QString::number(v) + "%");
    QSettings().setValue(QStringLiteral("similarFiles/minPct"), v);
    if (m_queryRunning.load()) return;
    runQuery();
}

void SimilarFilesDlg::onRowActivated(QTreeWidgetItem *it, int)
{
    qCInfo(catFind) << "onRowActivated";
    if (!it) {
        qCWarning(catFind) << "onRowActivated: null item";
        return;
    }
    const QString p = it->data(0, Qt::UserRole).toString();
    qCInfo(catFind) << "  selected path=" << p;
    if (p.isEmpty()) {
        qCWarning(catFind) << "onRowActivated: empty path on item";
        return;
    }
    m_chosen = p;
    qCInfo(catFind) << "emitting openSimilarRequested + accept()";
    emit openSimilarRequested(p);
    accept();
}

void SimilarFilesDlg::onRebuildIndex()
{
    qCInfo(catFind) << "onRebuildIndex start";
    if (!m_cfg) m_cfg = new Config();
    QStringList roots = m_cfg->scanFallback();
    for (const QString &r : m_cfg->dbRoots()) roots << r;
    qCInfo(catFind) << "rebuild roots count=" << roots.size();
    if (roots.isEmpty()) {
        qCWarning(catFind) << "onRebuildIndex: no roots configured";
        QMessageBox::warning(this, tr("Find similar"),
            tr("No scan roots configured.  Open Project Manager → "
               "WOLS Catalog → Settings to import paths from "
               "ols.cfg first."));
        return;
    }
    BuildIndexProgressDlg dlg(m_index, roots, this);
    dlg.exec();
    qCInfo(catFind) << "rebuild dialog closed; running query";
    runQuery();
}

}  // namespace winols
