"""
Canned engine — returns hard-coded sample data so you can smoke-test the
wire protocol end to end without a real ROM analyser.  Drop this into
__init__.py (or import from there) when you want to verify the rx14
client talks to the server correctly before plugging in the real logic.
"""
from __future__ import annotations

import hashlib

from . import EngineError


class CannedEngine:
    def __init__(self) -> None:
        pass

    def analyze(self, rom: bytes, family: str | None) -> dict:
        sha = hashlib.sha256(rom).hexdigest()
        return {
            "fid": sha,
            "ecu": {"name": "DEMO_ECU", "match": "canned"},
            "dtcs_total": 3,
            "dtcs_on":    2,
            "dtcs_off":   1,
            "dtcs": [
                {"idx": 0, "code": "P0420", "obd": "P0420", "man": "16804",
                 "status": "ON",  "info": "Catalyst efficiency low"},
                {"idx": 1, "code": "P0401", "obd": "P0401", "man": "16785",
                 "status": "ON",  "info": "EGR flow insufficient"},
                {"idx": 2, "code": "P2002", "obd": "P2002", "man": "17410",
                 "status": "OFF", "info": "DPF efficiency below threshold"},
            ],
        }

    def disable(self, rom: bytes, codes: list[str],
                family: str | None) -> bytes:
        if not codes:
            raise EngineError("no codes to disable")
        # Demo: stamp a marker into the first 16 bytes so the client can
        # see the file changed.  Real engines never modify a ROM this
        # carelessly — they patch only known DTC trigger sites.
        marker = b"DTCDIS:" + ",".join(codes[:5]).encode()[:9]
        if len(rom) < len(marker):
            raise EngineError("rom too small to apply demo marker")
        return marker + rom[len(marker):]

    def detect_features(self, rom: bytes, family: str | None) -> dict:
        return {
            "available": {
                "popcorn": {
                    "label": "Popcorn limiter",
                    "dtc_count": 0,
                    "has_map_mods": False,
                    "has_patches":  True,
                },
                "launch_control": {
                    "label": "Launch control",
                    "dtc_count": 1,
                    "has_map_mods": True,
                    "has_patches":  False,
                },
            },
        }

    def apply_features(self, rom: bytes, features: list[str],
                       family: str | None) -> bytes:
        if not features:
            raise EngineError("no features to apply")
        marker = b"FEATURE:" + ",".join(features[:3]).encode()[:8]
        if len(rom) < len(marker):
            raise EngineError("rom too small to apply demo marker")
        return marker + rom[len(marker):]
