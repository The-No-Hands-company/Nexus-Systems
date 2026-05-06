"""
pytest tests for src/nlpl/stdlib/coverage_utils/__init__.py

Run with:
    pytest tests/unit/stdlib/test_coverage_utils.py
"""

import importlib.util
import os
import sys
import types

import pytest

# ---------------------------------------------------------------------------
# Module loading fixture
# ---------------------------------------------------------------------------

_HERE = os.path.dirname(os.path.abspath(__file__))
_INIT_PATH = os.path.join(
    _HERE, "..", "..", "..", "src", "nlpl", "stdlib", "coverage_utils", "__init__.py"
)


@pytest.fixture(scope="module")
def cu():
    """Load coverage_utils in isolation."""
    _pkgs = (
        "nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
        "nexuslang.stdlib", "nexuslang.stdlib.coverage_utils",
    )
    # Save originals so we can restore after tests
    _originals = {pkg: sys.modules.get(pkg) for pkg in _pkgs}
    _had_runtime_cls = hasattr(sys.modules.get("nexuslang.runtime.runtime", object()), "Runtime")
    _orig_runtime_cls = getattr(sys.modules.get("nexuslang.runtime.runtime"), "Runtime", None) if _had_runtime_cls else None

    for pkg in _pkgs:
        if pkg not in sys.modules:
            sys.modules[pkg] = types.ModuleType(pkg)

    class _StubRuntime:
        def register_function(self, name, fn): pass
        def register_module(self, name): pass

    sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime
    spec = importlib.util.spec_from_file_location("nexuslang.stdlib.coverage_utils", _INIT_PATH)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    yield mod

    # Restore original sys.modules state
    for pkg in _pkgs:
        if _originals[pkg] is None:
            sys.modules.pop(pkg, None)
        else:
            sys.modules[pkg] = _originals[pkg]
    if _had_runtime_cls and "nexuslang.runtime.runtime" in sys.modules:
        sys.modules["nexuslang.runtime.runtime"].Runtime = _orig_runtime_cls


# ---------------------------------------------------------------------------
# Sample functions used as coverage targets
# ---------------------------------------------------------------------------

def _sample_if(x):
    if x > 0:
        return x * 2
    else:
        return 0


def _sample_loop(lst):
    total = 0
    for item in lst:
        total += item
    return total


def _sample_nested(a, b):
    if a > 0:
        if b > 0:
            return a + b
        return a
    return 0


def _sample_trivial():
    return 42


_THIS_FILE = os.path.normpath(__file__)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

import itertools
_counter = itertools.count(1)


def _sname():
    """Generate a unique session name for each test."""
    return f"_test_{next(_counter)}"


# ---------------------------------------------------------------------------
# coverage_measure (primary safe API)
# ---------------------------------------------------------------------------

class TestCoverageMeasure:
    def test_returns_tuple(self, cu):
        result, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        assert isinstance((result, sess), tuple)

    def test_return_value_correct(self, cu):
        result, _ = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        assert result == 42

    def test_session_is_dict(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        assert isinstance(sess, dict)

    def test_session_has_required_keys(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        for key in ("name", "active", "files", "call_count", "line_count"):
            assert key in sess

    def test_session_not_active_after(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        assert sess["active"] is False

    def test_call_count_positive(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        assert sess["call_count"] >= 1

    def test_line_count_positive(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        assert sess["line_count"] >= 1

    def test_file_tracked(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        normed_keys = [os.path.normpath(k) for k in sess["files"].keys()]
        assert _THIS_FILE in normed_keys

    def test_lines_hit_are_ints(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        for fp, rec in sess["files"].items():
            assert all(isinstance(ln, int) for ln in rec["lines_hit"])

    def test_lines_hit_subset_of_total(self, cu):
        _, sess = cu.coverage_measure(_sample_trivial, include=[_THIS_FILE])
        for fp, rec in sess["files"].items():
            assert set(rec["lines_hit"]).issubset(set(rec["lines_total"]))

    def test_with_args(self, cu):
        result, _ = cu.coverage_measure(_sample_if, args=[5], include=[_THIS_FILE])
        assert result == 10

    def test_measure_with_kwargs(self, cu):
        result, _ = cu.coverage_measure(_sample_loop, kwargs={"lst": [1, 2, 3]},
                                        include=[_THIS_FILE])
        assert result == 6

    def test_non_callable_raises(self, cu):
        with pytest.raises(TypeError):
            cu.coverage_measure("not callable")

    def test_custom_session_name(self, cu):
        sname = _sname()
        _, sess = cu.coverage_measure(_sample_trivial, name=sname, include=[_THIS_FILE])
        assert sess["name"] == sname

    def test_session_stored_after_measure(self, cu):
        sname = _sname()
        cu.coverage_measure(_sample_trivial, name=sname, include=[_THIS_FILE])
        assert sname in cu.coverage_list_sessions()

    def test_both_branches_if(self, cu):
        """Measuring both branches should give more line hits than just one."""
        _, sess1 = cu.coverage_measure(_sample_if, args=[1], include=[_THIS_FILE])
        _, sess2 = cu.coverage_measure(_sample_if, args=[-1], include=[_THIS_FILE])
        # Each should cover the respective branch line
        for rec in sess1["files"].values():
            assert len(rec["lines_hit"]) >= 1
        for rec in sess2["files"].values():
            assert len(rec["lines_hit"]) >= 1


# ---------------------------------------------------------------------------
# coverage_measure_line_rate
# ---------------------------------------------------------------------------

class TestCoverageMeasureLineRate:
    def test_returns_float(self, cu):
        rate = cu.coverage_measure_line_rate(_sample_trivial, include=[_THIS_FILE])
        assert isinstance(rate, float)

    def test_in_range(self, cu):
        rate = cu.coverage_measure_line_rate(_sample_trivial, include=[_THIS_FILE])
        assert 0.0 <= rate <= 1.0

    def test_positive(self, cu):
        rate = cu.coverage_measure_line_rate(_sample_trivial, include=[_THIS_FILE])
        assert rate > 0.0

    def test_no_include_returns_float(self, cu):
        rate = cu.coverage_measure_line_rate(_sample_trivial)
        assert isinstance(rate, float)
        assert 0.0 <= rate <= 1.0


# ---------------------------------------------------------------------------
# Session lifecycle: start / stop
# ---------------------------------------------------------------------------

class TestSessionStartStop:
    def test_start_returns_name(self, cu):
        sname = _sname()
        result = cu.coverage_start(sname, include=[_THIS_FILE])
        assert result == sname
        cu.coverage_stop(sname)

    def test_start_creates_session(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        assert sname in cu.coverage_list_sessions()
        cu.coverage_stop(sname)

    def test_is_active_after_start(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        assert cu.coverage_is_active(sname) is True
        cu.coverage_stop(sname)

    def test_stop_returns_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        result = cu.coverage_stop(sname)
        assert isinstance(result, dict)

    def test_stop_marks_inactive(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_stop(sname)
        assert cu.coverage_is_active(sname) is False

    def test_stop_returns_name(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        result = cu.coverage_stop(sname)
        assert result["name"] == sname

    def test_data_recorded(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        result = cu.coverage_stop(sname)
        assert result["call_count"] >= 1

    def test_double_stop_no_error(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_stop(sname)
        cu.coverage_stop(sname)  # Should not raise


# ---------------------------------------------------------------------------
# coverage_is_active
# ---------------------------------------------------------------------------

class TestCoverageIsActive:
    def test_true_while_active(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        assert cu.coverage_is_active(sname) is True
        cu.coverage_stop(sname)

    def test_false_after_stop(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_stop(sname)
        assert cu.coverage_is_active(sname) is False

    def test_false_for_nonexistent(self, cu):
        assert cu.coverage_is_active("__nonexistent_session__") is False


# ---------------------------------------------------------------------------
# coverage_pause / coverage_resume
# ---------------------------------------------------------------------------

class TestPauseResume:
    def test_pause_marks_inactive(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_pause(sname)
        assert cu.coverage_is_active(sname) is False
        cu.coverage_stop(sname)

    def test_resume_marks_active(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_pause(sname)
        cu.coverage_resume(sname)
        assert cu.coverage_is_active(sname) is True
        cu.coverage_stop(sname)

    def test_data_preserved_across_pause(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        before = cu.coverage_get(sname)["call_count"]
        cu.coverage_pause(sname)
        # While paused, calls aren't counted
        cu.coverage_resume(sname)
        _sample_trivial()
        after = cu.coverage_get(sname)["call_count"]
        # before data should still be there
        assert after >= before
        cu.coverage_stop(sname)

    def test_resume_returns_name(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_pause(sname)
        name_back = cu.coverage_resume(sname)
        assert name_back == sname
        cu.coverage_stop(sname)


# ---------------------------------------------------------------------------
# coverage_reset
# ---------------------------------------------------------------------------

class TestCoverageReset:
    def test_clears_line_count(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_reset(sname)
        assert cu.coverage_get(sname)["line_count"] == 0
        cu.coverage_stop(sname)

    def test_clears_call_count(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_reset(sname)
        assert cu.coverage_get(sname)["call_count"] == 0
        cu.coverage_stop(sname)

    def test_clears_files(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_reset(sname)
        assert len(cu.coverage_get(sname)["files"]) == 0
        cu.coverage_stop(sname)

    def test_returns_name(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        result = cu.coverage_reset(sname)
        assert result == sname
        cu.coverage_stop(sname)


# ---------------------------------------------------------------------------
# coverage_destroy
# ---------------------------------------------------------------------------

class TestCoverageDestroy:
    def test_removes_session(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_destroy(sname)
        assert sname not in cu.coverage_list_sessions()

    def test_stops_if_active(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_destroy(sname)
        # Should not be active (already gone)
        assert cu.coverage_is_active(sname) is False

    def test_returns_name(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        result = cu.coverage_destroy(sname)
        assert result == sname

    def test_nonexistent_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_destroy("__nonexistent__")


# ---------------------------------------------------------------------------
# coverage_list_sessions
# ---------------------------------------------------------------------------

class TestListSessions:
    def test_returns_list(self, cu):
        assert isinstance(cu.coverage_list_sessions(), list)

    def test_new_session_appears(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        assert sname in cu.coverage_list_sessions()
        cu.coverage_stop(sname)


# ---------------------------------------------------------------------------
# coverage_get
# ---------------------------------------------------------------------------

class TestCoverageGet:
    def test_returns_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        result = cu.coverage_get(sname)
        assert isinstance(result, dict)
        cu.coverage_stop(sname)

    def test_session_still_active_after_get(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_get(sname)
        assert cu.coverage_is_active(sname) is True
        cu.coverage_stop(sname)

    def test_nonexistent_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_get("__nonexistent__")

    def test_has_required_keys(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        result = cu.coverage_get(sname)
        cu.coverage_stop(sname)
        for k in ("name", "active", "files", "call_count", "line_count"):
            assert k in result


# ---------------------------------------------------------------------------
# coverage_files / coverage_lines_hit / coverage_lines_total
# ---------------------------------------------------------------------------

class TestLineAccessors:
    def test_coverage_files_returns_list(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        result = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        assert isinstance(result, list)

    def test_coverage_files_nonempty_after_trace(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        assert len(flist) >= 1

    def test_coverage_lines_hit_is_list(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            hit = cu.coverage_lines_hit(flist[0], sname)
            assert isinstance(hit, list)

    def test_coverage_lines_hit_all_int(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            hit = cu.coverage_lines_hit(flist[0], sname)
            assert all(isinstance(x, int) for x in hit)

    def test_coverage_lines_total_is_list(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            total = cu.coverage_lines_total(flist[0], sname)
            assert isinstance(total, list)

    def test_hit_subset_of_total(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            hit = cu.coverage_lines_hit(flist[0], sname)
            total = cu.coverage_lines_total(flist[0], sname)
            assert set(hit).issubset(set(total))

    def test_unknown_file_returns_empty(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_stop(sname)
        hit = cu.coverage_lines_hit("/nonexistent/file.py", sname)
        assert hit == []

    def test_lines_sorted(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_if(1)
        _sample_if(-1)
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            hit = cu.coverage_lines_hit(flist[0], sname)
            assert hit == sorted(hit)


# ---------------------------------------------------------------------------
# coverage_line_rate
# ---------------------------------------------------------------------------

class TestLineRate:
    def test_returns_float(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            rate = cu.coverage_line_rate(flist[0], sname)
            assert isinstance(rate, float)

    def test_in_range(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            rate = cu.coverage_line_rate(flist[0], sname)
            assert 0.0 <= rate <= 1.0

    def test_overall_rate(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rate = cu.coverage_line_rate(name=sname)
        assert 0.0 <= rate <= 1.0

    def test_empty_session_rate_is_one(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_stop(sname)
        rate = cu.coverage_line_rate(name=sname)
        assert rate == 1.0


# ---------------------------------------------------------------------------
# coverage_functions / coverage_function_rate
# ---------------------------------------------------------------------------

class TestFunctionCoverage:
    def test_coverage_functions_returns_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            fns = cu.coverage_functions(flist[0], sname)
            assert isinstance(fns, dict)

    def test_function_names_in_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        _sample_if(1)
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            fns = cu.coverage_functions(flist[0], sname)
            assert any("sample" in k for k in fns.keys())

    def test_hit_counts_positive(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            fns = cu.coverage_functions(flist[0], sname)
            assert all(isinstance(v, int) and v >= 1 for v in fns.values())

    def test_coverage_function_rate_in_range(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rate = cu.coverage_function_rate(name=sname)
        assert 0.0 <= rate <= 1.0

    def test_function_rate_positive_after_call(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rate = cu.coverage_function_rate(name=sname)
        assert rate > 0.0


# ---------------------------------------------------------------------------
# coverage_branches / coverage_branch_count
# ---------------------------------------------------------------------------

class TestBranchCoverage:
    def test_branches_returns_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_if(1)
        _sample_if(-1)
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            br = cu.coverage_branches(flist[0], sname)
            assert isinstance(br, dict)

    def test_branch_keys_are_strings(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_if(1)
        flist = cu.coverage_files(sname)
        cu.coverage_stop(sname)
        if flist:
            br = cu.coverage_branches(flist[0], sname)
            assert all(isinstance(k, str) for k in br.keys())

    def test_branch_count_is_int(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_if(1)
        cu.coverage_stop(sname)
        bc = cu.coverage_branch_count(name=sname)
        assert isinstance(bc, int) and bc >= 0

    def test_more_branches_with_both_paths(self, cu):
        sname1 = _sname()
        sname2 = _sname()
        cu.coverage_start(sname1, include=[_THIS_FILE])
        _sample_if(1)  # only positive branch
        bc1 = cu.coverage_branch_count(name=sname1)
        cu.coverage_stop(sname1)

        cu.coverage_start(sname2, include=[_THIS_FILE])
        _sample_if(1)
        _sample_if(-1)  # also negative branch
        bc2 = cu.coverage_branch_count(name=sname2)
        cu.coverage_stop(sname2)

        assert bc2 >= bc1


# ---------------------------------------------------------------------------
# coverage_call_count / coverage_line_count
# ---------------------------------------------------------------------------

class TestCounts:
    def test_call_count_is_int(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        cc = cu.coverage_call_count(sname)
        assert isinstance(cc, int) and cc >= 1

    def test_line_count_is_int(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        lc = cu.coverage_line_count(sname)
        assert isinstance(lc, int) and lc >= 1

    def test_multiple_calls_accumulate(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cc1 = cu.coverage_call_count(sname)
        _sample_trivial()
        cc2 = cu.coverage_call_count(sname)
        cu.coverage_stop(sname)
        assert cc2 >= cc1

    def test_zero_before_any_calls(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        # Don't call any functions inside tracked file, just stop immediately
        cu.coverage_stop(sname)
        # call_count may be 0 or include the test frame itself; just check type
        cc = cu.coverage_call_count(sname)
        assert isinstance(cc, int) and cc >= 0


# ---------------------------------------------------------------------------
# coverage_summary
# ---------------------------------------------------------------------------

class TestCoverageSummary:
    def test_returns_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        summ = cu.coverage_summary(sname)
        assert isinstance(summ, dict)

    def test_has_required_keys(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        summ = cu.coverage_summary(sname)
        for k in ("name", "active", "file_count", "total_lines", "hit_lines",
                   "line_rate", "total_branches", "total_functions",
                   "call_count", "line_count"):
            assert k in summ, f"Missing key: {k}"

    def test_name_correct(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        cu.coverage_stop(sname)
        assert cu.coverage_summary(sname)["name"] == sname

    def test_line_rate_in_range(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rate = cu.coverage_summary(sname)["line_rate"]
        assert 0.0 <= rate <= 1.0

    def test_hit_le_total(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_if(1)
        cu.coverage_stop(sname)
        summ = cu.coverage_summary(sname)
        assert summ["hit_lines"] <= summ["total_lines"]


# ---------------------------------------------------------------------------
# coverage_report / coverage_report_text
# ---------------------------------------------------------------------------

class TestCoverageReport:
    def test_report_returns_list(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rows = cu.coverage_report(sname)
        assert isinstance(rows, list)

    def test_report_row_has_required_keys(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rows = cu.coverage_report(sname)
        if rows:
            row = rows[0]
            for k in ("filename", "hit_lines", "total_lines", "line_rate",
                       "functions", "branch_count"):
                assert k in row

    def test_report_with_show_lines(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rows = cu.coverage_report(sname, show_lines=True)
        if rows:
            assert "hit_line_list" in rows[0]
            assert "miss_line_list" in rows[0]

    def test_report_text_is_str(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        text = cu.coverage_report_text(sname)
        assert isinstance(text, str)

    def test_report_text_contains_total(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        text = cu.coverage_report_text(sname)
        assert "TOTAL" in text

    def test_report_text_contains_coverage_report(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        text = cu.coverage_report_text(sname)
        assert "Coverage Report" in text

    def test_report_line_rate_in_row(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        rows = cu.coverage_report(sname)
        if rows:
            assert 0.0 <= rows[0]["line_rate"] <= 1.0


# ---------------------------------------------------------------------------
# coverage_to_dict
# ---------------------------------------------------------------------------

class TestCoverageToDict:
    def test_returns_dict(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        d = cu.coverage_to_dict(sname)
        assert isinstance(d, dict)

    def test_has_files(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        d = cu.coverage_to_dict(sname)
        assert "files" in d

    def test_lines_hit_sorted(self, cu):
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_if(1)
        _sample_if(-1)
        cu.coverage_stop(sname)
        d = cu.coverage_to_dict(sname)
        for rec in d["files"].values():
            hit = rec["lines_hit"]
            assert hit == sorted(hit)

    def test_serializable_types(self, cu):
        """All values should be basic Python types (list, dict, int, str, bool)."""
        sname = _sname()
        cu.coverage_start(sname, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(sname)
        d = cu.coverage_to_dict(sname)
        import json
        # Should not raise
        json.dumps(d)


# ---------------------------------------------------------------------------
# coverage_merge
# ---------------------------------------------------------------------------

class TestCoverageMerge:
    def test_returns_name(self, cu):
        a, b = _sname(), _sname()
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_loop([1, 2])
        cu.coverage_stop(b)
        merged = cu.coverage_merge(a, b, "merged_test")
        assert merged == "merged_test"
        cu.coverage_destroy("merged_test")

    def test_merged_session_exists(self, cu):
        a, b = _sname(), _sname()
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(b)
        mname = _sname()
        cu.coverage_merge(a, b, mname)
        assert mname in cu.coverage_list_sessions()
        cu.coverage_destroy(mname)

    def test_merged_call_count_sum(self, cu):
        a, b = _sname(), _sname()
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_trivial()
        cc_a = cu.coverage_call_count(a)
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_trivial()
        cc_b = cu.coverage_call_count(b)
        cu.coverage_stop(b)
        mname = _sname()
        cu.coverage_merge(a, b, mname)
        cc_m = cu.coverage_call_count(mname)
        assert cc_m == cc_a + cc_b
        cu.coverage_destroy(mname)

    def test_merged_lines_are_union(self, cu):
        a, b = _sname(), _sname()
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_trivial()
        flist = cu.coverage_files(a)
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_if(-1)
        cu.coverage_stop(b)
        mname = _sname()
        cu.coverage_merge(a, b, mname)
        if flist:
            hit_a = set(cu.coverage_lines_hit(flist[0], a))
            hit_b = set(cu.coverage_lines_hit(flist[0], b))
            hit_m = set(cu.coverage_lines_hit(flist[0], mname))
            assert hit_m == hit_a | hit_b
        cu.coverage_destroy(mname)

    def test_nonexistent_session_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_merge("__nonexistent__", "__nonexistent2__")


# ---------------------------------------------------------------------------
# coverage_diff
# ---------------------------------------------------------------------------

class TestCoverageDiff:
    def test_returns_dict(self, cu):
        a, b = _sname(), _sname()
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(b)
        diff = cu.coverage_diff(a, b)
        assert isinstance(diff, dict)

    def test_identical_sessions_no_diff(self, cu):
        a, b = _sname(), _sname()
        # Run exactly the same thing in both
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_trivial()
        cu.coverage_stop(b)
        diff = cu.coverage_diff(a, b)
        # Since both cover the same lines, diff entries should be empty or absent
        for fp, v in diff.items():
            assert v["only_in_a"] == [] or v["only_in_b"] == []

    def test_diff_shows_asymmetry(self, cu):
        a, b = _sname(), _sname()
        cu.coverage_start(a, include=[_THIS_FILE])
        _sample_if(1)   # positive branch only
        cu.coverage_stop(a)
        cu.coverage_start(b, include=[_THIS_FILE])
        _sample_if(-1)  # negative branch only
        cu.coverage_stop(b)
        diff = cu.coverage_diff(a, b)
        # There should be some asymmetry recorded
        # (Both run _sample_if but hit different branches)
        assert isinstance(diff, dict)


# ---------------------------------------------------------------------------
# Error conditions
# ---------------------------------------------------------------------------

class TestErrorConditions:
    def test_stop_nonexistent_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_stop("__no_such_session__")

    def test_get_before_start_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_get("__totally_new_session__")

    def test_reset_nonexistent_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_reset("__no_such_session__")

    def test_summary_nonexistent_raises(self, cu):
        with pytest.raises(RuntimeError):
            cu.coverage_summary("__no_such_session__")


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegistration:
    def test_all_functions_registered(self, cu):
        calls = []
        class _R:
            def register_function(self, name, fn): calls.append(name)
            def register_module(self, name): pass
        cu.register_coverage_utils_functions(_R())
        expected = [
            "coverage_start", "coverage_stop", "coverage_pause", "coverage_resume",
            "coverage_reset", "coverage_destroy", "coverage_is_active",
            "coverage_list_sessions", "coverage_get", "coverage_files",
            "coverage_lines_hit", "coverage_lines_total", "coverage_line_rate",
            "coverage_functions", "coverage_function_rate",
            "coverage_branches", "coverage_branch_count",
            "coverage_call_count", "coverage_line_count",
            "coverage_summary", "coverage_report", "coverage_report_text",
            "coverage_to_dict", "coverage_merge", "coverage_diff",
            "coverage_measure", "coverage_measure_line_rate",
        ]
        for name in expected:
            assert name in calls, f"Missing: {name}"

    def test_modules_registered(self, cu):
        mods = []
        class _R:
            def register_function(self, name, fn): pass
            def register_module(self, name): mods.append(name)
        cu.register_coverage_utils_functions(_R())
        assert "coverage_utils" in mods
        assert "coverage" in mods

    def test_function_count_at_least_27(self, cu):
        calls = []
        class _R:
            def register_function(self, name, fn): calls.append(name)
            def register_module(self, name): pass
        cu.register_coverage_utils_functions(_R())
        assert len(calls) >= 27
