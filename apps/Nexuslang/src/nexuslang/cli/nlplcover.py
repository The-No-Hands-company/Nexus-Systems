"""
nlpl-cover — line-coverage reporter for NexusLang programs
======================================================

Runs one or more NexusLang source files with coverage collection enabled and
produces line-coverage reports in text, JSON, and HTML formats.

Usage examples::

    # Run a single file, write coverage/ directory
    nlpl-cover path/to/program.nlpl

    # Run multiple files and merge into one report
    nlpl-cover file1.nlpl file2.nlpl --output cov_report/

    # Skip HTML, only produce JSON
    nlpl-cover program.nlpl --no-html

    # Show summary, skip writing files
    nlpl-cover program.nlpl --no-json --no-html

    # Fail (exit 1) if total coverage is below a threshold
    nlpl-cover program.nlpl --fail-under 80
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import List, Optional


def _run_coverage(
    source_paths: List[str],
    output_dir: str,
    write_json: bool,
    write_html: bool,
    fail_under: Optional[float],
    no_type_check: bool,
    debug: bool,
) -> int:
    """
    Core logic: collect coverage for each source file, merge, and report.

    Returns an exit code (0 = success).
    """
    # --- lazy imports (keep startup fast) -----------------------------------
    _src_root = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "..")
    )
    if _src_root not in sys.path:
        sys.path.insert(0, _src_root)

    from nexuslang.tooling.coverage import CoverageCollector, CoverageReport
    from nexuslang.main import run_program

    collector = CoverageCollector()
    collector.start()

    errors: List[str] = []

    for path in source_paths:
        resolved = os.path.abspath(path)
        try:
            source = Path(resolved).read_text(encoding="utf-8")
        except OSError as exc:
            print(f"[nlpl-cover] Cannot read {path}: {exc}", file=sys.stderr)
            errors.append(path)
            continue

        try:
            run_program(
                source,
                debug=debug,
                type_check=not no_type_check,
                file_path=resolved,
                coverage_collector=collector,
            )
        except Exception as exc:
            # Execution errors don't prevent reporting partial coverage
            print(
                f"[nlpl-cover] Warning: {path} raised {type(exc).__name__}: {exc}",
                file=sys.stderr,
            )

    collector.stop()

    # Build report covering all paths that were attempted
    existing_paths = [p for p in source_paths if Path(p).exists()]
    if existing_paths:
        report = collector.build_report(
            source_paths=[os.path.abspath(p) for p in existing_paths]
        )
    else:
        report = collector.build_report()

    if not report.files:
        print("[nlpl-cover] No coverage data collected.", file=sys.stderr)
        return 1 if errors else 0

    # --- Output -------------------------------------------------------------
    print(report.summary())

    if write_json or write_html:
        os.makedirs(output_dir, exist_ok=True)
        if write_json:
            json_path = os.path.join(output_dir, "coverage.json")
            report.write_json(json_path)
            print(f"[nlpl-cover] JSON  -> {json_path}")
        if write_html:
            report.write_html(output_dir)
            index = os.path.join(output_dir, "index.html")
            print(f"[nlpl-cover] HTML  -> {index}")

    # --- Threshold check ----------------------------------------------------
    if fail_under is not None and report.total_pct() < fail_under:
        print(
            f"[nlpl-cover] FAIL: coverage {report.total_pct():.1f}% < "
            f"required {fail_under:.1f}%",
            file=sys.stderr,
        )
        return 1

    return 1 if errors else 0


def _cli(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        prog="nlpl-cover",
        description="Run NexusLang program(s) with line-coverage collection",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Examples:\n"
            "  nlpl-cover program.nlpl\n"
            "  nlpl-cover *.nlpl --output cov/ --fail-under 80\n"
            "  nlpl-cover program.nlpl --no-html\n"
        ),
    )
    ap.add_argument(
        "files",
        nargs="+",
        metavar="FILE",
        help="NLPL source file(s) to run with coverage",
    )
    ap.add_argument(
        "--output", "-o",
        default="coverage",
        metavar="DIR",
        help="Output directory for report files (default: coverage/)",
    )
    ap.add_argument(
        "--no-json",
        action="store_true",
        help="Skip JSON report output",
    )
    ap.add_argument(
        "--no-html",
        action="store_true",
        help="Skip HTML report output",
    )
    ap.add_argument(
        "--fail-under",
        type=float,
        default=None,
        metavar="PCT",
        help="Exit with code 1 if total coverage is below PCT (0-100)",
    )
    ap.add_argument(
        "--no-type-check",
        action="store_true",
        help="Disable NexusLang type checking during execution",
    )
    ap.add_argument(
        "--debug",
        action="store_true",
        help="Enable NexusLang interpreter debug output",
    )

    args = ap.parse_args(argv)

    # Validate files exist
    missing = [f for f in args.files if not Path(f).exists()]
    if missing:
        for m in missing:
            print(f"[nlpl-cover] File not found: {m}", file=sys.stderr)
        return 1

    return _run_coverage(
        source_paths=args.files,
        output_dir=args.output,
        write_json=not args.no_json,
        write_html=not args.no_html,
        fail_under=args.fail_under,
        no_type_check=args.no_type_check,
        debug=args.debug,
    )


if __name__ == "__main__":
    sys.exit(_cli())
