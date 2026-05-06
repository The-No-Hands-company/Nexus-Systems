"""
NLPL CI Benchmark Runner
=========================

Provides production-quality benchmark orchestration:

- Register named benchmark cases in domain categories
- Run all cases with statistical sampling
- Compare against a saved JSON baseline
- Detect performance regressions (configurable threshold)
- Save per-run history to a timestamped directory
- Emit markdown + JSON reports
- Return non-zero exit code on regression (for CI pipelines)

Quick start::

    from benchmarks.benchmark_ci import CIBenchmarkRunner
    from benchmarks.suite import ALL_CASES

    runner = CIBenchmarkRunner()
    runner.register(ALL_CASES)
    report = runner.run()
    regressions = runner.compare_baseline('benchmarks/baselines/latest.json')
    if regressions:
        runner.report_markdown(sys.stdout)
        sys.exit(1)
    runner.save_baseline('benchmarks/baselines/latest.json')
    runner.save_history('benchmarks/history/')
"""

from __future__ import annotations

import json
import os
import statistics
import subprocess
import sys
import time
import traceback
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class BenchmarkCase:
    """A single benchmark case definition."""
    name: str
    category: str
    func: Callable[[], Any]
    warmup_iters: int = 3
    bench_iters: int = 20
    # Optional: expected time ceiling in seconds (used in --strict mode)
    soft_ceiling_s: Optional[float] = None
    # Optional: describes what 1 iteration measures
    unit: str = "op"

    def __repr__(self) -> str:
        return f"BenchmarkCase({self.category}/{self.name})"


@dataclass
class SampleStats:
    """Statistical summary of timing samples."""
    name: str
    category: str
    samples: List[float]

    @property
    def n(self) -> int:
        return len(self.samples)

    @property
    def mean_s(self) -> float:
        return statistics.mean(self.samples) if self.samples else 0.0

    @property
    def median_s(self) -> float:
        return statistics.median(self.samples) if self.samples else 0.0

    @property
    def stdev_s(self) -> float:
        return statistics.stdev(self.samples) if len(self.samples) >= 2 else 0.0

    @property
    def min_s(self) -> float:
        return min(self.samples) if self.samples else 0.0

    @property
    def max_s(self) -> float:
        return max(self.samples) if self.samples else 0.0

    @property
    def cv(self) -> float:
        """Coefficient of variation."""
        return self.stdev_s / self.mean_s if self.mean_s else 0.0

    @property
    def ops_per_sec(self) -> float:
        return 1.0 / self.mean_s if self.mean_s else 0.0

    @property
    def mean_ms(self) -> float:
        return self.mean_s * 1000

    @property
    def median_ms(self) -> float:
        return self.median_s * 1000

    @property
    def stdev_ms(self) -> float:
        return self.stdev_s * 1000

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "category": self.category,
            "n": self.n,
            "mean_ms": round(self.mean_ms, 6),
            "median_ms": round(self.median_ms, 6),
            "stdev_ms": round(self.stdev_ms, 6),
            "min_ms": round(self.min_s * 1000, 6),
            "max_ms": round(self.max_s * 1000, 6),
            "cv": round(self.cv, 4),
            "ops_per_sec": round(self.ops_per_sec, 2),
        }

    def row(self, baseline_ms: Optional[float] = None) -> str:
        delta = ""
        if baseline_ms is not None and baseline_ms > 0:
            pct = (self.mean_ms / baseline_ms - 1.0) * 100
            sign = "+" if pct >= 0 else ""
            delta = f"  {sign}{pct:.1f}%"
        return (
            f"  {self.category}/{self.name:<48s}"
            f"  {self.mean_ms:>9.3f} ms"
            f"  +/- {self.stdev_ms:>7.3f} ms"
            f"  {self.ops_per_sec:>12.0f} op/s"
            f"{delta}"
        )


@dataclass
class RegressionEntry:
    """One benchmark that changed significantly vs baseline."""
    stats: SampleStats
    baseline_ms: float
    ratio: float             # current / baseline  (> 1 = slower)

    @property
    def is_regression(self) -> bool:
        return self.ratio > 1.0

    @property
    def is_improvement(self) -> bool:
        return self.ratio < 1.0

    @property
    def pct_change(self) -> float:
        return (self.ratio - 1.0) * 100.0

    def summary(self) -> str:
        direction = "REGRESSION" if self.is_regression else "improvement"
        sign = "+" if self.pct_change >= 0 else ""
        return (
            f"  [{direction.upper()}] {self.stats.category}/{self.stats.name}"
            f"  {self.stats.mean_ms:.3f} ms vs baseline {self.baseline_ms:.3f} ms"
            f"  ({sign}{self.pct_change:.1f}%)"
        )


@dataclass
class BenchmarkReport:
    """Full results from one CI runner invocation."""
    run_at: datetime
    git_commit: str
    stats: List[SampleStats] = field(default_factory=list)
    errors: Dict[str, str] = field(default_factory=dict)

    def by_category(self) -> Dict[str, List[SampleStats]]:
        out: Dict[str, List[SampleStats]] = {}
        for s in self.stats:
            out.setdefault(s.category, []).append(s)
        return out

    def to_dict(self) -> Dict[str, Any]:
        return {
            "run_at": self.run_at.isoformat(),
            "git_commit": self.git_commit,
            "runs": [s.to_dict() for s in self.stats],
            "errors": self.errors,
        }

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent)

    def _baseline_map(self, baseline_path: str) -> Dict[str, float]:
        p = Path(baseline_path)
        if not p.exists():
            return {}
        data = json.loads(p.read_text(encoding="utf-8"))
        return {r["name"]: r["mean_ms"] for r in data.get("runs", [])}

    def markdown(self, baseline_path: Optional[str] = None) -> str:
        bmap = self._baseline_map(baseline_path) if baseline_path else {}
        lines: List[str] = []
        lines.append(f"# NexusLang Benchmark Report")
        lines.append(f"")
        lines.append(f"Generated: {self.run_at.isoformat()}  ")
        lines.append(f"Commit: `{self.git_commit}`  ")
        lines.append(f"")
        if self.errors:
            lines.append("## Errors")
            for name, msg in self.errors.items():
                lines.append(f"- **{name}**: {msg}")
            lines.append("")
        for cat, items in sorted(self.by_category().items()):
            lines.append(f"## {cat}")
            lines.append("")
            lines.append(f"| Benchmark | Mean (ms) | Stdev (ms) | op/s | vs Baseline |")
            lines.append(f"|-----------|----------:|----------:|-----:|------------:|")
            for s in items:
                base = bmap.get(s.name)
                delta = ""
                if base is not None and base > 0:
                    pct = (s.mean_ms / base - 1.0) * 100
                    sign = "+" if pct >= 0 else ""
                    delta = f"{sign}{pct:.1f}%"
                lines.append(
                    f"| {s.name} | {s.mean_ms:.3f} | {s.stdev_ms:.3f}"
                    f" | {s.ops_per_sec:.0f} | {delta} |"
                )
            lines.append("")
        return "\n".join(lines)


@dataclass
class BenchmarkHistoryEntry:
    """One entry written to the history directory."""
    report: BenchmarkReport
    filename: str


# ---------------------------------------------------------------------------
# CI Runner
# ---------------------------------------------------------------------------

class CIBenchmarkRunner:
    """
    Orchestrates running benchmark cases, comparing to baselines,
    saving history, and emitting CI-friendly exit codes.
    """

    def __init__(
        self,
        regression_threshold: float = 0.10,   # 10% slower = regression
        improvement_threshold: float = 0.05,  # 5% faster = improvement
        verbose: bool = False,
    ) -> None:
        self.regression_threshold = regression_threshold
        self.improvement_threshold = improvement_threshold
        self.verbose = verbose
        self._cases: List[BenchmarkCase] = []
        self._report: Optional[BenchmarkReport] = None

    # ------------------------------------------------------------------
    # Registration
    # ------------------------------------------------------------------

    def register(self, cases: List[BenchmarkCase]) -> None:
        """Add benchmark cases to the run set."""
        self._cases.extend(cases)

    def register_one(self, case: BenchmarkCase) -> None:
        """Add a single benchmark case."""
        self._cases.append(case)

    def clear(self) -> None:
        self._cases.clear()
        self._report = None

    # ------------------------------------------------------------------
    # Running
    # ------------------------------------------------------------------

    def run(
        self,
        categories: Optional[List[str]] = None,
    ) -> BenchmarkReport:
        """
        Execute all registered benchmark cases.

        Args:
            categories: If given, only run cases in these categories.

        Returns:
            BenchmarkReport with all results.
        """
        run_at = datetime.now(tz=timezone.utc)
        git_commit = _get_git_commit()
        stats: List[SampleStats] = []
        errors: Dict[str, str] = {}

        cases_to_run = self._cases
        if categories:
            cases_to_run = [c for c in self._cases if c.category in categories]

        for case in cases_to_run:
            if self.verbose:
                print(f"  running {case.category}/{case.name} ...", flush=True)
            try:
                s = _run_case(case)
                stats.append(s)
                if self.verbose:
                    print(f"    {s.mean_ms:.3f} ms", flush=True)
            except Exception as exc:
                msg = traceback.format_exc()
                errors[f"{case.category}/{case.name}"] = str(exc)
                if self.verbose:
                    print(f"    ERROR: {exc}", flush=True)

        self._report = BenchmarkReport(
            run_at=run_at,
            git_commit=git_commit,
            stats=stats,
            errors=errors,
        )
        return self._report

    # ------------------------------------------------------------------
    # Regression detection
    # ------------------------------------------------------------------

    def compare_baseline(
        self,
        baseline_path: str,
    ) -> List[RegressionEntry]:
        """
        Compare the most recent run against a saved baseline JSON file.
        Returns entries for benchmarks that changed beyond the thresholds.
        """
        if self._report is None:
            raise RuntimeError("Call run() before compare_baseline()")
        p = Path(baseline_path)
        if not p.exists():
            return []
        data = json.loads(p.read_text(encoding="utf-8"))
        baseline_by_name: Dict[str, float] = {
            r["name"]: r["mean_ms"] for r in data.get("runs", [])
        }
        entries: List[RegressionEntry] = []
        for s in self._report.stats:
            if s.name not in baseline_by_name:
                continue
            base_ms = baseline_by_name[s.name]
            if base_ms <= 0:
                continue
            ratio = s.mean_ms / base_ms
            if ratio > (1.0 + self.regression_threshold) or ratio < (1.0 - self.improvement_threshold):
                entries.append(RegressionEntry(stats=s, baseline_ms=base_ms, ratio=ratio))
        return entries

    def regressions_only(
        self,
        baseline_path: str,
    ) -> List[RegressionEntry]:
        """Like compare_baseline, but returns only regressions (not improvements)."""
        return [e for e in self.compare_baseline(baseline_path) if e.is_regression]

    def check_pass(self, baseline_path: str) -> bool:
        """Return True if no regressions were found vs the baseline."""
        return len(self.regressions_only(baseline_path)) == 0

    # ------------------------------------------------------------------
    # Baseline management
    # ------------------------------------------------------------------

    def save_baseline(self, path: str) -> None:
        """Save current report as the new baseline JSON."""
        if self._report is None:
            raise RuntimeError("Call run() before save_baseline()")
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(self._report.to_json(), encoding="utf-8")

    def load_baseline(self, path: str) -> Dict[str, float]:
        """Load a baseline JSON and return {name: mean_ms} mapping."""
        p = Path(path)
        if not p.exists():
            return {}
        data = json.loads(p.read_text(encoding="utf-8"))
        return {r["name"]: r["mean_ms"] for r in data.get("runs", [])}

    # ------------------------------------------------------------------
    # History
    # ------------------------------------------------------------------

    def save_history(self, directory: str) -> str:
        """
        Save the current report as a timestamped file in ``directory``.
        Returns the path written.
        """
        if self._report is None:
            raise RuntimeError("Call run() before save_history()")
        d = Path(directory)
        d.mkdir(parents=True, exist_ok=True)
        ts = self._report.run_at.strftime("%Y%m%d_%H%M%S")
        commit = self._report.git_commit[:8]
        fname = f"bench_{ts}_{commit}.json"
        fpath = d / fname
        fpath.write_text(self._report.to_json(), encoding="utf-8")
        return str(fpath)

    def load_history(self, directory: str) -> List[BenchmarkReport]:
        """
        Load all JSON files in ``directory`` as BenchmarkReport objects,
        sorted oldest-first by run_at.
        """
        d = Path(directory)
        reports: List[BenchmarkReport] = []
        if not d.is_dir():
            return reports
        for fpath in sorted(d.glob("bench_*.json")):
            try:
                data = json.loads(fpath.read_text(encoding="utf-8"))
                run_at = datetime.fromisoformat(data["run_at"])
                stats = [
                    SampleStats(
                        name=r["name"],
                        category=r.get("category", "unknown"),
                        samples=[r["mean_ms"] / 1000.0],  # reconstruct scalar
                    )
                    for r in data.get("runs", [])
                ]
                reports.append(BenchmarkReport(
                    run_at=run_at,
                    git_commit=data.get("git_commit", ""),
                    stats=stats,
                    errors=data.get("errors", {}),
                ))
            except Exception:
                pass
        return reports

    def history_trend(
        self,
        directory: str,
        benchmark_name: str,
    ) -> List[Tuple[datetime, float]]:
        """
        Return (timestamp, mean_ms) pairs for a benchmark across history.
        Useful for trend visualization.
        """
        result: List[Tuple[datetime, float]] = []
        for report in self.load_history(directory):
            for s in report.stats:
                if s.name == benchmark_name:
                    result.append((report.run_at, s.mean_ms))
        return result

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    def report_markdown(
        self,
        file=None,
        baseline_path: Optional[str] = None,
    ) -> str:
        """
        Write markdown table report.

        Args:
            file: File-like object to write to, or None to return as string.
            baseline_path: If given, includes delta columns vs the baseline.

        Returns:
            The markdown string.
        """
        if self._report is None:
            raise RuntimeError("Call run() before report_markdown()")
        md = self._report.markdown(baseline_path)
        if file is not None:
            file.write(md)
        return md

    def report_json(self) -> str:
        """Return JSON string of current report."""
        if self._report is None:
            raise RuntimeError("Call run() before report_json()")
        return self._report.to_json()

    def print_summary(self, baseline_path: Optional[str] = None) -> None:
        """Print human-readable summary to stdout."""
        if self._report is None:
            raise RuntimeError("Call run() before print_summary()")
        bmap: Dict[str, float] = {}
        if baseline_path:
            bmap = self.load_baseline(baseline_path)
        print(f"\nBenchmark Results  [{self._report.run_at.strftime('%Y-%m-%d %H:%M:%S UTC')}]")
        print("=" * 90)
        for cat, items in sorted(self._report.by_category().items()):
            print(f"\n  [{cat}]")
            for s in items:
                base = bmap.get(s.name)
                print(s.row(base))
        if self._report.errors:
            print(f"\n  Errors ({len(self._report.errors)}):")
            for name, msg in self._report.errors.items():
                print(f"    {name}: {msg}")
        print()

    # ------------------------------------------------------------------
    # Convenience: run from command line
    # ------------------------------------------------------------------

    def main(
        self,
        baseline_path: str = "benchmarks/baselines/latest.json",
        history_dir: str = "benchmarks/history",
        save: bool = True,
        fail_on_regression: bool = True,
    ) -> int:
        """
        Full CI pipeline. Returns exit code (0 = pass, 1 = regression).
        """
        print("Running benchmarks ...", flush=True)
        self.run()
        self.print_summary(baseline_path)

        regressions = self.regressions_only(baseline_path)
        if regressions:
            print(f"REGRESSIONS DETECTED ({len(regressions)}):")
            for r in regressions:
                print(r.summary())
        else:
            print("No regressions detected.")

        if save:
            try:
                path = self.save_history(history_dir)
                print(f"History saved: {path}")
                self.save_baseline(baseline_path)
                print(f"Baseline updated: {baseline_path}")
            except Exception as exc:
                print(f"Warning: could not save history/baseline: {exc}")

        return 1 if (regressions and fail_on_regression) else 0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _run_case(case: BenchmarkCase) -> SampleStats:
    """Run one BenchmarkCase and return statistics."""
    # Warmup
    for _ in range(case.warmup_iters):
        case.func()

    # Timed iterations
    samples: List[float] = []
    for _ in range(case.bench_iters):
        t0 = time.perf_counter()
        case.func()
        samples.append(time.perf_counter() - t0)

    return SampleStats(name=case.name, category=case.category, samples=samples)


def _get_git_commit() -> str:
    """Return short git commit hash, or 'unknown'."""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception:
        pass
    return "unknown"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    from benchmarks.suite import ALL_CASES  # type: ignore[import]

    runner = CIBenchmarkRunner(verbose=True)
    runner.register(ALL_CASES)
    sys.exit(runner.main())
