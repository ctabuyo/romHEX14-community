/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QPointer>

class QLineEdit;
class QListView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QAction;
class Project;

// One row in the command palette result list. The kind drives both the
// `[Project]` / `[Map]` / `[Action]` / `[Setting]` prefix shown in the
// delegate and the dispatch logic in MainWindow::actShowCommandPalette.
struct PaletteEntry {
    enum class Kind { Project, Map, Action, Setting };
    Kind     kind = Kind::Action;
    QString  name;       // primary, fuzzy-matched
    QString  subtitle;   // dimmed secondary line (brand/model, ECU, dims)
    QString  shortcut;   // optional hint shown on the right (may be empty)

    // Payloads (only one is populated, depending on `kind`):
    QString  projectPath;     // Kind::Project — registry .rx14proj path
    QPointer<Project> project; // Kind::Map     — owning project
    QString  mapName;         // Kind::Map     — name to look up in project->maps
    QPointer<QAction> action; // Kind::Action  — direct trigger target
    QString  settingId;       // Kind::Setting — opaque id, see MainWindow dispatcher
};

class CommandPalette : public QDialog {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget *parent = nullptr);

    // Replace the result set. Caller builds this fresh on every open so the
    // palette never shows stale projects/maps after the user closes a project.
    void setEntries(const QVector<PaletteEntry> &entries);

signals:
    // Fired when the user activates a row (Enter or double-click). The dialog
    // has already accept()ed by the time this fires; the slot in MainWindow
    // dispatches based on entry.kind.
    void activated(const PaletteEntry &entry);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void showEvent(QShowEvent *ev) override;

private slots:
    void onTextChanged(const QString &text);
    void activateCurrent();

private:
    void selectFirstRow();

    QLineEdit              *m_search    = nullptr;
    QListView              *m_list      = nullptr;
    QStandardItemModel     *m_model     = nullptr;
    QSortFilterProxyModel  *m_proxy     = nullptr;
    QVector<PaletteEntry>   m_entries;  // master list (proxy holds filtered view)
};
