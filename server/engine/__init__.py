"""
Engine interface — the boundary between the public API in app.py and
whatever real ROM-analysis logic you have.  app.py imports `Engine`,
`EngineError`, and `EngineUnavailable` from this module; if any of the
four methods below raise `EngineError("...")`, app.py turns that into a
422 with the message in the error body.

A fresh checkout of this repository contains *only* the placeholder
class below, which raises `EngineUnavailable` on construction and
therefore makes every analyse/apply call return 503.  This is on
purpose: the romHEX14 client expects the protocol contract documented
in app.py, but the actual DTC parser, feature catalog, and patch
generator are vendor-specific and typically proprietary.

To bring the API alive, replace this file with a real implementation.
You can keep the class name and signatures or wrap your own engine —
app.py only cares about the four method names and their return types.
A minimal smoke test that returns canned data is enough to verify the
wire protocol; see test_engine_canned.py for an example you can copy.
"""
from __future__ import annotations


class EngineUnavailable(RuntimeError):
    """Raised by Engine.__init__ when no real engine is wired up."""


class EngineError(RuntimeError):
    """Raised by an engine method when input is invalid or unsupported."""


class Engine:
    """
    Replace this with your own implementation.  All four methods are
    called from a Flask request thread; keep them non-blocking-ish and
    do your own thread pool / queue if a single call would take more
    than a few seconds.

    `family` is the canonical ECU family hint passed by the client
    (EDC17, MED17, SID, DENSO, ...) or None.  Treat it as advisory; if
    your auto-detect disagrees, prefer your own answer and surface the
    mismatch in the response payload.
    """

    def __init__(self) -> None:
        # Remove this raise once you have a real engine to plug in.
        raise EngineUnavailable(
            "no engine module installed on this server — "
            "see server/engine/__init__.py for the contract"
        )

    def analyze(self, rom: bytes, family: str | None) -> dict:
        """
        Inspect a ROM and return what DTCs are present.  The shape that
        the romHEX14 client expects:

            {
                "ok":         True,
                "fid":        "<sha256-of-rom-or-any-id>",
                "ecu":        {"name": "EDC17C46", "match": "exact"},
                "dtcs_total": 3274,
                "dtcs_on":    1842,
                "dtcs_off":   1432,
                "dtcs": [
                    {"idx": 0,
                     "code": "P0420",
                     "obd":  "P0420",
                     "man":  "16804",
                     "status": "ON",
                     "info":   "Catalyst System Efficiency Below Threshold"},
                    ...
                ],
            }

        Missing fields are filled with sensible defaults by app.py
        (`ok=True`, `fid` set to the upload sha).  Anything else passes
        through as-is, so you can return additional metadata if your UI
        opts in to display it later.
        """
        raise NotImplementedError

    def disable(self, rom: bytes, codes: list[str],
                family: str | None) -> bytes:
        """
        Return the patched ROM bytes with the listed DTCs disabled.
        `codes` is the list the client sent — usually OBD-II codes
        (P0420, U1112) but may also be manufacturer codes or numeric
        index strings, depending on what `analyze` reported.

        Raise `EngineError("...")` if a code can't be applied (unknown
        ECU, code not in the catalog, etc).  app.py converts that to a
        422 response with the message intact.
        """
        raise NotImplementedError

    def detect_features(self, rom: bytes, family: str | None) -> dict:
        """
        Return the catalog of features that are applicable to this ROM:

            {
                "ok":        True,
                "available": {
                    "popcorn": {
                        "label":         "Popcorn limiter",
                        "dtc_count":     2,
                        "has_map_mods":  True,
                        "has_patches":   True,
                    },
                    "launch_control": { ... },
                },
            }

        Each key is the feature id the client will send back to
        `apply_features` — keep them stable between releases of your
        engine.  `label` and the boolean hints feed the UI directly.
        """
        raise NotImplementedError

    def apply_features(self, rom: bytes, features: list[str],
                       family: str | None) -> bytes:
        """
        Return the patched ROM bytes with the listed feature ids
        applied.  Same error contract as `disable`.
        """
        raise NotImplementedError
