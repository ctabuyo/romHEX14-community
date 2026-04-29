/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QObject>
#include <QColor>

struct AppColors {
    // ── Map highlight bands ─────────────────────────────────────────────────
    // Applied to map regions in the hex editor, 2D waveform, and map overlay
    QColor mapBand[5];

    // ── Waveform curve row colors (2D view) ─────────────────────────────────
    QColor waveRow[8];

    // ── Hex editor ──────────────────────────────────────────────────────────
    QColor hexBg;          // cell area background
    QColor hexText;        // normal byte text
    QColor hexModified;    // modified byte text / bar highlight
    QColor hexSelected;    // selected cell fill
    QColor hexOffset;      // offset column + ASCII/bar sidebar
    QColor hexHeaderBg;    // column header strip background
    QColor hexHeaderText;  // column header labels
    QColor hexBarDefault;  // bar-view default bar color (no region)

    // ── 2D Waveform view ────────────────────────────────────────────────────
    QColor waveBg;         // plot area background
    QColor waveGridMajor;  // major grid lines
    QColor waveGridMinor;  // minor grid lines
    QColor waveLine;       // ROM waveform line color
    QColor waveOverviewBg; // minimap strip background

    // ── Map Overlay ──────────────────────────────────────────────────────────
    QColor mapCellBg;       // cell background when heat map is off
    QColor mapCellText;     // cell text when heat map is off
    QColor mapCellModified; // modified cell text when heat map is off
    QColor mapGridLine;     // grid line color when heat map is off
    QColor mapAxisXBg;      // X axis (column) header background
    QColor mapAxisXText;    // X axis header text
    QColor mapAxisYBg;      // Y axis (row) header background
    QColor mapAxisYText;    // Y axis header text

    // ── General UI ──────────────────────────────────────────────────────────
    QColor uiBg;        // main window / MDI area
    QColor uiPanel;     // sidebars, toolbars, panel headers
    QColor uiBorder;    // dividers and borders
    QColor uiText;      // primary text
    QColor uiTextDim;   // secondary / dimmed labels
    QColor uiAccent;    // highlight color (links, selection, active)
};

class AppConfig : public QObject {
    Q_OBJECT
public:
    static AppConfig &instance();

    // ── AI assistant write permission mode ──────────────────────────────────
    // Mirrors Claude Code's permission-mode concept: the user picks once per
    // session how proposed changes are handled, instead of per-call popups.
    enum class PermissionMode {
        Ask         = 0,   ///< Inline confirmation card before every write (default)
        AutoAccept  = 1,   ///< Apply writes immediately, log a card for visibility
        Plan        = 2,   ///< Reject writes; the AI must explain instead
    };

    AppColors      colors;
    bool           showLongMapNames = true;                 ///< Show map description instead of short name
    PermissionMode aiPermissionMode = PermissionMode::Ask;  ///< AI assistant write-tool gate

    void load();
    void save();
    void resetToDefaults();

signals:
    void colorsChanged();
    void displaySettingsChanged();
    void aiPermissionModeChanged(PermissionMode mode);

private:
    AppConfig();
    static void applyDefaults(AppColors &c);
};
