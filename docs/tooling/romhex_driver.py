"""
romhex_driver.py — high-level driver for the romHEX 14 application.

Wraps the launch lifecycle, locale switching (via Windows registry → QSettings),
window discovery (via pywinauto), and per-window screenshot capture (via PIL +
win32 BitBlt for accurate window grabs even when partially occluded).

Usage from a scenario:

    with RomHex(locale="en") as app:
        app.launch()
        app.wait_for_main_window()
        app.shot("main_window_empty", out_dir)
        app.open_menu("Project", "Import", "A2L…")
        dlg = app.wait_for_dialog("Import A2L")
        app.shot("a2l_import_dialog", out_dir, window=dlg)
"""
from __future__ import annotations

import os
import subprocess
import sys
import time
import winreg
from contextlib import contextmanager
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from PIL import Image, ImageGrab

# pywinauto is heavy; only import the bits we use
from pywinauto import Application, Desktop
from pywinauto.findwindows import ElementNotFoundError
from pywinauto.timings import TimeoutError as PWATimeoutError


REG_PATH = r"Software\CT14\RX14"
DEFAULT_TITLE = "romHEX 14"


# ────────────────────────────────────────────────────────────────────────────
# Registry helpers — set/restore QSettings keys before launch
# ────────────────────────────────────────────────────────────────────────────

@dataclass
class RegistrySnapshot:
    """Saves and restores selected registry values under HKCU\\Software\\CT14\\RX14."""
    keys: dict[str, tuple[Optional[int], Optional[object]]] = field(default_factory=dict)

    def save(self, name: str) -> None:
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_READ) as k:
                value, vtype = winreg.QueryValueEx(k, name)
                self.keys[name] = (vtype, value)
        except FileNotFoundError:
            self.keys[name] = (None, None)
        except OSError:
            self.keys[name] = (None, None)

    def set(self, name: str, value, vtype: int = winreg.REG_SZ) -> None:
        with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_WRITE) as k:
            winreg.SetValueEx(k, name, 0, vtype, value)

    def restore(self) -> None:
        for name, (vtype, value) in self.keys.items():
            try:
                with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, REG_PATH, 0, winreg.KEY_WRITE) as k:
                    if vtype is None:
                        try:
                            winreg.DeleteValue(k, name)
                        except FileNotFoundError:
                            pass
                    else:
                        winreg.SetValueEx(k, name, 0, vtype, value)
            except OSError as e:
                print(f"  warn: failed to restore registry key {name}: {e}", file=sys.stderr)


# ────────────────────────────────────────────────────────────────────────────
# Window screenshot — accurate per-window grab using PrintWindow
# ────────────────────────────────────────────────────────────────────────────

def grab_window(hwnd: int) -> Image.Image:
    """
    Grab a window by HWND using Win32 PrintWindow, which captures the window
    contents even if it is partially obscured by another window.

    Falls back to ImageGrab.grab(bbox=…) if PrintWindow fails.
    """
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32

    # Get window rect (DPI-aware)
    rect = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    width = rect.right - rect.left
    height = rect.bottom - rect.top
    if width <= 0 or height <= 0:
        raise RuntimeError(f"Window {hwnd:#x} has zero size")

    # Bring to foreground for cleanest capture
    try:
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.15)
    except Exception:
        pass

    # First try PrintWindow with PW_RENDERFULLCONTENT (Windows 8.1+)
    PW_RENDERFULLCONTENT = 0x00000002
    hwnd_dc = user32.GetWindowDC(hwnd)
    mem_dc = gdi32.CreateCompatibleDC(hwnd_dc)
    bitmap = gdi32.CreateCompatibleBitmap(hwnd_dc, width, height)
    gdi32.SelectObject(mem_dc, bitmap)

    result = user32.PrintWindow(hwnd, mem_dc, PW_RENDERFULLCONTENT)

    img: Optional[Image.Image] = None
    if result:
        # Extract bitmap bits
        bmp_info = ctypes.create_string_buffer(40)
        # BITMAPINFOHEADER
        ctypes.memset(bmp_info, 0, 40)
        ctypes.cast(bmp_info, ctypes.POINTER(ctypes.c_uint32))[0] = 40            # biSize
        ctypes.cast(ctypes.byref(bmp_info, 4), ctypes.POINTER(ctypes.c_int32))[0] = width
        ctypes.cast(ctypes.byref(bmp_info, 8), ctypes.POINTER(ctypes.c_int32))[0] = -height  # top-down
        ctypes.cast(ctypes.byref(bmp_info, 12), ctypes.POINTER(ctypes.c_uint16))[0] = 1     # planes
        ctypes.cast(ctypes.byref(bmp_info, 14), ctypes.POINTER(ctypes.c_uint16))[0] = 32    # bpp

        buf = ctypes.create_string_buffer(width * height * 4)
        DIB_RGB_COLORS = 0
        gdi32.GetDIBits(mem_dc, bitmap, 0, height, buf, bmp_info, DIB_RGB_COLORS)
        img = Image.frombuffer("RGB", (width, height), buf, "raw", "BGRX", 0, 1)

    # Cleanup GDI
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(hwnd, hwnd_dc)

    if img is None:
        # Fallback: screen-region grab
        img = ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom),
                             all_screens=True)
    return img


# ────────────────────────────────────────────────────────────────────────────
# Driver
# ────────────────────────────────────────────────────────────────────────────

class RomHex:
    """High-level driver for one rx14.exe session."""

    def __init__(
        self,
        exe_path: Path,
        locale: str = "en",
        title: str = DEFAULT_TITLE,
        startup_timeout: float = 30.0,
    ):
        self.exe_path = Path(exe_path)
        self.locale = locale
        self.title = title
        self.startup_timeout = startup_timeout
        self._proc: Optional[subprocess.Popen] = None
        self._app: Optional[Application] = None
        self._main = None
        self._snapshot = RegistrySnapshot()

    # ── lifecycle ───────────────────────────────────────────────────────

    def __enter__(self):
        # Snapshot existing values so we can restore them
        for key in ("language", "introShown"):
            self._snapshot.save(key)
        # Apply our overrides
        self._snapshot.set("language", self.locale)
        # introShown is stored as a Qt bool ("true"/"false" string) in registry
        self._snapshot.set("introShown", "true")
        return self

    def __exit__(self, *exc):
        self.close()
        self._snapshot.restore()

    # Default Qt bin dir for the project's Qt 6.10.2 MinGW build.
    # Override by setting the QT_BIN_DIR environment variable.
    QT_BIN_DIR_DEFAULT = r"C:\Qt\6.10.2\mingw_64\bin"
    QT_TOOLS_DIRS_DEFAULT = [
        r"C:\Qt\Tools\mingw1310_64\bin",
    ]

    def launch(self) -> None:
        if not self.exe_path.exists():
            raise FileNotFoundError(self.exe_path)
        # Bump system DPI awareness so screenshots aren't down-scaled
        env = os.environ.copy()
        env.setdefault("QT_ENABLE_HIGHDPI_SCALING", "1")
        # Inject Qt bin dirs into PATH so the app can find Qt6Widgets.dll etc.
        qt_bin = os.environ.get("QT_BIN_DIR", self.QT_BIN_DIR_DEFAULT)
        extra = [qt_bin] + self.QT_TOOLS_DIRS_DEFAULT
        env["PATH"] = ";".join(extra) + ";" + env.get("PATH", "")
        self._proc = subprocess.Popen(
            [str(self.exe_path)],
            cwd=str(self.exe_path.parent),
            env=env,
        )
        # pywinauto attaches by process ID
        deadline = time.monotonic() + self.startup_timeout
        last_err = None
        while time.monotonic() < deadline:
            try:
                self._app = Application(backend="uia").connect(
                    process=self._proc.pid, timeout=2
                )
                self._main = self._app.window(title_re=self.title + ".*")
                self._main.wait("exists ready visible", timeout=5)
                return
            except (ElementNotFoundError, PWATimeoutError, RuntimeError) as e:
                last_err = e
                time.sleep(0.5)
        raise TimeoutError(
            f"Main window did not appear within {self.startup_timeout}s: {last_err}"
        )

    def close(self) -> None:
        if self._proc is None:
            return
        try:
            if self._main is not None:
                try:
                    self._main.close()
                except Exception:
                    pass
            self._proc.terminate()
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()
        finally:
            self._proc = None
            self._app = None
            self._main = None

    # ── window helpers ─────────────────────────────────────────────────

    def main_window(self):
        if self._main is None:
            raise RuntimeError("Call launch() first")
        return self._main

    def wait_for_main_window(self, timeout: float = 10.0):
        self._main.wait("exists ready visible", timeout=timeout)
        # Settle: let initial timers fire (intro check, autosave init, etc.)
        time.sleep(1.5)
        return self._main

    def wait_for_dialog(self, title_re: str, timeout: float = 15.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                dlg = self._app.window(title_re=title_re)
                dlg.wait("exists visible", timeout=1)
                return dlg
            except (ElementNotFoundError, PWATimeoutError):
                time.sleep(0.3)
        raise TimeoutError(f"Dialog matching {title_re!r} did not appear")

    def close_dialog(self, dlg) -> None:
        try:
            dlg.close()
        except Exception:
            pass

    def open_menu(self, *path: str) -> None:
        """
        Navigate the main window menu, e.g. open_menu('Project', 'Import', 'A2L…').
        Uses pywinauto's MenuSelect which handles localized names if you pass them.
        """
        item_path = " -> ".join(path)
        self._main.menu_select(item_path)

    def send_keys(self, keys: str) -> None:
        from pywinauto.keyboard import send_keys
        send_keys(keys, with_spaces=True)

    # ── screenshot ──────────────────────────────────────────────────────

    def shot(self, name: str, out_dir: Path, window=None, settle_ms: int = 250) -> Path:
        """
        Capture a screenshot of `window` (defaults to main window) and save it
        to `out_dir / f"{name}.png"`. Returns the output path.
        """
        if window is None:
            window = self._main
        time.sleep(settle_ms / 1000)
        try:
            hwnd = window.handle
        except AttributeError:
            hwnd = window.element_info.handle
        img = grab_window(hwnd)
        out_dir.mkdir(parents=True, exist_ok=True)
        out_path = out_dir / f"{name}.png"
        img.save(out_path, "PNG", optimize=True)
        return out_path
