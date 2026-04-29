/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QList>

class IntroWizard : public QDialog {
    Q_OBJECT

public:
    explicit IntroWizard(QWidget *parent = nullptr);

    /// Shows the wizard only on first launch (checks QSettings flag).
    static void showIfFirstLaunch(QWidget *parent);

protected:
    void paintEvent(QPaintEvent *e) override;
    void timerEvent(QTimerEvent *e) override;

private:
    void buildSlides();
    QWidget *makeSlide(const QString &iconText, const QString &iconStyle,
                       const QString &title, const QString &desc,
                       const QString &featureHighlight,
                       QWidget *extraContent = nullptr);
    void goToSlide(int index);
    void updateButtons();
    void animateSlideContent(int index);

    QStackedWidget *m_stack = nullptr;
    QList<QLabel *> m_dots;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_skipBtn = nullptr;
    int m_currentSlide = 0;
    int m_slideCount = 0;

    // Background glow animation
    int m_bgTimerId = 0;
    qreal m_glowPhase = 0.0;
};
