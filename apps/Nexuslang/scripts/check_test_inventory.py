#!/usr/bin/env python3
"""
Test inventory gate
===================

Fails when the suite shrinks. Deleting tests is a legitimate thing to do
occasionally, but it must be a visible decision rather than a side effect of
making a red build green — so a genuine removal requires regenerating the
baseline in the same commit, which puts the numbers in the diff where a reviewer
sees them.

This exists because a change once removed 69 test files and 86 assertions from
16 more while stripping ~1,450 lines of validation from the language, and the
suite still reported green: the tests that would have failed were the ones
deleted. Coverage gates did not catch it either, since coverage is measured over
the tests that remain.

Usage:
    python scripts/check_test_inventory.py                  # check against baseline
    python scripts/check_test_inventory.py --update         # rewrite the baseline
    python scripts/check_test_inventory.py --tolerance 0.01 # allow a 1% test drop

Exit codes:
    0 - inventory is at or above the baseline
    1 - the suite shrank beyond tolerance
    2 - baseline missing (run --update to create it)
    3 - could not collect tests
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASELINE = REPO_ROOT / "tests" / "inventory-baseline.json"
TESTS_DIR = REPO_ROOT / "tests"


def count_test_files() -> int:
    return sum(1 for _ in TESTS_DIR.rglob("test_*.py"))


def count_collected_tests() -> int:
    """Ask pytest how many tests it can collect. Collection, not execution, so
    this stays cheap and does not care whether the suite is currently green."""
    proc = subprocess.run(
        [sys.executable, "-m", "pytest", str(TESTS_DIR), "--collect-only", "-q",
         "-p", "no:cacheprovider"],
        cwd=REPO_ROOT, capture_output=True, text=True,
    )
    # pytest prints e.g. "8656 tests collected in 3.21s"; it also prints
    # "N/M tests collected" when deselecting.
    match = re.search(r"(\d+)\s*(?:/\d+\s*)?tests? collected", proc.stdout)
    if not match:
        sys.stderr.write(
            "could not parse a collected-test count from pytest\n"
            f"--- stdout tail ---\n{proc.stdout[-2000:]}\n"
            f"--- stderr tail ---\n{proc.stderr[-2000:]}\n"
        )
        raise SystemExit(3)
    return int(match.group(1))


def measure() -> dict[str, int]:
    return {"test_files": count_test_files(), "collected_tests": count_collected_tests()}


def main() -> int:
    ap = argparse.ArgumentParser(description="Fail when the test suite shrinks")
    ap.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    ap.add_argument("--update", action="store_true",
                    help="rewrite the baseline from the current tree")
    ap.add_argument("--tolerance", type=float, default=0.0,
                    help="fractional drop in collected tests to tolerate (default 0.0)")
    args = ap.parse_args()

    current = measure()

    if args.update:
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        args.baseline.write_text(json.dumps(current, indent=2) + "\n")
        print(f"baseline written to {args.baseline.relative_to(REPO_ROOT)}")
        for key, value in current.items():
            print(f"  {key}: {value}")
        return 0

    if not args.baseline.exists():
        sys.stderr.write(
            f"baseline not found: {args.baseline}\n"
            "run: python scripts/check_test_inventory.py --update\n"
        )
        return 2

    baseline = json.loads(args.baseline.read_text())
    failures: list[str] = []

    # Test files are a hard floor: losing a whole file is never incidental.
    base_files = baseline.get("test_files", 0)
    if current["test_files"] < base_files:
        failures.append(
            f"test files dropped {base_files} -> {current['test_files']} "
            f"({base_files - current['test_files']} removed)"
        )

    base_tests = baseline.get("collected_tests", 0)
    floor = int(base_tests * (1.0 - args.tolerance))
    if current["collected_tests"] < floor:
        failures.append(
            f"collected tests dropped {base_tests} -> {current['collected_tests']} "
            f"(floor {floor} at tolerance {args.tolerance:.1%})"
        )

    for key in ("test_files", "collected_tests"):
        print(f"  {key}: {current[key]} (baseline {baseline.get(key, 0)})")

    if failures:
        sys.stderr.write("\nTest inventory shrank:\n")
        for f in failures:
            sys.stderr.write(f"  - {f}\n")
        sys.stderr.write(
            "\nIf the removal is intentional, regenerate the baseline in this same\n"
            "commit so the change is visible in review:\n"
            "    python scripts/check_test_inventory.py --update\n"
        )
        return 1

    print("test inventory OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
