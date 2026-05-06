"""
Tests for the CI benchmark runner infrastructure.

Covers:
- BenchmarkCase dataclass fields and repr
- SampleStats statistical properties (mean, stdev, cv, ops_per_sec, to_dict, row)
- RegressionEntry flags and pct_change
- BenchmarkReport serialization and markdown
- CIBenchmarkRunner: register, run, baseline, regression detection, history
- Domain suite case counts
- ALL_CASES aggregation
"""
from __future__ import annotations

import json
import os
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import List

import pytest

# Ensure project root is on sys.path so `benchmarks` package is importable.
_PROJECT_ROOT = Path(__file__).parent.parent.parent.resolve()
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from benchmarks.benchmark_ci import (
    BenchmarkCase,
    BenchmarkReport,
    CIBenchmarkRunner,
    RegressionEntry,
    SampleStats,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _fast_case(name: str = "fast", category: str = "test") -> BenchmarkCase:
    """A benchmark case that executes nearly instantly."""
    def noop() -> None:
        pass

    return BenchmarkCase(name=name, category=category, func=noop, warmup_iters=1, bench_iters=5)


def _slow_case(name: str = "slow", category: str = "test") -> BenchmarkCase:
    """A benchmark case that sleeps briefly."""
    def sleep_5ms() -> None:
        time.sleep(0.005)

    return BenchmarkCase(
        name=name, category=category, func=sleep_5ms, warmup_iters=1, bench_iters=3
    )


def _make_runner(**kwargs) -> CIBenchmarkRunner:
    return CIBenchmarkRunner(verbose=False, **kwargs)


# ---------------------------------------------------------------------------
# BenchmarkCase
# ---------------------------------------------------------------------------

class TestBenchmarkCase:
    def test_fields(self) -> None:
        def fn() -> None:
            pass

        case = BenchmarkCase(
            name="my_bench",
            category="test",
            func=fn,
            warmup_iters=2,
            bench_iters=10,
            soft_ceiling_s=0.5,
            unit="iter",
        )
        assert case.name == "my_bench"
        assert case.category == "test"
        assert case.func is fn
        assert case.warmup_iters == 2
        assert case.bench_iters == 10
        assert case.soft_ceiling_s == 0.5
        assert case.unit == "iter"

    def test_defaults(self) -> None:
        case = _fast_case()
        # Only warmup/bench overridden in helper; check the dataclass defaults individually
        assert case.soft_ceiling_s is None
        assert case.unit == "op"

    def test_repr(self) -> None:
        case = _fast_case("my_test", "mycat")
        r = repr(case)
        assert "mycat" in r
        assert "my_test" in r


# ---------------------------------------------------------------------------
# SampleStats
# ---------------------------------------------------------------------------

class TestSampleStats:
    def test_mean(self) -> None:
        s = SampleStats("x", "cat", [0.1, 0.2, 0.3])
        assert abs(s.mean_s - 0.2) < 1e-9
        assert abs(s.mean_ms - 200.0) < 1e-6

    def test_median(self) -> None:
        s = SampleStats("x", "cat", [0.1, 0.5, 0.3])
        assert abs(s.median_s - 0.3) < 1e-9

    def test_stdev_zero_for_single_sample(self) -> None:
        s = SampleStats("x", "cat", [0.1])
        assert s.stdev_s == 0.0

    def test_stdev_nonzero(self) -> None:
        s = SampleStats("x", "cat", [0.1, 0.3])
        assert s.stdev_s > 0.0

    def test_cv_zero_equal_samples(self) -> None:
        s = SampleStats("x", "cat", [0.1, 0.1, 0.1])
        assert s.cv == 0.0

    def test_cv_nonzero(self) -> None:
        s = SampleStats("x", "cat", [0.1, 0.2])
        assert s.cv > 0.0

    def test_ops_per_sec(self) -> None:
        s = SampleStats("x", "cat", [0.001])  # 1 ms mean
        assert abs(s.ops_per_sec - 1000.0) < 0.01

    def test_min_max(self) -> None:
        s = SampleStats("x", "cat", [0.3, 0.1, 0.5])
        assert s.min_s == 0.1
        assert s.max_s == 0.5

    def test_n(self) -> None:
        s = SampleStats("x", "cat", [0.1, 0.2, 0.3])
        assert s.n == 3

    def test_to_dict_keys(self) -> None:
        s = SampleStats("bench1", "algorithms", [0.001, 0.002, 0.001])
        d = s.to_dict()
        for key in ("name", "category", "n", "mean_ms", "median_ms", "stdev_ms",
                    "min_ms", "max_ms", "cv", "ops_per_sec"):
            assert key in d, f"Missing key: {key}"
        assert d["name"] == "bench1"
        assert d["category"] == "algorithms"

    def test_to_dict_values_in_ms(self) -> None:
        # 10 ms samples
        s = SampleStats("x", "cat", [0.01, 0.01, 0.01])
        d = s.to_dict()
        assert abs(d["mean_ms"] - 10.0) < 0.001

    def test_row_string(self) -> None:
        s = SampleStats("my_bench", "cat", [0.001])
        row = s.row()
        assert "my_bench" in row
        assert "ms" in row

    def test_row_with_baseline(self) -> None:
        s = SampleStats("x", "cat", [0.002])  # 2 ms
        row = s.row(baseline_ms=1.0)          # baseline 1 ms -> +100%
        assert "+" in row or "%" in row

    def test_empty_samples_safe(self) -> None:
        s = SampleStats("x", "cat", [])
        assert s.mean_s == 0.0
        assert s.stdev_s == 0.0
        assert s.cv == 0.0
        assert s.ops_per_sec == 0.0


# ---------------------------------------------------------------------------
# RegressionEntry
# ---------------------------------------------------------------------------

class TestRegressionEntry:
    def _make(self, mean_s: float, baseline_ms: float) -> RegressionEntry:
        stats = SampleStats("x", "cat", [mean_s])
        ratio = stats.mean_ms / baseline_ms if baseline_ms else 1.0
        return RegressionEntry(stats=stats, baseline_ms=baseline_ms, ratio=ratio)

    def test_is_regression_when_slower(self) -> None:
        e = self._make(0.002, 1.0)  # 2 ms vs 1 ms baseline
        assert e.is_regression is True
        assert e.is_improvement is False

    def test_is_improvement_when_faster(self) -> None:
        e = self._make(0.0005, 1.0)  # 0.5 ms vs 1 ms baseline
        assert e.is_improvement is True
        assert e.is_regression is False

    def test_pct_change_regression(self) -> None:
        e = self._make(0.002, 1.0)  # 2 ms vs 1 ms -> +100%
        assert abs(e.pct_change - 100.0) < 0.1

    def test_pct_change_improvement(self) -> None:
        e = self._make(0.0005, 1.0)  # 0.5 ms vs 1 ms -> -50%
        assert abs(e.pct_change - (-50.0)) < 0.1

    def test_summary_contains_name(self) -> None:
        stats = SampleStats("my_bench", "cat", [0.002])
        e = RegressionEntry(stats=stats, baseline_ms=1.0, ratio=2.0)
        summary = e.summary()
        assert "my_bench" in summary

    def test_summary_regression_label(self) -> None:
        stats = SampleStats("x", "cat", [0.002])
        e = RegressionEntry(stats=stats, baseline_ms=1.0, ratio=2.0)
        assert "REGRESSION" in e.summary().upper()

    def test_summary_improvement_label(self) -> None:
        stats = SampleStats("x", "cat", [0.0005])
        e = RegressionEntry(stats=stats, baseline_ms=1.0, ratio=0.5)
        summary = e.summary()
        assert "improvement" in summary.lower() or "IMPROVEMENT" in summary


# ---------------------------------------------------------------------------
# BenchmarkReport
# ---------------------------------------------------------------------------

class TestBenchmarkReport:
    def _make_report(self) -> BenchmarkReport:
        s1 = SampleStats("bench_a", "alpha", [0.001, 0.0012, 0.0011])
        s2 = SampleStats("bench_b", "alpha", [0.005, 0.0055, 0.0048])
        s3 = SampleStats("bench_c", "beta", [0.002, 0.0021, 0.0019])
        return BenchmarkReport(
            run_at=datetime(2026, 1, 15, 12, 0, 0, tzinfo=timezone.utc),
            git_commit="abcd123",
            stats=[s1, s2, s3],
            errors={},
        )

    def test_by_category(self) -> None:
        r = self._make_report()
        cats = r.by_category()
        assert "alpha" in cats
        assert "beta" in cats
        assert len(cats["alpha"]) == 2
        assert len(cats["beta"]) == 1

    def test_to_dict_structure(self) -> None:
        r = self._make_report()
        d = r.to_dict()
        assert "run_at" in d
        assert "git_commit" in d
        assert "runs" in d
        assert "errors" in d
        assert d["git_commit"] == "abcd123"
        assert len(d["runs"]) == 3

    def test_to_json_valid(self) -> None:
        r = self._make_report()
        j = r.to_json()
        parsed = json.loads(j)
        assert parsed["git_commit"] == "abcd123"

    def test_markdown_contains_category(self) -> None:
        r = self._make_report()
        md = r.markdown()
        assert "alpha" in md
        assert "beta" in md

    def test_markdown_contains_bench_names(self) -> None:
        r = self._make_report()
        md = r.markdown()
        assert "bench_a" in md
        assert "bench_c" in md

    def test_markdown_contains_commit(self) -> None:
        r = self._make_report()
        md = r.markdown()
        assert "abcd123" in md

    def test_markdown_with_baseline(self, tmp_path: Path) -> None:
        baseline = {
            "runs": [
                {"name": "bench_a", "mean_ms": 1.0},
                {"name": "bench_c", "mean_ms": 2.0},
            ]
        }
        bl_file = tmp_path / "baseline.json"
        bl_file.write_text(json.dumps(baseline), encoding="utf-8")
        r = self._make_report()
        md = r.markdown(str(bl_file))
        # Some delta markers should appear
        assert "%" in md

    def test_report_with_errors(self) -> None:
        r = BenchmarkReport(
            run_at=datetime.now(timezone.utc),
            git_commit="0000000",
            stats=[],
            errors={"bad_bench": "ZeroDivisionError"},
        )
        d = r.to_dict()
        assert d["errors"]["bad_bench"] == "ZeroDivisionError"
        md = r.markdown()
        assert "bad_bench" in md


# ---------------------------------------------------------------------------
# CIBenchmarkRunner integration
# ---------------------------------------------------------------------------

class TestCIBenchmarkRunner:
    def test_register_and_clear(self) -> None:
        runner = _make_runner()
        runner.register([_fast_case("a"), _fast_case("b")])
        assert len(runner._cases) == 2
        runner.clear()
        assert len(runner._cases) == 0

    def test_register_one(self) -> None:
        runner = _make_runner()
        runner.register_one(_fast_case("single"))
        assert len(runner._cases) == 1

    def test_run_returns_report(self) -> None:
        runner = _make_runner()
        runner.register([_fast_case("x"), _fast_case("y")])
        report = runner.run()
        assert isinstance(report, BenchmarkReport)
        assert len(report.stats) == 2

    def test_run_category_filter(self) -> None:
        runner = _make_runner()
        runner.register([
            _fast_case("a", "cat_a"),
            _fast_case("b", "cat_b"),
            _fast_case("c", "cat_a"),
        ])
        report = runner.run(categories=["cat_a"])
        assert len(report.stats) == 2
        for s in report.stats:
            assert s.category == "cat_a"

    def test_run_produces_nonzero_times(self) -> None:
        runner = _make_runner()
        runner.register([_slow_case("s")])
        report = runner.run()
        assert report.stats[0].mean_s > 0.001

    def test_run_error_recorded(self) -> None:
        def failing_fn() -> None:
            raise ValueError("oops")

        runner = _make_runner()
        runner.register_one(
            BenchmarkCase("bad", "test", failing_fn, warmup_iters=1, bench_iters=3)
        )
        report = runner.run()
        assert len(report.stats) == 0
        # Error key is "category/name" e.g. "test/bad"
        assert any("bad" in k for k in report.errors)

    def test_save_and_load_baseline(self, tmp_path: Path) -> None:
        runner = _make_runner()
        runner.register([_fast_case("b1"), _fast_case("b2")])
        runner.run()
        bl_path = str(tmp_path / "baseline.json")
        runner.save_baseline(bl_path)
        loaded = runner.load_baseline(bl_path)
        assert isinstance(loaded, dict)
        assert "b1" in loaded
        assert loaded["b1"] >= 0.0

    def test_load_missing_baseline_returns_empty(self, tmp_path: Path) -> None:
        runner = _make_runner()
        result = runner.load_baseline(str(tmp_path / "missing.json"))
        assert result == {}

    def test_compare_baseline_no_regression_fast(self, tmp_path: Path) -> None:
        """Same cases run twice — no meaningful regression."""
        runner = _make_runner(regression_threshold=0.5)
        runner.register([_fast_case("t")])
        runner.run()
        bl = str(tmp_path / "bl.json")
        runner.save_baseline(bl)
        # Run again and compare
        runner.clear()
        runner.register([_fast_case("t")])
        runner.run()
        entries = runner.compare_baseline(bl)
        # With 50% threshold, fast noop should not regress
        regressions = [e for e in entries if e.is_regression]
        assert len(regressions) == 0

    def test_compare_baseline_detects_regression(self, tmp_path: Path) -> None:
        """Craft a baseline with artificially low ms so current run is slower."""
        runner = _make_runner(regression_threshold=0.10)
        runner.register([_slow_case("slow_bench")])
        runner.run()
        # Write fake baseline claiming 0.001 ms (much faster)
        bl_data = {"runs": [{"name": "slow_bench", "mean_ms": 0.001}]}
        bl_path = tmp_path / "fake_baseline.json"
        bl_path.write_text(json.dumps(bl_data), encoding="utf-8")
        entries = runner.compare_baseline(str(bl_path))
        regressions = [e for e in entries if e.is_regression]
        assert len(regressions) >= 1

    def test_regressions_only(self, tmp_path: Path) -> None:
        runner = _make_runner(regression_threshold=0.10)
        runner.register([_slow_case("slow")])
        runner.run()
        bl_data = {"runs": [{"name": "slow", "mean_ms": 0.001}]}
        bl_path = tmp_path / "bl.json"
        bl_path.write_text(json.dumps(bl_data), encoding="utf-8")
        regressions = runner.regressions_only(str(bl_path))
        assert all(e.is_regression for e in regressions)

    def test_check_pass_true_with_no_regression(self, tmp_path: Path) -> None:
        runner = _make_runner(regression_threshold=0.50)
        runner.register([_fast_case("ok")])
        runner.run()
        bl = str(tmp_path / "bl.json")
        runner.save_baseline(bl)
        runner.clear()
        runner.register([_fast_case("ok")])
        runner.run()
        assert runner.check_pass(bl) is True

    def test_check_pass_false_with_regression(self, tmp_path: Path) -> None:
        runner = _make_runner(regression_threshold=0.10)
        runner.register([_slow_case("s")])
        runner.run()
        bl_data = {"runs": [{"name": "s", "mean_ms": 0.001}]}
        bl_path = tmp_path / "bl.json"
        bl_path.write_text(json.dumps(bl_data), encoding="utf-8")
        assert runner.check_pass(str(bl_path)) is False

    def test_save_and_load_history(self, tmp_path: Path) -> None:
        runner = _make_runner()
        runner.register([_fast_case("h")])
        runner.run()
        hist_dir = str(tmp_path / "history")
        runner.save_history(hist_dir)
        history = runner.load_history(hist_dir)
        assert len(history) >= 1
        assert isinstance(history[0], BenchmarkReport)

    def test_history_trend(self, tmp_path: Path) -> None:
        runner = _make_runner()
        runner.register([_fast_case("trend_bench")])
        hist_dir = str(tmp_path / "hist")
        # Run and save twice — sleep 1 s to ensure distinct filenames (timestamp-based naming)
        runner.run()
        runner.save_history(hist_dir)
        time.sleep(1.05)
        runner.clear()
        runner.register([_fast_case("trend_bench")])
        runner.run()
        runner.save_history(hist_dir)
        trend = runner.history_trend(hist_dir, "trend_bench")
        # Should have 2 entries, each a (datetime, float) pair
        assert len(trend) == 2
        for ts, val in trend:
            assert isinstance(ts, datetime)
            assert isinstance(val, float)

    def test_report_json_returns_string(self) -> None:
        runner = _make_runner()
        runner.register([_fast_case("j")])
        runner.run()
        j = runner.report_json()
        parsed = json.loads(j)
        assert "runs" in parsed

    def test_report_markdown_to_file(self, tmp_path: Path) -> None:
        runner = _make_runner()
        runner.register([_fast_case("m")])
        runner.run()
        out_file = tmp_path / "report.md"
        with open(str(out_file), "w", encoding="utf-8") as fh:
            runner.report_markdown(fh)
        content = out_file.read_text(encoding="utf-8")
        assert "Benchmark" in content or "benchmark" in content.lower()

    def test_print_summary_no_crash(self, capsys) -> None:
        runner = _make_runner()
        runner.register([_fast_case("ps")])
        runner.run()
        runner.print_summary()  # Should not raise

    def test_main_returns_zero_no_regression(self, tmp_path: Path) -> None:
        runner = _make_runner(regression_threshold=0.50)
        runner.register([_fast_case("main_ok")])
        runner.run()
        bl = str(tmp_path / "bl.json")
        runner.save_baseline(bl)
        runner.clear()
        runner.register([_fast_case("main_ok")])
        code = runner.main(
            baseline_path=bl,
            history_dir=str(tmp_path / "hist"),
            save=True,
            fail_on_regression=True,
        )
        assert code == 0

    def test_main_returns_nonzero_on_regression(self, tmp_path: Path) -> None:
        runner = _make_runner(regression_threshold=0.10)
        runner.register([_slow_case("main_slow")])
        runner.run()
        bl_data = {"runs": [{"name": "main_slow", "mean_ms": 0.001}]}
        bl_path = tmp_path / "bl.json"
        bl_path.write_text(json.dumps(bl_data), encoding="utf-8")
        runner.clear()
        runner.register([_slow_case("main_slow")])
        code = runner.main(
            baseline_path=str(bl_path),
            history_dir=None,
            save=False,
            fail_on_regression=True,
        )
        assert code != 0


# ---------------------------------------------------------------------------
# Domain suite case counts
# ---------------------------------------------------------------------------

class TestDomainSuites:
    def test_algorithm_cases_count(self) -> None:
        from benchmarks.suite.algorithms import ALGORITHM_CASES
        assert len(ALGORITHM_CASES) >= 15

    def test_numeric_cases_count(self) -> None:
        from benchmarks.suite.numerics import NUMERIC_CASES
        assert len(NUMERIC_CASES) >= 12

    def test_string_cases_count(self) -> None:
        from benchmarks.suite.strings import STRING_CASES
        assert len(STRING_CASES) >= 20

    def test_io_cases_count(self) -> None:
        from benchmarks.suite.io_ops import IO_CASES
        assert len(IO_CASES) >= 15

    def test_memory_cases_count(self) -> None:
        from benchmarks.suite.memory_ops import MEMORY_CASES
        assert len(MEMORY_CASES) >= 20

    def test_concurrency_cases_count(self) -> None:
        from benchmarks.suite.concurrency import CONCURRENCY_CASES
        assert len(CONCURRENCY_CASES) >= 12

    def test_all_cases_names_unique(self) -> None:
        from benchmarks.suite import ALL_CASES
        names = [c.name for c in ALL_CASES]
        assert len(names) == len(set(names)), "Duplicate benchmark case names detected"

    def test_all_cases_aggregation(self) -> None:
        from benchmarks.suite import (
            ALGORITHM_CASES,
            ALL_CASES,
            CONCURRENCY_CASES,
            IO_CASES,
            MEMORY_CASES,
            NUMERIC_CASES,
            STRING_CASES,
        )
        expected = (
            len(ALGORITHM_CASES)
            + len(NUMERIC_CASES)
            + len(STRING_CASES)
            + len(IO_CASES)
            + len(MEMORY_CASES)
            + len(CONCURRENCY_CASES)
        )
        assert len(ALL_CASES) == expected

    def test_all_cases_have_category_and_name(self) -> None:
        from benchmarks.suite import ALL_CASES
        for case in ALL_CASES:
            assert isinstance(case.name, str) and case.name
            assert isinstance(case.category, str) and case.category
            assert callable(case.func)

    def test_all_cases_positive_iters(self) -> None:
        from benchmarks.suite import ALL_CASES
        for case in ALL_CASES:
            assert case.warmup_iters >= 0
            assert case.bench_iters >= 1
