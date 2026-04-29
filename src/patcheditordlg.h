/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QSplitter>
#include "project.h"
#include "rompatch.h"

// Script editor for .rxpatch files.
// Can open an existing patch or receive one generated from a diff.
// Supports: preview, label edit, apply to current ROM, apply to linked ROM, save.
class PatchEditorDlg : public QDialog {
    Q_OBJECT

public:
    // Open with a patch generated from a diff
    PatchEditorDlg(const RomPatch &patch, Project *project, QWidget *parent = nullptr);
    // Open an existing .rxpatch file
    explicit PatchEditorDlg(Project *project, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onApplyCurrent();
    void onApplyLinked();
    void onApplyFile();
    void onOpen();

private:
    void build();
    void loadPatch(const RomPatch &p);
    RomPatch currentPatch() const;   // parse from editor JSON
    void applyToRom(QByteArray &rom,
                    const QString &romLabel,
                    const QMap<QString, uint32_t> &offsets = {});

    static QString safeFileName(const QString &label);

    Project        *m_project = nullptr;
    RomPatch        m_patch;

    QLineEdit      *m_labelEdit       = nullptr;
    QLineEdit      *m_srcEdit         = nullptr;
    QLineEdit      *m_tgtEdit         = nullptr;
    QTableWidget   *m_mapTable        = nullptr;
    QPlainTextEdit *m_jsonEdit        = nullptr;
    QLabel         *m_statusLabel     = nullptr;
    QPushButton    *m_applyCurrentBtn = nullptr;
    QPushButton    *m_applyLinkedBtn  = nullptr;
};
