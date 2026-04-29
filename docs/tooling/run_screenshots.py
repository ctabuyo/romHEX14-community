"""
run_screenshots.py — REAL screenshot orchestrator for romHEX 14.

Launches rx14.exe twice (once per locale), runs every registered scenario from
scenarios.py, and writes PNGs to:
    src/en/assets/screenshots/<chapter>/<name>.png
    src/zh/assets/screenshots/<chapter>/<name>.png

Markdown chapters reference screenshots via a relative path that's identical
in both languages, so MkDocs i18n auto-picks the right localized version.

Usage:
    python tooling/run_screenshots.py \\
        --app ../build-debug/rx14.exe \\
        --fixtures "C:/Users/Cris/Desktop/特调" \\
        --out-en src/en/assets/screenshots \\
        --out-zh src/zh/assets/screenshots \\
        [--locale en|zh|both] \\
        [--only <name>] \\
        [--list]

Use --locale en (or zh) to capture only one language while iterating, and
--only <name> to capture a single scenario for fast feedback.
"""
from __future__ import annotations

import argparse
import io
import sys
import traceback
from pathlib import Path

# Force UTF-8 stdout/stderr so we can print non-ASCII fixture paths on Windows
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# Make sibling import work regardless of cwd
sys.path.insert(0, str(Path(__file__).parent))

from romhex_driver import RomHex
import scenarios  # registers all @scenario-decorated functions
from scenarios import SCENARIOS


LOCALE_MAP = {"en": "en", "zh": "zh_CN"}


def run_locale(
    locale_short: str,
    exe: Path,
    fixtures: Path,
    out_root: Path,
    only: str | None,
) -> tuple[int, int]:
    """Run all scenarios for one locale. Returns (succeeded, failed)."""
    qt_locale = LOCALE_MAP[locale_short]
    print(f"\n══════ Locale: {locale_short} (QSettings = {qt_locale!r}) ══════")

    succeeded = 0
    failed = 0

    with RomHex(exe_path=exe, locale=qt_locale) as app:
        try:
            print("  launching app…")
            app.launch()
            app.wait_for_main_window()
            print(f"  app ready, main window pid={app._proc.pid}")
        except Exception as e:
            print(f"  ✗ failed to launch: {e}")
            traceback.print_exc()
            return (0, len(SCENARIOS))

        for chapter, name, fn in SCENARIOS:
            if only and only != name:
                continue
            chapter_dir = out_root / chapter
            print(f"  → [{chapter}] {name}")
            try:
                fn(app, chapter_dir, fixtures)
                succeeded += 1
            except Exception as e:
                failed += 1
                print(f"    ✗ {e}")
                if "--debug" in sys.argv:
                    traceback.print_exc()

    return (succeeded, failed)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True, type=Path, help="Path to rx14.exe (use the debug build)")
    ap.add_argument("--fixtures", required=True, type=Path, help="Fixtures root (e.g. C:/Users/Cris/Desktop/特调)")
    ap.add_argument("--out-en", required=True, type=Path)
    ap.add_argument("--out-zh", required=True, type=Path)
    ap.add_argument("--locale", choices=["en", "zh", "both"], default="both")
    ap.add_argument("--only", help="Run only the named scenario (without 'shot_' prefix)")
    ap.add_argument("--list", action="store_true", help="List registered scenarios and exit")
    args = ap.parse_args()

    if args.list:
        print(f"{len(SCENARIOS)} registered scenarios:\n")
        for chapter, name, _ in SCENARIOS:
            print(f"  [{chapter}]  {name}")
        return

    if not args.app.exists():
        print(f"✗ App not found: {args.app}", file=sys.stderr)
        print("  Hint: build the debug target first, or pass --app build-release/rx14.exe", file=sys.stderr)
        sys.exit(1)
    if not args.fixtures.exists():
        print(f"✗ Fixtures dir not found: {args.fixtures}", file=sys.stderr)
        sys.exit(1)

    print(f"App:       {args.app}")
    print(f"Fixtures:  {args.fixtures}")
    print(f"Scenarios: {len(SCENARIOS)} registered")
    if args.only:
        print(f"Filter:    only '{args.only}'")

    total_ok = 0
    total_fail = 0

    if args.locale in ("en", "both"):
        ok, fail = run_locale("en", args.app, args.fixtures, args.out_en, args.only)
        total_ok += ok
        total_fail += fail

    if args.locale in ("zh", "both"):
        ok, fail = run_locale("zh", args.app, args.fixtures, args.out_zh, args.only)
        total_ok += ok
        total_fail += fail

    print(f"\n══════ Done. Succeeded: {total_ok}  Failed: {total_fail} ══════")
    sys.exit(0 if total_fail == 0 else 1)


if __name__ == "__main__":
    main()
