"""
gen_docs.py — auto-generate manual pages from the romHEX 14 source tree.

Generates three appendices in both EN and ZH:
  - Appendix A: Menu & Shortcut Reference   (parsed from src/*.cpp QAction calls)
  - Appendix C: Bilingual Glossary          (parsed from translations/*.ts)
  - Appendix D: Changelog                   (parsed from `git log` of repo root)

Usage:
  python gen_docs.py --ts-en ../../translations/rx14_en.ts \\
                     --ts-zh ../../translations/rx14_zh_CN.ts \\
                     --src-dir ../../src \\
                     --repo-root ../.. \\
                     --out-en ../src/en \\
                     --out-zh ../src/zh
"""
from __future__ import annotations

import argparse
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

# ---------- helpers ----------------------------------------------------------

def write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    print(f"  wrote {path}")


# ---------- glossary ---------------------------------------------------------

def parse_ts(ts_path: Path) -> dict[tuple[str, str], str]:
    """Returns {(context, source): translation} from a Qt .ts file."""
    tree = ET.parse(ts_path)
    root = tree.getroot()
    out: dict[tuple[str, str], str] = {}
    for ctx in root.findall("context"):
        name = ctx.findtext("name") or ""
        for msg in ctx.findall("message"):
            src = msg.findtext("source") or ""
            tr_el = msg.find("translation")
            tr = (tr_el.text or "") if tr_el is not None else ""
            if src:
                out[(name, src)] = tr
    return out


def gen_glossary(ts_en: Path, ts_zh: Path, out_en: Path, out_zh: Path) -> None:
    en_map = parse_ts(ts_en)
    zh_map = parse_ts(ts_zh)

    # Build set of unique English source strings, taking the first translation
    # we find for each. Skip very long strings (likely sentences, not terms).
    pairs: dict[str, str] = {}
    for (ctx, src), _tr in en_map.items():
        if len(src) > 60 or "\n" in src:
            continue
        if src in pairs:
            continue
        zh = zh_map.get((ctx, src), "")
        if zh and zh != src:
            pairs[src] = zh

    sorted_pairs = sorted(pairs.items(), key=lambda kv: kv[0].lower())

    def render(lang: str) -> str:
        if lang == "en":
            head = "# Appendix C — Glossary EN/ZH\n\n"
            head += "Auto-generated from `translations/rx14_en.ts` and `translations/rx14_zh_CN.ts`. **Do not edit by hand** — regenerate with `make gen`.\n\n"
            head += "| English | 中文 |\n|---|---|\n"
        else:
            head = "# 附录 C — 术语表 EN/ZH\n\n"
            head += "从 `translations/rx14_en.ts` 与 `translations/rx14_zh_CN.ts` 自动生成。**请勿手动编辑** — 使用 `make gen` 重新生成。\n\n"
            head += "| English | 中文 |\n|---|---|\n"
        rows = "\n".join(
            f"| {en.replace('|', '\\|')} | {zh.replace('|', '\\|')} |"
            for en, zh in sorted_pairs
        )
        return head + rows + "\n"

    write(out_en / "appendix-c-glossary.md", render("en"))
    write(out_zh / "appendix-c-glossary.md", render("zh"))
    print(f"  glossary: {len(sorted_pairs)} terms")


# ---------- menu & shortcut reference ---------------------------------------

# Match QAction creation lines like:
#   auto *act = new QAction(tr("Open…"), this);
#   act->setShortcut(QKeySequence::Open);
#   act->setShortcut(QKeySequence("Ctrl+Shift+O"));
ACTION_RX = re.compile(r'new\s+QAction\s*\(\s*(?:[^,]*,\s*)?tr\(\s*"([^"]+)"', re.MULTILINE)
SHORTCUT_RX = re.compile(r'setShortcut\s*\(\s*(?:QKeySequence\s*\(\s*)?(?:QKeySequence::)?"?([^")]+)"?\s*\)?\s*\)')


def gen_menus(src_dir: Path, out_en: Path, out_zh: Path) -> None:
    rows: list[tuple[str, str, str]] = []  # (file, action, shortcut-or-empty)
    for cpp in sorted(src_dir.glob("*.cpp")):
        text = cpp.read_text(encoding="utf-8", errors="ignore")
        # Naive: pair each QAction with the nearest setShortcut on a following line.
        actions = list(ACTION_RX.finditer(text))
        for m in actions:
            action_label = m.group(1)
            tail = text[m.end(): m.end() + 600]
            sc = SHORTCUT_RX.search(tail)
            shortcut = sc.group(1) if sc else ""
            rows.append((cpp.name, action_label, shortcut))

    def render(lang: str) -> str:
        if lang == "en":
            head = "# Appendix A — Menu & Shortcut Reference\n\n"
            head += "Auto-generated from the application source. **Do not edit by hand.**\n\n"
            head += "| Source file | Action | Shortcut |\n|---|---|---|\n"
        else:
            head = "# 附录 A — 菜单与快捷键参考\n\n"
            head += "从应用程序源代码自动生成。**请勿手动编辑。**\n\n"
            head += "| 源文件 | 操作 | 快捷键 |\n|---|---|---|\n"
        body = "\n".join(
            f"| `{f}` | {a.replace('|', '\\|')} | `{s}` |" if s else f"| `{f}` | {a.replace('|', '\\|')} | — |"
            for f, a, s in rows
        )
        return head + body + "\n"

    write(out_en / "appendix-a-menus.md", render("en"))
    write(out_zh / "appendix-a-menus.md", render("zh"))
    print(f"  menu reference: {len(rows)} actions")


# ---------- changelog -------------------------------------------------------

def gen_changelog(repo_root: Path, out_en: Path, out_zh: Path) -> None:
    try:
        log = subprocess.check_output(
            ["git", "-C", str(repo_root), "log", "--pretty=format:%h|%ad|%s", "--date=short"],
            text=True, encoding="utf-8", errors="ignore",
        )
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"  git log failed: {e}")
        log = ""

    entries = []
    for line in log.splitlines():
        parts = line.split("|", 2)
        if len(parts) == 3:
            entries.append(tuple(parts))

    def render(lang: str) -> str:
        if lang == "en":
            head = "# Appendix D — Changelog\n\nAuto-generated from `git log`. **Do not edit by hand.**\n\n"
        else:
            head = "# 附录 D — 更新日志\n\n从 `git log` 自动生成。**请勿手动编辑。**\n\n"
        body = "\n".join(f"- **{date}** `{h}` — {msg}" for h, date, msg in entries)
        return head + body + "\n"

    write(out_en / "appendix-d-changelog.md", render("en"))
    write(out_zh / "appendix-d-changelog.md", render("zh"))
    print(f"  changelog: {len(entries)} commits")


# ---------- main -------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ts-en", required=True, type=Path)
    ap.add_argument("--ts-zh", required=True, type=Path)
    ap.add_argument("--src-dir", required=True, type=Path)
    ap.add_argument("--repo-root", required=True, type=Path)
    ap.add_argument("--out-en", required=True, type=Path)
    ap.add_argument("--out-zh", required=True, type=Path)
    args = ap.parse_args()

    print("Generating glossary…")
    gen_glossary(args.ts_en, args.ts_zh, args.out_en, args.out_zh)
    print("Generating menu reference…")
    gen_menus(args.src_dir, args.out_en, args.out_zh)
    print("Generating changelog…")
    gen_changelog(args.repo_root, args.out_en, args.out_zh)
    print("Done.")


if __name__ == "__main__":
    main()
