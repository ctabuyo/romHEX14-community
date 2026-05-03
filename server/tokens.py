#!/usr/bin/env python3
"""
Token management for the Pro tier of the cloud API.  Stores SHA-256
hashes — never raw secrets — under /etc/cloudfx/tokens.json (override
via the RX14_TOKENS_FILE environment variable to match your layout).

  tokens.py add <label>          generate a new token, print it once
  tokens.py list                 list active tokens
  tokens.py revoke <label-or-hash-prefix>

Run with sudo or as the service user.  After revoke you can keep the
process running; tokens are reloaded on every request so changes take
effect immediately.
"""
from __future__ import annotations

import hashlib
import json
import os
import secrets
import sys
from datetime import datetime, timezone
from pathlib import Path

TOKENS_FILE = Path(os.environ.get("RX14_TOKENS_FILE", "/etc/cloudfx/tokens.json"))


def _load() -> dict:
    if not TOKENS_FILE.exists():
        return {}
    return json.loads(TOKENS_FILE.read_text() or "{}")


def _save(d: dict) -> None:
    TOKENS_FILE.parent.mkdir(parents=True, exist_ok=True)
    tmp = TOKENS_FILE.with_suffix(".tmp")
    tmp.write_text(json.dumps(d, indent=2, sort_keys=True))
    os.chmod(tmp, 0o600)
    tmp.replace(TOKENS_FILE)


def _hash(raw: str) -> str:
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def cmd_add(label: str) -> int:
    raw = secrets.token_urlsafe(32)
    h   = _hash(raw)
    d   = _load()
    if h in d:
        print("error: hash collision (extremely unlikely) — retry",
              file=sys.stderr)
        return 1
    d[h] = {
        "label":  label,
        "issued": datetime.now(timezone.utc).isoformat(),
        "tier":   "pro",
    }
    _save(d)
    print("token issued — store this somewhere safe, it will not be shown again:")
    print()
    print("  " + raw)
    print()
    print(f"label: {label}")
    print(f"hash:  {h[:16]}...")
    return 0


def cmd_list() -> int:
    d = _load()
    if not d:
        print("(no tokens)")
        return 0
    print(f"{'label':<32}  {'issued':<26}  {'hash':<18}  tier")
    for h, entry in sorted(d.items(), key=lambda kv: kv[1].get("issued", "")):
        print(f"{entry.get('label', '?'):<32}  "
              f"{entry.get('issued', '?'):<26}  "
              f"{h[:16]}...  "
              f"{entry.get('tier', 'pro')}")
    return 0


def cmd_revoke(needle: str) -> int:
    d = _load()
    matched = [
        h for h, entry in d.items()
        if h.startswith(needle) or entry.get("label") == needle
    ]
    if not matched:
        print(f"error: no token matches {needle!r}", file=sys.stderr)
        return 1
    if len(matched) > 1:
        print(f"error: ambiguous — {len(matched)} tokens match", file=sys.stderr)
        for h in matched:
            print(f"  {h[:16]}...  {d[h].get('label', '?')}")
        return 1
    h = matched[0]
    label = d[h].get("label", "?")
    del d[h]
    _save(d)
    print(f"revoked: {label}  ({h[:16]}...)")
    return 0


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    cmd = argv[1]
    if cmd == "add" and len(argv) == 3:
        return cmd_add(argv[2])
    if cmd == "list" and len(argv) == 2:
        return cmd_list()
    if cmd == "revoke" and len(argv) == 3:
        return cmd_revoke(argv[2])
    print(__doc__.strip(), file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
