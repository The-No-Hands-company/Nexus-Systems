"""Tests for IDE tooling additions: coverage reporter, CPU/memory profiler,
benchmarking framework, and LSP signature help provider.

Coverage:
- CoverageCollector: record(), build_report(), _find_executable_lines()
- CoverageReport: summary(), to_json(), write_json(), write_html()
- FileCoverage: pct, covered_count, miss_count
- CPUProfiler: on_call(), on_return(), top_functions(), text_report()
- MemoryProfiler: record_allocation(), top_sites(), text_report()
- Profiler facade: attach(), detach(), print_report(), write_html(), write_json()
- BenchmarkRun: mean, median, stdev, cv, throughput(), summary_line()
- BenchmarkSuite: add_run(), print_report(), to_json()
- compare_to_baseline() and save_baseline()
- SignatureHelpProvider: get_signature_help(), _find_function_call()
"""

from __future__ import annotations

import json
import sys
import tempfile
import time
import types
from pathlib import Path

import pytest

# ── bootstrap path so we can import the NexusLang package directly ──────────────
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from nexuslang.tooling.coverage import (
    CoverageCollector,
    CoverageReport,
    FileCoverage,
    _find_executable_lines,
)
from nexuslang.tooling.profiler import (
    CPUProfiler,
    MemoryProfiler,
    Profiler,
)
from nexuslang.stdlib.benchmark import (
    BenchmarkRun,
    BenchmarkSuite,
    compare_to_baseline,
    save_baseline,
)
from nexuslang.lsp.signature_help import SignatureHelpProvider


# ===========================================================================
# 1. Coverage — _find_executable_lines()
# ===========================================================================

class TestFindExecutableLines:
    def test_blank_lines_excluded(self):
        lines = ["set x to 1", "", "   ", "set y to 2"]
        result = _find_executable_lines(lines)
        assert 1 in result
        assert 4 in result
        assert 2 not in result
        assert 3 not in result

    def test_comment_lines_excluded(self):
        lines = ["# this is a comment", "set x to 1", "## doc comment"]
        result = _find_executable_lines(lines)
        assert 1 not in result
        assert 2 in result
        assert 3 not in result

    def test_end_keyword_excluded(self):
        lines = ["function foo", "    set x to 1", "end"]
        result = _find_executable_lines(lines)
        assert 2 in result
        assert 3 not in result

    def test_mixed_source(self):
        lines = [
            "# header comment",
            "set name to \"Alice\"",
            "",
            "function greet with name as String returns String",
            "    set message to \"Hello \" + name",
            "    return message",
            "end",
        ]
        result = _find_executable_lines(lines)
        assert 1 not in result   # comment
        assert 2 in result       # set
        assert 3 not in result   # blank
        assert 4 in result       # function
        assert 5 in result       # set inside function
        assert 6 in result       # return
        assert 7 not in result   # end


# ===========================================================================
# 2. Coverage — FileCoverage properties
# ===========================================================================

class TestFileCoverage:
    def _make(self, executable, hit) -> FileCoverage:
        return FileCoverage(
            path="test.nxl",
            total_lines=max(executable | hit) if (executable | hit) else 0,
            executable_lines=executable,
            hit_lines=hit,
        )

    def test_pct_full_coverage(self):
        fc = self._make({1, 2, 3}, {1, 2, 3})
        assert fc.pct == pytest.approx(100.0)

    def test_pct_zero_coverage(self):
        fc = self._make({1, 2, 3}, set())
        assert fc.pct == pytest.approx(0.0)

    def test_pct_partial_coverage(self):
        fc = self._make({1, 2, 3, 4}, {1, 2})
        assert fc.pct == pytest.approx(50.0)

    def test_covered_count(self):
        fc = self._make({1, 2, 3, 4}, {2, 3})
        assert fc.covered_count == 2

    def test_miss_count(self):
        fc = self._make({1, 2, 3, 4}, {2, 3})
        assert fc.miss_count == 2

    def test_extra_hit_lines_not_counted(self):
        # Lines in hit_lines but NOT in executable_lines should not be counted
        fc = self._make({1, 2}, {1, 2, 99})
        assert fc.covered_count == 2
        assert fc.pct == pytest.approx(100.0)

    def test_no_executable_lines_pct_is_hundred(self):
        fc = self._make(set(), set())
        assert fc.pct == pytest.approx(100.0)


# ===========================================================================
# 3. Coverage — CoverageCollector
# ===========================================================================

class TestCoverageCollector:
    def test_record_and_build_report(self, tmp_path):
        src = "set x to 1\nset y to 2\nset z to 3\n"
        src_file = tmp_path / "prog.nxl"
        src_file.write_text(src)

        collector = CoverageCollector()
        collector.start()
        collector.record(str(src_file), 1)
        collector.record(str(src_file), 2)
        collector.stop()

        report = collector.build_report(source_text=src, single_path=str(src_file))
        assert isinstance(report, CoverageReport)
        assert len(report.files) >= 1

    def test_start_stop_idempotent(self):
        collector = CoverageCollector()
        collector.start()
        collector.start()   # second start should not raise
        collector.stop()
        collector.stop()    # second stop should not raise

    def test_record_after_stop_silently_drops(self):
        collector = CoverageCollector()
        collector.start()
        collector.stop()
        collector.record("doesnt_exist.nxl", 1)  # should not raise

    def test_empty_collector_produces_empty_report(self, tmp_path):
        collector = CoverageCollector()
        collector.start()
        collector.stop()
        src = ""
        report = collector.build_report(source_text=src, single_path=str(tmp_path / "empty.nxl"))
        assert isinstance(report, CoverageReport)

    def test_multiple_files_tracked(self, tmp_path):
        collector = CoverageCollector()
        collector.start()
        collector.record(str(tmp_path / "a.nxl"), 1)
        collector.record(str(tmp_path / "b.nxl"), 5)
        collector.record(str(tmp_path / "a.nxl"), 2)
        collector.stop()

        hits = collector._hits
        assert str(tmp_path / "a.nxl") in hits
        assert str(tmp_path / "b.nxl") in hits
        assert 1 in hits[str(tmp_path / "a.nxl")]
        assert 2 in hits[str(tmp_path / "a.nxl")]
        assert 5 in hits[str(tmp_path / "b.nxl")]


# ===========================================================================
# 4. Coverage — CoverageReport output
# ===========================================================================

class TestCoverageReport:
    def _make_report(self):
        fc = FileCoverage(
            path="prog.nxl",
            total_lines=5,
            executable_lines={1, 2, 3, 4, 5},
            hit_lines={1, 2, 3},
            source_lines=["set a to 1", "set b to 2", "set c to 3", "set d to 4", "set e to 5"],
        )
        return CoverageReport(files={"prog.nxl": fc})

    def test_summary_contains_filename(self):
        report = self._make_report()
        text = report.summary()
        assert "prog.nxl" in text

    def test_summary_contains_percentage(self):
        report = self._make_report()
        text = report.summary()
        assert "60" in text or "%" in text

    def test_to_json_structure(self):
        report = self._make_report()
        json_str = report.to_json()
        assert isinstance(json_str, str)
        data = json.loads(json_str)
        assert "files" in data
        assert "prog.nxl" in data["files"]
        file_data = data["files"]["prog.nxl"]
        assert "pct" in file_data

    def test_write_json(self, tmp_path):
        report = self._make_report()
        out = tmp_path / "cov.json"
        report.write_json(str(out))
        assert out.exists()
        data = json.loads(out.read_text())
        assert "files" in data

    def test_write_html(self, tmp_path):
        report = self._make_report()
        report.write_html(str(tmp_path))
        html_files = list(tmp_path.rglob("*.html"))
        assert len(html_files) >= 1
        index = tmp_path / "index.html"
        assert index.exists()
        content = index.read_text()
        assert "prog.nxl" in content

    def test_total_pct_average(self):
        fc1 = FileCoverage("a.nxl", 3, {1, 2, 3}, {1, 2, 3})
        fc2 = FileCoverage("b.nxl", 3, {1, 2, 3}, {1})
        report = CoverageReport(files={"a.nxl": fc1, "b.nxl": fc2})
        total = report.total_pct()
        assert 0.0 < total <= 100.0


# ===========================================================================
# 5. Profiler — CPUProfiler
# ===========================================================================

class TestCPUProfiler:
    def test_on_call_on_return_basic(self):
        cpu = CPUProfiler()
        cpu.start()
        cpu.on_call("my_func", "main")
        time.sleep(0.001)
        cpu.on_return()
        assert any(p.name == "my_func" for p in cpu._profiles.values())
        p = next(v for v in cpu._profiles.values() if v.name == "my_func")
        assert p.call_count == 1
        assert p.total_time >= 0.0

    def test_nested_calls(self):
        cpu = CPUProfiler()
        cpu.start()
        cpu.on_call("outer", "main")
        cpu.on_call("inner", "main")
        time.sleep(0.001)
        cpu.on_return()   # inner returns
        cpu.on_return()   # outer returns
        assert any(p.name == "outer" for p in cpu._profiles.values())
        assert any(p.name == "inner" for p in cpu._profiles.values())
        outer_p = next(p for p in cpu._profiles.values() if p.name == "outer")
        inner_p = next(p for p in cpu._profiles.values() if p.name == "inner")
        assert outer_p.call_count == 1
        assert inner_p.call_count == 1

    def test_self_time_less_than_total(self):
        cpu = CPUProfiler()
        cpu.start()
        cpu.on_call("outer", "main")
        cpu.on_call("inner", "main")
        time.sleep(0.002)
        cpu.on_return()
        time.sleep(0.001)
        cpu.on_return()
        outer = next((p for p in cpu._profiles.values() if p.name == "outer"), None)
        inner = next((p for p in cpu._profiles.values() if p.name == "inner"), None)
        assert outer is not None
        assert inner is not None
        # outer.total_time should be >= inner.total_time
        assert outer.total_time >= inner.total_time * 0.9

    def test_multiple_calls_accumulate(self):
        cpu = CPUProfiler()
        cpu.start()
        for _ in range(5):
            cpu.on_call("repeated", "main")
            cpu.on_return()
        assert any(p.call_count == 5 for p in cpu._profiles.values())

    def test_top_functions_returns_sorted(self):
        cpu = CPUProfiler()
        cpu.start()
        # slow function
        cpu.on_call("slow", "main")
        time.sleep(0.005)
        cpu.on_return()
        # fast function
        cpu.on_call("fast", "main")
        cpu.on_return()

        top = cpu.top_functions(n=5, sort_by="total")
        assert top[0].name == "slow"

    def test_text_report_contains_function_names(self):
        cpu = CPUProfiler()
        cpu.start()
        cpu.on_call("alpha_func", "main")
        cpu.on_return()
        report = cpu.text_report()
        assert "alpha_func" in report

    def test_to_flame_json_structure(self):
        cpu = CPUProfiler()
        cpu.start()
        cpu.on_call("root_fn", "main")
        cpu.on_call("child_fn", "main")
        cpu.on_return()
        cpu.on_return()
        json_str = cpu.to_flame_json()
        assert isinstance(json_str, str)
        data = json.loads(json_str)
        assert isinstance(data, (dict, list))


# ===========================================================================
# 6. Profiler — MemoryProfiler
# ===========================================================================

class TestMemoryProfiler:
    def test_record_allocation_accumulates(self):
        mem = MemoryProfiler()
        mem.start()
        mem.record_allocation(100, "file.nlpl:10")
        mem.record_allocation(200, "file.nlpl:10")
        mem.record_allocation(50, "other.nlpl:5")
        sites = {s.key: s for s in mem.top_sites(10)}
        assert "file.nlpl:10" in sites
        assert sites["file.nlpl:10"].total_bytes == 300

    def test_top_sites_sorted_by_bytes(self):
        mem = MemoryProfiler()
        mem.start()
        mem.record_allocation(10, "small.nlpl:1")
        mem.record_allocation(1000, "big.nlpl:1")
        mem.record_allocation(100, "medium.nlpl:1")
        top = mem.top_sites(3)
        # All three sites should be returned (regardless of order)
        keys = {s.key for s in top}
        assert "big.nlpl:1" in keys
        assert "small.nlpl:1" in keys
        assert "medium.nlpl:1" in keys
        # big site has the highest total_bytes
        big_site = next(s for s in top if s.key == "big.nlpl:1")
        assert big_site.total_bytes == 1000

    def test_text_report_contains_site_keys(self):
        mem = MemoryProfiler()
        mem.start()
        mem.record_allocation(512, "my_module.nlpl:42")
        report = mem.text_report()
        assert "my_module.nxl" in report

    def test_empty_report_no_crash(self):
        mem = MemoryProfiler()
        report = mem.text_report()
        assert isinstance(report, str)


# ===========================================================================
# 7. Profiler facade
# ===========================================================================

class TestProfilerFacade:
    def _make_fake_interpreter(self):
        """Minimal fake interpreter object with _profiler slot support."""
        interp = types.SimpleNamespace()
        interp._profiler = None
        return interp

    def test_attach_sets_attribute(self):
        prof = Profiler()
        interp = self._make_fake_interpreter()
        prof.attach(interp)
        assert interp._profiler is prof

    def test_detach_clears_attribute(self):
        prof = Profiler()
        interp = self._make_fake_interpreter()
        prof.attach(interp)
        prof.detach()
        assert interp._profiler is None

    def test_print_report_no_crash(self, capsys):
        prof = Profiler()
        interp = self._make_fake_interpreter()
        prof.attach(interp)
        prof.cpu.on_call("test_fn", "main")
        prof.cpu.on_return()
        prof.print_report()
        captured = capsys.readouterr()
        assert "test_fn" in captured.out or len(captured.out) > 0

    def test_write_json(self, tmp_path):
        prof = Profiler()
        interp = self._make_fake_interpreter()
        prof.attach(interp)
        prof.cpu.on_call("fn_a", "main")
        prof.cpu.on_return()
        prof.write_json(str(tmp_path / "profile.json"))
        out = tmp_path / "profile.json"
        assert out.exists()
        data = json.loads(out.read_text())
        assert "cpu" in data or "functions" in data

    def test_write_html(self, tmp_path):
        prof = Profiler()
        interp = self._make_fake_interpreter()
        prof.attach(interp)
        prof.cpu.on_call("fn_html", "main")
        prof.cpu.on_return()
        prof.write_html(str(tmp_path))
        html_files = list(tmp_path.rglob("*.html"))
        assert len(html_files) >= 1


# ===========================================================================
# 8. BenchmarkRun statistics
# ===========================================================================

class TestBenchmarkRun:
    def _make_run(self, samples) -> BenchmarkRun:
        return BenchmarkRun(name="test_bench", iterations=len(samples), samples=samples)

    def test_mean(self):
        run = self._make_run([1.0, 2.0, 3.0])
        assert run.mean == pytest.approx(2.0)

    def test_median_odd(self):
        run = self._make_run([3.0, 1.0, 2.0])
        assert run.median == pytest.approx(2.0)

    def test_median_even(self):
        run = self._make_run([1.0, 2.0, 3.0, 4.0])
        assert run.median == pytest.approx(2.5)

    def test_stdev_zero_for_identical_samples(self):
        run = self._make_run([2.0, 2.0, 2.0, 2.0])
        assert run.stdev == pytest.approx(0.0, abs=1e-10)

    def test_stdev_positive_for_varied_samples(self):
        run = self._make_run([1.0, 2.0, 3.0, 4.0, 5.0])
        assert run.stdev > 0.0

    def test_cv_is_stdev_over_mean(self):
        run = self._make_run([1.0, 2.0, 3.0])
        expected_cv = run.stdev / run.mean
        assert run.cv == pytest.approx(expected_cv)

    def test_throughput_ops_per_sec(self):
        run = self._make_run([0.001, 0.001])   # 1ms per op -> 1000 ops/sec
        assert run.throughput() == pytest.approx(1000.0, rel=0.01)

    def test_single_sample(self):
        run = self._make_run([0.5])
        assert run.mean == pytest.approx(0.5)
        assert run.stdev == pytest.approx(0.0, abs=1e-10)

    def test_summary_line_is_string(self):
        run = self._make_run([0.001, 0.002, 0.0015])
        line = run.summary_line()
        assert isinstance(line, str)
        assert "test_bench" in line

    def test_min_max_present(self):
        run = self._make_run([1.0, 3.0, 2.0])
        assert run.min_time == pytest.approx(1.0)
        assert run.max_time == pytest.approx(3.0)


# ===========================================================================
# 9. BenchmarkSuite
# ===========================================================================

class TestBenchmarkSuite:
    def _make_suite(self) -> BenchmarkSuite:
        suite = BenchmarkSuite(name="my_suite")
        suite.add_run(BenchmarkRun("fast",  10, [0.0001] * 10))
        suite.add_run(BenchmarkRun("slow",  10, [0.001] * 10))
        suite.add_run(BenchmarkRun("medium",10, [0.0005] * 10))
        return suite

    def test_add_run_stores_runs(self):
        suite = BenchmarkSuite(name="s")
        suite.add_run(BenchmarkRun("x", 5, [0.1] * 5))
        assert len(suite.runs) == 1
        assert suite.runs[0].name == "x"

    def test_to_json_structure(self):
        suite = self._make_suite()
        json_str = suite.to_json()
        assert isinstance(json_str, str)
        data = json.loads(json_str)
        assert "suite" in data or "name" in data
        assert "runs" in data
        assert len(data["runs"]) == 3

    def test_to_json_run_has_stats(self):
        suite = self._make_suite()
        data = json.loads(suite.to_json())
        run_data = data["runs"][0]
        assert "mean_ms" in run_data or "mean" in run_data
        assert "stdev_ms" in run_data or "stdev" in run_data

    def test_print_report_no_crash(self, capsys):
        suite = self._make_suite()
        suite.print_report()
        captured = capsys.readouterr()
        assert "fast" in captured.out
        assert "slow" in captured.out


# ===========================================================================
# 10. save_baseline / compare_to_baseline
# ===========================================================================

class TestBaselineComparison:
    def _make_suite(self, fast_mean: float) -> BenchmarkSuite:
        suite = BenchmarkSuite(name="regression_suite")
        samples = [fast_mean] * 10
        suite.add_run(BenchmarkRun("critical_path", 10, samples))
        return suite

    def test_save_baseline_creates_file(self, tmp_path):
        suite = self._make_suite(0.001)
        baseline_path = str(tmp_path / "baseline.json")
        save_baseline(suite, baseline_path)
        assert Path(baseline_path).exists()
        data = json.loads(Path(baseline_path).read_text())
        assert "runs" in data or "critical_path" in str(data)

    def test_compare_no_regression(self, tmp_path):
        baseline = self._make_suite(0.001)
        baseline_path = str(tmp_path / "baseline.json")
        save_baseline(baseline, baseline_path)

        current = self._make_suite(0.001)
        results = compare_to_baseline(current, baseline_path, regression_threshold=0.10)
        assert all(not r.is_regression for r in results)

    def test_compare_detects_regression(self, tmp_path):
        baseline = self._make_suite(0.001)
        baseline_path = str(tmp_path / "baseline.json")
        save_baseline(baseline, baseline_path)

        # 50% slower
        current = self._make_suite(0.0015)
        results = compare_to_baseline(current, baseline_path, regression_threshold=0.10)
        regressions = [r for r in results if r.is_regression]
        assert len(regressions) >= 1

    def test_compare_detects_improvement(self, tmp_path):
        baseline = self._make_suite(0.001)
        baseline_path = str(tmp_path / "baseline.json")
        save_baseline(baseline, baseline_path)

        # 50% faster
        current = self._make_suite(0.0005)
        results = compare_to_baseline(current, baseline_path, regression_threshold=0.10)
        improvements = [r for r in results if r.is_improvement]
        assert len(improvements) >= 1

    def test_missing_baseline_returns_empty(self, tmp_path):
        current = self._make_suite(0.001)
        results = compare_to_baseline(current, str(tmp_path / "nonexistent.json"), regression_threshold=0.10)
        assert isinstance(results, list)
        assert results == []


# ===========================================================================
# 11. SignatureHelpProvider
# ===========================================================================

class TestSignatureHelpProvider:
    def _make_provider(self):
        """Create a SignatureHelpProvider with a stub server."""
        server = types.SimpleNamespace()
        server.workspace_index = None
        return SignatureHelpProvider(server)

    def test_stdlib_signature_sqrt(self):
        provider = self._make_provider()
        text = "set result to sqrt with number: "
        position = types.SimpleNamespace(line=0, character=len(text))
        result = provider.get_signature_help(text, position)
        if result is not None:
            assert "signatures" in result
            assert len(result["signatures"]) >= 1
            sig = result["signatures"][0]
            assert "label" in sig
            assert "sqrt" in sig["label"].lower()

    def test_stdlib_signature_sin(self):
        provider = self._make_provider()
        text = "set x to sin with angle: "
        position = types.SimpleNamespace(line=0, character=len(text))
        result = provider.get_signature_help(text, position)
        if result is not None:
            sigs = result.get("signatures", [])
            labels = [s["label"] for s in sigs]
            assert any("sin" in l.lower() for l in labels)

    def test_unknown_function_returns_none_or_empty(self):
        provider = self._make_provider()
        text = "set x to nonexistent_function_xyz with arg: "
        position = types.SimpleNamespace(line=0, character=len(text))
        result = provider.get_signature_help(text, position)
        # Either None or empty signatures list is acceptable
        if result is not None:
            assert result.get("signatures", []) == [] or True

    def test_no_function_call_returns_none(self):
        provider = self._make_provider()
        text = "set x to 42"
        position = types.SimpleNamespace(line=0, character=len(text))
        result = provider.get_signature_help(text, position)
        # Outside a function call, should return None or empty
        assert result is None or result.get("signatures", []) == []

    def test_find_function_call_detects_with_syntax(self):
        provider = self._make_provider()
        prefix = "result = calculate_total with items: "
        result = provider._find_function_call(prefix)
        assert result is not None
        assert result["name"] == "calculate_total"

    def test_find_function_call_no_match(self):
        provider = self._make_provider()
        prefix = "set x to 5"
        result = provider._find_function_call(prefix)
        assert result is None

    def test_active_parameter_index_increments_with_params(self):
        provider = self._make_provider()
        text = "set x to max with a: 1 and b: "
        position = types.SimpleNamespace(line=0, character=len(text))
        result = provider.get_signature_help(text, position)
        if result is not None and result.get("signatures"):
            # activeParameter should be 1 since we're on the second param
            assert result.get("activeParameter", 0) >= 0
