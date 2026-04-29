/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include "project.h"

class QCloseEvent;

class NewProjectDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    // Fill in metadata on an existing Project object
    void applyTo(Project *project) const;

    QString romPath() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void browseROM();
    void onBrandChanged(const QString &brand);
    void onModelChanged(const QString &model);

private:
    void setupCompleter(QComboBox *combo);

    QLineEdit      *m_romPath       = nullptr;
    QComboBox      *m_brand         = nullptr;
    QComboBox      *m_model         = nullptr;
    QComboBox      *m_engine        = nullptr;
    QComboBox      *m_ecuType       = nullptr;
    QLineEdit      *m_swNumber      = nullptr;
    QComboBox      *m_transmission  = nullptr;
    QLineEdit      *m_displacement  = nullptr;
    QSpinBox       *m_year          = nullptr;
    QPlainTextEdit *m_notes         = nullptr;
    QPushButton    *m_btnOk         = nullptr;
};
