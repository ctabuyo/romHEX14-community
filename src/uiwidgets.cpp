/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "uiwidgets.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFontDatabase>
#include <QStyle>
#include <QCheckBox>
#include <QLineEdit>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>

namespace Theme {

QString primaryButtonQss()
{
    return QStringLiteral(
        "QPushButton#primary {"
        "  background: %1; color: %2;"
        "  border: 1px solid %1;"
        "  border-radius: %3px;"
        "  padding: 6px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#primary:hover  { background: %4; border-color: %4; }"
        "QPushButton#primary:pressed{ background: %5; border-color: %5; }"
        "QPushButton#primary:disabled {"
        "  background: %6; color: %7; border-color: %8;"
        "}")
        .arg(primary, textOnPrimary)
        .arg(radiusButton)
        .arg(primaryHover, primaryPressed,
             bgHover, textDim, border);
}

QString destructiveButtonQss()
{
    return QStringLiteral(
        "QPushButton#destructive {"
        "  background: transparent; color: %1;"
        "  border: 1px solid %1;"
        "  border-radius: %2px;"
        "  padding: 6px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton#destructive:hover  { background: %1; color: %3; }"
        "QPushButton#destructive:pressed{ background: %4; color: %3; border-color: %4; }")
        .arg(danger)
        .arg(radiusButton)
        .arg(textOnPrimary, dangerButton);
}

QString flatButtonQss()
{
    return QStringLiteral(
        "QPushButton#flat {"
        "  background: transparent; color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: %3px;"
        "  padding: 6px 14px;"
        "}"
        "QPushButton#flat:hover  { border-color: %4; color: %4; }"
        "QPushButton#flat:pressed{ background: rgba(31,111,235,0.18); color: %4; }")
        .arg(textPrimary, border)
        .arg(radiusButton)
        .arg(accent);
}

QString cardFrameQss()
{
    return QStringLiteral(
        "QFrame[role=\"card\"] {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: %4px;"
        "}")
        .arg(bgCard, textPrimary, border)
        .arg(radiusCard);
}

QString sunkenInputQss()
{
    // Inputs that live inside cards should be sunken below the card surface.
    return QStringLiteral(
        "QLineEdit[role=\"sunken\"], QPlainTextEdit[role=\"sunken\"], QTextEdit[role=\"sunken\"] {"
        "  background: %1; border: 1px solid %2; border-radius: %3px;"
        "  padding: 6px 8px;"
        "}"
        "QLineEdit[role=\"sunken\"]:focus, QPlainTextEdit[role=\"sunken\"]:focus, "
        "QTextEdit[role=\"sunken\"]:focus {"
        "  border-color: %4; background: %5;"
        "}")
        .arg(bgSunken, border)
        .arg(radiusInput)
        .arg(primary, bgCard);
}

QString pillQss(const QString &kind)
{
    QString fg = textMuted;
    QString bg = "rgba(139, 148, 158, 0.12)";
    QString br = border;
    if (kind == QLatin1String("success")) {
        fg = success;
        bg = "rgba(63, 185, 80, 0.14)";
        br = "rgba(63, 185, 80, 0.40)";
    } else if (kind == QLatin1String("warning")) {
        fg = warning;
        bg = "rgba(240, 136, 62, 0.14)";
        br = "rgba(240, 136, 62, 0.40)";
    } else if (kind == QLatin1String("danger")) {
        fg = danger;
        bg = "rgba(248, 81, 73, 0.14)";
        br = "rgba(248, 81, 73, 0.45)";
    } else if (kind == QLatin1String("info")) {
        fg = info;
        bg = "rgba(121, 192, 255, 0.14)";
        br = "rgba(121, 192, 255, 0.45)";
    }
    return QStringLiteral(
        "QLabel[role=\"pill\"][kind=\"%1\"] {"
        "  color: %2;"
        "  background: %3;"
        "  border: 1px solid %4;"
        "  border-radius: 9px;"
        "  padding: 1px 10px;"
        "  font-size: 8pt;"
        "  font-weight: bold;"
        "  letter-spacing: 0.5px;"
        "}")
        .arg(kind, fg, bg, br);
}

QString objectStyles()
{
    QString s;
    s += primaryButtonQss();
    s += destructiveButtonQss();
    s += flatButtonQss();
    s += cardFrameQss();
    s += sunkenInputQss();
    s += pillQss(QStringLiteral("neutral"));
    s += pillQss(QStringLiteral("success"));
    s += pillQss(QStringLiteral("warning"));
    s += pillQss(QStringLiteral("danger"));
    s += pillQss(QStringLiteral("info"));
    return s;
}

} // namespace Theme

// ── UI helpers ──────────────────────────────────────────────────────────────

namespace UI {

QFrame *makeCard(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setProperty("role", "card");
    // Allow the global QSS to style it via the [role="card"] selector.
    return f;
}

QFrame *makeHeaderStrip(const QString &iconGlyph,
                        const QString &title,
                        const QString &subtitle,
                        const QString &pillText,
                        QWidget *parent)
{
    auto *strip = new QFrame(parent);
    strip->setObjectName("headerStrip");
    strip->setStyleSheet(QStringLiteral(
        "QFrame#headerStrip { background: %1; border-bottom: 1px solid %2; }")
        .arg(Theme::bgRoot, Theme::borderSubtle));
    strip->setFixedHeight(68);

    auto *lay = new QHBoxLayout(strip);
    lay->setContentsMargins(Theme::spaceXL, Theme::spaceM,
                             Theme::spaceXL, Theme::spaceM);
    lay->setSpacing(Theme::spaceM);

    if (!iconGlyph.isEmpty()) {
        auto *tile = new QLabel(iconGlyph, strip);
        tile->setAlignment(Qt::AlignCenter);
        tile->setFixedSize(40, 40);
        tile->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: rgba(88, 166, 255, 0.12);"
            "  border: 1px solid rgba(88, 166, 255, 0.35);"
            "  border-radius: 10px;"
            "  color: %1;"
            "  font-size: 18pt;"
            "  font-weight: bold;"
            "}").arg(Theme::accent));
        lay->addWidget(tile, 0, Qt::AlignVCenter);
    }

    auto *col = new QVBoxLayout();
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(2);
    auto *titleLbl = new QLabel(title, strip);
    titleLbl->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2pt; font-weight: 800; letter-spacing: 1px; background: transparent;")
        .arg(Theme::textPrimary).arg(15));
    auto *subLbl = new QLabel(subtitle, strip);
    subLbl->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2pt; letter-spacing: 0.4px; background: transparent;")
        .arg(Theme::textMuted).arg(Theme::captionPt));
    col->addWidget(titleLbl);
    col->addWidget(subLbl);
    lay->addLayout(col, 1);

    if (!pillText.isEmpty()) {
        auto *pill = makePill(pillText, QStringLiteral("success"), strip);
        lay->addWidget(pill, 0, Qt::AlignVCenter);
    }

    return strip;
}

QPushButton *makePrimaryButton(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setObjectName("primary");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPushButton *makeSecondaryButton(const QString &text, QWidget *parent)
{
    // Inherits the default QPushButton styling from the global QSS.
    auto *b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPushButton *makeDestructiveButton(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setObjectName("destructive");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPushButton *makeFlatButton(const QString &text, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setObjectName("flat");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QLabel *makeSectionHeader(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2pt; font-weight: bold;"
        " letter-spacing: 2px; background: transparent;")
        .arg(Theme::textMuted).arg(Theme::captionPt));
    return l;
}

QLabel *makePill(const QString &text, const QString &kind, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setProperty("role", "pill");
    l->setProperty("kind", kind);
    l->setAlignment(Qt::AlignCenter);
    return l;
}

QWidget *makeFormField(const QString &label,
                       QWidget *input,
                       const QString &helper,
                       bool required,
                       QWidget *parent)
{
    auto *w = new QWidget(parent);
    w->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    QString labelHtml = QStringLiteral(
        "<span style='color:%1; font-size:%2pt; font-weight:bold;"
        " letter-spacing:0.5px; text-transform:uppercase;'>%3</span>")
        .arg(Theme::textMuted).arg(Theme::captionPt)
        .arg(label.toHtmlEscaped());
    if (required) {
        labelHtml += QStringLiteral(
            "<span style='color:%1; font-weight:bold;'>&nbsp;*</span>")
            .arg(Theme::danger);
    }
    auto *labelLbl = new QLabel(labelHtml, w);
    labelLbl->setTextFormat(Qt::RichText);
    lay->addWidget(labelLbl);

    if (input) {
        // Mark the input as sunken so global QSS gives it the right look.
        input->setProperty("role", "sunken");
        // Reapply style if already polished.
        input->style()->unpolish(input);
        input->style()->polish(input);
        lay->addWidget(input);
    }

    if (!helper.isEmpty()) {
        auto *helpLbl = new QLabel(helper, w);
        helpLbl->setStyleSheet(QStringLiteral(
            "color: %1; font-size: %2pt; background: transparent;")
            .arg(Theme::textMuted).arg(Theme::captionPt));
        helpLbl->setWordWrap(true);
        lay->addWidget(helpLbl);
    }

    return w;
}

QFont fixedFont(int pt)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(pt);
    return f;
}

QFont uiFont(int pt, int weight)
{
    QFont f("Segoe UI");
    f.setPointSize(pt);
    if (weight >= 0) f.setWeight(QFont::Weight(weight));
    return f;
}

// ── RiskyChangeConfirmDialog ────────────────────────────────────────────────

RiskyChangeConfirmDialog::RiskyChangeConfirmDialog(QWidget *parent)
    : QDialog(parent)
{
    setModal(true);
    setMinimumSize(560, 480);
    resize(640, 540);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QLabel  { color: %2; background: transparent; }"
        "%3 %4 %5 %6 %7 %8 %9 %10 %11")
        .arg(Theme::bgRoot, Theme::textPrimary,
             Theme::primaryButtonQss(),
             Theme::destructiveButtonQss(),
             Theme::flatButtonQss(),
             Theme::cardFrameQss(),
             Theme::sunkenInputQss(),
             Theme::pillQss(QStringLiteral("neutral")),
             Theme::pillQss(QStringLiteral("warning")),
             Theme::pillQss(QStringLiteral("danger")))
        + Theme::pillQss(QStringLiteral("info")));

    m_actionText = tr("Apply");

    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(Theme::spaceL, Theme::spaceL,
                                Theme::spaceL, Theme::spaceL);
    m_root->setSpacing(Theme::spaceM);

    // ── Header strip ─────────────────────────────────────────────────────
    m_headerStrip = new QFrame(this);
    m_headerStrip->setFixedHeight(50);
    m_headerStrip->setObjectName("riskHeaderStrip");
    auto *stripLay = new QHBoxLayout(m_headerStrip);
    stripLay->setContentsMargins(0, 0, Theme::spaceM, 0);
    stripLay->setSpacing(Theme::spaceM);

    m_riskEdge = new QFrame(m_headerStrip);
    m_riskEdge->setFixedWidth(3);
    stripLay->addWidget(m_riskEdge);

    m_headlineLabel = new QLabel(m_headerStrip);
    QFont titleFont = uiFont(Theme::titlePt);
    titleFont.setBold(true);
    m_headlineLabel->setFont(titleFont);
    m_headlineLabel->setWordWrap(true);
    m_headlineLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(Theme::textBright));
    stripLay->addWidget(m_headlineLabel, 1);

    m_riskPill = makePill(QStringLiteral("CAUTION"), QStringLiteral("warning"),
                          m_headerStrip);
    stripLay->addWidget(m_riskPill, 0, Qt::AlignVCenter);
    m_root->addWidget(m_headerStrip);

    // ── Description card ────────────────────────────────────────────────
    m_descCard = makeCard(this);
    auto *descLay = new QVBoxLayout(m_descCard);
    descLay->setContentsMargins(Theme::spaceM, Theme::spaceM,
                                 Theme::spaceM, Theme::spaceM);
    m_descLabel = new QLabel(m_descCard);
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet(QStringLiteral(
        "color: %1; background: transparent;").arg(Theme::textPrimary));
    descLay->addWidget(m_descLabel);
    m_root->addWidget(m_descCard);

    // ── Changes card ────────────────────────────────────────────────────
    m_changesCard = makeCard(this);
    auto *changesLay = new QVBoxLayout(m_changesCard);
    changesLay->setContentsMargins(Theme::spaceS, Theme::spaceS,
                                    Theme::spaceS, Theme::spaceS);

    m_changesModel = new QStandardItemModel(0, 4, this);
    m_changesModel->setHeaderData(0, Qt::Horizontal, tr("Item"));
    m_changesModel->setHeaderData(1, Qt::Horizontal, tr("Before"));
    m_changesModel->setHeaderData(2, Qt::Horizontal, tr("After"));
    m_changesModel->setHeaderData(3, Qt::Horizontal, tr("Δ"));

    m_changesView = new QTableView(m_changesCard);
    m_changesView->setModel(m_changesModel);
    m_changesView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_changesView->setSelectionMode(QAbstractItemView::NoSelection);
    m_changesView->setAlternatingRowColors(false);
    m_changesView->verticalHeader()->setVisible(false);
    m_changesView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_changesView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_changesView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_changesView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_changesView->setStyleSheet(QStringLiteral(
        "QTableView { background: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: %4pt; }"
        "QTableView::item { padding: 4px 8px; }"
        "QHeaderView::section { background: %5; color: %6; border: none;"
        "  border-bottom: 1px solid %3; padding: 6px 8px; font-weight: bold;"
        "  font-size: %7pt; }")
        .arg(Theme::bgSunken, Theme::textPrimary, Theme::border)
        .arg(Theme::bodyPt)
        .arg(Theme::bgCard, Theme::textMuted)
        .arg(Theme::captionPt));
    changesLay->addWidget(m_changesView);

    m_emptyChangesNote = new QLabel(tr("No per-cell breakdown available"), m_changesCard);
    m_emptyChangesNote->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2pt; background: transparent;"
        " padding: %3px;")
        .arg(Theme::textMuted).arg(Theme::captionPt).arg(Theme::spaceS));
    m_emptyChangesNote->setAlignment(Qt::AlignCenter);
    m_emptyChangesNote->hide();
    changesLay->addWidget(m_emptyChangesNote);

    m_root->addWidget(m_changesCard, 1);

    // ── Snapshot option row (hidden by default) ─────────────────────────
    m_snapshotRow = new QFrame(this);
    m_snapshotRow->setStyleSheet("background: transparent;");
    auto *snapLay = new QHBoxLayout(m_snapshotRow);
    snapLay->setContentsMargins(Theme::spaceS, 0, Theme::spaceS, 0);
    snapLay->setSpacing(Theme::spaceS);

    m_snapshotCheck = new QCheckBox(tr("Save a version snapshot before applying"),
                                    m_snapshotRow);
    m_snapshotCheck->setChecked(true);
    m_snapshotCheck->setStyleSheet(QStringLiteral(
        "QCheckBox { color: %1; background: transparent; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }")
        .arg(Theme::textPrimary));
    snapLay->addWidget(m_snapshotCheck);

    auto *helper = new QLabel(tr("(recommended — lets you undo)"), m_snapshotRow);
    helper->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2pt; background: transparent;")
        .arg(Theme::textMuted).arg(Theme::captionPt));
    snapLay->addWidget(helper);
    snapLay->addStretch();

    m_snapshotRow->hide();
    m_root->addWidget(m_snapshotRow);

    // ── Type-to-confirm row (hidden by default) ─────────────────────────
    m_typedRow = new QFrame(this);
    m_typedRow->setStyleSheet("background: transparent;");
    auto *typedLay = new QHBoxLayout(m_typedRow);
    typedLay->setContentsMargins(Theme::spaceS, 0, Theme::spaceS, 0);
    typedLay->setSpacing(Theme::spaceS);
    m_typedEdit = new QLineEdit(m_typedRow);
    m_typedEdit->setProperty("role", "sunken");
    typedLay->addWidget(m_typedEdit, 1);
    m_typedRow->hide();
    m_root->addWidget(m_typedRow);

    connect(m_typedEdit, &QLineEdit::textChanged, this,
            [this]() { updateApplyEnabled(); });

    // ── Footer ──────────────────────────────────────────────────────────
    m_footerRow = new QFrame(this);
    m_footerRow->setStyleSheet("background: transparent;");
    auto *footerLay = new QHBoxLayout(m_footerRow);
    footerLay->setContentsMargins(0, 0, 0, 0);
    footerLay->setSpacing(Theme::spaceS);
    footerLay->addStretch();

    m_cancelBtn = makeFlatButton(tr("Cancel"), m_footerRow);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    footerLay->addWidget(m_cancelBtn);

    m_applyBtn = makePrimaryButton(m_actionText, m_footerRow);
    m_applyBtn->setDefault(true);
    connect(m_applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    footerLay->addWidget(m_applyBtn);

    m_root->addWidget(m_footerRow);

    updateRiskVisuals();
    updateApplyEnabled();
}

void RiskyChangeConfirmDialog::setHeadline(const QString &text)
{
    m_headlineLabel->setText(text);
    setWindowTitle(text);
}

void RiskyChangeConfirmDialog::setDescription(const QString &text)
{
    m_descLabel->setText(text);
}

void RiskyChangeConfirmDialog::setRisk(Risk risk)
{
    m_risk = risk;
    updateRiskVisuals();
    rebuildPrimaryButton();
}

void RiskyChangeConfirmDialog::setChanges(const QVector<ChangeRow> &changes)
{
    m_changesModel->removeRows(0, m_changesModel->rowCount());
    if (changes.isEmpty()) {
        m_changesView->hide();
        m_emptyChangesNote->show();
        return;
    }

    m_changesView->show();
    m_emptyChangesNote->hide();

    const QFont mono = fixedFont(Theme::bodyPt);
    for (const ChangeRow &row : changes) {
        auto *label = new QStandardItem(row.label);
        auto *oldV  = new QStandardItem(row.oldValue);
        auto *newV  = new QStandardItem(row.newValue);
        auto *dlt   = new QStandardItem(row.delta);
        oldV->setFont(mono);
        newV->setFont(mono);
        dlt->setFont(mono);
        m_changesModel->appendRow(QList<QStandardItem*>{ label, oldV, newV, dlt });
    }
}

void RiskyChangeConfirmDialog::setSnapshotOption(bool show, bool defaultChecked)
{
    m_snapshotRow->setVisible(show);
    if (show) m_snapshotCheck->setChecked(defaultChecked);
}

bool RiskyChangeConfirmDialog::snapshotChecked() const
{
    return m_snapshotRow->isVisible() && m_snapshotCheck->isChecked();
}

void RiskyChangeConfirmDialog::setActionText(const QString &text)
{
    m_actionText = text;
    if (m_applyBtn) m_applyBtn->setText(text);
}

void RiskyChangeConfirmDialog::setRequireTypedConfirmation(const QString &phrase)
{
    m_typedPhrase = phrase;
    if (phrase.isEmpty()) {
        m_typedRow->hide();
    } else {
        m_typedEdit->clear();
        m_typedEdit->setPlaceholderText(tr("Type '%1' to confirm").arg(phrase));
        m_typedRow->show();
    }
    updateApplyEnabled();
}

void RiskyChangeConfirmDialog::updateApplyEnabled()
{
    bool ok = true;
    if (!m_typedPhrase.isEmpty())
        ok = (m_typedEdit->text() == m_typedPhrase);
    if (m_applyBtn) m_applyBtn->setEnabled(ok);
}

void RiskyChangeConfirmDialog::updateRiskVisuals()
{
    QString edgeColor, pillKind, pillText;
    switch (m_risk) {
    case Risk::Info:
        edgeColor = Theme::accent;
        pillKind  = QStringLiteral("info");
        pillText  = tr("INFO");
        break;
    case Risk::Caution:
        edgeColor = Theme::warning;
        pillKind  = QStringLiteral("warning");
        pillText  = tr("CAUTION");
        break;
    case Risk::Danger:
        edgeColor = Theme::danger;
        pillKind  = QStringLiteral("danger");
        pillText  = tr("DESTRUCTIVE");
        break;
    }
    m_riskEdge->setStyleSheet(QStringLiteral("background: %1;").arg(edgeColor));
    m_riskPill->setText(pillText);
    m_riskPill->setProperty("kind", pillKind);
    m_riskPill->style()->unpolish(m_riskPill);
    m_riskPill->style()->polish(m_riskPill);

    m_headerStrip->setStyleSheet(QStringLiteral(
        "QFrame#riskHeaderStrip { background: %1; border: 1px solid %2;"
        " border-radius: %3px; }")
        .arg(Theme::bgCard, Theme::border).arg(Theme::radiusCard));
}

void RiskyChangeConfirmDialog::rebuildPrimaryButton()
{
    if (!m_applyBtn || !m_footerRow) return;
    auto *layout = qobject_cast<QHBoxLayout*>(m_footerRow->layout());
    if (!layout) return;

    const bool wantDestructive = (m_risk == Risk::Danger);
    const bool isDestructive   = (m_applyBtn->objectName() == QLatin1String("destructive"));
    if (wantDestructive == isDestructive) {
        m_applyBtn->setText(m_actionText);
        return;
    }

    layout->removeWidget(m_applyBtn);
    delete m_applyBtn;
    m_applyBtn = wantDestructive
                   ? makeDestructiveButton(m_actionText, m_footerRow)
                   : makePrimaryButton(m_actionText, m_footerRow);
    m_applyBtn->setDefault(true);
    connect(m_applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(m_applyBtn);
    updateApplyEnabled();
}

} // namespace UI
