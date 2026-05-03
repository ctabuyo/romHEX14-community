"""
romHEX14 cloud API — reference implementation.

Run with `flask --app app run --port 5095` for development, or under
gunicorn behind a TLS-terminating reverse proxy (sample units in this
directory) for production.

The handlers here cover the wire protocol exposed to the rx14 client:
auth, rate limits, upload persistence, request validation, response
shape.  The actual analysis and patching logic lives behind a small
`engine` interface — see `engine/__init__.py` and the README.  A fresh
checkout returns 503 from every analyse/apply call until the operator
provides their own engine implementation; that boundary is intentional.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import secrets
import time
from collections import defaultdict, deque
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from flask import Flask, Response, jsonify, request

try:
    from engine import Engine, EngineError, EngineUnavailable
    _engine: Engine | None = Engine()
    _engine_error: str | None = None
except EngineUnavailable as e:
    _engine = None
    _engine_error = str(e)
except Exception as e:                                     # pragma: no cover
    _engine = None
    _engine_error = f"engine import failed: {e!r}"


# Paths and constants.  Override via env if the system layout differs;
# defaults match the systemd unit + nginx vhost shipped in this folder.

UPLOAD_ROOT = Path(os.environ.get("RX14_UPLOAD_ROOT", "/var/lib/cloudfx/uploads"))
TOKENS_FILE = Path(os.environ.get("RX14_TOKENS_FILE", "/etc/cloudfx/tokens.json"))

MAX_UPLOAD_BYTES = int(os.environ.get("RX14_MAX_UPLOAD_BYTES", 16 * 1024 * 1024))
ROM_EXTENSIONS = {".bin", ".rom", ".hex", ".ori", ".s19", ".mpc", ".kp", ".ols"}

DTC_RATE_LIMIT_PER_IP_PER_HOUR        = 30
FEATURES_RATE_LIMIT_PER_TOKEN_PER_MIN = 30

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = MAX_UPLOAD_BYTES + 1 * 1024 * 1024  # +1MiB for form


# Token storage.  Tokens are stored hashed (SHA-256) — leaking the file
# alone never grants access; you'd need to brute-force the preimage,
# which is infeasible for our 32-byte secrets.

def _load_tokens() -> dict[str, dict]:
    try:
        return json.loads(TOKENS_FILE.read_text())
    except FileNotFoundError:
        return {}
    except Exception as e:
        app.logger.error("tokens load failed: %s", e)
        return {}


def _hash_token(raw: str) -> str:
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def _validate_bearer() -> dict | None:
    auth = request.headers.get("Authorization", "")
    if not auth.startswith("Bearer "):
        return None
    raw = auth[7:].strip()
    if not raw:
        return None
    return _load_tokens().get(_hash_token(raw))


# Rate limiting.  In-memory sliding window — fine for a single gunicorn
# worker pool because the buckets reset on restart and the limits are
# loose by design.  Swap for a redis backend if you scale horizontally.

_dtc_buckets:      dict[str, deque[float]] = defaultdict(deque)
_features_buckets: dict[str, deque[float]] = defaultdict(deque)


def _client_ip() -> str:
    fwd = request.headers.get("X-Forwarded-For", "")
    if fwd:
        return fwd.split(",")[0].strip()
    return request.remote_addr or "0.0.0.0"


def _rate_limit_dtc() -> tuple[bool, int]:
    ip = _client_ip()
    now = time.time()
    window = 3600.0
    bucket = _dtc_buckets[ip]
    while bucket and bucket[0] < now - window:
        bucket.popleft()
    if len(bucket) >= DTC_RATE_LIMIT_PER_IP_PER_HOUR:
        return False, int(bucket[0] + window - now)
    bucket.append(now)
    return True, 0


def _rate_limit_features(token_hash: str) -> tuple[bool, int]:
    now = time.time()
    window = 60.0
    bucket = _features_buckets[token_hash]
    while bucket and bucket[0] < now - window:
        bucket.popleft()
    if len(bucket) >= FEATURES_RATE_LIMIT_PER_TOKEN_PER_MIN:
        return False, int(bucket[0] + window - now)
    bucket.append(now)
    return True, 0


# Upload persistence.  We never use the user-supplied filename as part of
# the on-disk path — only as metadata in a sidecar JSON.  This kills
# both filename-collision and path-traversal concerns in one move.

_SAFE_NAME_RX = re.compile(r"[^A-Za-z0-9._-]+")


def _sanitise_user_filename(name: str | None) -> str:
    if not name:
        return "(unset)"
    base = os.path.basename(name)
    return _SAFE_NAME_RX.sub("_", base)[:120] or "(unset)"


def _store_upload(content: bytes, *, kind: str, user_filename: str | None,
                  family_hint: str | None, extra_meta: dict | None = None) -> str:
    """
    Write the ROM under a deterministic, opaque filename.  Returns the
    sha256 of the bytes so callers can correlate uploads with later log
    entries without needing the path.
    """
    UPLOAD_ROOT.mkdir(parents=True, exist_ok=True)
    sha = hashlib.sha256(content).hexdigest()
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    suffix = secrets.token_hex(4)
    stem = f"{ts}_{sha[:16]}_{suffix}"
    (UPLOAD_ROOT / f"{stem}.bin").write_bytes(content)
    meta = {
        "sha256":          sha,
        "kind":            kind,
        "user_filename":   _sanitise_user_filename(user_filename),
        "family_hint":     family_hint,
        "size":            len(content),
        "received_at":     ts,
        "client_ip":       _client_ip(),
    }
    if extra_meta:
        meta.update(extra_meta)
    (UPLOAD_ROOT / f"{stem}.meta.json").write_text(
        json.dumps(meta, indent=2, sort_keys=True))
    return sha


# Request helpers.

def _read_rom_field() -> tuple[bytes, str]:
    f = request.files.get("rom")
    if not f:
        return b"", ""
    name = f.filename or ""
    ext = os.path.splitext(name)[1].lower()
    if ext and ext not in ROM_EXTENSIONS:
        # Soft check — allow unknown extensions but log them; tooling in the
        # field uses dozens of suffixes and we don't want to block legit
        # workflows over a typo.
        app.logger.info("rom upload with unusual ext %r", ext)
    content = f.read()
    return content, name


def _normalise_family_hint(raw: str | None) -> str | None:
    if not raw:
        return None
    raw = raw.strip().upper()
    return raw or None


def _need_engine_or_503():
    """Return a 503 JSON response when no engine is wired up."""
    return jsonify(ok=False,
                   error="engine not available",
                   detail=_engine_error or
                          "no engine module installed on this server"), 503


def _need_rom_or_400():
    return jsonify(ok=False, error="missing 'rom' field"), 400


# Public endpoints.

@app.route("/v1/health")
def health():
    return jsonify(ok=True, service="cloudfx", ts=int(time.time()),
                   engine="ready" if _engine else "absent")


@app.errorhandler(413)
def _too_large(_e):
    return jsonify(ok=False, error="payload too large",
                   limit=MAX_UPLOAD_BYTES), 413


@app.errorhandler(404)
def _not_found(_e):
    return jsonify(ok=False, error="not found"), 404


@app.route("/v1/dtc/analyze", methods=["POST"])
def dtc_analyze():
    rom, fname = _read_rom_field()
    if not rom:
        return _need_rom_or_400()
    ok, retry = _rate_limit_dtc()
    if not ok:
        return jsonify(ok=False, error="rate limit exceeded",
                       retry_after_sec=retry), 429
    if _engine is None:
        return _need_engine_or_503()

    family = _normalise_family_hint(request.form.get("family"))
    sha = _store_upload(rom, kind="dtc.analyze",
                        user_filename=fname, family_hint=family)
    try:
        result = _engine.analyze(rom, family)
    except EngineError as e:
        return jsonify(ok=False, error=str(e)), 422
    result.setdefault("ok", True)
    result.setdefault("fid", sha)
    return jsonify(result)


@app.route("/v1/dtc/disable", methods=["POST"])
def dtc_disable():
    rom, fname = _read_rom_field()
    if not rom:
        return _need_rom_or_400()
    ok, retry = _rate_limit_dtc()
    if not ok:
        return jsonify(ok=False, error="rate limit exceeded",
                       retry_after_sec=retry), 429
    if _engine is None:
        return _need_engine_or_503()

    codes_raw = request.form.get("codes", "").strip()
    codes = [c.strip() for c in codes_raw.split(",") if c.strip()]
    if not codes:
        return jsonify(ok=False, error="missing 'codes' field"), 400
    family = _normalise_family_hint(request.form.get("family"))
    _store_upload(rom, kind="dtc.disable",
                  user_filename=fname, family_hint=family,
                  extra_meta={"codes": codes})
    try:
        patched = _engine.disable(rom, codes, family)
    except EngineError as e:
        return jsonify(ok=False, error=str(e)), 422

    resp = Response(patched, mimetype="application/octet-stream")
    resp.headers["X-cloudfx-tier"] = "free"
    resp.headers["X-cloudfx-bytes-in"]  = str(len(rom))
    resp.headers["X-cloudfx-bytes-out"] = str(len(patched))
    return resp


@app.route("/v1/features/detect", methods=["POST"])
def features_detect():
    tok = _validate_bearer()
    if not tok:
        return jsonify(ok=False, error="invalid or missing token"), 401
    ok, retry = _rate_limit_features(_hash_token(
        request.headers.get("Authorization", "")[7:].strip()))
    if not ok:
        return jsonify(ok=False, error="rate limit exceeded",
                       retry_after_sec=retry), 429
    rom, fname = _read_rom_field()
    if not rom:
        return _need_rom_or_400()
    if _engine is None:
        return _need_engine_or_503()

    family = _normalise_family_hint(request.form.get("family"))
    _store_upload(rom, kind="features.detect",
                  user_filename=fname, family_hint=family,
                  extra_meta={"token_label": tok.get("label")})
    try:
        result = _engine.detect_features(rom, family)
    except EngineError as e:
        return jsonify(ok=False, error=str(e)), 422
    result.setdefault("ok", True)
    return jsonify(result)


@app.route("/v1/features/apply", methods=["POST"])
def features_apply():
    tok = _validate_bearer()
    if not tok:
        return jsonify(ok=False, error="invalid or missing token"), 401
    ok, retry = _rate_limit_features(_hash_token(
        request.headers.get("Authorization", "")[7:].strip()))
    if not ok:
        return jsonify(ok=False, error="rate limit exceeded",
                       retry_after_sec=retry), 429
    rom, fname = _read_rom_field()
    if not rom:
        return _need_rom_or_400()
    if _engine is None:
        return _need_engine_or_503()

    feats_raw = request.form.get("features", "").strip()
    feats = [f.strip() for f in feats_raw.split(",") if f.strip()]
    if not feats:
        return jsonify(ok=False, error="missing 'features' field"), 400
    family = _normalise_family_hint(request.form.get("family"))
    _store_upload(rom, kind="features.apply",
                  user_filename=fname, family_hint=family,
                  extra_meta={"features": feats,
                              "token_label": tok.get("label")})
    try:
        patched = _engine.apply_features(rom, feats, family)
    except EngineError as e:
        return jsonify(ok=False, error=str(e)), 422

    resp = Response(patched, mimetype="application/octet-stream")
    resp.headers["X-cloudfx-tier"] = "pro"
    resp.headers["X-cloudfx-bytes-in"]  = str(len(rom))
    resp.headers["X-cloudfx-bytes-out"] = str(len(patched))
    return resp


if __name__ == "__main__":                                 # pragma: no cover
    app.run(host="127.0.0.1", port=5095, debug=False)
