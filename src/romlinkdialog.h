/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QPlainTextEdit>
#include <QCheckBox>
#include "project.h"
#include "romlinker.h"

class QCloseEvent;

// Two-page wizard for linking a ROM file to a project.
// Page 0: file picker + label + link button + progress
// Page 1: results table
class RomLinkDialog : public QDialog {
    Q_OBJECT

public:
    explicit RomLinkDialog(Project *project, QWidget *parent = nullptr);

    // After exec() == Accepted, use these to obtain results
    LinkedRom              result()  const { return m_result; }
    const RomLinkSession  &session() const { return m_session; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void browseFile();
    void startLink();
    void onProgress(const QString &msg, int pct);
    void onFinished(const RomLinkSession &session);

private:
    void buildPage0();
    void buildPage1(const RomLinkSession &session);
    void applyResult(const RomLinkSession &session);

    Project        *m_project   = nullptr;
    RomLinker      *m_linker    = nullptr;

    QStackedWidget *m_stack     = nullptr;
    QPushButton    *m_btnNext   = nullptr;

    // Page 0 widgets
    QLineEdit      *m_pathEdit  = nullptr;
    QLineEdit      *m_labelEdit = nullptr;
    QCheckBox      *m_refCheck  = nullptr;
    QProgressBar   *m_progress  = nullptr;
    QPlainTextEdit *m_log       = nullptr;

    // Page 1 widgets
    QTableWidget   *m_table     = nullptr;
    QLabel         *m_summaryL  = nullptr;

    LinkedRom       m_result;
    RomLinkSession  m_session;
};
