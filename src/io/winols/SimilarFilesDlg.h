/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Find Similar Files dialog (Sprint I).
 *
 * Mirrors the WinOLS "Similar import objects" wizard step in layout:
 * `% Project | % Data area | Source | Make | Model | ECU | SW`,
 * with a min-similarity slider and per-row tooltips.  Uses our
 * `SimilarityIndex` for the lookup; cross-references each match
 * with the WinOLS catalog (Cache_*.db) for richer metadata
 * (vehicle, ECU, software number) when the file is one we already
 * know about.
 *
 * Bundled with `BuildIndexProgressDlg` (private subclass) that
 * runs the first-time index build with progress + cancel + ETA.
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QString>
#include <QVector>

#include <atomic>

class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class QTreeWidget;
class QTreeWidgetItem;

namespace winols {

class Config;
class SimilarityIndex;
struct CatalogRecord;
struct RomFingerprint;
struct SimilarityMatch;

class SimilarFilesDlg : public QDialog {
    Q_OBJECT
public:
    SimilarFilesDlg(const QString &sourcePath,
                    const QByteArray &romBytes,
                    QWidget *parent = nullptr);
    ~SimilarFilesDlg() override;

    /// Path the user double-clicked / picked, or empty if cancelled.
    QString chosenPath() const { return m_chosen; }

signals:
    void openSimilarRequested(const QString &path);
    void compareWithRequested(const QString &path);

private slots:
    void onMinChanged(int v);
    void onRowActivated(QTreeWidgetItem *it, int col);
    void onRebuildIndex();

private:
    void buildUi();
    void runQuery();
    void onQueryFinished(const QVector<SimilarityMatch> &matches, qint64 ms);
    void populateTable(const QVector<SimilarityMatch> &matches);
    void enrichFromCatalog();
    void applyCatalogToRows();

    QString m_sourcePath;
    QByteArray m_romBytes;
    QString m_chosen;

    Config            *m_cfg   = nullptr;
    SimilarityIndex   *m_index = nullptr;
    RomFingerprint    *m_needle = nullptr;
    std::atomic<bool> m_catalogLoading{false};

    QTreeWidget  *m_tree     = nullptr;
    QSlider      *m_minSlide = nullptr;
    QLabel       *m_minLabel = nullptr;
    QLabel       *m_status   = nullptr;
    QProgressBar *m_busyBar  = nullptr;
    QPushButton  *m_openBtn  = nullptr;
    QPushButton  *m_cmpBtn   = nullptr;
    QPushButton  *m_rebuild  = nullptr;
    std::atomic<bool> m_queryRunning{false};
};

}  // namespace winols
