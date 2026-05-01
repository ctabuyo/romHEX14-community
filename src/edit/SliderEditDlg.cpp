/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "edit/SliderEditDlg.h"

#include "waveformeditor.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

namespace {
constexpr int kSliderTicks = 1000;   // resolution: maxVal-minVal split into 1000

double sliderToValue(int ticks, double minVal, double maxVal)
{
    if (kSliderTicks <= 0) return minVal;
    const double t = static_cast<double>(ticks) / kSliderTicks;
    return minVal + t * (maxVal - minVal);
}

int valueToSlider(double value, double minVal, double maxVal)
{
    if (maxVal <= minVal) return 0;
    const double t = (value - minVal) / (maxVal - minVal);
    return static_cast<int>(qBound(0.0, t, 1.0) * kSliderTicks);
}
}

SliderEditDlg::SliderEditDlg(WaveformEditor *editor, int start, int end,
                             double initialValue, double minVal, double maxVal,
                             QWidget *parent)
    : QDialog(parent), m_editor(editor), m_start(start), m_end(end),
      m_minVal(minVal), m_maxVal(maxVal),
      m_lastValue(qBound(minVal, initialValue, maxVal))
{
    setWindowTitle(tr("Change by slider"));
    setModal(true);
    buildUi(m_lastValue, minVal, maxVal);
}

void SliderEditDlg::buildUi(double initialValue, double minVal, double maxVal)
{
    auto *root = new QVBoxLayout(this);

    m_summary = new QLabel(tr("Drag the slider — the selection updates live."));
    m_summary->setStyleSheet("color:#8b949e; font-size:9pt;");
    root->addWidget(m_summary);

    auto *row = new QHBoxLayout();
    row->addWidget(new QLabel(QString::number(minVal)));
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, kSliderTicks);
    m_slider->setValue(valueToSlider(initialValue, minVal, maxVal));
    connect(m_slider, &QSlider::valueChanged,
            this, &SliderEditDlg::onSliderChanged);
    row->addWidget(m_slider, 1);
    row->addWidget(new QLabel(QString::number(maxVal)));
    root->addLayout(row);

    m_spin = new QDoubleSpinBox();
    m_spin->setRange(minVal, maxVal);
    m_spin->setDecimals(3);
    m_spin->setValue(initialValue);
    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SliderEditDlg::onSpinChanged);
    root->addWidget(m_spin);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(bb);
}

void SliderEditDlg::applyPreview(double value)
{
    if (!m_editor) return;
    m_editor->setAbsolute(m_start, m_end, value);
    m_lastValue = value;
    ++m_previewCount;
}

void SliderEditDlg::onSliderChanged(int rawTicks)
{
    const double v = sliderToValue(rawTicks, m_minVal, m_maxVal);
    if (m_spin) {
        QSignalBlocker blk(m_spin);
        m_spin->setValue(v);
    }
    applyPreview(v);
}

void SliderEditDlg::onSpinChanged(double v)
{
    if (m_slider) {
        QSignalBlocker blk(m_slider);
        m_slider->setValue(valueToSlider(v, m_minVal, m_maxVal));
    }
    applyPreview(v);
}

void SliderEditDlg::reject()
{
    // Roll back every preview we pushed onto WaveformEditor's undo stack
    // so the user gets exactly the pre-dialog state when they Cancel.
    if (m_editor) {
        while (m_previewCount > 0 && m_editor->canUndo()) {
            m_editor->undo();
            --m_previewCount;
        }
    }
    QDialog::reject();
}
