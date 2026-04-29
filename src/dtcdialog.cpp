/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dtcdialog.h"
#include "project.h"
#include "uiwidgets.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>

DtcDialog::DtcDialog(Project *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setWindowTitle(tr("DTC Manager"));
    setMinimumSize(750, 500);
    resize(850, 600);
    setStyleSheet(
        "QDialog { background:#0d1117; }"
        "QLabel  { color:#c9d1d9; background:transparent; }"
        "QLineEdit { background:#161b22; color:#c9d1d9; border:1px solid #30363d;"
        "  border-radius:4px; padding:6px 10px; font-size:10pt; }"
        "QLineEdit:focus { border-color:#1f6feb; }"
        "QTableWidget { background:#0d1117; color:#c9d1d9; border:1px solid #21262d;"
        "  gridline-color:#21262d; font-size:9pt; }"
        "QTableWidget::item { padding:4px 8px; }"
        "QTableWidget::item:selected { background:#1f6feb; }"
        "QHeaderView::section { background:#161b22; color:#8b949e; border:none;"
        "  border-bottom:1px solid #30363d; padding:6px 8px; font-weight:bold; font-size:8pt; }"
        "QPushButton { background:#21262d; color:#c9d1d9; border:1px solid #30363d;"
        "  border-radius:5px; padding:6px 16px; font-size:9pt; }"
        "QPushButton:hover { border-color:#58a6ff; color:#58a6ff; }"
        "QPushButton:disabled { color:#484f58; border-color:#21262d; }"
    );

    buildUi();
    loadDtcEntries();
    populateTable();
}

void DtcDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(14, 14, 14, 14);

    // Header
    auto *hdr = new QLabel(
        QString::fromUtf8("\xe2\x9a\xa0 ") + tr("DTC Manager — Diagnostic Trouble Codes"), this);
    hdr->setStyleSheet("color:#f0883e; font-size:13pt; font-weight:bold;");
    root->addWidget(hdr);

    // Bosch notice
    auto *notice = new QLabel(
        QString::fromUtf8("\xe2\x84\xb9 ") +
        tr("Alpha — currently supports Bosch ECUs only (MED17, ME17, EDC17). "
           "Requires A2L import with all VALUE maps selected."), this);
    notice->setWordWrap(true);
    notice->setStyleSheet("color:#8b949e; font-size:8pt; padding:2px 4px;"
                          "background:rgba(255,255,255,0.03); border:1px solid #21262d;"
                          "border-radius:4px;");
    root->addWidget(notice);

    // Search bar
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search by DFC name, P-code, or description... (e.g. P0420, Lambda, Catalyst)"));
    root->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) {
        populateTable(t);
    });

    // Stats
    m_stats = new QLabel(this);
    m_stats->setStyleSheet("color:#8b949e; font-size:8pt;");
    root->addWidget(m_stats);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        tr("Status"), tr("DTC Code"), tr("DFC Name"), tr("Control Mask"), tr("Description")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 80);
    m_table->setColumnWidth(2, 250);
    m_table->setColumnWidth(3, 80);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        toggleEntry(row);
    });

    // Buttons
    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_btnToggle = new QPushButton(tr("Toggle Selected"), this);
    m_btnToggle->setStyleSheet(
        "QPushButton { background:#1f3a5f; color:#58a6ff; border-color:#1f6feb; }"
        "QPushButton:hover { background:#1f6feb; color:white; }");
    connect(m_btnToggle, &QPushButton::clicked, this, &DtcDialog::toggleSelected);
    btnRow->addWidget(m_btnToggle);

    m_btnAllOff = new QPushButton(tr("All OFF"), this);
    m_btnAllOff->setStyleSheet(
        "QPushButton { background:#3d1f1f; color:#ff7b72; border-color:#da3633; }"
        "QPushButton:hover { background:#da3633; color:white; }");
    connect(m_btnAllOff, &QPushButton::clicked, this, [this]() {
        for (auto &e : m_entries) {
            if (e.hasCtlMsk) e.ctlMskCurrent = 0;
        }
        populateTable(m_search->text());
    });
    btnRow->addWidget(m_btnAllOff);

    m_btnRestore = new QPushButton(tr("Restore All"), this);
    connect(m_btnRestore, &QPushButton::clicked, this, [this]() {
        for (auto &e : m_entries) {
            if (e.hasCtlMsk) e.ctlMskCurrent = e.ctlMskOriginal;
        }
        populateTable(m_search->text());
    });
    btnRow->addWidget(m_btnRestore);

    btnRow->addStretch();

    m_btnApply = new QPushButton(tr("Apply Changes"), this);
    m_btnApply->setStyleSheet(
        "QPushButton { background:#238636; color:white; border-color:#2ea043; font-weight:bold; }"
        "QPushButton:hover { background:#2ea043; }");
    connect(m_btnApply, &QPushButton::clicked, this, &DtcDialog::applyChanges);
    btnRow->addWidget(m_btnApply);

    auto *btnClose = new QPushButton(tr("Close"), this);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(btnClose);

    root->addLayout(btnRow);
}

void DtcDialog::loadDtcEntries()
{
    if (!m_project) return;
    m_entries.clear();

    const QByteArray &rom = m_project->currentData;
    const QByteArray &origRom = m_project->originalData;
    ByteOrder bo = m_project->byteOrder;

    // Index maps by name for cross-referencing
    QHash<QString, const MapInfo*> mapByName;
    for (const auto &m : m_project->maps)
        mapByName[m.name] = &m;

    // Find all DFC_CtlMsk entries → these are the primary DTC enable/disable switches
    QRegularExpression reCtl(R"(DFC_CtlMsk\d*\.DFC_(\w+)_C)");
    QRegularExpression reDisbl(R"(DFC_DisblMsk\d*\.DFC_(\w+)_C)");
    QRegularExpression reDtcx(R"(DFES_DTCX\.DFC_(\w+)_C)");

    // Collect unique DFC names and their addresses
    QHash<QString, DtcEntry> byDfc;

    for (const auto &m : m_project->maps) {
        auto matchCtl = reCtl.match(m.name);
        if (matchCtl.hasMatch()) {
            QString dfc = matchCtl.captured(1);
            auto &e = byDfc[dfc];
            e.dfcName = "DFC_" + dfc;
            e.ctlMskAddr = m.address;
            e.hasCtlMsk = true;
            // Read current and original values
            if ((int)(m.address + 1) <= rom.size()) {
                e.ctlMskCurrent = (uint8_t)rom[m.address];
                e.ctlMskOriginal = (int)(m.address + 1) <= origRom.size()
                    ? (uint8_t)origRom[m.address] : e.ctlMskCurrent;
            }
            continue;
        }

        auto matchDisbl = reDisbl.match(m.name);
        if (matchDisbl.hasMatch()) {
            QString dfc = matchDisbl.captured(1);
            byDfc[dfc].dfcName = "DFC_" + dfc;
            byDfc[dfc].disblMskAddr = m.address;
            byDfc[dfc].hasDisblMsk = true;
            continue;
        }

        auto matchDtcx = reDtcx.match(m.name);
        if (matchDtcx.hasMatch()) {
            QString dfc = matchDtcx.captured(1);
            byDfc[dfc].dfcName = "DFC_" + dfc;
            byDfc[dfc].dtcxAddr = m.address;
            byDfc[dfc].hasDtcx = true;

            // Read DTC code (uint16)
            if ((int)(m.address + 2) <= rom.size()) {
                uint16_t raw;
                if (bo == ByteOrder::BigEndian)
                    raw = ((uint8_t)rom[m.address] << 8) | (uint8_t)rom[m.address + 1];
                else
                    raw = (uint8_t)rom[m.address] | ((uint8_t)rom[m.address + 1] << 8);
                byDfc[dfc].pCode = rawToPCode(raw);
            }
            continue;
        }
    }

    // Convert to sorted vector, only include entries that have a control mask
    for (auto it = byDfc.begin(); it != byDfc.end(); ++it) {
        if (it->hasCtlMsk)
            m_entries.append(*it);
    }

    qDebug() << "DTC Manager: scanned" << m_project->maps.size() << "maps,"
             << "found" << byDfc.size() << "unique DFCs,"
             << m_entries.size() << "with CtlMsk";

    std::sort(m_entries.begin(), m_entries.end(), [](const DtcEntry &a, const DtcEntry &b) {
        // Sort: P-code first (if available), then DFC name
        if (!a.pCode.isEmpty() && b.pCode.isEmpty()) return true;
        if (a.pCode.isEmpty() && !b.pCode.isEmpty()) return false;
        if (!a.pCode.isEmpty() && !b.pCode.isEmpty()) return a.pCode < b.pCode;
        return a.dfcName < b.dfcName;
    });
}

QString DtcDialog::rawToPCode(uint16_t raw) const
{
    if (raw == 0 || raw == 0xFFFF) return {};

    // SAE J2012 encoding: bits 15-14 = prefix, 13-0 = code
    // 00=P, 01=C, 10=B, 11=U
    char prefix;
    int topBits = (raw >> 14) & 3;
    switch (topBits) {
    case 0: prefix = 'P'; break;
    case 1: prefix = 'C'; break;
    case 2: prefix = 'B'; break;
    case 3: prefix = 'U'; break;
    default: prefix = 'P';
    }

    // Decode: second digit = bits 13-12, then bits 11-8, 7-4, 3-0
    int d2 = (raw >> 12) & 0x3;
    int d3 = (raw >> 8) & 0xF;
    int d4 = (raw >> 4) & 0xF;
    int d5 = raw & 0xF;
    QString code = QString("%1%2%3%4%5").arg(prefix).arg(d2).arg(d3,1,16).arg(d4,1,16).arg(d5,1,16).toUpper();

    // Sanity check: if the raw bytes are ASCII (0x30-0x46 range), this is a
    // text-encoded DTC, not binary SAE. Return empty and let the DFC name be the identifier.
    uint8_t hi = (raw >> 8) & 0xFF;
    uint8_t lo = raw & 0xFF;
    bool isAsciiHex = ((hi >= 0x30 && hi <= 0x39) || (hi >= 0x41 && hi <= 0x46))
                   && ((lo >= 0x30 && lo <= 0x39) || (lo >= 0x41 && lo <= 0x46));
    if (isAsciiHex) return {}; // ASCII-encoded, can't reliably decode from 2 bytes alone

    return code;
}

void DtcDialog::populateTable(const QString &filter)
{
    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    QString f = filter.trimmed().toLower();
    int total = 0, enabled = 0, disabled = 0, modified = 0;

    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &e = m_entries[i];
        total++;

        // Build description early so we can search against it
        static const QHash<QString, QString> knownDtcSearch = {
            {"KAT","Catalyst P0420"}, {"KAT2","Catalyst P0430"}, {"LSU","O2 sensor Lambda"},
            {"LASH","O2 sensor Lambda downstream"}, {"MFI","Misfire P0300"}, {"EGR","EGR exhaust gas recirculation"},
            {"AGR","AGR EGR"}, {"DPF","Diesel particulate filter"}, {"PF","Particulate filter GPF"},
            {"TRB","Turbocharger turbo"}, {"WG","Wastegate turbo"}, {"BKS","Brake switch"},
            {"DKV","Throttle valve"}, {"EZV","Ignition timing"}, {"KS1","Knock sensor"},
            {"KS2","Knock sensor"}, {"NWS","Camshaft"}, {"KWS","Crankshaft"},
            {"INJ","Injector"}, {"HDR","Fuel pressure rail"}, {"SAP","Secondary air pump"},
            {"TEV","EVAP tank vent"}, {"NOX","NOx sensor"}, {"SCR","SCR AdBlue"},
            {"ETK","Electronic throttle"}, {"CFC","Catalytic converter"},
        };
        QString shortN = e.dfcName.mid(4);
        QString searchable = e.dfcName + " " + e.pCode;
        for (auto it = knownDtcSearch.begin(); it != knownDtcSearch.end(); ++it) {
            if (shortN.startsWith(it.key(), Qt::CaseInsensitive)) {
                searchable += " " + it.value();
                break;
            }
        }

        // Filter
        if (!f.isEmpty()) {
            bool match = searchable.toLower().contains(f);
            if (!match) continue;
        }

        if (e.isEnabled()) enabled++; else disabled++;
        if (e.ctlMskCurrent != e.ctlMskOriginal) modified++;

        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Status icon
        auto *statusItem = new QTableWidgetItem(e.isEnabled()
            ? QString::fromUtf8("\xe2\x9c\x85")    // ✅
            : QString::fromUtf8("\xe2\x9b\x94"));  // ⛔
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setData(Qt::UserRole, i); // store index
        m_table->setItem(row, 0, statusItem);

        // P-code
        auto *codeItem = new QTableWidgetItem(e.pCode.isEmpty() ? "—" : e.pCode);
        codeItem->setForeground(e.pCode.isEmpty() ? QColor("#484f58") : QColor("#79c0ff"));
        m_table->setItem(row, 1, codeItem);

        // DFC name
        auto *nameItem = new QTableWidgetItem(e.dfcName);
        nameItem->setForeground(QColor("#c9d1d9"));
        m_table->setItem(row, 2, nameItem);

        // Control mask value
        auto *valItem = new QTableWidgetItem(
            QString("0x%1").arg(e.ctlMskCurrent, 2, 16, QChar('0')).toUpper());
        if (e.ctlMskCurrent != e.ctlMskOriginal) {
            valItem->setForeground(QColor("#f0883e"));
            valItem->setToolTip(tr("Original: 0x%1").arg(e.ctlMskOriginal, 2, 16, QChar('0')).toUpper());
        } else {
            valItem->setForeground(e.isEnabled() ? QColor("#3fb950") : QColor("#ff7b72"));
        }
        valItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, valItem);

        // Readable description: known DFC→description map + CamelCase split
        static const QHash<QString, QString> knownDtc = {
            {"KAT",  "Catalyst efficiency (P0420)"},
            {"KAT2", "Catalyst efficiency bank 2 (P0430)"},
            {"LSU",  "O2 sensor upstream"},
            {"LSU2", "O2 sensor upstream bank 2"},
            {"LASH", "O2 sensor downstream (Lambda after cat)"},
            {"LASH2","O2 sensor downstream bank 2"},
            {"MFI",  "Misfire detection (P0300)"},
            {"EGR",  "EGR valve / exhaust gas recirculation"},
            {"AGR",  "AGR (EGR) valve"},
            {"DPF",  "Diesel particulate filter"},
            {"PF",   "Particulate filter (GPF/DPF)"},
            {"ADBLUE","AdBlue / SCR system"},
            {"SCR",  "Selective catalytic reduction"},
            {"NOX",  "NOx sensor"},
            {"TRB",  "Turbocharger"},
            {"WG",   "Wastegate"},
            {"BKS",  "Brake switch"},
            {"DKV",  "Throttle valve / throttle body"},
            {"EZV",  "Ignition timing"},
            {"KS1",  "Knock sensor 1"},
            {"KS2",  "Knock sensor 2"},
            {"NWS",  "Camshaft position sensor"},
            {"KWS",  "Crankshaft position sensor"},
            {"INJ",  "Injector"},
            {"HDR",  "Fuel pressure (high pressure rail)"},
            {"ETK",  "Electronic throttle"},
            {"LSV",  "Idle air control valve"},
            {"HSV",  "Intake manifold flap"},
            {"SAP",  "Secondary air pump"},
            {"TEV",  "Tank vent valve / EVAP"},
            {"CFC",  "Catalytic converter front (close-coupled)"},
            {"MIL",  "Malfunction indicator lamp"},
            {"OBD",  "OBD monitoring"},
        };
        QString shortName = e.dfcName.mid(4); // strip "DFC_"
        // Try matching known prefixes
        QString desc;
        for (auto it = knownDtc.begin(); it != knownDtc.end(); ++it) {
            if (shortName.startsWith(it.key(), Qt::CaseInsensitive)) {
                desc = it.value();
                break;
            }
        }
        if (desc.isEmpty()) {
            desc = shortName;
            desc.replace(QRegularExpression("([a-z])([A-Z])"), "\\1 \\2");
            desc.replace("_", " ");
        }
        auto *descItem = new QTableWidgetItem(desc);
        descItem->setForeground(QColor("#8b949e"));
        m_table->setItem(row, 4, descItem);
    }

    m_table->setSortingEnabled(true);

    m_stats->setText(tr("%1 DTCs total  |  %2 enabled  |  %3 disabled  |  %4 modified")
        .arg(total).arg(enabled).arg(disabled).arg(modified));
}

void DtcDialog::toggleEntry(int row)
{
    auto *item = m_table->item(row, 0);
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_entries.size()) return;

    auto &e = m_entries[idx];
    if (!e.hasCtlMsk) return;

    e.ctlMskCurrent = e.isEnabled() ? 0 : e.ctlMskOriginal;
    if (e.ctlMskOriginal == 0) e.ctlMskCurrent = 0xFF; // was already 0, toggle back to enabled

    populateTable(m_search->text());
}

void DtcDialog::toggleSelected()
{
    auto sel = m_table->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;

    for (const auto &idx : sel) {
        int row = idx.row();
        auto *item = m_table->item(row, 0);
        if (!item) continue;
        int i = item->data(Qt::UserRole).toInt();
        if (i < 0 || i >= m_entries.size()) continue;
        auto &e = m_entries[i];
        if (!e.hasCtlMsk) continue;
        e.ctlMskCurrent = e.isEnabled() ? 0 : e.ctlMskOriginal;
        if (e.ctlMskOriginal == 0) e.ctlMskCurrent = 0xFF;
    }
    populateTable(m_search->text());
}

void DtcDialog::applyChanges()
{
    if (!m_project) return;

    QByteArray &rom = m_project->currentData;

    // Gather pending changes
    struct Pending { int idx; uint8_t oldVal; uint8_t newVal; };
    QVector<Pending> pending;
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &e = m_entries[i];
        if (!e.hasCtlMsk) continue;
        if (e.ctlMskCurrent == e.ctlMskOriginal) continue;
        if ((int)(e.ctlMskAddr + 1) > rom.size()) continue;
        pending.append({ i, e.ctlMskOriginal, e.ctlMskCurrent });
    }

    if (pending.isEmpty()) {
        QMessageBox::information(this, tr("DTC Manager"), tr("No changes to apply."));
        return;
    }

    // Build change rows for confirm dialog
    QVector<UI::RiskyChangeConfirmDialog::ChangeRow> rows;
    rows.reserve(pending.size());
    for (const Pending &p : pending) {
        const auto &e = m_entries[p.idx];
        UI::RiskyChangeConfirmDialog::ChangeRow r;
        QString code = e.pCode.isEmpty() ? e.dfcName
                                         : QStringLiteral("%1 (%2)").arg(e.pCode, e.dfcName);
        r.label    = code;
        r.oldValue = QString("0x%1").arg(p.oldVal, 2, 16, QChar('0')).toUpper();
        r.newValue = QString("0x%1").arg(p.newVal, 2, 16, QChar('0')).toUpper();
        r.delta    = (p.newVal == 0) ? tr("OFF")
                   : (p.oldVal == 0 ? tr("ON") : QString());
        rows.append(r);
    }

    UI::RiskyChangeConfirmDialog confirm(this);
    confirm.setHeadline(tr("Modify %n DTC(s)", "", pending.size()));
    confirm.setDescription(tr(
        "This will rewrite ECU bytes that control diagnostic trouble codes. "
        "The change will be applied to the active ROM. A version snapshot "
        "will let you revert if needed."));
    confirm.setRisk(UI::RiskyChangeConfirmDialog::Risk::Caution);
    confirm.setChanges(rows);
    confirm.setSnapshotOption(true, true);
    if (pending.size() > 10)
        confirm.setRequireTypedConfirmation(QStringLiteral("MODIFY"));
    confirm.setActionText(tr("Apply"));

    if (confirm.exec() != QDialog::Accepted)
        return;

    if (confirm.snapshotChecked())
        m_project->snapshotVersion(tr("Before DTC edit"));

    int changed = 0;
    for (const Pending &p : pending) {
        const auto &e = m_entries[p.idx];
        rom[e.ctlMskAddr] = (char)p.newVal;
        ++changed;
    }

    if (changed > 0) {
        m_project->modified = true;
        emit m_project->dataChanged();
        QMessageBox::information(this, tr("DTC Manager"),
            tr("%1 DTC(s) modified in ROM.\n\nRemember to save the project and export the ROM.").arg(changed));
    }
}
