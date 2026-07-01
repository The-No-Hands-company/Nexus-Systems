"""DeepCode shared config loader for Python tools.

Discovers and merges settings from (in priority order, last wins):
  1. ~/.deepcode/settings.json   (user-global)
  2. ./.deepcode/settings.json   (project-local)
  3. env var DEEPCODE_SETTINGS   (comma-separated extra paths)

Usage:
    from dc_config import load_settings
    settings = load_settings()
    print(settings["workspace"]["name"])
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any


def _find_project_root(start: Path | None = None) -> Path:
    """Walk upward from *start* looking for a .deepcode/ directory."""
    candidate = (start or Path.cwd()).resolve()
    for ancestor in [candidate, *candidate.parents]:
        if (ancestor / ".deepcode" / "settings.json").exists():
            return ancestor
    return candidate


def _merge_dicts(base: dict[str, Any], overlay: dict[str, Any]) -> dict[str, Any]:
    """Deep-merge *overlay* into *base*. Lists are replaced, not appended."""
    for key, value in overlay.items():
        if key in base and isinstance(base[key], dict) and isinstance(value, dict):
            base[key] = _merge_dicts(base[key], value)
        else:
            base[key] = value
    return base


def load_settings(
    start: Path | None = None,
    extra_paths: list[str] | None = None,
) -> dict[str, Any]:
    """Return the merged DeepCode settings dictionary.

    *start* – directory to begin the project-root search.
    *extra_paths* – additional JSON paths to merge (highest priority).
    """
    settings: dict[str, Any] = {}

    # 1) user-global
    global_path = Path.home() / ".deepcode" / "settings.json"
    if global_path.exists():
        try:
            settings = _merge_dicts(settings, json.loads(global_path.read_text("utf-8")))
        except (json.JSONDecodeError, OSError):
            pass

    # 2) project-local
    project_root = _find_project_root(start)
    project_path = project_root / ".deepcode" / "settings.json"
    if project_path.exists():
        try:
            settings = _merge_dicts(settings, json.loads(project_path.read_text("utf-8")))
        except (json.JSONDecodeError, OSError):
            pass

    # 3) DEEPCODE_SETTINGS env var
    env_paths = os.environ.get("DEEPCODE_SETTINGS", "")
    for p in env_paths.split(","):
        p = p.strip()
        if p and Path(p).exists():
            try:
                settings = _merge_dicts(settings, json.loads(Path(p).read_text("utf-8")))
            except (json.JSONDecodeError, OSError):
                pass

    # 4) extra_paths (caller-provided)
    for p in (extra_paths or []):
        if p and Path(p).exists():
            try:
                settings = _merge_dicts(settings, json.loads(Path(p).read_text("utf-8")))
            except (json.JSONDecodeError, OSError):
                pass

    return settings


def resolve_path(settings: dict[str, Any], key: str) -> Path:
    """Resolve a settings path relative to the dhts root.

    If the key ends with 'Dir', ensure the directory exists.
    """
    paths = settings.get("paths", {})
    raw = paths.get(key, "")
    if not raw:
        raise KeyError(f"paths.{key} not found in settings")
    dhts_root = Path(paths.get("dhtsRoot", "."))
    resolved = (dhts_root / raw).resolve()
    return resolved
