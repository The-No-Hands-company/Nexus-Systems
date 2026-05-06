"""
lsp/telemetry.py
================

Local/dev telemetry for NexusLang LSP diagnostics.

Tracks which error codes are emitted most frequently so we know which error
messages deserve the most documentation and copy-editing attention.

This module is deliberately low-footprint:
  - No network calls, no external services, fully offline.
  - Writes a single JSON file: ~/.nlpl/telemetry/diagnostic_counts.json
  - Telemetry is silently disabled (never raises) if the file cannot be written
    (sandboxed environments, read-only filesystems, CI, etc.).
  - No PII is collected — only error codes and aggregate counts.

Reading the telemetry:
    python dev_tools/check_error_messages.py --show-telemetry

Resetting the counters:
    python dev_tools/check_error_messages.py --reset-telemetry
"""

from __future__ import annotations

import json
import os
import threading
from collections import Counter
from datetime import date
from pathlib import Path
from typing import Dict, List, Optional, Counter

_TELEMETRY_DIR = Path.home() / ".nxl" / "telemetry"
_COUNTS_FILE = _TELEMETRY_DIR / "diagnostic_counts.json"

# Module-level lock so concurrent LSP threads don't corrupt the file.
_lock = threading.Lock()


class DiagnosticTelemetry:
    """
    Records diagnostic code frequencies to a local JSON file.

    Usage (inside DiagnosticsProvider):
        self._telemetry = DiagnosticTelemetry()
        ...
        self._telemetry.record(diagnostics)  # list of LSP Diagnostic dicts
    """

    def record(self, diagnostics: List[Dict]) -> None:
        """
        Increment counters for every error code present in `diagnostics`.
        Silently no-ops if telemetry cannot be persisted.
        """
        if not diagnostics:
            return
        codes = [str(d["code"]) for d in diagnostics if d.get("code")]
        if not codes:
            return
        try:
            _persist_counts(Counter(codes))
        except Exception:
            pass  # Never let telemetry errors propagate to the caller


# ---------------------------------------------------------------------------
# Persistence helpers (module-level, shared across all provider instances)
# ---------------------------------------------------------------------------

def _load_data() -> dict:
    """Load existing telemetry JSON (or return an empty skeleton)."""
    if not _COUNTS_FILE.exists():
        return {"counts": {}, "sessions": 0, "first_seen": str(date.today()), "last_updated": ""}
    try:
        return json.loads(_COUNTS_FILE.read_text(encoding="utf-8"))
    except Exception:
        return {"counts": {}, "sessions": 0, "first_seen": str(date.today()), "last_updated": ""}


def _persist_counts(new_counts: Counter) -> None:
    """Merge new_counts into the on-disk JSON and save."""
    with _lock:
        _TELEMETRY_DIR.mkdir(parents=True, exist_ok=True)
        data = _load_data()
        existing: Dict[str, int] = data.get("counts", {})
        for code, n in new_counts.items():
            existing[code] = existing.get(code, 0) + n
        data["counts"] = existing
        data["sessions"] = data.get("sessions", 0) + 1
        data["last_updated"] = str(date.today())
        _COUNTS_FILE.write_text(json.dumps(data, indent=2, sort_keys=True), encoding="utf-8")


# ---------------------------------------------------------------------------
# Public read / reset helpers (used by check_error_messages.py)
# ---------------------------------------------------------------------------

def get_counts() -> Dict[str, int]:
    """Return a dict of {error_code: total_count}, sorted by frequency."""
    data = _load_data()
    counts = data.get("counts", {})
    return dict(sorted(counts.items(), key=lambda kv: kv[1], reverse=True))


def get_metadata() -> dict:
    """Return telemetry metadata (sessions, dates)."""
    data = _load_data()
    return {
        "sessions": data.get("sessions", 0),
        "first_seen": data.get("first_seen", "unknown"),
        "last_updated": data.get("last_updated", "unknown"),
        "telemetry_file": str(_COUNTS_FILE),
    }


def reset_counts() -> None:
    """Erase all telemetry counters (keeps the file, zeros out counts)."""
    with _lock:
        if not _COUNTS_FILE.exists():
            return
        data = _load_data()
        data["counts"] = {}
        data["sessions"] = 0
        data["last_updated"] = str(date.today())
        _COUNTS_FILE.write_text(json.dumps(data, indent=2, sort_keys=True), encoding="utf-8")
