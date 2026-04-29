/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once
#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include "appconfig.h"

class QCloseEvent;

class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildColorsPage();
    void buildDisplayPage();
    void buildAIPage();
    void loadAIProviderFields(int index);
    void saveAISettings();

    QWidget *makeColorRow(const QString &label, QColor &colorRef);

    QListWidget    *m_nav   = nullptr;
    QStackedWidget *m_stack = nullptr;

    // Working copy of colors edited in the dialog
    AppColors m_working;

    // AI settings widgets
    QComboBox *m_aiProviderCombo = nullptr;
    QLineEdit *m_aiKeyEdit      = nullptr;
    QLineEdit *m_aiModelEdit    = nullptr;
    QLineEdit *m_aiUrlEdit      = nullptr;
    QCheckBox *m_showLongNamesCheck = nullptr;
    QLabel    *m_supportLabel       = nullptr;

    // AI provider registry (mirrors AIAssistant)
    struct AIProviderEntry {
        QString name;
        QString label;
        QString baseUrl;
        QString defaultModel;
        bool    isClaude = false;
        int     tier     = 2;  // 0 = green/best  1 = amber/good  2 = red/limited
    };
    QVector<AIProviderEntry> m_aiProviders;
};
