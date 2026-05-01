/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * "Change by slider" dialog with live preview.
 *
 * Tuner drags a QSlider; on every move we call WaveformEditor::setAbsolute
 * on the live selection so the 2D / 3D widgets repaint in real time.
 * Cancel undoes the cumulative effect; OK keeps the last preview.
 */

#pragma once

#include <QDialog>

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class WaveformEditor;

class SliderEditDlg : public QDialog {
    Q_OBJECT
public:
    /// @p editor must outlive the dialog.  Selection is captured at
    /// construction time and reused on every preview tick.
    SliderEditDlg(WaveformEditor *editor, int start, int end,
                  double initialValue, double minVal, double maxVal,
                  QWidget *parent = nullptr);

    /// Final value the user committed (only valid after exec() == Accepted).
    double committedValue() const { return m_lastValue; }

protected:
    void reject() override;     // override to undo cumulative preview

private slots:
    void onSliderChanged(int rawTicks);
    void onSpinChanged(double v);

private:
    void buildUi(double initialValue, double minVal, double maxVal);
    void applyPreview(double value);

    WaveformEditor *m_editor   = nullptr;
    int             m_start    = 0;
    int             m_end      = 0;
    double          m_minVal   = 0;
    double          m_maxVal   = 0;
    double          m_lastValue = 0;
    int             m_previewCount = 0;   // how many ops we pushed onto undo

    QSlider        *m_slider   = nullptr;
    QDoubleSpinBox *m_spin     = nullptr;
    QLabel         *m_summary  = nullptr;
};
