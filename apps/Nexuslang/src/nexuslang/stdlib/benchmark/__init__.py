"""
Benchmarking Framework for NexusLang
================================

Provides micro-benchmark infrastructure with statistical analysis,
baseline comparison, and regression detection.

NLPL programs use these via the stdlib registration::

    import benchmark

    benchmark with function: my_function and name: "sort 1000 items"
    benchmark_range with function: heavy_func and min: 100 and max: 10000 and step: 100

Python usage::

    from nexuslang.stdlib.benchmark import register_benchmark_functions
    register_benchmark_functions(runtime)
"""

from __future__ import annotations

import json
import math
import os
import statistics
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

from ...runtime.runtime import Runtime


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class BenchmarkRun:
    """Results of a single benchmark iteration batch."""
    name: str
    iterations: int
    samples: List[float]       # seconds per iteration
    warmup_count: int = 0

    @property
    def count(self) -> int:
        return len(self.samples)

    @property
    def mean(self) -> float:
        return statistics.mean(self.samples) if self.samples else 0.0

    @property
    def median(self) -> float:
        return statistics.median(self.samples) if self.samples else 0.0

    @property
    def stdev(self) -> float:
        return statistics.stdev(self.samples) if len(self.samples) >= 2 else 0.0

    @property
    def min_time(self) -> float:
        return min(self.samples) if self.samples else 0.0

    @property
    def max_time(self) -> float:
        return max(self.samples) if self.samples else 0.0

    @property
    def cv(self) -> float:
        """Coefficient of variation (stdev / mean)."""
        return self.stdev / self.mean if self.mean else 0.0

    def throughput(self) -> float:
        """Operations per second (1 / mean)."""
        return 1.0 / self.mean if self.mean else 0.0

    def summary_line(self) -> str:
        mean_ms = self.mean * 1000
        stdev_ms = self.stdev * 1000
        return (
            f"  {self.name:<50s}"
            f"  {mean_ms:>9.3f} ms"
            f"  +/- {stdev_ms:>7.3f} ms"
            f"  ({self.throughput():>10.0f} op/s)"
            f"  [n={self.count}]"
        )


@dataclass
class BenchmarkSuite:
    """Collection of benchmark runs."""
    name: str
    runs: List[BenchmarkRun] = field(default_factory=list)

    def add_run(self, run: BenchmarkRun) -> None:
        self.runs.append(run)

    def print_report(self) -> None:
        print(f"\nBenchmark suite: {self.name}")
        print("=" * 80)
        for run in self.runs:
            print(run.summary_line())
        print()

    def to_json(self) -> str:
        data = {
            "suite": self.name,
            "runs": [
                {
                    "name": r.name,
                    "iterations": r.iterations,
                    "samples": r.count,
                    "mean_ms": round(r.mean * 1000, 6),
                    "median_ms": round(r.median * 1000, 6),
                    "stdev_ms": round(r.stdev * 1000, 6),
                    "min_ms": round(r.min_time * 1000, 6),
                    "max_ms": round(r.max_time * 1000, 6),
                    "cv": round(r.cv, 4),
                    "throughput_ops": round(r.throughput(), 2),
                }
                for r in self.runs
            ],
        }
        return json.dumps(data, indent=2)


# ---------------------------------------------------------------------------
# Compare against baseline
# ---------------------------------------------------------------------------

@dataclass
class RegressionResult:
    run: BenchmarkRun
    baseline_mean_ms: float
    ratio: float            # current / baseline  (>1 = slower = regression)
    is_regression: bool
    is_improvement: bool

    def summary(self) -> str:
        pct = (self.ratio - 1.0) * 100.0
        direction = "REGRESSION" if self.is_regression else (
            "improvement" if self.is_improvement else "no change"
        )
        return (
            f"  {self.run.name:<50s}"
            f"  {self.run.mean*1000:>9.3f} ms vs baseline {self.baseline_mean_ms:>9.3f} ms"
            f"  {pct:>+.1f}%  [{direction}]"
        )


def compare_to_baseline(
    suite: BenchmarkSuite,
    baseline_path: str,
    regression_threshold: float = 0.10,   # 10% slower = regression
    improvement_threshold: float = 0.05,  # 5% faster = improvement
) -> List[RegressionResult]:
    """
    Compare benchmark suite results against a saved baseline JSON file.

    Returns a list of RegressionResult for each run that has a baseline.
    """
    if not Path(baseline_path).exists():
        return []

    baseline_data = json.loads(Path(baseline_path).read_text(encoding="utf-8"))
    baseline_by_name: Dict[str, float] = {}
    for r in baseline_data.get("runs", []):
        baseline_by_name[r["name"]] = r["mean_ms"]

    results = []
    for run in suite.runs:
        if run.name not in baseline_by_name:
            continue
        base_ms = baseline_by_name[run.name]
        curr_ms = run.mean * 1000
        ratio = curr_ms / base_ms if base_ms > 0 else 1.0
        results.append(
            RegressionResult(
                run=run,
                baseline_mean_ms=base_ms,
                ratio=ratio,
                is_regression=ratio > (1.0 + regression_threshold),
                is_improvement=ratio < (1.0 - improvement_threshold),
            )
        )
    return results


def save_baseline(suite: BenchmarkSuite, baseline_path: str) -> None:
    """Save current suite results as the new baseline."""
    Path(baseline_path).parent.mkdir(parents=True, exist_ok=True)
    Path(baseline_path).write_text(suite.to_json(), encoding="utf-8")


# ---------------------------------------------------------------------------
# Low-level timing helper
# ---------------------------------------------------------------------------

def _time_callable(
    func: Callable,
    args: Tuple,
    iterations: int,
    warmup: int = 3,
) -> BenchmarkRun:
    """
    Run ``func(*args)`` for ``iterations`` times (after warmup) and
    collect per-iteration sample times.
    """
    # Warmup
    for _ in range(warmup):
        try:
            func(*args)
        except Exception:
            pass

    samples = []
    for _ in range(iterations):
        t0 = time.perf_counter()
        try:
            func(*args)
        except Exception:
            pass
        samples.append(time.perf_counter() - t0)

    return BenchmarkRun(
        name=getattr(func, "__name__", str(func)),
        iterations=iterations,
        samples=samples,
        warmup_count=warmup,
    )


# ---------------------------------------------------------------------------
# NexusLang stdlib registration
# ---------------------------------------------------------------------------

# Global suite registry
_suites: Dict[str, BenchmarkSuite] = {}
_default_suite: Optional[BenchmarkSuite] = None


def _get_or_create_suite(name: str = "default") -> BenchmarkSuite:
    global _default_suite
    if name not in _suites:
        _suites[name] = BenchmarkSuite(name)
    suite = _suites[name]
    _default_suite = suite
    return suite


def benchmark(
    func: Callable,
    name: str = "",
    iterations: int = 100,
    warmup: int = 3,
    suite: str = "default",
) -> Dict[str, Any]:
    """
    Benchmark ``func`` and display/return results.

    Args:
        func:       Function to benchmark (zero or one argument).
        name:       Display name for the benchmark.
        iterations: Number of measurement iterations.
        warmup:     Warmup iterations (not measured).
        suite:      Suite name to add result to.
    """
    bench_name = name or getattr(func, "__name__", "anonymous")
    run = _time_callable(func, (), iterations=iterations, warmup=warmup)
    run.name = bench_name
    s = _get_or_create_suite(suite)
    s.add_run(run)
    print(run.summary_line())
    return {
        "name": bench_name,
        "mean_ms": run.mean * 1000,
        "median_ms": run.median * 1000,
        "stdev_ms": run.stdev * 1000,
        "min_ms": run.min_time * 1000,
        "max_ms": run.max_time * 1000,
        "throughput_ops": run.throughput(),
        "samples": run.count,
    }


def benchmark_range(
    func: Callable,
    min_val: int = 1,
    max_val: int = 1000,
    step: int = 100,
    name: str = "",
    iterations: int = 50,
    suite: str = "default",
) -> List[Dict[str, Any]]:
    """
    Benchmark ``func(n)`` for n in range(min_val, max_val+1, step).

    Useful for complexity analysis.
    """
    results = []
    for n in range(min_val, max_val + 1, step):
        bench_name = f"{name or getattr(func, '__name__', 'f')}(n={n})"
        run = _time_callable(func, (n,), iterations=iterations, warmup=2)
        run.name = bench_name
        s = _get_or_create_suite(suite)
        s.add_run(run)
        print(run.summary_line())
        results.append({"n": n, "mean_ms": run.mean * 1000})
    return results


def create_benchmark_suite(name: str) -> str:
    _get_or_create_suite(name)
    return name


def run_benchmark_suite(name: str = "default") -> Dict[str, Any]:
    if name not in _suites:
        raise ValueError(f"No benchmark suite named '{name}'")
    suite = _suites[name]
    suite.print_report()
    return {"suite": name, "runs": len(suite.runs)}


def save_benchmark_baseline(path: str, suite: str = "default") -> None:
    if suite not in _suites:
        raise ValueError(f"No benchmark suite named '{suite}'")
    save_baseline(_suites[suite], path)
    print(f"Saved baseline to: {path}")


def check_benchmark_regression(
    path: str,
    suite: str = "default",
    threshold: float = 0.10,
) -> bool:
    """
    Compare current suite against baseline at ``path``.

    Returns True if no regressions found, False otherwise.
    """
    if suite not in _suites:
        raise ValueError(f"No benchmark suite named '{suite}'")
    results = compare_to_baseline(
        _suites[suite], path, regression_threshold=threshold
    )
    any_regression = False
    for r in results:
        print(r.summary())
        if r.is_regression:
            any_regression = True
    if not results:
        print("  (No matching baseline entries found)")
    return not any_regression


def benchmark_stats(suite: str = "default") -> Dict[str, Any]:
    """Return statistical summary for all runs in a suite."""
    if suite not in _suites:
        return {}
    s = _suites[suite]
    return {
        r.name: {
            "mean_ms": round(r.mean * 1000, 6),
            "median_ms": round(r.median * 1000, 6),
            "stdev_ms": round(r.stdev * 1000, 6),
            "cv": round(r.cv, 4),
            "min_ms": round(r.min_time * 1000, 6),
            "max_ms": round(r.max_time * 1000, 6),
            "throughput_ops": round(r.throughput(), 2),
        }
        for r in s.runs
    }


def time_function(func: Callable, iterations: int = 1000) -> float:
    """Simple timing — return mean execution time in milliseconds."""
    run = _time_callable(func, (), iterations=iterations, warmup=5)
    mean_ms = run.mean * 1000
    print(
        f"  {getattr(func, '__name__', 'function')}: "
        f"{mean_ms:.3f} ms avg over {iterations} iterations"
    )
    return mean_ms


def _register_benchmark_run_functions(runtime: Runtime) -> None:
    runtime.register_function("benchmark", benchmark)
    runtime.register_function("benchmark_range", benchmark_range)
    runtime.register_function("time_function", time_function)


def _register_benchmark_suite_functions(runtime: Runtime) -> None:
    runtime.register_function("create_benchmark_suite", create_benchmark_suite)
    runtime.register_function("run_benchmark_suite", run_benchmark_suite)
    runtime.register_function("save_benchmark_baseline", save_benchmark_baseline)
    runtime.register_function("check_benchmark_regression", check_benchmark_regression)
    runtime.register_function("benchmark_stats", benchmark_stats)


def _register_benchmark_aliases(runtime: Runtime) -> None:
    runtime.register_function("bench", benchmark)
    runtime.register_function("timeit", time_function)


def register_benchmark_functions(runtime: Runtime) -> None:
    """Register benchmarking functions with the NexusLang runtime."""
    _register_benchmark_run_functions(runtime)
    _register_benchmark_suite_functions(runtime)
    _register_benchmark_aliases(runtime)


__all__ = [
    "BenchmarkRun",
    "BenchmarkSuite",
    "RegressionResult",
    "compare_to_baseline",
    "save_baseline",
    "register_benchmark_functions",
]
