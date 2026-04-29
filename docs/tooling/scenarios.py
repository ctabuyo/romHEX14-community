"""
scenarios.py — registry of screenshot scenarios for romHEX 14.

Each scenario is a small function that takes:
  - `app`: a RomHex driver instance (with .locale set to "en" or "zh_CN")
  - `out`: the per-chapter output directory
  - `fixtures`: the fixtures root path

and produces one or more PNG files via `app.shot()`.

Menu navigation uses pywinauto's `menu_select()` with locale-aware paths from
the MENU table below. Strings come from `translations/rx14_*.ts`.

Add a new screenshot by:
  1. Adding any new menu paths needed to MENU
  2. Writing a function `def shot_my_thing(app, out, fixtures): ...`
  3. Decorating it with `@scenario("chapter-id")`
  4. Re-running `make screenshots`
"""
from __future__ import annotations

import time
from pathlib import Path
from typing import Callable

from romhex_driver import RomHex


# ────────────────────────────────────────────────────────────────────────────
# Localized menu paths
# ────────────────────────────────────────────────────────────────────────────
#
# Format: MENU[key][locale] -> path string for pywinauto's menu_select.
# pywinauto matches the *visible* menu text (after Qt strips '&' accelerators).
# In ZH the menu titles include "(P)" / "(E)" suffixes which ARE visible — so
# they must appear in the path.

MENU = {
    "import_a2l": {
        "en":    "Project->Import A2L…",
        "zh_CN": "项目(P)->导入 A2L…",
    },
    "import_ols": {
        "en":    "Project->Import OLS…",
        "zh_CN": "项目(P)->导入 OLS…",
    },
    "import_kp": {
        "en":    "Project->Import KP…",
        "zh_CN": "项目(P)->导入 KP…",
    },
    "ai_functions": {
        "en":    "Project->AI Functions…",
        "zh_CN": "项目(P)->AI 功能…",
    },
    "verify_checksum": {
        "en":    "Project->Verify Checksum",
        "zh_CN": "项目(P)->校验和验证",
    },
    "patch_editor": {
        "en":    "Project->Patch Editor…",
        "zh_CN": "项目(P)->补丁编辑器…",
    },
    "preferences": {
        "en":    "Miscellaneous->Preferences…",
        "zh_CN": "杂项(M)->首选项…(P)",
    },
    "about": {
        "en":    "?->About RX14",
        "zh_CN": "?->关于 RX14(A)",
    },
}


def menu_path(app: RomHex, key: str) -> str:
    """Return the menu path string for the current locale."""
    paths = MENU[key]
    return paths.get(app.locale, paths["en"])


# ────────────────────────────────────────────────────────────────────────────
# Scenario registry
# ────────────────────────────────────────────────────────────────────────────

SCENARIOS: list[tuple[str, str, Callable]] = []


def scenario(chapter: str):
    def deco(fn: Callable):
        SCENARIOS.append((chapter, fn.__name__.removeprefix("shot_"), fn))
        return fn
    return deco


# ────────────────────────────────────────────────────────────────────────────
# Scenarios
# ────────────────────────────────────────────────────────────────────────────


@scenario("04-user-interface")
def shot_main_window_empty(app: RomHex, out: Path, fixtures: Path):
    """The main window with no project loaded — first thing a user sees."""
    app.shot("main_window_empty", out)


@scenario("04-user-interface")
def shot_main_window_project_menu(app: RomHex, out: Path, fixtures: Path):
    """The Project menu open, showing all import options."""
    app.send_keys("%p")          # Alt+P opens Project in BOTH locales (P accelerator)
    time.sleep(0.5)
    app.shot("main_window_project_menu", out)
    app.send_keys("{ESC}")
    time.sleep(0.2)


@scenario("13-ai-functions")
def shot_ai_assistant_dock(app: RomHex, out: Path, fixtures: Path):
    """The main window with the AI assistant dock visible (default state)."""
    app.shot("ai_assistant_dock", out)


@scenario("06-a2l-import")
def shot_a2l_import_file_dialog(app: RomHex, out: Path, fixtures: Path):
    """The file picker that appears when you choose Project → Import A2L."""
    try:
        app.main_window().menu_select(menu_path(app, "import_a2l"))
        # Qt's QFileDialog appears with a localized title
        dlg = app.wait_for_dialog(r"(Import A2L File|导入 A2L|Open).*", timeout=8)
        time.sleep(0.4)
        app.shot("a2l_import_file_dialog", out, window=dlg)
        app.close_dialog(dlg)
        time.sleep(0.3)
    except Exception as e:
        print(f"    ⚠ {e}")


@scenario("13-ai-functions")
def shot_ai_functions_dialog(app: RomHex, out: Path, fixtures: Path):
    """The AI Functions dialog (Project → AI Functions…)."""
    try:
        app.main_window().menu_select(menu_path(app, "ai_functions"))
        # AI Functions may pop a "requires Pro account" or "needs A2L first"
        # message box if no project is loaded — that itself is a useful shot.
        dlg = app.wait_for_dialog(r"(AI Functions|AI 功能|romHEX 14).*", timeout=6)
        time.sleep(0.4)
        app.shot("ai_functions_dialog", out, window=dlg)
        app.close_dialog(dlg)
        time.sleep(0.3)
    except Exception as e:
        print(f"    ⚠ {e}")


@scenario("11-checksum-manager")
def shot_verify_checksum(app: RomHex, out: Path, fixtures: Path):
    """Project → Verify Checksum (no project loaded → info dialog)."""
    try:
        app.main_window().menu_select(menu_path(app, "verify_checksum"))
        dlg = app.wait_for_dialog(r"(Checksum|校验和|romHEX 14).*", timeout=6)
        time.sleep(0.4)
        app.shot("verify_checksum_dialog", out, window=dlg)
        app.close_dialog(dlg)
        time.sleep(0.3)
    except Exception as e:
        print(f"    ⚠ {e}")


@scenario("15-settings")
def shot_preferences_dialog(app: RomHex, out: Path, fixtures: Path):
    """Miscellaneous → Preferences."""
    try:
        app.main_window().menu_select(menu_path(app, "preferences"))
        dlg = app.wait_for_dialog(r"(Preferences|Configuration|首选项|配置).*", timeout=6)
        time.sleep(0.5)
        app.shot("preferences_dialog", out, window=dlg)
        app.close_dialog(dlg)
        time.sleep(0.3)
    except Exception as e:
        print(f"    ⚠ {e}")


@scenario("15-settings")
def shot_about_dialog(app: RomHex, out: Path, fixtures: Path):
    """? → About RX14."""
    try:
        app.main_window().menu_select(menu_path(app, "about"))
        dlg = app.wait_for_dialog(r"(About|关于|RX14|romHEX).*", timeout=6)
        time.sleep(0.4)
        app.shot("about_dialog", out, window=dlg)
        app.close_dialog(dlg)
        time.sleep(0.3)
    except Exception as e:
        print(f"    ⚠ {e}")
