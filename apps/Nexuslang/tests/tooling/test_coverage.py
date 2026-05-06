"""
Tests for the NexusLang coverage reporting system.

Covers:
  - FileCoverage data model and properties
  - CoverageReport aggregation and output formats
  - CoverageCollector record / build_report
  - _find_executable_lines heuristic
  - TestRunner coverage_enabled integration
  - nlpl-cover CLI argument parsing
"""

import json
import os
import sys
import tempfile
import textwrap
from pathlib import Path
from typing import Set

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src"))

from nexuslang.tooling.coverage import (
    CoverageCollector,
    CoverageReport,
    FileCoverage,
    _find_executable_lines,
    _pct_class,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_file_coverage(
    path: str = "test.nxl",
    total: int = 10,
    executable: Set[int] = frozenset({1, 2, 3, 4, 5}),
    hit: Set[int] = frozenset({1, 2, 3}),
    source_lines=None,
) -> FileCoverage:
    return FileCoverage(
        path=path,
        total_lines=total,
        executable_lines=set(executable),
        hit_lines=set(hit),
        source_lines=source_lines or [f"line {i}\n" for i in range(1, total + 1)],
    )


def _make_report(*file_coverages: FileCoverage) -> CoverageReport:
    report = CoverageReport(generated_at="2026-02-27T00:00:00")
    for fc in file_coverages:
        report.files[fc.path] = fc
    return report


# ===========================================================================
# FileCoverage properties
# ===========================================================================


class TestFileCoverage:
    def test_covered_count_intersection(self):
        fc = _make_file_coverage(
            executable={1, 2, 3, 4, 5}, hit={1, 2, 6}  # 6 is not executable
        )
        assert fc.covered_count == 2

    def test_executable_count(self):
        fc = _make_file_coverage(executable={1, 3, 5})
        assert fc.executable_count == 3

    def test_miss_count(self):
        fc = _make_file_coverage(executable={1, 2, 3, 4}, hit={1, 2})
        assert fc.miss_count == 2

    def test_pct_basic(self):
        fc = _make_file_coverage(executable={1, 2, 3, 4}, hit={1, 2})
        assert fc.pct == pytest.approx(50.0)

    def test_pct_full_coverage(self):
        fc = _make_file_coverage(executable={1, 2, 3}, hit={1, 2, 3})
        assert fc.pct == pytest.approx(100.0)

    def test_pct_zero_coverage(self):
        fc = _make_file_coverage(executable={1, 2, 3}, hit=set())
        assert fc.pct == pytest.approx(0.0)

    def test_pct_empty_executable_is_100(self):
        fc = _make_file_coverage(executable=set(), hit=set())
        assert fc.pct == pytest.approx(100.0)

    def test_missed_lines_sorted(self):
        fc = _make_file_coverage(executable={5, 1, 3, 2}, hit={1})
        assert fc.missed_lines() == [2, 3, 5]

    def test_missed_lines_empty_when_fully_covered(self):
        fc = _make_file_coverage(executable={1, 2}, hit={1, 2})
        assert fc.missed_lines() == []

    def test_line_status_hit(self):
        fc = _make_file_coverage(executable={1, 2, 3}, hit={2})
        assert fc.line_status(2) == "hit"

    def test_line_status_miss(self):
        fc = _make_file_coverage(executable={1, 2, 3}, hit={2})
        assert fc.line_status(1) == "miss"

    def test_line_status_nocode(self):
        fc = _make_file_coverage(executable={1, 2, 3}, hit={1})
        assert fc.line_status(5) == "nocode"

    def test_source_lines_stored(self):
        fc = _make_file_coverage(source_lines=["line one\n", "line two\n"])
        assert fc.source_lines[0] == "line one\n"

    def test_total_lines_stored(self):
        fc = _make_file_coverage(total=20)
        assert fc.total_lines == 20

    def test_path_stored(self):
        fc = _make_file_coverage(path="/some/path.nxl")
        assert fc.path == "/some/path.nxl"


# ===========================================================================
# CoverageReport aggregation
# ===========================================================================


class TestCoverageReport:
    def test_total_executable_sums_files(self):
        fc1 = _make_file_coverage("a.nxl", executable={1, 2, 3})
        fc2 = _make_file_coverage("b.nxl", executable={1, 2})
        report = _make_report(fc1, fc2)
        assert report.total_executable() == 5

    def test_total_covered_sums_files(self):
        fc1 = _make_file_coverage("a.nxl", executable={1, 2, 3}, hit={1, 2})
        fc2 = _make_file_coverage("b.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc1, fc2)
        assert report.total_covered() == 3

    def test_total_pct_correct(self):
        fc1 = _make_file_coverage("a.nxl", executable={1, 2, 3, 4}, hit={1, 2})
        fc2 = _make_file_coverage("b.nxl", executable={1, 2}, hit={1, 2})
        report = _make_report(fc1, fc2)
        # 4 out of 6 = 66.67%
        assert report.total_pct() == pytest.approx(4 / 6 * 100)

    def test_total_pct_100_when_no_executable(self):
        report = _make_report()  # empty report
        assert report.total_pct() == pytest.approx(100.0)

    def test_generated_at_stored(self):
        report = CoverageReport(generated_at="2026-02-27T12:00:00")
        assert report.generated_at == "2026-02-27T12:00:00"

    def test_files_dict_accessible(self):
        fc = _make_file_coverage("x.nxl")
        report = _make_report(fc)
        assert "x.nxl" in report.files


# ===========================================================================
# CoverageReport.summary()
# ===========================================================================


class TestCoverageReportSummary:
    def test_summary_contains_total_row(self):
        fc = _make_file_coverage("a.nxl", executable={1, 2, 3}, hit={1})
        report = _make_report(fc)
        summary = report.summary()
        assert "TOTAL" in summary

    def test_summary_contains_file_name(self):
        fc = _make_file_coverage("myfile.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc)
        assert "myfile.nxl" in report.summary()

    def test_summary_contains_coverage_percentage(self):
        fc = _make_file_coverage("f.nxl", executable={1, 2, 3, 4}, hit={1, 2, 3, 4})
        report = _make_report(fc)
        assert "100%" in report.summary()

    def test_summary_has_header_row(self):
        report = CoverageReport()
        summary = report.summary()
        assert "File" in summary or "Cover" in summary

    def test_summary_returns_string(self):
        report = CoverageReport()
        assert isinstance(report.summary(), str)


# ===========================================================================
# CoverageReport.to_json() / write_json()
# ===========================================================================


class TestCoverageReportJson:
    def test_to_json_returns_string(self):
        fc = _make_file_coverage()
        report = _make_report(fc)
        result = report.to_json()
        assert isinstance(result, str)

    def test_to_json_has_meta_key(self):
        fc = _make_file_coverage("a.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc)
        data = json.loads(report.to_json())
        assert "meta" in data

    def test_to_json_has_files_key(self):
        fc = _make_file_coverage("a.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc)
        data = json.loads(report.to_json())
        assert "files" in data

    def test_to_json_meta_pct_correct(self):
        fc = _make_file_coverage("a.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc)
        data = json.loads(report.to_json())
        assert data["meta"]["total_pct"] == pytest.approx(50.0, abs=0.1)

    def test_to_json_hit_lines_sorted(self):
        fc = _make_file_coverage("a.nxl", executable={1, 2, 3, 4}, hit={4, 1, 2})
        report = _make_report(fc)
        data = json.loads(report.to_json())
        hit = data["files"]["a.nxl"]["hit"]
        assert hit == sorted(hit)

    def test_to_json_missed_lines_correct(self):
        fc = _make_file_coverage("a.nxl", executable={1, 2, 3}, hit={1, 2})
        report = _make_report(fc)
        data = json.loads(report.to_json())
        assert data["files"]["a.nxl"]["missed"] == [3]

    def test_write_json_creates_file(self, tmp_path):
        fc = _make_file_coverage("f.nxl", executable={1}, hit={1})
        report = _make_report(fc)
        out = str(tmp_path / "cov.json")
        report.write_json(out)
        assert os.path.exists(out)
        data = json.loads(Path(out).read_text())
        assert "meta" in data

    def test_write_json_creates_parent_dir(self, tmp_path):
        fc = _make_file_coverage()
        report = _make_report(fc)
        out = str(tmp_path / "nested" / "dir" / "cov.json")
        report.write_json(out)
        assert os.path.exists(out)

    def test_to_json_executable_list_sorted(self):
        fc = _make_file_coverage("a.nxl", executable={5, 1, 3}, hit=set())
        report = _make_report(fc)
        data = json.loads(report.to_json())
        exe = data["files"]["a.nxl"]["executable"]
        assert exe == sorted(exe)


# ===========================================================================
# CoverageReport.write_html()
# ===========================================================================


class TestCoverageReportHtml:
    def test_write_html_creates_index(self, tmp_path):
        fc = _make_file_coverage("a.nxl", executable={1}, hit={1})
        report = _make_report(fc)
        report.write_html(str(tmp_path))
        assert (tmp_path / "index.html").exists()

    def test_write_html_creates_css(self, tmp_path):
        fc = _make_file_coverage("a.nxl", executable={1}, hit={1})
        report = _make_report(fc)
        report.write_html(str(tmp_path))
        assert (tmp_path / "coverage.css").exists()

    def test_write_html_creates_per_file_page(self, tmp_path):
        fc = FileCoverage(
            path=str(tmp_path / "prog.nxl"),
            total_lines=3,
            executable_lines={1, 2},
            hit_lines={1},
            source_lines=["set x to 1\n", "set y to 2\n", "\n"],
        )
        report = _make_report(fc)
        report.write_html(str(tmp_path))
        html_files = list(tmp_path.glob("*.html"))
        assert len(html_files) >= 2  # index.html + one per-file page

    def test_index_html_contains_file_link(self, tmp_path):
        fc = _make_file_coverage("mymodule.nxl", executable={1}, hit={1})
        report = _make_report(fc)
        report.write_html(str(tmp_path))
        index_content = (tmp_path / "index.html").read_text()
        assert "mymodule" in index_content

    def test_write_html_creates_output_dir(self, tmp_path):
        subdir = tmp_path / "new_dir"
        fc = _make_file_coverage("a.nxl", executable={1}, hit=set())
        report = _make_report(fc)
        report.write_html(str(subdir))
        assert subdir.exists()


# ===========================================================================
# _find_executable_lines
# ===========================================================================


class TestFindExecutableLines:
    def test_empty_line_not_executable(self):
        lines = ["", "set x to 1"]
        result = _find_executable_lines(lines)
        assert 1 not in result
        assert 2 in result

    def test_comment_line_not_executable(self):
        lines = ["# this is a comment", "set x to 1"]
        result = _find_executable_lines(lines)
        assert 1 not in result
        assert 2 in result

    def test_blank_line_not_executable(self):
        lines = ["   ", "set x to 1"]
        result = _find_executable_lines(lines)
        assert 1 not in result

    def test_end_keyword_not_executable(self):
        lines = ["end"]
        result = _find_executable_lines(lines)
        assert 1 not in result

    def test_end_struct_not_executable(self):
        lines = ["end struct"]
        result = _find_executable_lines(lines)
        assert 1 not in result

    def test_end_class_not_executable(self):
        lines = ["end class"]
        result = _find_executable_lines(lines)
        assert 1 not in result

    def test_end_function_not_executable(self):
        lines = ["end function"]
        result = _find_executable_lines(lines)
        assert 1 not in result

    def test_function_definition_line_executable(self):
        lines = ["function calculate with x as Integer returns Integer"]
        result = _find_executable_lines(lines)
        assert 1 in result

    def test_class_definition_line_executable(self):
        lines = ["class MyClass"]
        result = _find_executable_lines(lines)
        assert 1 in result

    def test_normal_statement_executable(self):
        lines = ["set counter to 0"]
        result = _find_executable_lines(lines)
        assert 1 in result

    def test_print_statement_executable(self):
        lines = ["print text hello"]
        result = _find_executable_lines(lines)
        assert 1 in result

    def test_if_statement_executable(self):
        lines = ["if x is greater than 0"]
        result = _find_executable_lines(lines)
        assert 1 in result

    def test_returns_set(self):
        lines = ["set x to 1"]
        result = _find_executable_lines(lines)
        assert isinstance(result, set)

    def test_multiple_lines_mixed(self):
        lines = [
            "# comment",         # 1: skip
            "",                  # 2: skip
            "set x to 1",        # 3: executable
            "   ",               # 4: skip
            "print text x",      # 5: executable
        ]
        result = _find_executable_lines(lines)
        assert result == {3, 5}

    def test_only_comments_returns_empty(self):
        lines = ["# a", "# b", "# c"]
        result = _find_executable_lines(lines)
        assert result == set()


# ===========================================================================
# _pct_class
# ===========================================================================


class TestPctClass:
    def test_90_is_high(self):
        assert _pct_class(90.0) == "high"

    def test_100_is_high(self):
        assert _pct_class(100.0) == "high"

    def test_75_is_medium(self):
        assert _pct_class(75.0) == "medium"

    def test_89_is_medium(self):
        assert _pct_class(89.9) == "medium"

    def test_74_is_low(self):
        assert _pct_class(74.9) == "low"

    def test_0_is_low(self):
        assert _pct_class(0.0) == "low"


# ===========================================================================
# CoverageCollector
# ===========================================================================


class TestCoverageCollector:
    def test_record_adds_line(self):
        col = CoverageCollector()
        col.start()
        col.record("f.nxl", 5)
        assert 5 in col._hits.get("f.nxl", set())

    def test_record_ignores_zero_line(self):
        col = CoverageCollector()
        col.start()
        col.record("f.nxl", 0)
        assert "f.nxl" not in col._hits

    def test_record_ignores_negative_line(self):
        col = CoverageCollector()
        col.start()
        col.record("f.nxl", -3)
        assert "f.nxl" not in col._hits

    def test_record_ignores_when_inactive(self):
        col = CoverageCollector()
        # Not started — _active is False
        col.record("f.nxl", 1)
        assert "f.nxl" not in col._hits

    def test_start_sets_active(self):
        col = CoverageCollector()
        col.start()
        assert col._active is True

    def test_stop_clears_active(self):
        col = CoverageCollector()
        col.start()
        col.stop()
        assert col._active is False

    def test_record_after_stop_ignored(self):
        col = CoverageCollector()
        col.start()
        col.stop()
        col.record("f.nxl", 5)
        assert "f.nxl" not in col._hits

    def test_different_files_tracked_separately(self):
        col = CoverageCollector()
        col.start()
        col.record("a.nxl", 1)
        col.record("b.nxl", 2)
        assert 1 in col._hits["a.nxl"]
        assert 2 in col._hits["b.nxl"]
        assert "b.nxl" not in col._hits.get("a.nxl", {})

    def test_same_line_recorded_once(self):
        col = CoverageCollector()
        col.start()
        col.record("f.nxl", 3)
        col.record("f.nxl", 3)
        assert col._hits["f.nxl"] == {3}

    def test_multiple_lines_per_file(self):
        col = CoverageCollector()
        col.start()
        for line in [1, 3, 5, 7]:
            col.record("f.nxl", line)
        assert col._hits["f.nxl"] == {1, 3, 5, 7}

    def test_build_report_single_path_in_memory(self, tmp_path):
        src = "set x to 1\n# comment\nprint text x\n"
        src_path = str(tmp_path / "prog.nxl")
        Path(src_path).write_text(src)

        col = CoverageCollector()
        col.start()
        col.record(src_path, 1)
        col.record(src_path, 3)
        col.stop()

        report = col.build_report(single_path=src_path, source_text=src)
        assert src_path in report.files
        fc = report.files[src_path]
        assert 1 in fc.hit_lines
        assert 3 in fc.hit_lines

    def test_build_report_unhit_lines_recorded_as_miss(self, tmp_path):
        src = "set x to 1\nset y to 2\nprint text x\n"
        src_path = str(tmp_path / "p.nxl")
        Path(src_path).write_text(src)

        col = CoverageCollector()
        col.start()
        col.record(src_path, 1)
        col.stop()

        report = col.build_report(single_path=src_path, source_text=src)
        fc = report.files[src_path]
        assert 1 in fc.hit_lines
        assert 2 not in fc.hit_lines

    def test_build_report_with_source_paths(self, tmp_path):
        src = "set x to 0\n"
        f1 = tmp_path / "f1.nxl"
        f1.write_text(src)
        f2 = tmp_path / "f2.nxl"
        f2.write_text(src)

        col = CoverageCollector()
        col.start()
        col.record(str(f1), 1)
        col.stop()

        report = col.build_report(source_paths=[str(f1), str(f2)])
        assert str(f1) in report.files
        assert str(f2) in report.files

    def test_build_report_falls_back_to_hit_paths(self, tmp_path):
        src = "set z to 0\n"
        f = tmp_path / "z.nxl"
        f.write_text(src)

        col = CoverageCollector()
        col.start()
        col.record(str(f), 1)
        col.stop()

        report = col.build_report()  # no explicit paths
        assert str(f) in report.files

    def test_attach_sets_coverage_collector_on_interpreter(self):
        col = CoverageCollector()

        class FakeInterpreter:
            _coverage_collector = None

        interp = FakeInterpreter()
        col.attach(interp)
        assert interp._coverage_collector is col
        assert col._active is True

    def test_detach_clears_coverage_collector(self):
        col = CoverageCollector()

        class FakeInterpreter:
            _coverage_collector = None

        interp = FakeInterpreter()
        col.attach(interp)
        col.detach()
        assert interp._coverage_collector is None
        assert col._active is False


# ===========================================================================
# TestRunner coverage integration
# ===========================================================================


class TestRunnerCoverageIntegration:
    def test_coverage_enabled_false_by_default(self):
        from nexuslang.tooling.test_runner import TestRunner
        runner = TestRunner()
        assert runner.coverage_enabled is False

    def test_coverage_enabled_can_be_set(self):
        from nexuslang.tooling.test_runner import TestRunner
        runner = TestRunner(coverage_enabled=True)
        assert runner.coverage_enabled is True

    def test_coverage_output_dir_default(self):
        from nexuslang.tooling.test_runner import TestRunner
        runner = TestRunner()
        assert runner.coverage_output_dir == "coverage"

    def test_coverage_output_dir_custom(self):
        from nexuslang.tooling.test_runner import TestRunner
        runner = TestRunner(coverage_output_dir="my_cov/")
        assert runner.coverage_output_dir == "my_cov/"

    def test_build_coverage_report_returns_none_when_no_hits(self):
        from nexuslang.tooling.test_runner import TestRunner
        runner = TestRunner()
        summary = {"files": [
            {"coverage_hits": {}},
            {"coverage_hits": {}},
        ]}
        result = runner.build_coverage_report(summary)
        assert result is None

    def test_build_coverage_report_merges_hits(self, tmp_path):
        from nexuslang.tooling.test_runner import TestRunner
        # Create a real source file so build_report can read it
        src = "set x to 1\nset y to 2\n"
        f = tmp_path / "prog.nxl"
        f.write_text(src)
        src_path = str(f)

        runner = TestRunner()
        summary = {"files": [
            {"coverage_hits": {src_path: [1]}},
            {"coverage_hits": {src_path: [2]}},
        ]}
        report = runner.build_coverage_report(summary)
        assert report is not None
        assert src_path in report.files
        fc = report.files[src_path]
        assert 1 in fc.hit_lines
        assert 2 in fc.hit_lines

    def test_file_result_contains_coverage_hits_key(self):
        from nexuslang.tooling.test_runner import _run_file_collect
        # Run against a non-existent file to get error result quickly
        result = _run_file_collect("/nonexistent_xyz_12345.nxl", None,
                                   coverage_enabled=False)
        assert "coverage_hits" in result

    def test_file_result_coverage_hits_empty_when_disabled(self):
        from nexuslang.tooling.test_runner import _run_file_collect
        result = _run_file_collect("/nonexistent_xyz_12345.nxl", None,
                                   coverage_enabled=False)
        assert result["coverage_hits"] == {}

    def test_write_coverage_skips_when_no_hits(self, tmp_path):
        from nexuslang.tooling.test_runner import TestRunner
        runner = TestRunner(coverage_output_dir=str(tmp_path))
        summary = {"files": [{"coverage_hits": {}}]}
        # Should not raise and should not write files
        runner.write_coverage(summary)
        assert not (tmp_path / "coverage.json").exists()


# ===========================================================================
# nlpl-cover CLI argument parsing
# ===========================================================================


class TestNlplCoverCli:
    def _parse(self, argv):
        """Return the parsed argparse Namespace without running anything."""
        import argparse
        sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src"))
        # Import just the argparse setup by re-reading the module
        from nexuslang.cli import nlplcover as mod
        # We test by checking the _cli function accepts the args without error
        # (using --help would sys.exit)
        return argv

    def test_default_output_dir(self, tmp_path, monkeypatch):
        """_cli should default output to 'coverage'."""
        nxl_file = tmp_path / "prog.nxl"
        nxl_file.write_text("set x to 1\n")
        from nexuslang.cli.nlplcover import _cli
        # Patch _run_coverage to avoid actual execution
        import nexuslang.cli.nlplcover as mod
        called_with = {}
        def fake_run(**kwargs):
            called_with.update(kwargs)
            return 0
        monkeypatch.setattr(mod, "_run_coverage", fake_run)
        _cli([str(nxl_file)])
        assert called_with.get("output_dir") == "coverage"

    def test_custom_output_dir(self, tmp_path, monkeypatch):
        nxl_file = tmp_path / "prog.nxl"
        nxl_file.write_text("set x to 1\n")
        from nexuslang.cli.nlplcover import _cli
        import nexuslang.cli.nlplcover as mod
        called_with = {}
        def fake_run(**kwargs):
            called_with.update(kwargs)
            return 0
        monkeypatch.setattr(mod, "_run_coverage", fake_run)
        _cli([str(nxl_file), "--output", "my_coverage/"])
        assert called_with.get("output_dir") == "my_coverage/"

    def test_no_json_flag(self, tmp_path, monkeypatch):
        nxl_file = tmp_path / "prog.nxl"
        nxl_file.write_text("set x to 1\n")
        from nexuslang.cli.nlplcover import _cli
        import nexuslang.cli.nlplcover as mod
        called_with = {}
        def fake_run(**kwargs):
            called_with.update(kwargs)
            return 0
        monkeypatch.setattr(mod, "_run_coverage", fake_run)
        _cli([str(nxl_file), "--no-json"])
        assert called_with.get("write_json") is False

    def test_no_html_flag(self, tmp_path, monkeypatch):
        nxl_file = tmp_path / "prog.nxl"
        nxl_file.write_text("set x to 1\n")
        from nexuslang.cli.nlplcover import _cli
        import nexuslang.cli.nlplcover as mod
        called_with = {}
        def fake_run(**kwargs):
            called_with.update(kwargs)
            return 0
        monkeypatch.setattr(mod, "_run_coverage", fake_run)
        _cli([str(nxl_file), "--no-html"])
        assert called_with.get("write_html") is False

    def test_fail_under_parsed(self, tmp_path, monkeypatch):
        nxl_file = tmp_path / "prog.nxl"
        nxl_file.write_text("set x to 1\n")
        from nexuslang.cli.nlplcover import _cli
        import nexuslang.cli.nlplcover as mod
        called_with = {}
        def fake_run(**kwargs):
            called_with.update(kwargs)
            return 0
        monkeypatch.setattr(mod, "_run_coverage", fake_run)
        _cli([str(nxl_file), "--fail-under", "80"])
        assert called_with.get("fail_under") == pytest.approx(80.0)

    def test_missing_file_returns_exit_1(self, tmp_path):
        from nexuslang.cli.nlplcover import _cli
        result = _cli(["/this_file_does_not_exist_xyz.nxl"])
        assert result == 1

    def test_fail_under_passes_when_coverage_sufficient(self, tmp_path):
        nxl_file = tmp_path / "prog.nxl"
        nxl_file.write_text("set x to 1\n")
        from nexuslang.cli.nlplcover import _cli
        import nexuslang.cli.nlplcover as mod
        def fake_run(**kwargs):
            return 0
        import unittest.mock as mock
        with mock.patch.object(mod, "_run_coverage", fake_run):
            result = _cli([str(nxl_file), "--fail-under", "50"])
        assert result == 0


# ===========================================================================
# CoverageReport multi-file aggregation roundtrip
# ===========================================================================


class TestCoverageReportMultiFile:
    def test_multi_file_json_has_all_paths(self):
        fc1 = _make_file_coverage("a.nxl", executable={1}, hit={1})
        fc2 = _make_file_coverage("b.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc1, fc2)
        data = json.loads(report.to_json())
        assert "a.nxl" in data["files"]
        assert "b.nxl" in data["files"]

    def test_multi_file_total_pct_in_json(self):
        fc1 = _make_file_coverage("a.nxl", executable={1, 2}, hit={1})
        fc2 = _make_file_coverage("b.nxl", executable={1, 2}, hit={1})
        report = _make_report(fc1, fc2)
        data = json.loads(report.to_json())
        # 2 hit out of 4 = 50%
        assert data["meta"]["total_pct"] == pytest.approx(50.0, abs=0.1)

    def test_zero_executable_correctly_100pct(self):
        # File with only comments — no executable lines
        fc = FileCoverage(
            path="empty.nxl",
            total_lines=3,
            executable_lines=set(),
            hit_lines=set(),
            source_lines=["# a\n", "# b\n", "\n"],
        )
        assert fc.pct == pytest.approx(100.0)
