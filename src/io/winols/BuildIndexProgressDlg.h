/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Modal progress dialog for `SimilarityIndex::rebuild()`.
 *
 * Built for what may be a multi-hour overnight scan, so the UI is
 * deliberately verbose: big progress bar, files / rate / ETA grid,
 * scrolling activity log, pause / cancel.  Spawns the rebuild on a
 * `QtConcurrent::run` worker; cancel and pause are wired via the
 * SimilarityIndex's atomics.
 */

#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QString>
#include <QStringList>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTimer;

namespace winols {

class SimilarityIndex;

class BuildIndexProgressDlg : public QDialog {
    Q_OBJECT
public:
    BuildIndexProgressDlg(SimilarityIndex *idx,
                          const QStringList &roots,
                          QWidget *parent = nullptr);

private slots:
    void onProgress(int processed, int total, qint64 totalBytes,
                    qint64 elapsedMs, const QString &currentPath);
    void onFinished(int processed, bool cancelled);
    void onTick();

private:
    SimilarityIndex      *m_idx;
    QFutureWatcher<void> *m_watcher = nullptr;

    QLabel       *m_subhead    = nullptr;
    QLabel       *m_lblFiles   = nullptr;
    QLabel       *m_lblRate    = nullptr;
    QLabel       *m_lblElapsed = nullptr;
    QLabel       *m_lblEta     = nullptr;
    QLabel       *m_lblCurrent = nullptr;
    QLabel       *m_lblHint    = nullptr;
    QPlainTextEdit *m_log      = nullptr;
    QProgressBar *m_bar        = nullptr;
    QPushButton  *m_pause      = nullptr;
    QPushButton  *m_cancel     = nullptr;
    QTimer       *m_tick       = nullptr;
    QElapsedTimer m_tickWall;
    QString       m_lastPath;
    int           m_lastTotal = 0;
    bool          m_done = false;

    static QString humanDuration(qint64 ms);
};

}  // namespace winols
