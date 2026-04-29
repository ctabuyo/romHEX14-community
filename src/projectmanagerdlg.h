/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

class QCloseEvent;

class ProjectManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectManagerDialog(QWidget *parent = nullptr);

    // Non-empty if the user double-clicked / pressed Open
    QString selectedPath() const { return m_selectedPath; }

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void openProjectRequested(const QString &path);
    void newProjectRequested();

private slots:
    void onOpen();
    void onRename();
    void onRemove();
    void onDeleteFile();
    void onShowInExplorer();
    void onDoubleClick(int row, int col);
    void onSelectionChanged();
    void onFilterChanged(const QString &text);
    void refresh();

private:
    void buildTable(const QString &filter = {});
    QString pathAt(int row) const;

    QTableWidget *m_table      = nullptr;
    QLineEdit    *m_filterEdit = nullptr;
    QPushButton  *m_openBtn    = nullptr;
    QPushButton  *m_renameBtn  = nullptr;
    QPushButton  *m_removeBtn  = nullptr;
    QPushButton  *m_deleteBtn  = nullptr;
    QLabel       *m_countLbl   = nullptr;
    QString       m_selectedPath;
};
