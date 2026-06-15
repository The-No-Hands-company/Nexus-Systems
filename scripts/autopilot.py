#!/usr/bin/env python3
"""Root launcher for ecosystem autopilot slash commands."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
TARGET = ROOT / "dhts" / "ecosystem-autopilot" / "autopilot.py"


def _load_target_main():
    if not TARGET.exists():
        print(f"autopilot target not found: {TARGET}", file=sys.stderr)
        return None

    spec = importlib.util.spec_from_file_location("ecosystem_autopilot", TARGET)
    if spec is None or spec.loader is None:
        print("failed to load ecosystem autopilot module", file=sys.stderr)
        return None

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return getattr(module, "main", None)


def main(argv: list[str]) -> int:
    target_main = _load_target_main()
    if target_main is None:
        return 1
    return int(target_main(argv))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
