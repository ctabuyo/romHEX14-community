/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "projectmanagerdlg.h"
#include "appconstants.h"
#include "projectregistry.h"
#include "project.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDialogButtonBox>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QCloseEvent>

static const QString kStyle =
    "QDialog { background:#1c2128; color:#e6edf3; }"
    "QTableWidget {"
    "  background:#0d1117; color:#e6edf3;"
    "  gridline-color:#21262d;"
    "  border:1px solid #30363d;"
    "  selection-background-color:#1f6feb;"
    "  selection-color:#ffffff;"
    "}"
    "QTableWidget::item { padding:4px 8px; border:none; }"
    "QHeaderView::section {"
    "  background:#161b22; color:#8b949e;"
    "  border:none; border-right:1px solid #30363d;"
    "  border-bottom:1px solid #30363d;"
    "  padding:4px 8px;"
    "  font-weight:bold;"
    "}"
    "QLineEdit {"
    "  background:#0d1117; color:#e6edf3;"
    "  border:1px solid #30363d; border-radius:4px; padding:4px 8px;"
    "}"
    "QPushButton {"
    "  background:#21262d; color:#e6edf3;"
    "  border:1px solid #30363d; border-radius:4px;"
    "  padding:5px 14px;"
    "}"
    "QPushButton:hover  { background:#2d333b; }"
    "QPushButton:pressed{ background:#161b22; }"
    "QPushButton:disabled{ color:#484f58; }"
    "QPushButton#openBtn { border-color:#238636; color:#3fb950; }"
    "QPushButton#openBtn:hover { background:#1b4721; }"
    "QPushButton#deleteBtn { border-color:#da3633; color:#f85149; }"
    "QPushButton#deleteBtn:hover { background:#4a1a1a; }"
    "QLabel { color:#8b949e; }"
    "QScrollBar:vertical { background:#0d1117; width:10px; border:none; }"
    "QScrollBar::handle:vertical { background:#30363d; border-radius:5px; min-height:30px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";

// Column indices
enum Col { ColName=0, ColVehicle, ColECU, ColClient, ColModified, ColPath, ColCount };

static QIcon makeA2LBadge()
{
    QPixmap px(36, 16);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#1b4721"));
    p.setPen(QPen(QColor("#238636"), 1));
    p.drawRoundedRect(px.rect().adjusted(0,0,-1,-1), 3, 3);
    p.setPen(QColor("#3fb950"));
    QFont f = p.font(); f.setPixelSize(9); f.setBold(true); p.setFont(f);
    p.drawText(px.rect(), Qt::AlignCenter, "A2L");
    return QIcon(px);
}

ProjectManagerDialog::ProjectManagerDialog(QWidget *parent)
    : QDialog(parent, Qt::Window | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                     | Qt::WindowMaximizeButtonHint)
{
    setWindowTitle(tr("Project Manager"));
    setMinimumSize(900, 520);
    setStyleSheet(kStyle);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_tabs = new QTabWidget(this);
    m_tabs->setStyleSheet(
        "QTabWidget::pane { border:1px solid #30363d; background:#1c2128; }"
        "QTabBar::tab { background:#161b22; color:#8b949e; padding:6px 18px; "
        "  border:1px solid #30363d; border-bottom:none; }"
        "QTabBar::tab:selected { background:#1c2128; color:#e6edf3; }");
    outer->addWidget(m_tabs);

    auto *localTab = new QWidget(m_tabs);
    auto *root = new QVBoxLayout(localTab);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // ── Top bar: filter + count ────────────────────────────────────────────────
    auto *topRow = new QHBoxLayout();
    auto *filterLbl = new QLabel(tr("Filter:"));
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("Search by name, brand, ECU…"));
    m_filterEdit->setFixedWidth(320);
    m_countLbl = new QLabel();

    auto *refreshBtn = new QPushButton(tr("Refresh"));
    refreshBtn->setToolTip(tr("Reload project list from disk"));

    topRow->addWidget(filterLbl);
    topRow->addWidget(m_filterEdit);
    topRow->addSpacing(16);
    topRow->addWidget(m_countLbl);
    topRow->addStretch();
    topRow->addWidget(refreshBtn);
    root->addLayout(topRow);

    // ── Table ─────────────────────────────────────────────────────────────────
    m_table = new QTableWidget(0, ColCount);
    m_table->setHorizontalHeaderLabels(
        {tr("Project"), tr("Vehicle"), tr("ECU Type"), tr("Client"),
         tr("Last modified"), tr("File path")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(ColName,     QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColVehicle,  QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColECU,      QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColClient,   QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColModified, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColPath,     QHeaderView::Stretch);
    m_table->setColumnWidth(ColName,     200);
    m_table->setColumnWidth(ColVehicle,  180);
    m_table->setColumnWidth(ColECU,      120);
    m_table->setColumnWidth(ColClient,   120);
    m_table->setColumnWidth(ColModified, 130);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(
        m_table->styleSheet() +
        "QTableWidget { alternate-background-color:#111820; }");
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_table, 1);

    // ── Bottom buttons ────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout();

    auto *newBtn = new QPushButton(tr("New Project…"));
    newBtn->setToolTip(tr("Create a new project"));

    m_openBtn = new QPushButton(tr("Open"));
    m_openBtn->setObjectName("openBtn");
    m_openBtn->setToolTip(tr("Open selected project"));
    m_openBtn->setEnabled(false);

    m_renameBtn = new QPushButton(tr("Rename…"));
    m_renameBtn->setToolTip(tr("Rename the project"));
    m_renameBtn->setEnabled(false);

    m_removeBtn = new QPushButton(tr("Remove from list"));
    m_removeBtn->setToolTip(tr("Remove from project manager (file is kept on disk)"));
    m_removeBtn->setEnabled(false);

    m_deleteBtn = new QPushButton(tr("Delete file…"));
    m_deleteBtn->setObjectName("deleteBtn");
    m_deleteBtn->setToolTip(tr("Permanently delete the project file from disk"));
    m_deleteBtn->setEnabled(false);

    auto *closeBtn = new QPushButton(tr("Close"));

    btnRow->addWidget(newBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(m_openBtn);
    btnRow->addWidget(m_renameBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ProjectManagerDialog::onFilterChanged);
    connect(refreshBtn,   &QPushButton::clicked,   this, &ProjectManagerDialog::refresh);
    connect(newBtn,       &QPushButton::clicked,   this, [this](){
        emit newProjectRequested();
        accept();
    });
    connect(m_openBtn,    &QPushButton::clicked,   this, &ProjectManagerDialog::onOpen);
    connect(m_renameBtn,  &QPushButton::clicked,   this, &ProjectManagerDialog::onRename);
    connect(m_removeBtn,  &QPushButton::clicked,   this, &ProjectManagerDialog::onRemove);
    connect(m_deleteBtn,  &QPushButton::clicked,   this, &ProjectManagerDialog::onDeleteFile);
    connect(closeBtn,     &QPushButton::clicked,   this, &QDialog::reject);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ProjectManagerDialog::onDoubleClick);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ProjectManagerDialog::onSelectionChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        int row = m_table->rowAt(pos.y());
        if (row < 0) return;
        m_table->selectRow(row);
        QMenu ctx(this);
        ctx.setStyleSheet(
            "QMenu { background:#21262d; color:#e6edf3; border:1px solid #30363d; }"
            "QMenu::item:selected { background:#1f6feb; }");
        ctx.addAction(tr("Open"),             this, &ProjectManagerDialog::onOpen);
        ctx.addAction(tr("Rename…"),          this, &ProjectManagerDialog::onRename);
        ctx.addSeparator();
        ctx.addAction(tr("Show in Explorer"), this, &ProjectManagerDialog::onShowInExplorer);
        ctx.addSeparator();
        ctx.addAction(tr("Remove from list"), this, &ProjectManagerDialog::onRemove);
        ctx.addAction(tr("Delete file…"),     this, &ProjectManagerDialog::onDeleteFile);
        ctx.exec(m_table->viewport()->mapToGlobal(pos));
    });

    buildTable();

    m_tabs->addTab(localTab, tr("Local Projects"));
    m_tabs->addTab(buildCatalogTab(), tr("WOLS Catalog"));

    restoreGeometry(rx14::appSettings()
                    .value("dialogGeometry/ProjectManagerDialog").toByteArray());
}

ProjectManagerDialog::~ProjectManagerDialog() = default;

void ProjectManagerDialog::closeEvent(QCloseEvent *event)
{
    rx14::appSettings()
        .setValue("dialogGeometry/ProjectManagerDialog", saveGeometry());
    QDialog::closeEvent(event);
}

// ── Helpers ──────────────────────────────────────────────────────────────────

QString ProjectManagerDialog::pathAt(int row) const
{
    if (row < 0 || row >= m_table->rowCount()) return {};
    auto *item = m_table->item(row, ColPath);
    return item ? item->text() : QString();
}

void ProjectManagerDialog::buildTable(const QString &filter)
{
    m_table->setRowCount(0);

    const auto entries = ProjectRegistry::instance().entries();
    int shown = 0;
    for (const auto &e : entries) {
        if (!filter.isEmpty()) {
            bool match = e.name.contains(filter, Qt::CaseInsensitive)
                      || e.brand.contains(filter, Qt::CaseInsensitive)
                      || e.model.contains(filter, Qt::CaseInsensitive)
                      || e.ecuType.contains(filter, Qt::CaseInsensitive)
                      || e.clientName.contains(filter, Qt::CaseInsensitive)
                      || e.path.contains(filter, Qt::CaseInsensitive);
            if (!match) continue;
        }

        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setRowHeight(row, 28);

        bool exists = QFileInfo::exists(e.path);
        QColor dimColor("#484f58");

        auto *nameItem = new QTableWidgetItem(e.name.isEmpty() ? QFileInfo(e.path).baseName() : e.name);
        if (!exists) nameItem->setForeground(dimColor);
        else         nameItem->setForeground(QColor("#e6edf3"));
        if (e.hasA2L)
            nameItem->setIcon(makeA2LBadge());
        m_table->setItem(row, ColName, nameItem);

        QString vehicle = e.brand;
        if (!e.model.isEmpty()) vehicle += (vehicle.isEmpty() ? "" : " ") + e.model;
        m_table->setItem(row, ColVehicle,  new QTableWidgetItem(vehicle));
        m_table->setItem(row, ColECU,      new QTableWidgetItem(e.ecuType));
        m_table->setItem(row, ColClient,   new QTableWidgetItem(e.clientName));

        QString modStr = e.changedAt.isValid()
            ? e.changedAt.toString("yyyy-MM-dd HH:mm")
            : (e.createdAt.isValid() ? e.createdAt.toString("yyyy-MM-dd HH:mm") : tr("Unknown"));
        auto *modItem = new QTableWidgetItem(modStr);
        modItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, ColModified, modItem);

        auto *pathItem = new QTableWidgetItem(e.path);
        if (!exists) {
            pathItem->setForeground(dimColor);
            pathItem->setToolTip(tr("File not found on disk"));
        }
        m_table->setItem(row, ColPath, pathItem);

        ++shown;
    }

    m_countLbl->setText(tr("%1 project(s)").arg(shown));
    onSelectionChanged();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ProjectManagerDialog::refresh()
{
    buildTable(m_filterEdit->text());
}

void ProjectManagerDialog::onFilterChanged(const QString &text)
{
    buildTable(text);
}

void ProjectManagerDialog::onSelectionChanged()
{
    int row = m_table->currentRow();
    bool has = (row >= 0);
    bool exists = has && QFileInfo::exists(pathAt(row));
    m_openBtn->setEnabled(exists);
    m_renameBtn->setEnabled(has && exists);
    m_removeBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has && exists);
}

void ProjectManagerDialog::onOpen()
{
    int row = m_table->currentRow();
    QString path = pathAt(row);
    if (path.isEmpty() || !QFileInfo::exists(path)) return;
    m_selectedPath = path;
    emit openProjectRequested(path);
    accept();
}

void ProjectManagerDialog::onRename()
{
    int row = m_table->currentRow();
    QString path = pathAt(row);
    if (path.isEmpty() || !QFileInfo::exists(path)) return;

    QString current = m_table->item(row, ColName)->text();
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Project"),
        tr("New name:"), QLineEdit::Normal, current, &ok);
    if (!ok || newName.trimmed().isEmpty() || newName == current) return;
    newName = newName.trimmed();

    // Patch the project JSON file
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    root["name"] = newName;
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(root).toJson());
    f.close();

    // Update registry
    ProjectRegistry::instance().renameProject(path, newName);

    // Update table cell
    m_table->item(row, ColName)->setText(newName);
}

void ProjectManagerDialog::onDoubleClick(int row, int /*col*/)
{
    m_table->selectRow(row);
    onOpen();
}

void ProjectManagerDialog::onRemove()
{
    int row = m_table->currentRow();
    QString path = pathAt(row);
    if (path.isEmpty()) return;

    QMessageBox mb(this);
    mb.setWindowTitle(tr("Remove Project"));
    mb.setIcon(QMessageBox::Warning);
    mb.setText(tr("Remove <b>%1</b> from the project list?")
                   .arg(QFileInfo(path).fileName()));
    mb.setInformativeText(tr("The file on disk is NOT deleted."));
    auto *btnRemove = mb.addButton(tr("Remove"), QMessageBox::DestructiveRole);
    mb.addButton(QMessageBox::Cancel);
    mb.setDefaultButton(qobject_cast<QPushButton *>(mb.button(QMessageBox::Cancel)));
    mb.exec();
    if (mb.clickedButton() != btnRemove) return;

    ProjectRegistry::instance().unregisterProject(path);
    m_table->removeRow(row);
    m_countLbl->setText(tr("%1 project(s)").arg(m_table->rowCount()));
    onSelectionChanged();
}

void ProjectManagerDialog::onDeleteFile()
{
    int row = m_table->currentRow();
    QString path = pathAt(row);
    if (path.isEmpty()) return;

    QMessageBox mb(this);
    mb.setWindowTitle(tr("Delete Project File"));
    mb.setIcon(QMessageBox::Critical);
    mb.setText(tr("Permanently delete <b>%1</b>?")
                   .arg(QFileInfo(path).fileName()));
    mb.setInformativeText(tr("This cannot be undone."));
    auto *btnDelete = mb.addButton(tr("Delete"), QMessageBox::DestructiveRole);
    mb.addButton(QMessageBox::Cancel);
    mb.setDefaultButton(qobject_cast<QPushButton *>(mb.button(QMessageBox::Cancel)));
    mb.exec();
    if (mb.clickedButton() != btnDelete) return;

    if (QFile::remove(path))
        ProjectRegistry::instance().unregisterProject(path);
    else
        QMessageBox::critical(this, tr("Error"),
            tr("Could not delete file:\n%1").arg(path));

    m_table->removeRow(row);
    m_countLbl->setText(tr("%1 project(s)").arg(m_table->rowCount()));
    onSelectionChanged();
}

void ProjectManagerDialog::onShowInExplorer()
{
    int row = m_table->currentRow();
    QString path = pathAt(row);
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

// ═══════════════════════════════════════════════════════════════════════════════
// WOLS Catalog tab
// ═══════════════════════════════════════════════════════════════════════════════

#include "io/winols/WinOlsCatalogReader.h"
#include "io/winols/WinOlsConfig.h"
#include "io/winols/WinOlsOpener.h"
#include "io/winols/OlsCfgParser.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFileDialog>

QWidget *ProjectManagerDialog::buildCatalogTab()
{
    if (!m_winCfg) m_winCfg = new winols::Config();

    // First-run convenience: if scanFallback is empty, auto-import the
    // directory list embedded in WinOLS' ols.cfg.  Universal across
    // installs because WinOLS itself stores the user's path picker
    // there.
    if (m_winCfg->scanFallback().isEmpty()) {
        const QStringList found = winols::OlsCfgParser::extractScanRoots();
        if (!found.isEmpty()) m_winCfg->setScanFallback(found);
    }

    auto *w = new QWidget(m_tabs);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    auto *srcLbl = new QLabel(tr("Source:"));
    m_catCombo = new QComboBox();
    m_catCombo->setMinimumWidth(360);
    m_catCfgBtn  = new QPushButton(tr("Settings\xe2\x80\xa6"));
    m_catCfgBtn->setToolTip(tr("Configure WOLS catalog roots and "
                               "per-cache folder mapping"));
    m_catRefresh = new QPushButton(tr("Refresh"));
    m_catRefresh->setToolTip(tr("Re-scan WOLS catalog folders"));
    auto *catBuildIdx = new QPushButton(tr("Build similarity index\xe2\x80\xa6"));
    catBuildIdx->setToolTip(tr(
        "Walk every .ols / .kp / .bin under the configured roots and "
        "compute a fuzzy fingerprint for each.  Required for the "
        "Find Similar Files feature.  May take hours for terabyte "
        "collections; can be paused and resumed."));
    topRow->addWidget(srcLbl);
    topRow->addWidget(m_catCombo, 1);
    topRow->addWidget(catBuildIdx);
    topRow->addWidget(m_catRefresh);
    topRow->addWidget(m_catCfgBtn);
    lay->addLayout(topRow);

    auto *searchRow = new QHBoxLayout();
    auto *searchLbl = new QLabel(tr("Search:"));
    m_catFilter = new QLineEdit();
    m_catFilter->setPlaceholderText(tr("e.g. BMW 320d EDC17"));
    m_catStatus = new QLabel();
    searchRow->addWidget(searchLbl);
    searchRow->addWidget(m_catFilter, 1);
    searchRow->addWidget(m_catStatus);
    lay->addLayout(searchRow);

    m_catTree = new QTreeWidget();
    m_catTree->setColumnCount(8);
    m_catTree->setHeaderLabels(
        {tr("Make"), tr("Model"), tr("Engine"),
         tr("Power"), tr("ECU make"), tr("ECU model"),
         tr("File"), tr("Versions")});
    m_catTree->setRootIsDecorated(false);
    m_catTree->setAlternatingRowColors(true);
    m_catTree->setSortingEnabled(true);
    m_catTree->setUniformRowHeights(true);
    m_catTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_catTree->setStyleSheet(
        "QTreeWidget { background:#0d1117; color:#e6edf3; "
        "  alternate-background-color:#111820; border:1px solid #30363d; "
        "  selection-background-color:#1f6feb; selection-color:#ffffff; }"
        "QHeaderView::section { background:#161b22; color:#8b949e; "
        "  border:none; border-right:1px solid #30363d; "
        "  border-bottom:1px solid #30363d; padding:4px 8px; "
        "  font-weight:bold; }");
    lay->addWidget(m_catTree, 1);

    auto *btnRow = new QHBoxLayout();
    m_catOpenBtn = new QPushButton(tr("Open"));
    m_catOpenBtn->setObjectName("openBtn");
    m_catOpenBtn->setEnabled(false);
    auto *catClose = new QPushButton(tr("Close"));
    btnRow->addStretch();
    btnRow->addWidget(m_catOpenBtn);
    btnRow->addWidget(catClose);
    lay->addLayout(btnRow);

    connect(m_catCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectManagerDialog::onCatalogChanged);
    connect(m_catFilter, &QLineEdit::textChanged,
            this, &ProjectManagerDialog::onCatalogFilterChanged);
    connect(m_catTree, &QTreeWidget::itemDoubleClicked,
            this, &ProjectManagerDialog::onCatalogActivated);
    connect(m_catTree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        m_catOpenBtn->setEnabled(!m_catTree->selectedItems().isEmpty());
    });
    connect(m_catOpenBtn, &QPushButton::clicked, this, [this]() {
        const auto sel = m_catTree->selectedItems();
        if (!sel.isEmpty()) onCatalogActivated(sel.first(), 0);
    });
    connect(m_catCfgBtn,  &QPushButton::clicked,
            this, &ProjectManagerDialog::onCatalogConfigure);
    connect(m_catRefresh, &QPushButton::clicked,
            this, &ProjectManagerDialog::onCatalogRefresh);
    connect(catBuildIdx, &QPushButton::clicked, this, [this]() {
        emit buildSimilarityIndexRequested();
    });
    connect(catClose,     &QPushButton::clicked, this, &QDialog::reject);

    loadCatalogList();
    return w;
}

void ProjectManagerDialog::loadCatalogList()
{
    m_catCombo->blockSignals(true);
    m_catCombo->clear();
    const auto dbs = m_winCfg->discoverCacheDbs();
    if (dbs.isEmpty()) {
        m_catCombo->addItem(tr("(no Cache_*.db found \xe2\x80\x94 configure roots)"));
        m_catCombo->setEnabled(false);
        m_catStatus->setText(tr("Set WOLS catalog roots in Settings"));
    } else {
        m_catCombo->setEnabled(true);
        for (const QString &p : dbs) {
            QFileInfo fi(p);
            m_catCombo->addItem(fi.fileName(), p);
        }
    }
    m_catCombo->blockSignals(false);
    if (m_catCombo->count() > 0 && m_catCombo->isEnabled())
        loadCatalog(m_catCombo->itemData(0).toString());
}

void ProjectManagerDialog::loadCatalog(const QString &dbPath)
{
    m_catTree->clear();
    if (dbPath.isEmpty()) return;
    QString err;
    auto records = winols::WinOlsCatalogReader::dumpAll(dbPath, &err);
    if (!err.isEmpty()) {
        m_catStatus->setText(tr("Error: %1").arg(err));
        return;
    }
    m_catTree->setSortingEnabled(false);
    for (const auto &r : records) {
        QString versions = r.versionsInfo;
        int nl = versions.indexOf('\n');
        if (nl > 0) versions = versions.left(nl) + QStringLiteral(" \xe2\x80\xa6");
        auto *it = new QTreeWidgetItem({
            r.make, r.model, r.engine, r.power,
            r.ecuMake, r.ecuModel, r.filename, versions
        });
        it->setData(0, Qt::UserRole, r.id);
        it->setData(0, Qt::UserRole + 1, r.filename);
        it->setData(0, Qt::UserRole + 2, dbPath);
        it->setToolTip(7, r.versionsInfo);
        it->setToolTip(0,
            QStringLiteral("WOLS #%1\n%2\n%3 \xc2\xb7 %4 \xc2\xb7 %5\n"
                           "Created: %6\nModified: %7")
                .arg(r.winolsNumber)
                .arg(r.filename)
                .arg(r.fileType)
                .arg(r.language)
                .arg(r.swNumber)
                .arg(r.createdAt.toString(Qt::ISODate))
                .arg(r.modifiedAt.toString(Qt::ISODate)));
        m_catTree->addTopLevelItem(it);
    }
    m_catTree->setSortingEnabled(true);
    m_catStatus->setText(tr("%1 records").arg(records.size()));
}

void ProjectManagerDialog::applyCatalogFilter(const QString &needle)
{
    if (needle.isEmpty()) {
        for (int i = 0; i < m_catTree->topLevelItemCount(); ++i)
            m_catTree->topLevelItem(i)->setHidden(false);
        return;
    }
    int shown = 0;
    for (int i = 0; i < m_catTree->topLevelItemCount(); ++i) {
        auto *it = m_catTree->topLevelItem(i);
        bool match = false;
        for (int c = 0; c < it->columnCount(); ++c) {
            if (it->text(c).contains(needle, Qt::CaseInsensitive)) {
                match = true; break;
            }
        }
        it->setHidden(!match);
        if (match) ++shown;
    }
    m_catStatus->setText(tr("%1 / %2 records")
                            .arg(shown).arg(m_catTree->topLevelItemCount()));
}

void ProjectManagerDialog::onCatalogChanged(int idx)
{
    Q_UNUSED(idx);
    if (!m_catCombo->isEnabled()) return;
    loadCatalog(m_catCombo->currentData().toString());
}

void ProjectManagerDialog::onCatalogFilterChanged(const QString &text)
{
    applyCatalogFilter(text.trimmed());
}

void ProjectManagerDialog::onCatalogActivated(QTreeWidgetItem *item, int)
{
    if (!item || !m_winCfg) return;
    const QString filename = item->data(0, Qt::UserRole + 1).toString();
    const QString dbPath   = item->data(0, Qt::UserRole + 2).toString();
    const QString dbBase   = QFileInfo(dbPath).fileName();

    QString err;
    QString resolved = winols::Opener::resolve(*m_winCfg, dbBase, filename, &err);
    if (resolved.isEmpty()) {
        QMessageBox::warning(this, tr("WOLS catalog"),
            tr("Could not locate %1.\n\n%2\n\nUse Settings to map %3 "
               "to its source folder, or add a scan-fallback root.")
                .arg(filename, err, dbBase));
        return;
    }
    emit openOlsRequested(resolved);
    accept();
}

void ProjectManagerDialog::onCatalogConfigure()
{
    if (!m_winCfg) m_winCfg = new winols::Config();

    // Offer the most universal fix first: re-import paths from
    // WinOLS' ols.cfg.  Works on any install regardless of where
    // the user's .ols files actually live.
    auto reply = QMessageBox::question(this, tr("WOLS Catalog settings"),
        tr("Import path list from WinOLS' ols.cfg?\n\nThis pulls every "
           "folder you configured in WinOLS' \"Choose different path\" "
           "dialog and adds them as scan-fallback roots.  Existing "
           "scan-fallback entries are preserved.\n\nClick No to "
           "configure paths manually instead."),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes);
    if (reply == QMessageBox::Cancel) return;

    if (reply == QMessageBox::Yes) {
        const QStringList found = winols::OlsCfgParser::extractScanRoots();
        if (found.isEmpty()) {
            QMessageBox::warning(this, tr("WOLS Catalog settings"),
                tr("No paths found in ols.cfg (looked in %1).\n\n"
                   "Falling back to manual configuration.")
                    .arg(winols::OlsCfgParser::defaultPath()));
        } else {
            QStringList existing = m_winCfg->scanFallback();
            int added = 0;
            for (const QString &p : found) {
                if (!existing.contains(p)) { existing << p; ++added; }
            }
            m_winCfg->setScanFallback(existing);
            QMessageBox::information(this, tr("WOLS Catalog settings"),
                tr("Imported %1 new path(s) from ols.cfg "
                   "(%2 already configured).\n\n"
                   "Total scan-fallback roots: %3.")
                    .arg(added).arg(found.size() - added).arg(existing.size()));
            loadCatalogList();
            return;
        }
    }

    // Manual: pick a single dbRoot dir + a fileRoot for the current
    // Cache_*.db.
    QStringList dbRoots = m_winCfg->dbRoots();
    QString dbDir = QFileDialog::getExistingDirectory(this,
        tr("Pick a WinOLS database folder (contains Cache_*.db)"),
        dbRoots.isEmpty() ? QString() : dbRoots.first());
    if (dbDir.isEmpty()) return;
    if (!dbRoots.contains(dbDir)) dbRoots.prepend(dbDir);
    m_winCfg->setDbRoots(dbRoots);

    if (m_catCombo->isEnabled() && m_catCombo->count() > 0) {
        const QString dbBase =
            QFileInfo(m_catCombo->currentData().toString()).fileName();
        QString fileDir = QFileDialog::getExistingDirectory(this,
            tr("Pick the source folder for %1 (contains the .ols files)")
                .arg(dbBase));
        if (!fileDir.isEmpty()) m_winCfg->setFileRoot(dbBase, fileDir);
    }
    loadCatalogList();
}

void ProjectManagerDialog::onCatalogRefresh()
{
    loadCatalogList();
}

QWidget *ProjectManagerDialog::buildLocalTab() { return nullptr; /* unused — built inline in ctor */ }
