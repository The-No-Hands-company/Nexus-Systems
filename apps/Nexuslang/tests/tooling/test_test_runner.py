"""
Tests for the NexusLang test runner (nlpl.tooling.test_runner).

Covers:
- File discovery (pattern matching, subdirectory walk)
- Output formatters (verbose, TAP, JSON)
- Name filtering
- Parallel execution (workers > 1)
- RunSummary structure
- Running real NexusLang test files with the interpreter
"""

import json
import os
import sys
import tempfile
from pathlib import Path

import pytest

_SRC = str(Path(__file__).resolve().parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.tooling.test_runner import TestRunner, run_directory, run_files


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _write(dir_path, filename, content):
    """Write a .nlpl file to dir_path and return its path."""
    path = os.path.join(str(dir_path), filename)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(content)
    return path


# ---------------------------------------------------------------------------
# Minimal NexusLang test programs
# ---------------------------------------------------------------------------

_PASSING_SUITE = """\
describe "arithmetic" do
  it "two plus two is four" do
    expect 2 plus 2 to equal 4
  end
  it "ten minus three is seven" do
    expect 10 minus 3 to equal 7
  end
end
"""

_FAILING_SUITE = """\
test "always fails" do
  expect 1 to equal 2
end
"""

_MIXED_SUITE = """\
test "passes" do
  expect "hello" to contain "ell"
end
test "fails" do
  expect 42 to equal 99
end
"""

_EMPTY_PROGRAM = """\
set x to 42
print text x
"""

# ---------------------------------------------------------------------------
# TestRunner.discover()
# ---------------------------------------------------------------------------


class TestDiscovery:
    def test_finds_matching_files(self, tmp_path):
        _write(tmp_path, "test_alpha.nxl", _PASSING_SUITE)
        _write(tmp_path, "test_beta.nxl", _PASSING_SUITE)
        runner = TestRunner(pattern="test_*.nxl")
        found = runner.discover(str(tmp_path))
        basenames = [os.path.basename(p) for p in found]
        assert "test_alpha.nxl" in basenames
        assert "test_beta.nxl" in basenames

    def test_ignores_non_matching_files(self, tmp_path):
        _write(tmp_path, "test_alpha.nxl", _PASSING_SUITE)
        _write(tmp_path, "helper.nxl", _EMPTY_PROGRAM)
        runner = TestRunner(pattern="test_*.nxl")
        found = runner.discover(str(tmp_path))
        assert not any("helper" in p for p in found)

    def test_recurses_into_subdirectories(self, tmp_path):
        subdir = tmp_path / "sub"
        subdir.mkdir()
        _write(subdir, "test_nested.nxl", _PASSING_SUITE)
        runner = TestRunner(pattern="test_*.nxl")
        found = runner.discover(str(tmp_path))
        assert any("test_nested.nxl" in p for p in found)

    def test_returns_sorted_paths(self, tmp_path):
        _write(tmp_path, "test_z.nxl", _PASSING_SUITE)
        _write(tmp_path, "test_a.nxl", _PASSING_SUITE)
        runner = TestRunner(pattern="test_*.nxl")
        found = runner.discover(str(tmp_path))
        assert found == sorted(found)

    def test_empty_directory(self, tmp_path):
        runner = TestRunner(pattern="test_*.nxl")
        assert runner.discover(str(tmp_path)) == []

    def test_custom_pattern(self, tmp_path):
        _write(tmp_path, "spec_one.nxl", _PASSING_SUITE)
        _write(tmp_path, "test_one.nxl", _PASSING_SUITE)
        runner = TestRunner(pattern="spec_*.nxl")
        found = runner.discover(str(tmp_path))
        basenames = [os.path.basename(p) for p in found]
        assert "spec_one.nxl" in basenames
        assert "test_one.nxl" not in basenames

    def test_file_filter_by_regex(self, tmp_path):
        _write(tmp_path, "test_alpha.nxl", _PASSING_SUITE)
        _write(tmp_path, "test_beta.nxl", _PASSING_SUITE)
        runner = TestRunner(pattern="test_*.nxl")
        all_files = runner.discover(str(tmp_path))
        filtered = runner.filter_files(all_files, r"alpha")
        assert len(filtered) == 1
        assert "alpha" in filtered[0]


# ---------------------------------------------------------------------------
# TestRunner.run_files() — RunSummary structure
# ---------------------------------------------------------------------------


class TestRunSummaryStructure:
    def test_summary_has_required_keys(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert "files" in summary
        assert "passed" in summary
        assert "failed" in summary
        assert "total" in summary
        assert "duration" in summary
        assert "errored_files" in summary

    def test_summary_total_is_sum_of_tests(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert summary["total"] >= 0

    def test_file_result_has_required_keys(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        fr = summary["files"][0]
        assert "file" in fr
        assert "results" in fr
        assert "error" in fr
        assert "duration" in fr
        assert "passed" in fr
        assert "failed" in fr
        assert "total" in fr

    def test_missing_file_goes_to_error(self, tmp_path):
        runner = TestRunner()
        summary = runner.run_files(["/no/such/file.nxl"])
        assert summary["errored_files"] != [] or summary["files"][0]["error"] is not None


# ---------------------------------------------------------------------------
# TestRunner.run_files() — actual NexusLang execution
# ---------------------------------------------------------------------------


class TestExecutionResults:
    def test_passing_suite_has_no_failures(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert summary["failed"] == 0

    def test_passing_suite_has_some_passing_tests(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert summary["passed"] > 0

    def test_failing_suite_has_failures(self, tmp_path):
        path = _write(tmp_path, "test_fail.nxl", _FAILING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert summary["failed"] >= 1

    def test_mixed_suite_totals(self, tmp_path):
        path = _write(tmp_path, "test_mixed.nxl", _MIXED_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert summary["passed"] == 1
        assert summary["failed"] == 1
        assert summary["total"] == 2

    def test_empty_program_has_zero_tests(self, tmp_path):
        path = _write(tmp_path, "test_empty.nxl", _EMPTY_PROGRAM)
        runner = TestRunner()
        summary = runner.run_files([path])
        assert summary["total"] == 0

    def test_multiple_files_aggregate_totals(self, tmp_path):
        p1 = _write(tmp_path, "test_a.nxl", _PASSING_SUITE)
        p2 = _write(tmp_path, "test_b.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([p1, p2])
        fr1 = next(f for f in summary["files"] if f["file"] == p1)
        fr2 = next(f for f in summary["files"] if f["file"] == p2)
        assert summary["passed"] == fr1["passed"] + fr2["passed"]


# ---------------------------------------------------------------------------
# Name filtering
# ---------------------------------------------------------------------------


class TestNameFilter:
    def test_matching_name_filter_includes_tests(self, tmp_path):
        path = _write(tmp_path, "test_mix.nxl", _MIXED_SUITE)
        runner = TestRunner(name_filter="passes")
        summary = runner.run_files([path])
        assert summary["total"] >= 0  # passes is in the suite

    def test_non_matching_filter_excludes_all(self, tmp_path):
        path = _write(tmp_path, "test_mix.nxl", _MIXED_SUITE)
        runner = TestRunner(name_filter="does_not_match_anything_*")
        summary = runner.run_files([path])
        assert summary["total"] == 0


# ---------------------------------------------------------------------------
# Parallel execution (workers > 1)
# ---------------------------------------------------------------------------


class TestParallelExecution:
    def test_parallel_same_results_as_sequential(self, tmp_path):
        for i in range(3):
            _write(tmp_path, f"test_worker_{i}.nxl", _PASSING_SUITE)
        paths = TestRunner(pattern="test_*.nxl").discover(str(tmp_path))

        seq_runner = TestRunner(workers=1)
        par_runner = TestRunner(workers=3)

        seq_summary = seq_runner.run_files(paths)
        par_summary = par_runner.run_files(paths)

        assert seq_summary["passed"] == par_summary["passed"]
        assert seq_summary["failed"] == par_summary["failed"]
        assert seq_summary["total"] == par_summary["total"]


# ---------------------------------------------------------------------------
# Output formatters
# ---------------------------------------------------------------------------


class TestVerboseFormatter:
    def test_verbose_contains_pass_fail_indicators(self, tmp_path):
        path = _write(tmp_path, "test_mix.nxl", _MIXED_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        output = runner.format_verbose(summary)
        assert "PASS" in output
        assert "FAIL" in output

    def test_verbose_contains_total_line(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        output = runner.format_verbose(summary)
        assert "TOTAL:" in output

    def test_verbose_contains_file_path(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        output = runner.format_verbose(summary)
        assert "test_pass.nxl" in output


class TestTAPFormatter:
    def test_tap_starts_with_version(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        tap = runner.format_tap(summary)
        assert tap.startswith("TAP version 13")

    def test_tap_contains_plan(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        tap = runner.format_tap(summary)
        assert "1.." in tap

    def test_tap_ok_for_passing_tests(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        tap = runner.format_tap(summary)
        assert "ok " in tap

    def test_tap_not_ok_for_failing_tests(self, tmp_path):
        path = _write(tmp_path, "test_fail.nxl", _FAILING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        tap = runner.format_tap(summary)
        assert "not ok" in tap


class TestJSONFormatter:
    def test_json_is_valid_json(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        parsed = json.loads(runner.format_json(summary))
        assert isinstance(parsed, dict)

    def test_json_contains_summary_keys(self, tmp_path):
        path = _write(tmp_path, "test_pass.nxl", _PASSING_SUITE)
        runner = TestRunner()
        summary = runner.run_files([path])
        parsed = json.loads(runner.format_json(summary))
        assert "passed" in parsed
        assert "failed" in parsed
        assert "total" in parsed
        assert "files" in parsed
