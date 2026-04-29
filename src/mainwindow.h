/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QTimer>
#include <QTranslator>
#include <QFutureWatcher>
#include "io/ols/MapAutoDetect.h"   // ols::MapCandidate
#include "romcomparedlg.h"
#include "projectview.h"
#include "mapoverlay.h"
#include "a2lparser.h"
#include "a2limportdialog.h"
#include "project.h"
#include "newprojectdialog.h"
#include "romlinkdialog.h"
#include "featuregate.h"
#include "accountwidget.h"
#include "apiclient.h"

class AIAssistant;
class UpdateChecker;
class CommandPalette;
struct PaletteEntry;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    enum class PanelFilter { All, Modified, Starred, Recent, TypeValue, TypeCurve, TypeMap };

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    // Project menu
    void actProjectManager();
    void actNewProject();
    void actOpenProject();
    void actSaveProject();
    void actSaveProjectAs();
    void actCloseProject();
    void actImportA2L();
    void actImportKP();
    void actImportOlsProject();
    void actAddVersion();
    void actExportROM();
    void actExportOlsProject();
    void actVerifyChecksum();
    void actCorrectChecksum();

    // View / Window
    void actGoHome();       // close all projects (save prompt) → welcome page
    void actTileWindows();
    void actCascadeWindows();
    void actCompareProjects();

    // ROM linking / compare
    void actLinkRom();
    void actImportLinkedVersion();
    void actCompareRoms();
    void actCompareHex();
    void actImportMapPack();
    void actOpenPatchEditor();

    // Navigation
    void actPrevMap();
    void actNextMap();

    // VSCode-style command palette (Cmd/Ctrl+K). Builds a fresh entry list
    // from the current m_projects + ProjectRegistry + every QAction registered
    // on this window, then shows the popup.
    void actShowCommandPalette();

    // Internal
    void onSubWindowActivated(QMdiSubWindow *sw);
    void onMapActivated(const MapInfo &map, Project *project);
    void onStatusMessage(const QString &msg);
    void onTreeItemClicked(QTreeWidgetItem *item, int col);
    void refreshRecentMapsStrip();

    // Toolbar display-format toggles
    void onDataSizeChanged();
    void onDisplayFmtChanged();
    void onByteOrderChanged();
    void onSignChanged();

    // 2D view sync
    void onWaveSyncScroll(int scrollOffset);
    void onSyncViewSwitch(int index);

private:
    void buildActions();
    void buildMenuBar();
    void buildToolBars();
    void buildLeftPanel();
    void buildWelcomePage();
    void updateCentralPage();
    void loadLanguage(const QString &lang);
    void retranslateUi();

    QMdiSubWindow *openProject(Project *project);
    void loadROMIntoProject(Project *project, const QString &romPath);
    void loadA2LIntoProject(Project *project, const QString &a2lPath);
    void runMapAutoDetectOnImport(Project *project);
    void cancelMapScan(Project *project);
    // Refresh the "Saved · Ns ago / Modified" status-bar indicator.
    void updateAutoSaveStatus();
    void refreshProjectTree();     // thin wrapper — arms debounce timer
    void refreshProjectTreeNow();  // actual rebuild

    ProjectView *activeView()    const;
    Project     *activeProject() const;
    void broadcastAvailableProjects();   // push m_projects to all ProjectViews
    void spawnLinkedRomSubwindowsFor(Project *parent);  // re-open child windows after load
    QSet<Project *> m_expandAllOnNextBuild;             // projects that should auto-expand all map groups on the next tree rebuild

    void editProjectInfo();
    void actAutoDetectMaps();   // Misc → Auto-detect Maps… (OLS-style scanner)
    void actAutoDetectEcu();    // Misc → Auto-detect ECU… (73-detector chain port)
    void applyDisplayFormat();

    void autoRetryFailed(const QVector<QPair<QString,QString>> &failed,
                         const QString &lang,
                         const QVector<QPair<QString,QString>> &allItems,
                         int prevApplied, int totalMaps);
    void applyTreeFilter();    // push current format state → overlay
    void applyUiTheme();       // rebuild stylesheets from AppConfig UI colors
    void retileWindows();      // resize all visible sub-windows to fill MDI viewport
    void showMapOverlay(const QByteArray &romData, const MapInfo &map,
                        ByteOrder byteOrder, Project *project = nullptr);

    // ── Translators ───────────────────────────────────────────────────
    QTranslator m_qtTr;
    QTranslator m_appTr;

    // ── Layout ────────────────────────────────────────────────────────
    QSplitter   *m_mainSplitter    = nullptr;
    QWidget     *m_leftPanel       = nullptr;
    QLabel      *m_leftPanelTitle  = nullptr;
    QTreeWidget *m_projectTree     = nullptr;
    QWidget     *m_recentMapsStrip = nullptr;
    QHBoxLayout *m_recentMapsRow   = nullptr;
    QLabel      *m_recentMapsEmpty = nullptr;
    QLabel      *m_recentMapsTitle = nullptr;
    QLineEdit   *m_filterEdit           = nullptr;
    QPushButton *m_filterChangedBtn     = nullptr;   // "changed only" toggle
    QMdiArea    *m_mdi                  = nullptr;
    QStackedWidget *m_centralStack  = nullptr;
    QWidget        *m_welcomePage   = nullptr;
    QToolBar       *m_formatToolbar  = nullptr;
    QToolBar       *m_projectToolbar = nullptr;
    QAction        *m_actAutoScanOnLoad = nullptr;
    QHash<Project*, QFutureWatcher<QVector<ols::MapCandidate>>*>
        m_mapScanWatchers;
    QWidget        *m_scanStatusWidget = nullptr;
    QLabel         *m_scanStatusLabel  = nullptr;
    QProgressBar   *m_scanStatusBar    = nullptr;
    // VSCode-style auto-save indicator + state.
    QLabel         *m_saveStatusLabel  = nullptr;
    QDateTime       m_lastAutoSaveTime;
    QTimer         *m_saveStatusTickTimer = nullptr;
    QSet<Project*>  m_promptedFirstSave;
    // Auto-save mode persisted in QSettings under "autoSaveMode":
    //   "off"             — no auto-save (manual Ctrl+S only)
    //   "afterDelay"      — debounced after each edit (default)
    //   "onFocusChange"   — when project view loses focus
    //   "onWindowDeactivate" — when application loses focus
    QAction        *m_actAutoSaveOff      = nullptr;
    QAction        *m_actAutoSaveDelay    = nullptr;
    QAction        *m_actAutoSaveFocus    = nullptr;
    QAction        *m_actAutoSaveDeact    = nullptr;

    // ── Panel filter ──────────────────────────────────────────────────
    PanelFilter m_panelFilter = PanelFilter::All;
    QVector<QPair<Project*, QString>> m_recentMaps;  // newest first, max 20
    // Filter chip buttons (stored for styling updates)
    QPushButton *m_chipAll      = nullptr;
    QPushButton *m_chipModified = nullptr;
    QPushButton *m_chipStarred  = nullptr;
    QPushButton *m_chipRecent   = nullptr;
    QPushButton *m_chipValue    = nullptr;
    QPushButton *m_chipCurve    = nullptr;
    QPushButton *m_chipMap      = nullptr;

    // ── Menus (stored for retranslation) ──────────────────────────────
    QMenu *m_menuProject = nullptr;
    QMenu *m_menuEdit    = nullptr;
    QMenu *m_menuView    = nullptr;
    QMenu *m_menuSel     = nullptr;
    QMenu *m_menuFind    = nullptr;
    QMenu *m_menuMisc    = nullptr;
    QMenu *m_menuLang    = nullptr;
    QMenu *m_menuWindow  = nullptr;
    QMenu *m_menuHelp    = nullptr;

    // ── Per-map overlays ──────────────────────────────────────────────
    // Composite key: (Project*, mapName) so the same map name from
    // different projects gets separate overlay windows.
    using OverlayKey = QPair<Project*, QString>;
    QMap<OverlayKey, QPointer<MapOverlay>> m_overlays;

    // ── A2L parser ────────────────────────────────────────────────────
    A2LParser *m_parser          = nullptr;
    Project   *m_parsingProject  = nullptr;

    // ── Auto-save ──────────────────────────────────────────────────────
    QTimer *m_autoSaveTimer = nullptr;
    void    autoSaveAll();
    void    scheduleAutoSave();

    // ── Debounced project-tree refresh ─────────────────────────────────
    QTimer *m_treeRefreshTimer = nullptr;

    // ── Projects / navigation state ───────────────────────────────────
    QVector<Project *> m_projects;
    int                m_currentMapIdx = -1; // index into activeProject->maps

    // ── Font size ─────────────────────────────────────────────────────
    int        m_fontSize       = 10;
    QLabel    *m_fontSizeLabel  = nullptr;
    int        m_treeFontSize   = 10;
    QLabel    *m_treeFontLabel  = nullptr;

    // ── Display format state (reflected in toolbar toggles) ───────────
    int       m_dataSize    = 1;      // 1/2/4 bytes
    bool      m_dataFloat   = false;
    ByteOrder m_byteOrder   = ByteOrder::BigEndian;
    bool      m_signed      = false;
    int       m_displayFmt  = 0;      // 0=dec 1=hex 2=bin 3=pct
    bool      m_showDiff    = false;
    bool      m_heightColors= true;

    // ── File / Project actions ─────────────────────────────────────────
    QAction *m_actProjectMgr = nullptr;
    QAction *m_actNew        = nullptr;
    QAction *m_actOpen       = nullptr;
    QAction *m_actSave       = nullptr;
    QAction *m_actSaveAs     = nullptr;
    QAction *m_actClose      = nullptr;
    QAction *m_actHome       = nullptr;
    QAction *m_actImportA2L  = nullptr;
    QAction *m_actImportKP     = nullptr;
    // Single OLS-import action — toolbar OLS button + Project menu
    // "Import OLS…" entry both reference this. Replaces the previous
    // m_actImportOLS / m_actImportOLS pair which routed to the same
    // implementation anyway.
    QAction *m_actImportOLS = nullptr;
    QAction *m_actAddVersion = nullptr;
    QAction *m_actExport        = nullptr;
    QAction *m_actExportOLS  = nullptr;

    // ── ROM linking / compare ──────────────────────────────────────────
    QAction *m_actLinkRom         = nullptr;
    QAction *m_actImportLinkedVer = nullptr;
    QAction *m_actCompareRoms     = nullptr;
    QAction *m_actCompareHex      = nullptr;
    QAction *m_actImportMapPack   = nullptr;
    QAction *m_actPatchEditor     = nullptr;
    QAction *m_actDtcManager      = nullptr;
    QAction *m_actAIFunctions     = nullptr;
    QAction *m_actVerifyChecksum  = nullptr;
    QAction *m_actCorrectChecksum = nullptr;

    // ── Window actions ─────────────────────────────────────────────────
    QAction *m_actTile    = nullptr;
    QAction *m_actCascade = nullptr;
    QAction *m_actCompare = nullptr;

    // ── Navigation actions ─────────────────────────────────────────────
    QAction *m_actPrevMap     = nullptr;
    QAction *m_actNextMap     = nullptr;
    QAction *m_actSyncCursors = nullptr;

    // ── Command palette (Cmd/Ctrl+K) ───────────────────────────────────
    QAction        *m_actCmdPalette = nullptr;
    CommandPalette *m_cmdPalette    = nullptr;  // lazily constructed

    // ── Map operation actions ──────────────────────────────────────────
    QAction *m_actOptimize   = nullptr;
    QAction *m_actDifference = nullptr;
    QAction *m_actIgnore     = nullptr;
    QAction *m_actFactor     = nullptr;
    QAction *m_actOrigFactor = nullptr;

    // ── Data size group (exclusive checkable) ──────────────────────────
    QAction *m_act8bit  = nullptr;
    QAction *m_act16bit = nullptr;
    QAction *m_act32bit = nullptr;
    QAction *m_actFloat = nullptr;
    QActionGroup *m_grpDataSize = nullptr;

    // ── Byte order group ───────────────────────────────────────────────
    QAction *m_actLo = nullptr;
    QAction *m_actHi = nullptr;
    QActionGroup *m_grpByteOrder = nullptr;

    // ── Sign group ─────────────────────────────────────────────────────
    QAction *m_actSigned   = nullptr;
    QAction *m_actUnsigned = nullptr;
    QActionGroup *m_grpSign = nullptr;

    // ── Display format group ───────────────────────────────────────────
    QAction *m_actDec = nullptr;
    QAction *m_actHex = nullptr;
    QAction *m_actBin = nullptr;
    QAction *m_actPct = nullptr;
    QActionGroup *m_grpDisplayFmt = nullptr;

    // ── Visual toggles ─────────────────────────────────────────────────
    QAction *m_actHeightColors = nullptr;

    // ── AI Assistant ───────────────────────────────────────────────────
    QAction      *m_actToggleAI  = nullptr;
    AIAssistant  *m_aiAssistant  = nullptr;

    // ── Account ────────────────────────────────────────────────────────
    AccountWidget *m_accountWidget  = nullptr;
    QAction       *m_actAccount     = nullptr;

    // ── AI Translation ─────────────────────────────────────────────────
    QPushButton   *m_btnTranslateAll = nullptr;
    QComboBox     *m_langCombo       = nullptr;
    // In-memory session cache: map name → TranslationResult
    QMap<QString, TranslationResult> m_translations;

    // ── Update checker ──────────────────────────────────────────────────
    UpdateChecker *m_updateChecker = nullptr;
    QFrame        *m_updateBar     = nullptr;
    QLabel        *m_updateLabel   = nullptr;
    QProgressBar  *m_updateProgress = nullptr;
    QPushButton   *m_updateBtn     = nullptr;
    QString        m_updateUrl;
    QString        m_updateVersion;
    QString        m_updateChangelog;
};
