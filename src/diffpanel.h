/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Differences panel — WinOLS-style side panel that lists every byte/word
 * that differs between two open projects (A and B).  Optionally, a third
 * project (C, the "target") can be picked, and the user can copy selected
 * differences from A or B into C at the same (or aligned) addresses.
 *
 * Alignment
 * ─────────
 * Two ROMs of the same firmware version compare byte-for-byte; two ROMs
 * of different builds typically have the same calibration data but at
 * different file offsets.  The panel models this via an `AlignmentMap`
 * per (A,B) pair: layer-1 is a single global delta the user can nudge
 * with ◄ ► buttons; layer-3+ adds per-region offsets discovered by
 * "Find selection in B" (graphical search) or by ghost-suggestion
 * background scans.
 *
 * Hosted in a QDockWidget on the right side of MainWindow.  Refreshes
 * automatically when the project list changes or when any open project's
 * data changes (provided the dock is visible).
 */

#pragma once

#include <QHash>
#include <QPair>
#include <QVector>
#include <QWidget>

#include "alignmentmap.h"
#include "romdata.h"   // ByteOrder

class Project;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;

class DiffPanel : public QWidget {
    Q_OBJECT
public:
    explicit DiffPanel(QWidget *parent = nullptr);

    /// Replace the picker contents with @p projects.  Called by MainWindow
    /// every time a project opens/closes.  The current A/B/C selection is
    /// preserved by file path when possible.
    void setProjects(const QVector<Project *> &projects);

    /// Hint that @p p is the user-active project — used as the default
    /// pre-selected value for picker A on the first call.
    void setActiveProject(Project *p);

    /// Push current toolbar display state down so cell formatting matches
    /// the rest of the app.  Called whenever MainWindow toggles
    /// dataSize/byteOrder/displayFmt/sign.
    void setDisplayParams(int dataSize, ByteOrder bo,
                          int displayFmt, bool isSigned);

    Project *projectA() const;
    Project *projectB() const;
    Project *projectC() const;

    /// Translate an address in A to the corresponding addresses in B / C
    /// using the current alignment map.  Returns -1 outside any region.
    /// Used by MainWindow when a diff row is clicked to navigate B and C.
    qint64 mapAtoB(qint64 addrA) const;
    qint64 mapAtoC(qint64 addrA) const;

    /// Translate any source-project address to its corresponding target
    /// address via the alignment, going through A as the canonical
    /// coordinate.  Returns -1 if either project is unaligned (outside
    /// any region).  Used by MainWindow's Sync Cursors handlers so
    /// scrolling B physically slides A and C with their respective
    /// deltas applied — this is what makes the waveform comparison feel
    /// "the way it should" instead of jumping to arbitrary positions.
    qint64 translate(Project *fromProj, qint64 fromAddr,
                     Project *toProj) const;

    /// Global delta of @p p relative to A (0 for A, deltaB for B,
    /// deltaC for C, -1 if @p p is none of A/B/C or no global region
    /// exists).  Layer-1 helper — for layer-4 (per-region) callers
    /// should use `translate()` with the actual address.
    qint64 globalDeltaToA(Project *p) const;

signals:
    /// User single-clicked a diff row.  Carries the *A-side* address —
    /// MainWindow translates it to B's and C's via mapAtoB/C() before
    /// scrolling each ProjectView.
    void rowActivated(quint32 addressA);

    /// User finished a copy operation.  @p target is the project whose
    /// bytes were just modified (so MainWindow can flash a toast / scroll
    /// it into view).  Optional — handler may ignore.
    void copyApplied(Project *target, int wordCount);

    /// The current pair's alignment changed (offset nudged, region
    /// added/edited/cleared).  MainWindow uses this to immediately
    /// re-sync open waveform/hex views so the user sees B's curve
    /// physically slide on screen as ◀ ▶ buttons are pressed.
    void alignmentChanged();

private slots:
    void onSelectionChanged();
    void onCopyClicked();
    void onTableActivated(int row, int col);
    void onPickerChanged();
    void onOffsetSpinBChanged(int v);
    void onOffsetSpinCChanged(int v);
    void onNudge(int delta, bool forC);
    void onResetAlignment();

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    void buildUi();
    void rebuildPickers();
    void recompute();
    void doCopy(bool selectedOnly);

    AlignmentMap *currentMap();           ///< per-pair, lazily created
    void          ensureGlobalRegion();   ///< guarantees layer-1 single region
    void          syncOffsetSpinsFromMap();

    Project *projectFromCombo(QComboBox *cb) const;
    int      wordSize() const { return m_dataSize > 0 ? m_dataSize : 1; }
    quint32  readWord(const QByteArray &d, qint64 addr) const;
    QString  formatAddr(qint64 addr) const;
    QString  formatWord(quint32 v) const;
    QString  formatDelta(quint32 a, quint32 b) const;

    // ── Data / state ──────────────────────────────────────────────────
    QVector<Project *> m_projects;
    Project *m_active = nullptr;

    int       m_dataSize    = 1;
    ByteOrder m_byteOrder   = ByteOrder::BigEndian;
    int       m_displayFmt  = 1;     // 0=dec 1=hex 2=bin 3=pct (we use hex by default)
    bool      m_isSigned    = false;

    // recompute() is expensive (full byte scan + per-cell QTableWidgetItem).
    // We skip it when the panel is hidden and rerun on first showEvent.
    bool      m_recomputePending = false;

    // Per-pair alignment maps, keyed by ordered (A*,B*) pointers so the
    // user's manual offsets persist across A/B picker swaps within one
    // session.  Sidecar persistence (warstwa 7) saves these to disk.
    using PairKey = QPair<Project *, Project *>;
    QHash<PairKey, AlignmentMap> m_alignments;
    PairKey makePairKey() const;

    // ── UI ────────────────────────────────────────────────────────────
    QComboBox    *m_pickA       = nullptr;
    QComboBox    *m_pickB       = nullptr;
    QComboBox    *m_pickC       = nullptr;
    QRadioButton *m_srcA        = nullptr;
    QRadioButton *m_srcB        = nullptr;
    QPushButton  *m_btnCopySel  = nullptr;
    QPushButton  *m_btnCopyAll  = nullptr;
    QLabel       *m_summary     = nullptr;
    QTableWidget *m_table       = nullptr;

    // Layer-1 alignment widgets (B and C have independent rows).  Each
    // row has: label, spinbox (signed offset in bytes), nudge buttons
    // ◄◄◄ ◄◄ ◄ ► ►► ►►► (steps ±256 / ±16 / ±1).
    QSpinBox     *m_spinOffsetB = nullptr;
    QSpinBox     *m_spinOffsetC = nullptr;
    QWidget      *m_rowAlignC   = nullptr;   // hidden when C is "— none —"
    QPushButton  *m_btnReset    = nullptr;
};
