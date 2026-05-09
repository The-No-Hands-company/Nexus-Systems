"""
NLPL Test Runner
================
Discovers, filters, and executes NexusLang test programs.  Supports parallel
execution and multiple output formats.

Usage (programmatic):
    from nexuslang.tooling.test_runner import TestRunner

    runner = TestRunner(workers=4)
    results = runner.run_directory("test_programs/")
    print(runner.format_verbose(results))

Usage (CLI via this module):
    python -m nexuslang.tooling.test_runner test_programs/ --filter "my_suite*"
    python -m nexuslang.tooling.test_runner test_programs/ --format tap
    python -m nexuslang.tooling.test_runner test_programs/ --format json --workers 8
"""

import fnmatch
import json
import os
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Dict, List, Optional


__all__ = [
    "TestRunner",
    "FileTestResult",
    "RunSummary",
    "run_directory",
    "run_files",
]


# ---------------------------------------------------------------------------
# Data classes (plain dicts for simplicity, described below)
# ---------------------------------------------------------------------------
# Each individual test result (from interpreter._collected_test_results) has:
#   { "name": str, "suite": str, "passed": bool, "error": str|None, "duration": float }

# A FileTestResult bundles results for one .nlpl file:
#   { "file": str, "results": [...], "error": None|str,
#     "duration": float, "passed": int, "failed": int, "total": int }

# RunSummary is the aggregate:
#   { "files": [...], "passed": int, "failed": int, "total": int,
#     "duration": float, "errored_files": [...] }


def _run_file_collect(path: str, name_filter: Optional[str],
                      coverage_enabled: bool = False) -> Dict:
    """
    Parse and execute one NexusLang source file, collecting its test results.

    This function creates an independent Runtime + Interpreter instance so it
    is safe to call from multiple threads simultaneously.

    Parameters
    ----------
    path:
        Absolute or relative path to the .nlpl file.
    name_filter:
        Optional glob pattern (e.g. ``"*arithmetic*"``) applied to individual
        test / describe block names.  Tests not matching the pattern are NOT
        reported (they are still executed to support shared setup code, but
        their results are discarded from the output).

    Returns a FileTestResult dict.
    """
    start = time.time()

    # -- Read source --------------------------------------------------------
    try:
        with open(path, "r", encoding="utf-8") as fh:
            source = fh.read()
    except OSError as exc:
        return {
            "file": path, "results": [], "error": f"Cannot read file: {exc}",
            "duration": 0.0, "passed": 0, "failed": 0, "total": 0,
            "coverage_hits": {},
        }

    # -- Set up interpreter chain -------------------------------------------
    try:
        # Lazy imports so the module can be imported without the full nlpl
        # package being available during testing of the runner itself.
        _pkg = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "..")
        )
        if _pkg not in sys.path:
            sys.path.insert(0, _pkg)

        from nexuslang.parser.lexer import Lexer
        from nexuslang.parser.parser import Parser
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.compiler.preprocessor import preprocess_ast, host_target

        runtime = Runtime()
        runtime.module_path = os.path.abspath(path)
        register_stdlib(runtime)

        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens, source=source)
        ast = parser.parse()
        preprocess_ast(ast, target=host_target())

        interpreter = Interpreter(runtime, source=source)

        # Attach coverage collector if requested
        _cov_collector = None
        if coverage_enabled:
            from nexuslang.tooling.coverage import CoverageCollector
            _cov_collector = CoverageCollector()
            interpreter._coverage_collector = _cov_collector

        interpreter.interpret(ast)

        raw_results = interpreter._collected_test_results
        coverage_hits = {}
        if _cov_collector is not None:
            coverage_hits = {
                p: sorted(lines) for p, lines in _cov_collector._hits.items()
            }

    except Exception as exc:
        duration = time.time() - start
        return {
            "file": path,
            "results": [],
            "error": f"{type(exc).__name__}: {exc}",
            "duration": duration,
            "passed": 0,
            "failed": 0,
            "total": 0,
            "coverage_hits": {},
        }

    # -- Apply name filter --------------------------------------------------
    if name_filter:
        filtered = [
            r for r in raw_results
            if fnmatch.fnmatch(r.get("name", ""), name_filter)
            or fnmatch.fnmatch(r.get("suite", ""), name_filter)
        ]
    else:
        filtered = list(raw_results)

    passed = sum(1 for r in filtered if r["passed"])
    failed = len(filtered) - passed
    duration = time.time() - start

    return {
        "file": path,
        "results": filtered,
        "error": None,
        "duration": duration,
        "passed": passed,
        "failed": failed,
        "total": len(filtered),
        "coverage_hits": coverage_hits,
    }


# ---------------------------------------------------------------------------
# TestRunner
# ---------------------------------------------------------------------------


class TestRunner:
    """
    Discover and execute NexusLang test programs.

    Parameters
    ----------
    workers:
        Number of parallel workers.  ``1`` means sequential execution.
        Workers share the GIL but NexusLang test files are I/O-bound during
        parsing, so parallelism still provides a meaningful speedup for
        large suites.  Use ``workers=1`` for deterministic output ordering.
    pattern:
        Glob pattern for file discovery, e.g. ``"test_*.nxl"`` (default).
    name_filter:
        Optional glob pattern applied to individual test / describe names.
    quiet:
        Suppress per-test output (only totals).
    """

    __test__ = False

    def __init__(
        self,
        workers: int = 1,
        pattern: str = "test_*.nxl",
        name_filter: Optional[str] = None,
        quiet: bool = False,
        coverage_enabled: bool = False,
        coverage_output_dir: str = "coverage",
    ):
        self.workers = max(1, workers)
        self.pattern = pattern
        self.name_filter = name_filter
        self.quiet = quiet
        self.coverage_enabled = coverage_enabled
        self.coverage_output_dir = coverage_output_dir

    # ------------------------------------------------------------------
    # Discovery
    # ------------------------------------------------------------------

    def discover(self, directory: str) -> List[str]:
        """Return all .nlpl files under *directory* matching ``self.pattern``.

        Files are returned in sorted order so runs are deterministic.
        """
        matches: List[str] = []
        for root, _dirs, files in os.walk(directory):
            for filename in sorted(files):
                if fnmatch.fnmatch(filename, self.pattern):
                    matches.append(os.path.join(root, filename))
        return sorted(matches)

    def filter_files(self, paths: List[str], regex: str) -> List[str]:
        """Filter a list of file paths using a regex against the filename."""
        pat = re.compile(regex)
        return [p for p in paths if pat.search(os.path.basename(p))]

    # ------------------------------------------------------------------
    # Execution
    # ------------------------------------------------------------------

    def run_files(self, paths: List[str]) -> Dict:
        """Execute a list of NexusLang test files and return a RunSummary."""
        start = time.time()
        file_results: List[Dict] = []
        errored: List[str] = []

        if self.workers == 1:
            # Sequential — preserves output ordering
            for path in paths:
                fr = _run_file_collect(path, self.name_filter,
                                       coverage_enabled=self.coverage_enabled)
                file_results.append(fr)
                if fr["error"]:
                    errored.append(path)
        else:
            # Parallel — submit all then collect in completion order
            with ThreadPoolExecutor(max_workers=self.workers) as pool:
                future_map = {
                    pool.submit(_run_file_collect, p, self.name_filter,
                                self.coverage_enabled): p
                    for p in paths
                }
                # Preserve input ordering for consistent output
                ordered = {p: None for p in paths}
                for future in as_completed(future_map):
                    path = future_map[future]
                    try:
                        fr = future.result()
                    except Exception as exc:
                        fr = {
                            "file": path, "results": [], "error": str(exc),
                            "duration": 0.0, "passed": 0, "failed": 0, "total": 0,
                            "coverage_hits": {},
                        }
                    ordered[path] = fr
                    if fr["error"]:
                        errored.append(path)

                file_results = list(ordered.values())

        total_passed = sum(fr["passed"] for fr in file_results)
        total_failed = sum(fr["failed"] for fr in file_results)
        total_tests = sum(fr["total"] for fr in file_results)

        return {
            "files": file_results,
            "passed": total_passed,
            "failed": total_failed,
            "total": total_tests,
            "duration": time.time() - start,
            "errored_files": errored,
        }

    def run_directory(self, directory: str) -> Dict:
        """Discover all test files in *directory* and run them."""
        paths = self.discover(directory)
        return self.run_files(paths)

    def build_coverage_report(self, summary: Dict):
        """
        Build a ``CoverageReport`` from the coverage_hits data in *summary*.

        Each file result in ``summary["files"]`` may contain a
        ``"coverage_hits"`` dict mapping source-file path to a sorted list
        of hit line numbers.  This method merges all hits across all files
        and builds a ``CoverageReport`` with executable-line analysis.

        Returns ``None`` when no coverage data is available (i.e. the runner
        was not configured with ``coverage_enabled=True``).
        """
        from nexuslang.tooling.coverage import CoverageCollector

        # Merge all hits from all file results
        collector = CoverageCollector()
        collector.start()
        for fr in summary.get("files", []):
            for src_path, hit_lines in fr.get("coverage_hits", {}).items():
                for line in hit_lines:
                    collector.record(src_path, line)

        if not collector._hits:
            return None

        return collector.build_report()

    def write_coverage(self, summary: Dict, output_dir: Optional[str] = None) -> None:
        """
        Build and write coverage report files (JSON + HTML) from *summary*.

        Args:
            summary: RunSummary dict returned by ``run_files()``.
            output_dir: Directory to write into.  Defaults to
                        ``self.coverage_output_dir``.
        """
        import os
        report = self.build_coverage_report(summary)
        if report is None:
            return
        out = output_dir or self.coverage_output_dir
        os.makedirs(out, exist_ok=True)
        report.write_json(os.path.join(out, "coverage.json"))
        report.write_html(out)
        print(report.summary())

    # ------------------------------------------------------------------
    # Formatters
    # ------------------------------------------------------------------

    def format_verbose(self, summary: Dict) -> str:
        """Human-readable verbose output (default).

        Mirrors the style of the inline _print_test_summary but aggregates
        results across multiple files.
        """
        lines: List[str] = []
        sep = "=" * 70

        for fr in summary["files"]:
            if fr["error"]:
                lines.append(f"\n[ERROR] {fr['file']}")
                lines.append(f"  {fr['error']}")
                continue
            if fr["total"] == 0:
                # No test blocks found in this file — skip silently
                continue

            lines.append(f"\n{sep}")
            lines.append(f"File: {fr['file']}")
            for r in fr["results"]:
                status = "PASS" if r["passed"] else "FAIL"
                suite_prefix = f"[{r['suite']}] " if r.get("suite") else ""
                lines.append(
                    f"  [{status}] {suite_prefix}{r['name']} ({r['duration']:.3f}s)"
                )
                if r.get("error"):
                    lines.append(f"         {r['error']}")
            lines.append(sep)
            rate = (fr["passed"] / fr["total"] * 100) if fr["total"] else 0.0
            lines.append(
                f"  {fr['passed']}/{fr['total']} passed ({rate:.1f}%) "
                f"in {fr['duration']:.3f}s"
            )

        lines.append(f"\n{'=' * 70}")
        rate = (summary["passed"] / summary["total"] * 100) if summary["total"] else 0.0
        lines.append(
            f"TOTAL: {summary['passed']}/{summary['total']} passed "
            f"({rate:.1f}%)  --  {summary['duration']:.3f}s"
        )
        if summary["failed"]:
            lines.append(f"FAILURES: {summary['failed']} test(s) failed")
        if summary["errored_files"]:
            lines.append(f"ERRORS IN: {', '.join(summary['errored_files'])}")
        lines.append("")

        return "\n".join(lines)

    def format_tap(self, summary: Dict) -> str:
        """TAP (Test Anything Protocol) version 13 output."""
        lines: List[str] = ["TAP version 13"]
        n = summary["total"]
        lines.append(f"1..{n}")
        test_num = 0
        for fr in summary["files"]:
            if fr["error"]:
                test_num += 1
                lines.append(
                    f"not ok {test_num} - {os.path.basename(fr['file'])} "
                    f"# error: {fr['error']}"
                )
                continue
            for r in fr["results"]:
                test_num += 1
                status = "ok" if r["passed"] else "not ok"
                suite = f"[{r['suite']}] " if r.get("suite") else ""
                desc = f"{suite}{r['name']}"
                line = f"{status} {test_num} - {desc}"
                if not r["passed"] and r.get("error"):
                    line += f" # {r['error']}"
                lines.append(line)
        return "\n".join(lines) + "\n"

    def format_json(self, summary: Dict) -> str:
        """JSON output — machine-readable for CI/CD integration."""
        return json.dumps(summary, indent=2, default=str)


# ---------------------------------------------------------------------------
# Convenience functions (module-level)
# ---------------------------------------------------------------------------


def run_directory(
    directory: str,
    pattern: str = "test_*.nxl",
    name_filter: Optional[str] = None,
    workers: int = 1,
    fmt: str = "verbose",
    quiet: bool = False,
) -> int:
    """Discover and run all test files, print results, return exit code."""
    runner = TestRunner(workers=workers, pattern=pattern,
                        name_filter=name_filter, quiet=quiet)
    summary = runner.run_directory(directory)
    if fmt == "tap":
        print(runner.format_tap(summary))
    elif fmt == "json":
        print(runner.format_json(summary))
    else:
        print(runner.format_verbose(summary))
    return 0 if summary["failed"] == 0 and not summary["errored_files"] else 1


def run_files(
    paths: List[str],
    name_filter: Optional[str] = None,
    workers: int = 1,
    fmt: str = "verbose",
) -> int:
    """Run a specific list of .nlpl files, print results, return exit code."""
    runner = TestRunner(workers=workers, name_filter=name_filter)
    summary = runner.run_files(paths)
    if fmt == "tap":
        print(runner.format_tap(summary))
    elif fmt == "json":
        print(runner.format_json(summary))
    else:
        print(runner.format_verbose(summary))
    return 0 if summary["failed"] == 0 and not summary["errored_files"] else 1


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------


def _cli():
    import argparse

    ap = argparse.ArgumentParser(
        prog="nlpl-test",
        description="NLPL test runner — discover and run .nlpl test programs",
    )
    ap.add_argument("paths", nargs="+",
                    help="Directories to discover tests in, or individual .nlpl files")
    ap.add_argument("--pattern", default="test_*.nxl",
                    help="Glob pattern for test file discovery (default: test_*.nxl)")
    ap.add_argument("--filter", dest="name_filter", default=None,
                    help="Glob pattern to filter individual test/describe names")
    ap.add_argument("--file-filter", dest="file_filter", default=None,
                    help="Regex applied to filenames (further restricts discovery)")
    ap.add_argument("--workers", "-j", type=int, default=1,
                    help="Parallel worker count (default: 1)")
    ap.add_argument("--format", dest="fmt", choices=["verbose", "tap", "json"],
                    default="verbose", help="Output format (default: verbose)")
    ap.add_argument("--quiet", "-q", action="store_true",
                    help="Suppress per-test output lines")
    ap.add_argument("--coverage", action="store_true",
                    help="Collect and report line coverage for executed .nlpl files")
    ap.add_argument("--coverage-dir", dest="coverage_dir", default="coverage",
                    help="Output directory for coverage report (default: coverage/)")

    args = ap.parse_args()

    runner = TestRunner(
        workers=args.workers,
        pattern=args.pattern,
        name_filter=args.name_filter,
        quiet=args.quiet,
        coverage_enabled=args.coverage,
        coverage_output_dir=args.coverage_dir,
    )

    # Separate directories from explicit file paths
    all_files: List[str] = []
    for p in args.paths:
        if os.path.isdir(p):
            found = runner.discover(p)
            if args.file_filter:
                found = runner.filter_files(found, args.file_filter)
            all_files.extend(found)
        elif os.path.isfile(p):
            all_files.append(p)
        else:
            print(f"Warning: {p!r} is not a file or directory — skipping",
                  file=sys.stderr)

    if not all_files:
        print("No test files found.", file=sys.stderr)
        sys.exit(1)

    summary = runner.run_files(all_files)

    if args.fmt == "tap":
        print(runner.format_tap(summary))
    elif args.fmt == "json":
        print(runner.format_json(summary))
    else:
        print(runner.format_verbose(summary))

    if args.coverage:
        runner.write_coverage(summary)

    sys.exit(0 if summary["failed"] == 0 and not summary["errored_files"] else 1)


if __name__ == "__main__":
    _cli()
