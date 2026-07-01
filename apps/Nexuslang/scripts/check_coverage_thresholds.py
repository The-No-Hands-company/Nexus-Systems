#!/usr/bin/env python3
"""
scripts/check_coverage_thresholds.py

Reads coverage.json produced by pytest-cov and enforces per-component
minimum coverage thresholds. Exits with code 1 if any threshold is missed.

Usage:
    python scripts/check_coverage_thresholds.py [--json coverage.json]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, Tuple

# ---------------------------------------------------------------------------
# Component-priority coverage policy.
# The policy emphasizes production-critical components first.
# ---------------------------------------------------------------------------
COMPONENT_POLICIES: Dict[str, Dict[str, object]] = {
    "parser": {
        "label": "Parser & Lexer",
        "threshold": 85.0,
        "priority": "P0-Critical",
        "prefixes": ["src/nlpl/parser", "src/nexuslang/parser"],
        "test_hint": "pytest tests/unit/compiler/ -q",
    },
    "interpreter": {
        "label": "Interpreter",
        "threshold": 80.0,
        "priority": "P0-Critical",
        "prefixes": ["src/nlpl/interpreter", "src/nexuslang/interpreter"],
        "test_hint": "pytest tests/unit/interpreter/ -q",
    },
    "typesystem": {
        "label": "Type System",
        "threshold": 75.0,
        "priority": "P0-Critical",
        "prefixes": ["src/nlpl/typesystem", "src/nexuslang/typesystem"],
        "test_hint": "pytest tests/unit/type_system/ -q",
    },
    "lsp": {
        "label": "LSP",
        "threshold": 80.0,
        "priority": "P0-Critical",
        "prefixes": ["src/nlpl/lsp", "src/nexuslang/lsp"],
        "test_hint": "pytest tests/tooling/test_lsp*.py -q",
    },
    "stdlib": {
        "label": "Standard Library",
        "threshold": 80.0,
        "priority": "P1-Supporting",
        "prefixes": ["src/nlpl/stdlib", "src/nexuslang/stdlib"],
        "test_hint": "pytest tests/unit/stdlib/ -q",
    },
    "runtime": {
        "label": "Runtime",
        "threshold": 75.0,
        "priority": "P1-Supporting",
        "prefixes": ["src/nlpl/runtime", "src/nexuslang/runtime"],
        "test_hint": "pytest tests/unit/runtime/ -q",
    },
}

GLOBAL_THRESHOLD: float = 80.0

COMPONENT_TEST_HINTS: Dict[str, str] = {
    component_name: str(policy["test_hint"])
    for component_name, policy in COMPONENT_POLICIES.items()
}


def _normalise_path(p: str) -> str:
    """Convert backslashes to forward slashes and strip leading ./"""
    return p.replace("\\", "/").lstrip("./")


def _load_coverage(json_path: Path) -> dict:
    with json_path.open() as fh:
        return json.load(fh)


def _component_stats(
    files: dict,
) -> Dict[str, Tuple[int, int]]:
    """Return {component_name: (covered_lines, total_lines)}."""
    stats: Dict[str, Tuple[int, int]] = {}
    for fname, data in files.items():
        fn = _normalise_path(fname)
        for component_name, policy in COMPONENT_POLICIES.items():
            prefixes = [
                _normalise_path(str(prefix))
                for prefix in policy.get("prefixes", [])
            ]
            if any(fn.startswith(prefix) for prefix in prefixes):
                covered, total = stats.get(component_name, (0, 0))
                summary = data.get("summary", {})
                covered += summary.get("covered_lines", 0)
                total += summary.get("num_statements", 0)
                stats[component_name] = (covered, total)
                break
    return stats


def _build_markdown_summary(
    component_rows: list[dict[str, object]],
    global_pct: float,
    global_ok: bool,
    failures: list[str],
    json_path: Path,
) -> str:
    lines: list[str] = []
    lines.append("## Coverage Gate Summary")
    lines.append("")
    lines.append(f"- Coverage source: `{json_path}`")
    lines.append(f"- Global threshold: {GLOBAL_THRESHOLD:.1f}%")
    lines.append(f"- Global coverage: {global_pct:.1f}%")
    lines.append(f"- Status: {'PASS' if global_ok and not failures else 'FAIL'}")
    lines.append("")
    lines.append("| Priority | Component | Coverage | Threshold | Status |")
    lines.append("|---|---|---:|---:|---|")
    for row in component_rows:
        lines.append(
            "| "
            f"{row['priority']} | {row['label']} | {row['coverage_pct']:.1f}% | {row['threshold_pct']:.1f}% | {row['status']} |"
        )

    lines.append(
        f"| TOTAL (all src/nlpl) | {global_pct:.1f}% | {GLOBAL_THRESHOLD:.1f}% | {'OK' if global_ok else 'FAIL'} |"
    )

    if failures:
        lines.append("")
        lines.append("### Failures")
        for failure in failures:
            lines.append(f"- {failure}")
        lines.append("")
        lines.append("### Suggested next commands")
        lines.append("- `pytest tests/ -q --cov=src/nlpl --cov-report=json:coverage.json`")
        for component_name, hint in COMPONENT_TEST_HINTS.items():
            lines.append(f"- For {component_name}: `{hint}`")

    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Enforce per-component coverage thresholds."
    )
    parser.add_argument(
        "--json",
        dest="json_path",
        default="coverage.json",
        help="Path to coverage.json (default: coverage.json)",
    )
    parser.add_argument(
        "--no-fail",
        action="store_true",
        help="Print results but do not exit non-zero on failure",
    )
    parser.add_argument(
        "--summary-json",
        type=Path,
        default=None,
        help="Optional path to write machine-readable summary JSON",
    )
    parser.add_argument(
        "--summary-markdown",
        type=Path,
        default=None,
        help="Optional path to write markdown summary for CI step summaries",
    )
    args = parser.parse_args(argv)

    json_path = Path(args.json_path)
    if not json_path.exists():
        missing_failure = (
            f"coverage source not found: {json_path}. "
            "Run pytest with --cov-report=json:<file> before invoking this gate."
        )
        summary_payload = {
            "status": "fail",
            "coverage_json": str(json_path),
            "global": {
                "coverage_pct": 0.0,
                "threshold_pct": GLOBAL_THRESHOLD,
                "status": "FAIL",
            },
            "components": [],
            "failures": [missing_failure],
        }
        if args.summary_json is not None:
            args.summary_json.write_text(
                json.dumps(summary_payload, indent=2), encoding="utf-8"
            )
        if args.summary_markdown is not None:
            args.summary_markdown.write_text(
                "\n".join(
                    [
                        "## Coverage Gate Summary",
                        "",
                        f"- Coverage source: `{json_path}`",
                        f"- Global threshold: {GLOBAL_THRESHOLD:.1f}%",
                        "- Status: FAIL",
                        "",
                        "### Failures",
                        f"- {missing_failure}",
                        "",
                        "### Suggested next commands",
                        "- `pytest tests/ -q --cov=src/nlpl --cov-report=json:coverage.json`",
                        "- `python scripts/check_coverage_thresholds.py --json coverage.json`",
                    ]
                )
                + "\n",
                encoding="utf-8",
            )
        print(f"[coverage-check] ERROR: {json_path} not found.", file=sys.stderr)
        print(
            "  Run: pytest tests/ --cov=src/nlpl --cov-report=json",
            file=sys.stderr,
        )
        return 1 if not args.no_fail else 0

    data = _load_coverage(json_path)
    files: dict = data.get("files", {})
    total_summary = data.get("totals", {})

    global_pct: float = total_summary.get("percent_covered", 0.0)
    component_stats = _component_stats(files)

    # ---------------------------------------------------------------------------
    # Print report
    # ---------------------------------------------------------------------------
    COL_W = 26
    print()
    print("=" * 70)
    print("Coverage threshold report")
    print("=" * 70)
    print(f"{'Component':<{COL_W}} {'Priority':>14}  {'Coverage':>10}  {'Threshold':>10}  {'Pass':>6}")
    print("-" * 70)

    failures: list[str] = []
    component_rows: list[dict[str, object]] = []

    for component_name, policy in COMPONENT_POLICIES.items():
        label = str(policy["label"])
        threshold = float(policy["threshold"])
        priority = str(policy["priority"])
        covered, total = component_stats.get(component_name, (0, 0))
        if total == 0:
            pct = 0.0
            note = " (no files found)"
        else:
            pct = 100.0 * covered / total
            note = ""
        ok = pct >= threshold
        status = "OK" if ok else "FAIL"
        print(
            f"{label:<{COL_W}} {priority:>14}  {pct:>9.1f}%  {threshold:>9.1f}%  {status:>6}{note}"
        )
        component_rows.append(
            {
                "name": component_name,
                "label": label,
                "priority": priority,
                "coverage_pct": pct,
                "threshold_pct": threshold,
                "status": status,
                "covered_lines": covered,
                "num_statements": total,
            }
        )
        if not ok:
            suggestion = COMPONENT_TEST_HINTS.get(component_name)
            suggestion_suffix = (
                f". Suggested check: {suggestion}" if suggestion else ""
            )
            failures.append(
                f"[{priority}] {label}: {pct:.1f}% < {threshold:.1f}% (short by {threshold - pct:.1f}pp){suggestion_suffix}"
            )

    print("-" * 70)
    global_ok = global_pct >= GLOBAL_THRESHOLD
    global_status = "OK" if global_ok else "FAIL"
    print(
        f"{'TOTAL (all src/nlpl)':<{COL_W}} {global_pct:>9.1f}%  "
        f"{GLOBAL_THRESHOLD:>9.1f}%  {global_status:>6}"
    )
    print("=" * 70)

    if not global_ok:
        failures.append(
            f"GLOBAL: {global_pct:.1f}% < {GLOBAL_THRESHOLD:.1f}% "
            f"(short by {GLOBAL_THRESHOLD - global_pct:.1f}pp)"
        )

    summary_payload = {
        "status": "pass" if not failures else "fail",
        "coverage_json": str(json_path),
        "global": {
            "coverage_pct": round(global_pct, 2),
            "threshold_pct": GLOBAL_THRESHOLD,
            "status": "OK" if global_ok else "FAIL",
        },
        "components": component_rows,
        "failures": failures,
    }

    if args.summary_json is not None:
        args.summary_json.write_text(
            json.dumps(summary_payload, indent=2), encoding="utf-8"
        )

    if args.summary_markdown is not None:
        args.summary_markdown.write_text(
            _build_markdown_summary(
                component_rows=component_rows,
                global_pct=global_pct,
                global_ok=global_ok,
                failures=failures,
                json_path=json_path,
            ),
            encoding="utf-8",
        )

    if failures:
        print()
        print("[coverage-check] FAILED - thresholds not met:")
        for msg in failures:
            print(f"  - {msg}")
        print()
        return 0 if args.no_fail else 1

    print()
    print("[coverage-check] All thresholds met.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
