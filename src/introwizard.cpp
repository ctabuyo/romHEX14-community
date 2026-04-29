/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "introwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QSettings>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QRandomGenerator>
#define _USE_MATH_DEFINES
#include <cmath>

// ── Particle struct ──────────────────────────────────────────────────────────
struct Particle { float x, y, vx, vy, r, opacity; };
static QVector<Particle> s_particles;

static void initParticles(int w, int h) {
    s_particles.clear();
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 40; ++i) {
        Particle p;
        p.x = rng->bounded(w); p.y = rng->bounded(h);
        p.vx = (rng->bounded(100) - 50) / 180.0f;
        p.vy = (rng->bounded(100) - 50) / 180.0f;
        p.r = 1.0f + rng->bounded(15) / 10.0f;
        p.opacity = 0.08f + rng->bounded(25) / 100.0f;
        s_particles.append(p);
    }
}

// ── Style constants ──────────────────────────────────────────────────────────
static const char *kPrimaryBtnSS =
    "QPushButton {"
    "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
    "    stop:0 #3a91d0, stop:1 #7c3aed);"
    "  color: #ffffff; border: none; border-radius: 10px;"
    "  padding: 12px 36px; font-size: 11pt; font-weight: bold; }"
    "QPushButton:hover {"
    "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
    "    stop:0 #4da8e8, stop:1 #9b5cf5); }";

static const char *kGhostBtnSS =
    "QPushButton {"
    "  background: rgba(15,22,41,0.5); color: #a9b6d3;"
    "  border: 1px solid rgba(231,238,252,0.12); border-radius: 10px;"
    "  padding: 12px 28px; font-size: 10pt; font-weight: 500; }"
    "QPushButton:hover { border-color: #3a91d0; color: #e7eefc; }";

static const char *kSkipBtnSS =
    "QPushButton {"
    "  background: transparent; color: #484f58;"
    "  border: none; font-size: 9pt; padding: 6px 12px; }"
    "QPushButton:hover { color: #8b949e; }";

// ── Static entry point ──────────────────────────────────────────────────────
void IntroWizard::showIfFirstLaunch(QWidget *parent)
{
    QSettings s("CT14", "RX14");
    if (s.value("introShown", false).toBool())
        return;

    IntroWizard dlg(parent);
    dlg.exec();

    s.setValue("introShown", true);
}

// ── Constructor ──────────────────────────────────────────────────────────────
IntroWizard::IntroWizard(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setFixedSize(940, 600);
    setAttribute(Qt::WA_TranslucentBackground, false);

    initParticles(940, 600);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top gradient line
    auto *topLine = new QFrame;
    topLine->setFixedHeight(3);
    topLine->setStyleSheet(
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 transparent, stop:0.2 #3a91d0, stop:0.5 #7c3aed,"
        "stop:0.8 #3a91d0, stop:1 transparent);");
    root->addWidget(topLine);

    // Slide stack
    m_stack = new QStackedWidget;
    m_stack->setStyleSheet("QStackedWidget { background: transparent; }");
    root->addWidget(m_stack, 1);

    buildSlides();

    // ── Bottom bar ───────────────────────────────────────────────────────────
    auto *bottomBar = new QWidget;
    bottomBar->setFixedHeight(72);
    bottomBar->setStyleSheet("background: rgba(8,12,24,0.95); border-top: 1px solid rgba(58,145,208,0.15);");
    auto *blay = new QHBoxLayout(bottomBar);
    blay->setContentsMargins(32, 0, 32, 0);

    m_skipBtn = new QPushButton(tr("Skip"));
    m_skipBtn->setStyleSheet(kSkipBtnSS);
    m_skipBtn->setCursor(Qt::PointingHandCursor);
    connect(m_skipBtn, &QPushButton::clicked, this, &QDialog::accept);
    blay->addWidget(m_skipBtn);

    blay->addStretch();

    // Dot indicators
    auto *dotLayout = new QHBoxLayout;
    dotLayout->setSpacing(10);
    for (int i = 0; i < m_slideCount; ++i) {
        auto *dot = new QLabel;
        dot->setFixedSize(i == 0 ? 24 : 10, 10);
        dot->setStyleSheet(i == 0
            ? "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #3a91d0,stop:1 #7c3aed);"
              " border-radius: 5px;"
            : "background: #21262d; border-radius: 5px;");
        dotLayout->addWidget(dot);
        m_dots.append(dot);
    }
    blay->addLayout(dotLayout);

    blay->addStretch();

    m_backBtn = new QPushButton(tr("Back"));
    m_backBtn->setStyleSheet(kGhostBtnSS);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentSlide > 0) goToSlide(m_currentSlide - 1);
    });
    blay->addWidget(m_backBtn);

    m_nextBtn = new QPushButton(tr("Next  \u2192"));
    m_nextBtn->setStyleSheet(kPrimaryBtnSS);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentSlide < m_slideCount - 1)
            goToSlide(m_currentSlide + 1);
        else
            accept();
    });
    blay->addWidget(m_nextBtn);

    root->addWidget(bottomBar);

    updateButtons();
    m_bgTimerId = startTimer(30);
    QTimer::singleShot(100, this, [this]() { animateSlideContent(0); });
}

// ── Background painting ─────────────────────────────────────────────────────
void IntroWizard::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Base background — lighter than the app so it pops
    p.fillRect(rect(), QColor(12, 18, 32));

    // Outer border glow
    QPen borderPen(QColor(58, 145, 208, 80), 2);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

    // Large animated glow orbs
    float cx1 = 470.0f + 180.0f * std::cos(m_glowPhase * 0.4f);
    float cy1 = 200.0f + 80.0f * std::sin(m_glowPhase * 0.3f);
    QRadialGradient g1(cx1, cy1, 300);
    g1.setColorAt(0.0, QColor(58, 145, 208, 22));
    g1.setColorAt(0.5, QColor(58, 145, 208, 8));
    g1.setColorAt(1.0, Qt::transparent);
    p.fillRect(rect(), g1);

    float cx2 = 470.0f - 150.0f * std::cos(m_glowPhase * 0.25f + 2.0f);
    float cy2 = 350.0f + 60.0f * std::sin(m_glowPhase * 0.35f + 1.0f);
    QRadialGradient g2(cx2, cy2, 250);
    g2.setColorAt(0.0, QColor(124, 58, 237, 18));
    g2.setColorAt(0.5, QColor(124, 58, 237, 6));
    g2.setColorAt(1.0, Qt::transparent);
    p.fillRect(rect(), g2);

    // Particles + connections
    int w = width(), h = height();
    for (auto &pt : s_particles) {
        pt.x += pt.vx; pt.y += pt.vy;
        if (pt.x < 0) pt.x = w; if (pt.x > w) pt.x = 0;
        if (pt.y < 0) pt.y = h; if (pt.y > h) pt.y = 0;
    }
    // Connections
    for (int i = 0; i < s_particles.size(); ++i) {
        for (int j = i + 1; j < s_particles.size(); ++j) {
            float dx = s_particles[i].x - s_particles[j].x;
            float dy = s_particles[i].y - s_particles[j].y;
            float d = std::sqrt(dx*dx + dy*dy);
            if (d < 120) {
                p.setPen(QPen(QColor(58, 145, 208, int(18 * (1.0f - d / 120.0f))), 0.5));
                p.drawLine(QPointF(s_particles[i].x, s_particles[i].y),
                           QPointF(s_particles[j].x, s_particles[j].y));
            }
        }
    }
    // Dots
    p.setPen(Qt::NoPen);
    for (const auto &pt : s_particles) {
        QRadialGradient pg(pt.x, pt.y, pt.r * 3);
        pg.setColorAt(0, QColor(58, 145, 208, int(pt.opacity * 255)));
        pg.setColorAt(1, Qt::transparent);
        p.setBrush(pg);
        p.drawEllipse(QPointF(pt.x, pt.y), pt.r * 3, pt.r * 3);
    }

    // Subtle grid overlay
    p.setPen(QColor(58, 145, 208, 6));
    for (int x = 0; x < w; x += 80) p.drawLine(x, 0, x, h);
    for (int y = 0; y < h; y += 80) p.drawLine(0, y, w, y);
}

void IntroWizard::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_bgTimerId) {
        m_glowPhase += 0.015f;
        update();
    }
}

// ── Slide factory ────────────────────────────────────────────────────────────
QWidget *IntroWizard::makeSlide(const QString &iconText,
                                const QString &iconStyle,
                                const QString &title,
                                const QString &desc,
                                const QString &featureHighlight,
                                QWidget *extraContent)
{
    auto *slide = new QWidget;
    slide->setStyleSheet("background: transparent;");

    auto *lay = new QVBoxLayout(slide);
    lay->setContentsMargins(80, 36, 80, 20);
    lay->setSpacing(0);
    lay->setAlignment(Qt::AlignCenter);

    // Icon with glow circle behind it
    auto *iconWrap = new QWidget;
    iconWrap->setFixedSize(120, 120);
    iconWrap->setStyleSheet("background: transparent;");
    auto *iconLay = new QVBoxLayout(iconWrap);
    iconLay->setContentsMargins(0, 0, 0, 0);
    auto *iconLabel = new QLabel(iconText);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet(iconStyle);
    iconLay->addWidget(iconLabel);
    lay->addWidget(iconWrap, 0, Qt::AlignCenter);
    lay->addSpacing(20);

    // Title
    auto *titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(
        "color: #e7eefc; font-size: 26pt; font-weight: 900;"
        " letter-spacing: -0.5px; background: transparent;");
    lay->addWidget(titleLabel);
    lay->addSpacing(16);

    // Description
    auto *descLabel = new QLabel(desc);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(
        "color: #8896b3; font-size: 11pt; background: transparent;"
        " padding: 0 30px; line-height: 170%;");
    lay->addWidget(descLabel);
    lay->addSpacing(20);

    // Feature chips
    if (!featureHighlight.isEmpty()) {
        auto *chip = new QLabel(featureHighlight);
        chip->setAlignment(Qt::AlignCenter);
        chip->setStyleSheet(
            "color: #58a6ff; background: rgba(58,145,208,0.08);"
            " border: 1px solid rgba(58,145,208,0.2);"
            " border-radius: 16px; padding: 8px 20px;"
            " font-size: 9pt; font-weight: 500;");
        lay->addWidget(chip, 0, Qt::AlignCenter);
    }

    if (extraContent) {
        lay->addSpacing(16);
        lay->addWidget(extraContent, 0, Qt::AlignCenter);
    }

    lay->addStretch();
    return slide;
}

void IntroWizard::buildSlides()
{
    // Shared icon style with glow effect
    auto iconSS = [](const QString &color) {
        return QString(
            "font-size: 56pt; background: rgba(%1,0.08);"
            " border: 1px solid rgba(%1,0.15); border-radius: 40px;"
            " padding: 10px; min-width: 80px; min-height: 80px;")
            .arg(color);
    };

    // ── Slide 1: Welcome ─────────────────────────────────────────────────────
    m_stack->addWidget(makeSlide(
        "14",
        "color: #3a91d0; font-size: 64pt; font-weight: 900;"
        " background: rgba(58,145,208,0.06); border: 1px solid rgba(58,145,208,0.12);"
        " border-radius: 40px; padding: 10px 20px; letter-spacing: -3px;",
        tr("Welcome to romHEX 14"),
        tr("Professional ECU calibration software with AI-powered assistance. "
           "Let\u2019s take a quick tour of what you can do."),
        tr("\u2728 Quick tour \u2014 takes 30 seconds")
    ));

    // ── Slide 2: A2L Parser ──────────────────────────────────────────────────
    m_stack->addWidget(makeSlide(
        "\xF0\x9F\x93\x84",
        iconSS("58,145,208"),
        tr("Full A2L / DAMOS Support"),
        tr("Import A2L and DAMOS definition files to automatically detect maps, axes, "
           "scaling, and data types. Supports Bosch MED17, EDC17, ME7, Siemens, "
           "Continental, Denso and more."),
        tr("\u2713 STD_AXIS \u00b7 COM_AXIS \u00b7 FIX_AXIS \u00b7 COMPU_METHOD \u00b7 RECORD_LAYOUT")
    ));

    // ── Slide 3: Map Editor & ROM Linking ────────────────────────────────────
    m_stack->addWidget(makeSlide(
        "\xF0\x9F\x97\x82",
        iconSS("34,197,94"),
        tr("Visual Map Editor"),
        tr("Edit maps with a professional overlay \u2014 toggleable heat map, "
           "3D surface view, inline editing. Link modified ROMs to originals "
           "for automatic map address detection."),
        tr("\u2713 Heat Map \u00b7 3D View \u00b7 CSV Export \u00b7 ROM Linking \u00b7 Map Packs")
    ));

    // ── Slide 4: AI Assistant & Version Control ──────────────────────────────
    m_stack->addWidget(makeSlide(
        "\xF0\x9F\xA4\x96",
        iconSS("124,58,237"),
        tr("AI-Powered Tuning"),
        tr("Chat with Claude AI to search maps, modify values, and perform common "
           "operations like decat, DPF delete, EGR off, pops & bangs. "
           "All changes are versioned \u2014 roll back anytime."),
        tr("\u2713 30+ Tools \u00b7 Decat \u00b7 DPF \u00b7 EGR \u00b7 Boost \u00b7 Speed Limiter")
    ));

    // ── Slide 5: Register & Login ────────────────────────────────────────────
    auto *accountBtns = new QWidget;
    accountBtns->setStyleSheet("background: transparent;");
    auto *abLay = new QVBoxLayout(accountBtns);
    abLay->setContentsMargins(0, 0, 0, 0);
    abLay->setSpacing(12);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(14);

    auto *registerBtn = new QPushButton(tr("\xF0\x9F\x93\x9D  Register Free"));
    registerBtn->setStyleSheet(kPrimaryBtnSS);
    registerBtn->setCursor(Qt::PointingHandCursor);
    connect(registerBtn, &QPushButton::clicked, this, []() {
        QString lang = QSettings("CT14", "RX14").value("language", "en").toString();
        QDesktopServices::openUrl(QUrl("https://example.com/register?lang=" + lang));
    });
    btnRow->addWidget(registerBtn);

    auto *loginBtn = new QPushButton(tr("\xF0\x9F\x94\x91  Log In"));
    loginBtn->setStyleSheet(kGhostBtnSS);
    loginBtn->setCursor(Qt::PointingHandCursor);
    // Log In triggers the in-app login dialog, not a URL
    connect(loginBtn, &QPushButton::clicked, this, [this]() {
        accept(); // close wizard
        // The login dialog will be triggered from mainwindow after wizard closes
        QMetaObject::invokeMethod(parent(), "actShowLogin", Qt::QueuedConnection);
    });
    btnRow->addWidget(loginBtn);

    abLay->addLayout(btnRow);

    auto *skipLink = new QPushButton(tr("Skip for now \u2014 you can register later"));
    skipLink->setStyleSheet(kSkipBtnSS);
    skipLink->setCursor(Qt::PointingHandCursor);
    connect(skipLink, &QPushButton::clicked, this, &QDialog::accept);
    abLay->addWidget(skipLink, 0, Qt::AlignCenter);

    m_stack->addWidget(makeSlide(
        "\xF0\x9F\x91\xA4",
        iconSS("245,158,11"),
        tr("Create Your Account"),
        tr("Register for free to unlock AI map translation, cloud sync, and "
           "priority support. Already have an account? Log in to get started."),
        QString(),
        accountBtns
    ));

    m_slideCount = m_stack->count();
}

// ── Navigation ───────────────────────────────────────────────────────────────
void IntroWizard::goToSlide(int index)
{
    if (index < 0 || index >= m_slideCount || index == m_currentSlide)
        return;

    QWidget *oldSlide = m_stack->widget(m_currentSlide);
    auto *fadeOut = new QGraphicsOpacityEffect(oldSlide);
    oldSlide->setGraphicsEffect(fadeOut);
    auto *animOut = new QPropertyAnimation(fadeOut, "opacity", this);
    animOut->setDuration(180);
    animOut->setStartValue(1.0);
    animOut->setEndValue(0.0);

    const int target = index;
    connect(animOut, &QPropertyAnimation::finished, this, [this, target, oldSlide]() {
        oldSlide->setGraphicsEffect(nullptr);
        m_currentSlide = target;
        m_stack->setCurrentIndex(m_currentSlide);
        updateButtons();
        animateSlideContent(m_currentSlide);
    });

    animOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void IntroWizard::animateSlideContent(int index)
{
    QWidget *slide = m_stack->widget(index);
    auto *fadeIn = new QGraphicsOpacityEffect(slide);
    fadeIn->setOpacity(0.0);
    slide->setGraphicsEffect(fadeIn);

    auto *anim = new QPropertyAnimation(fadeIn, "opacity", this);
    anim->setDuration(400);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [slide]() {
        slide->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void IntroWizard::updateButtons()
{
    // Update dots — active dot is wider (pill shape)
    for (int i = 0; i < m_dots.size(); ++i) {
        bool active = (i == m_currentSlide);
        m_dots[i]->setFixedSize(active ? 24 : 10, 10);
        m_dots[i]->setStyleSheet(active
            ? "background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
              "stop:0 #3a91d0,stop:1 #7c3aed); border-radius: 5px;"
            : "background: #21262d; border-radius: 5px;");
    }

    m_backBtn->setVisible(m_currentSlide > 0);

    if (m_currentSlide == m_slideCount - 1) {
        m_nextBtn->setText(tr("Get Started  \u2713"));
        m_skipBtn->hide();
    } else {
        m_nextBtn->setText(tr("Next  \u2192"));
        m_skipBtn->show();
    }
}
