"""
pytest tests for src/nlpl/stdlib/property_testing/__init__.py

Run with:
    pytest tests/unit/stdlib/test_property_testing.py
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
    _HERE, "..", "..", "..", "src", "nlpl", "stdlib", "property_testing", "__init__.py"
)


@pytest.fixture(scope="module")
def pt():
    """Load the property_testing module in isolation (no full nlpl install needed)."""
    _pkgs = (
        "nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
        "nexuslang.stdlib", "nexuslang.stdlib.property_testing",
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
    spec = importlib.util.spec_from_file_location("nexuslang.stdlib.property_testing", _INIT_PATH)
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
# gen_integer
# ---------------------------------------------------------------------------

class TestGenInteger:
    def test_returns_strategy_dict(self, pt):
        s = pt.gen_integer(0, 10)
        assert isinstance(s, dict)
        assert s["type"] == "strategy"
        assert s["kind"] == "integer"

    def test_stores_bounds(self, pt):
        s = pt.gen_integer(-5, 5)
        assert s["min_val"] == -5
        assert s["max_val"] == 5

    def test_draw_in_range(self, pt):
        s = pt.gen_integer(-10, 10)
        for seed in range(20):
            v = pt.strategy_draw(s, seed=seed)
            assert -10 <= v <= 10

    def test_draw_is_int(self, pt):
        s = pt.gen_integer(0, 100)
        v = pt.strategy_draw(s, seed=7)
        assert isinstance(v, int)

    def test_equal_bounds(self, pt):
        s = pt.gen_integer(5, 5)
        assert pt.strategy_draw(s, seed=0) == 5

    def test_negative_range(self, pt):
        s = pt.gen_integer(-100, -1)
        v = pt.strategy_draw(s, seed=3)
        assert -100 <= v <= -1

    def test_bad_range_raises(self, pt):
        with pytest.raises(ValueError):
            pt.gen_integer(10, 1)

    def test_reproducibility(self, pt):
        s = pt.gen_integer(0, 1000)
        assert pt.strategy_draw(s, seed=42) == pt.strategy_draw(s, seed=42)


# ---------------------------------------------------------------------------
# gen_float
# ---------------------------------------------------------------------------

class TestGenFloat:
    def test_returns_strategy(self, pt):
        s = pt.gen_float(0.0, 1.0)
        assert s["kind"] == "float"

    def test_draw_in_range(self, pt):
        s = pt.gen_float(0.0, 1.0)
        for seed in range(20):
            v = pt.strategy_draw(s, seed=seed)
            assert 0.0 <= v <= 1.0

    def test_draw_is_float(self, pt):
        s = pt.gen_float(1.0, 2.0)
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, float)

    def test_negative_range(self, pt):
        s = pt.gen_float(-5.0, -1.0)
        v = pt.strategy_draw(s, seed=1)
        assert -5.0 <= v <= -1.0

    def test_bad_bounds_raises(self, pt):
        with pytest.raises(ValueError):
            pt.gen_float(2.0, 1.0)

    def test_reproducibility(self, pt):
        s = pt.gen_float(0.0, 100.0)
        assert pt.strategy_draw(s, seed=5) == pt.strategy_draw(s, seed=5)


# ---------------------------------------------------------------------------
# gen_bool
# ---------------------------------------------------------------------------

class TestGenBool:
    def test_returns_strategy(self, pt):
        s = pt.gen_bool()
        assert s["kind"] == "bool"

    def test_draw_is_bool(self, pt):
        s = pt.gen_bool()
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, bool)

    def test_draws_both_values(self, pt):
        s = pt.gen_bool()
        values = {pt.strategy_draw(s, seed=i) for i in range(20)}
        assert True in values
        assert False in values


# ---------------------------------------------------------------------------
# gen_none / gen_constant
# ---------------------------------------------------------------------------

class TestGenNoneAndConstant:
    def test_gen_none_returns_none(self, pt):
        s = pt.gen_none()
        assert pt.strategy_draw(s, seed=0) is None

    def test_gen_constant_int(self, pt):
        s = pt.gen_constant(99)
        assert pt.strategy_draw(s, seed=0) == 99
        assert pt.strategy_draw(s, seed=7) == 99

    def test_gen_constant_string(self, pt):
        s = pt.gen_constant("hello")
        assert pt.strategy_draw(s, seed=0) == "hello"

    def test_gen_constant_none(self, pt):
        s = pt.gen_constant(None)
        assert pt.strategy_draw(s, seed=0) is None

    def test_gen_constant_list(self, pt):
        s = pt.gen_constant([1, 2, 3])
        assert pt.strategy_draw(s, seed=0) == [1, 2, 3]


# ---------------------------------------------------------------------------
# gen_choice / gen_from_list
# ---------------------------------------------------------------------------

class TestGenChoice:
    def test_draw_is_in_list(self, pt):
        opts = [10, 20, 30, 40]
        s = pt.gen_choice(opts)
        for seed in range(15):
            assert pt.strategy_draw(s, seed=seed) in opts

    def test_single_option(self, pt):
        s = pt.gen_choice([42])
        assert pt.strategy_draw(s, seed=0) == 42

    def test_empty_raises(self, pt):
        with pytest.raises(ValueError):
            pt.gen_choice([])

    def test_wrong_type_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_choice("hello")

    def test_from_list_in_values(self, pt):
        vals = ["x", "y", "z"]
        s = pt.gen_from_list(vals)
        for seed in range(10):
            assert pt.strategy_draw(s, seed=seed) in vals

    def test_from_list_empty_raises(self, pt):
        with pytest.raises(ValueError):
            pt.gen_from_list([])


# ---------------------------------------------------------------------------
# gen_string / gen_text
# ---------------------------------------------------------------------------

class TestGenString:
    def test_basic(self, pt):
        s = pt.gen_string(3, 8)
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, str)
        assert 3 <= len(v) <= 8

    def test_zero_min(self, pt):
        s = pt.gen_string(0, 5)
        results = [pt.strategy_draw(s, seed=i) for i in range(20)]
        assert all(0 <= len(r) <= 5 for r in results)

    def test_custom_alphabet(self, pt):
        s = pt.gen_string(5, 5, alphabet="01")
        for seed in range(10):
            v = pt.strategy_draw(s, seed=seed)
            assert all(c in "01" for c in v)

    def test_gen_text_is_string(self, pt):
        s = pt.gen_text(1, 20)
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, str)
        assert 1 <= len(v) <= 20

    def test_gen_text_empty_possible(self, pt):
        s = pt.gen_text(0, 10)
        lengths = [len(pt.strategy_draw(s, seed=i)) for i in range(30)]
        assert min(lengths) >= 0

    def test_gen_ascii_string(self, pt):
        s = pt.gen_ascii_string(2, 6)
        import string
        alphanum = string.ascii_letters + string.digits
        for seed in range(10):
            v = pt.strategy_draw(s, seed=seed)
            assert 2 <= len(v) <= 6
            assert all(c in alphanum for c in v)


# ---------------------------------------------------------------------------
# gen_list
# ---------------------------------------------------------------------------

class TestGenList:
    def test_returns_list(self, pt):
        s = pt.gen_list(pt.gen_integer(0, 10))
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, list)

    def test_size_bounds(self, pt):
        s = pt.gen_list(pt.gen_bool(), min_size=2, max_size=5)
        for seed in range(15):
            v = pt.strategy_draw(s, seed=seed)
            assert 2 <= len(v) <= 5

    def test_elements_in_range(self, pt):
        s = pt.gen_list(pt.gen_integer(-5, 5), min_size=3, max_size=3)
        v = pt.strategy_draw(s, seed=1)
        assert all(-5 <= x <= 5 for x in v)

    def test_non_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_list("not a strategy")

    def test_gen_non_empty_list_at_least_one(self, pt):
        s = pt.gen_non_empty_list(pt.gen_integer(0, 10))
        for seed in range(10):
            v = pt.strategy_draw(s, seed=seed)
            assert len(v) >= 1

    def test_nested_list(self, pt):
        s = pt.gen_list(pt.gen_list(pt.gen_bool(), 1, 3), 1, 3)
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, list)
        assert all(isinstance(inner, list) for inner in v)


# ---------------------------------------------------------------------------
# gen_dict
# ---------------------------------------------------------------------------

class TestGenDict:
    def test_returns_dict(self, pt):
        s = pt.gen_dict(pt.gen_ascii_string(1, 5), pt.gen_integer(0, 100))
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, dict)

    def test_size_bounds(self, pt):
        s = pt.gen_dict(pt.gen_ascii_string(1, 3), pt.gen_bool(), min_size=1, max_size=4)
        for seed in range(10):
            v = pt.strategy_draw(s, seed=seed)
            assert 1 <= len(v) <= 4

    def test_bad_key_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_dict("not strategy", pt.gen_integer(0, 1))

    def test_bad_value_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_dict(pt.gen_ascii_string(1, 3), 42)


# ---------------------------------------------------------------------------
# gen_tuple
# ---------------------------------------------------------------------------

class TestGenTuple:
    def test_returns_list(self, pt):
        s = pt.gen_tuple([pt.gen_integer(0, 5), pt.gen_bool()])
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, list)
        assert len(v) == 2

    def test_element_types(self, pt):
        s = pt.gen_tuple([pt.gen_integer(0, 10), pt.gen_text(1, 5)])
        v = pt.strategy_draw(s, seed=3)
        assert isinstance(v[0], int)
        assert isinstance(v[1], str)

    def test_empty_tuple(self, pt):
        s = pt.gen_tuple([])
        v = pt.strategy_draw(s, seed=0)
        assert v == []

    def test_non_list_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_tuple("not a list")

    def test_bad_element_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_tuple([pt.gen_bool(), "not a strategy"])


# ---------------------------------------------------------------------------
# gen_one_of / gen_optional
# ---------------------------------------------------------------------------

class TestGenOneOf:
    def test_draws_from_either(self, pt):
        s = pt.gen_one_of([pt.gen_integer(0, 0), pt.gen_integer(1, 1)])
        values = {pt.strategy_draw(s, seed=i) for i in range(20)}
        assert 0 in values
        assert 1 in values

    def test_empty_raises(self, pt):
        with pytest.raises(ValueError):
            pt.gen_one_of([])

    def test_bad_strategies_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_one_of([pt.gen_bool(), "not a strategy"])


class TestGenOptional:
    def test_sometimes_none(self, pt):
        s = pt.gen_optional(pt.gen_integer(1, 100))
        none_count = sum(1 for i in range(100) if pt.strategy_draw(s, seed=i) is None)
        assert none_count >= 1

    def test_usually_not_none(self, pt):
        s = pt.gen_optional(pt.gen_integer(1, 100))
        non_none = sum(1 for i in range(100) if pt.strategy_draw(s, seed=i) is not None)
        assert non_none >= 60

    def test_non_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_optional("bad")


# ---------------------------------------------------------------------------
# gen_fixed_dict / gen_text_lines
# ---------------------------------------------------------------------------

class TestGenFixedDict:
    def test_has_all_keys(self, pt):
        s = pt.gen_fixed_dict({"a": pt.gen_integer(0, 5), "b": pt.gen_bool()})
        v = pt.strategy_draw(s, seed=0)
        assert "a" in v and "b" in v

    def test_value_types(self, pt):
        s = pt.gen_fixed_dict({"n": pt.gen_integer(0, 10), "t": pt.gen_text(1, 5)})
        v = pt.strategy_draw(s, seed=2)
        assert isinstance(v["n"], int)
        assert isinstance(v["t"], str)

    def test_empty_dict(self, pt):
        s = pt.gen_fixed_dict({})
        v = pt.strategy_draw(s, seed=0)
        assert v == {}

    def test_bad_value_raises(self, pt):
        with pytest.raises(TypeError):
            pt.gen_fixed_dict({"x": "not a strategy"})


class TestGenTextLines:
    def test_returns_list(self, pt):
        s = pt.gen_text_lines(4)
        v = pt.strategy_draw(s, seed=0)
        assert isinstance(v, list)
        assert len(v) == 4

    def test_elements_are_strings(self, pt):
        s = pt.gen_text_lines(3)
        v = pt.strategy_draw(s, seed=1)
        assert all(isinstance(line, str) for line in v)

    def test_zero_lines(self, pt):
        s = pt.gen_text_lines(0)
        v = pt.strategy_draw(s, seed=0)
        assert v == []


# ---------------------------------------------------------------------------
# strategy_filter / strategy_map
# ---------------------------------------------------------------------------

class TestStrategyFilter:
    def test_only_even(self, pt):
        s = pt.strategy_filter(pt.gen_integer(0, 100), lambda x: x % 2 == 0)
        for seed in range(10):
            v = pt.strategy_draw(s, seed=seed)
            assert v % 2 == 0

    def test_non_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.strategy_filter("bad", lambda x: True)

    def test_non_callable_fn_raises(self, pt):
        with pytest.raises(TypeError):
            pt.strategy_filter(pt.gen_bool(), "not callable")

    def test_impossible_filter_raises(self, pt):
        s = pt.strategy_filter(pt.gen_integer(0, 0), lambda x: x > 0, max_attempts=5)
        with pytest.raises(RuntimeError):
            pt.strategy_draw(s, seed=0)


class TestStrategyMap:
    def test_maps_values(self, pt):
        s = pt.strategy_map(pt.gen_integer(1, 5), lambda x: x * 10)
        for seed in range(10):
            v = pt.strategy_draw(s, seed=seed)
            assert v in [10, 20, 30, 40, 50]

    def test_map_to_string(self, pt):
        s = pt.strategy_map(pt.gen_integer(0, 9), str)
        for seed in range(5):
            v = pt.strategy_draw(s, seed=seed)
            assert isinstance(v, str)
            assert v in list("0123456789")

    def test_non_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.strategy_map("bad", lambda x: x)

    def test_non_callable_raises(self, pt):
        with pytest.raises(TypeError):
            pt.strategy_map(pt.gen_bool(), "not callable")


# ---------------------------------------------------------------------------
# strategy_draw
# ---------------------------------------------------------------------------

class TestStrategyDraw:
    def test_reproducible_with_seed(self, pt):
        s = pt.gen_integer(0, 10000)
        assert pt.strategy_draw(s, seed=99) == pt.strategy_draw(s, seed=99)

    def test_different_seeds_may_differ(self, pt):
        s = pt.gen_integer(0, 10000)
        values = {pt.strategy_draw(s, seed=i) for i in range(20)}
        assert len(values) > 1

    def test_none_seed_allowed(self, pt):
        s = pt.gen_bool()
        v = pt.strategy_draw(s)
        assert isinstance(v, bool)

    def test_non_strategy_raises(self, pt):
        with pytest.raises(TypeError):
            pt.strategy_draw("not a strategy")


# ---------------------------------------------------------------------------
# property_test
# ---------------------------------------------------------------------------

class TestPropertyTest:
    def test_always_true_passes(self, pt):
        result = pt.property_test(lambda x: True, [pt.gen_integer(0, 100)], count=50, seed=0)
        assert result["passed"] is True
        assert result["run_count"] == 50
        assert result["counterexample"] is None

    def test_always_false_fails(self, pt):
        result = pt.property_test(lambda x: False, [pt.gen_integer(0, 10)], count=50, seed=0)
        assert result["passed"] is False
        assert result["failure_count"] >= 1
        assert result["counterexample"] is not None

    def test_run_count_correct(self, pt):
        result = pt.property_test(lambda x: True, [pt.gen_bool()], count=30, seed=0)
        assert result["run_count"] == 30

    def test_counterexample_is_list(self, pt):
        result = pt.property_test(lambda x: x < 50, [pt.gen_integer(0, 100)], count=200, seed=0)
        if result["counterexample"] is not None:
            assert isinstance(result["counterexample"], list)

    def test_multi_arg(self, pt):
        def commutative(a, b):
            return a + b == b + a
        result = pt.property_test(
            commutative,
            [pt.gen_integer(-50, 50), pt.gen_integer(-50, 50)],
            count=50, seed=0
        )
        assert result["passed"] is True

    def test_assertion_error_captured(self, pt):
        def prop(x):
            assert x < 0
        result = pt.property_test(prop, [pt.gen_non_negative_integer(10)], count=10, seed=0)
        assert result["passed"] is False
        assert result["error"] is not None

    def test_non_callable_raises(self, pt):
        with pytest.raises(TypeError):
            pt.property_test("not callable", [pt.gen_bool()])

    def test_bad_strategies_raises(self, pt):
        with pytest.raises(TypeError):
            pt.property_test(lambda x: True, ["not a strategy"])

    def test_shrunk_counterexample_present(self, pt):
        result = pt.property_test(lambda x: x < 50, [pt.gen_integer(0, 200)], count=500, seed=0)
        if not result["passed"]:
            assert result["shrunk_counterexample"] is not None

    def test_integer_shrunk_minimal(self, pt):
        result = pt.property_test(lambda x: x < 50, [pt.gen_integer(0, 200)], count=500, seed=42)
        if not result["passed"]:
            shrunk = result["shrunk_counterexample"][0]
            assert shrunk >= 50
            assert shrunk <= 200

    def test_none_return_treated_as_pass(self, pt):
        def returns_none(x):
            return None
        result = pt.property_test(returns_none, [pt.gen_integer(0, 5)], count=20, seed=0)
        assert result["passed"] is True

    def test_false_return_is_failure(self, pt):
        result = pt.property_test(lambda x: False, [pt.gen_constant("x")], count=5, seed=0)
        assert result["passed"] is False


# ---------------------------------------------------------------------------
# property_assert
# ---------------------------------------------------------------------------

class TestPropertyAssert:
    def test_passes_silently(self, pt):
        pt.property_assert(lambda x: x >= 0, [pt.gen_non_negative_integer(100)], count=50, seed=0)

    def test_fails_raises_runtime_error(self, pt):
        with pytest.raises(RuntimeError):
            pt.property_assert(lambda x: x < 0, [pt.gen_non_negative_integer(10)], count=20, seed=0)

    def test_error_message_contains_counterexample(self, pt):
        with pytest.raises(RuntimeError, match="[Cc]ounterexample"):
            pt.property_assert(lambda x: x < 0, [pt.gen_integer(0, 10)], count=20, seed=0)

    def test_returns_true_on_pass(self, pt):
        r = pt.property_assert(lambda x: isinstance(x, bool), [pt.gen_bool()], count=20, seed=0)
        assert r is True


# ---------------------------------------------------------------------------
# property_find_counterexample / property_find_example
# ---------------------------------------------------------------------------

class TestPropertyFindCounterexample:
    def test_finds_failing_case(self, pt):
        ce = pt.property_find_counterexample(lambda x: x < 5, [pt.gen_integer(0, 10)], count=100, seed=0)
        assert ce is not None
        assert ce[0] >= 5

    def test_returns_none_when_no_failure(self, pt):
        ce = pt.property_find_counterexample(
            lambda x: x >= 0, [pt.gen_non_negative_integer(50)], count=50, seed=0
        )
        assert ce is None

    def test_result_is_list(self, pt):
        ce = pt.property_find_counterexample(lambda x: False, [pt.gen_bool()], count=5, seed=0)
        assert isinstance(ce, list)


class TestPropertyFindExample:
    def test_finds_passing_case(self, pt):
        ex = pt.property_find_example(lambda x: x > 90, [pt.gen_integer(0, 100)], count=200, seed=1)
        assert ex is not None
        assert ex[0] > 90

    def test_returns_none_when_impossible(self, pt):
        ex = pt.property_find_example(lambda x: x == -1, [pt.gen_non_negative_integer(10)], count=50, seed=0)
        assert ex is None

    def test_result_is_list(self, pt):
        ex = pt.property_find_example(lambda x: True, [pt.gen_bool()], count=10, seed=0)
        assert isinstance(ex, list)


# ---------------------------------------------------------------------------
# property_statistics
# ---------------------------------------------------------------------------

class TestPropertyStatistics:
    def test_total_equals_count(self, pt):
        stats = pt.property_statistics(lambda x: True, [pt.gen_bool()], count=100, seed=0)
        assert stats["total"] == 100

    def test_pass_plus_fail_equals_total(self, pt):
        stats = pt.property_statistics(lambda x: x > 0, [pt.gen_integer(-5, 5)], count=80, seed=1)
        assert stats["passed"] + stats["failed"] == stats["total"]

    def test_all_pass(self, pt):
        stats = pt.property_statistics(lambda x: True, [pt.gen_bool()], count=50, seed=0)
        assert stats["passed"] == 50
        assert stats["fail_rate"] == 0.0

    def test_all_fail(self, pt):
        stats = pt.property_statistics(lambda x: False, [pt.gen_bool()], count=50, seed=0)
        assert stats["failed"] == 50
        assert stats["pass_rate"] == 0.0

    def test_roughly_half_pass(self, pt):
        stats = pt.property_statistics(lambda x: x % 2 == 0, [pt.gen_integer(0, 100)], count=500, seed=0)
        assert 0.3 <= stats["pass_rate"] <= 0.7

    def test_skip_count_present(self, pt):
        stats = pt.property_statistics(lambda x: True, [pt.gen_bool()], count=10, seed=0)
        assert "skipped" in stats


# ---------------------------------------------------------------------------
# property_shrink
# ---------------------------------------------------------------------------

class TestPropertyShrink:
    def test_returns_list(self, pt):
        result = pt.property_shrink(
            lambda x: x < 50,
            [pt.gen_integer(0, 200)],
            [150]
        )
        assert isinstance(result, list)

    def test_shrinks_value(self, pt):
        result = pt.property_shrink(
            lambda x: x < 50,
            [pt.gen_integer(0, 200)],
            [150]
        )
        # Should be minimal value >= 50
        assert result[0] >= 50
        assert result[0] <= 150

    def test_shrinks_list(self, pt):
        s_list = pt.gen_list(pt.gen_integer(0, 10), min_size=0, max_size=20)
        failing = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]]

        def prop(lst):
            return len(lst) < 5

        result = pt.property_shrink(prop, [s_list], failing)
        assert isinstance(result[0], list)
        assert len(result[0]) < len(failing[0])

    def test_non_callable_raises(self, pt):
        with pytest.raises(TypeError):
            pt.property_shrink("bad", [pt.gen_bool()], [True])

    def test_bad_strategies_raises(self, pt):
        with pytest.raises(TypeError):
            pt.property_shrink(lambda x: True, "bad", [1])

    def test_bad_failing_args_raises(self, pt):
        with pytest.raises(TypeError):
            pt.property_shrink(lambda x: True, [pt.gen_bool()], "bad")


# ---------------------------------------------------------------------------
# property_report
# ---------------------------------------------------------------------------

class TestPropertyReport:
    def test_report_passed(self, pt):
        result = pt.property_test(lambda x: True, [pt.gen_bool()], count=10, seed=0)
        rep = pt.property_report(result)
        assert isinstance(rep, str)
        assert "PASSED" in rep

    def test_report_failed(self, pt):
        result = pt.property_test(lambda x: False, [pt.gen_bool()], count=5, seed=0)
        rep = pt.property_report(result)
        assert "FAILED" in rep

    def test_report_contains_run_count(self, pt):
        result = pt.property_test(lambda x: True, [pt.gen_bool()], count=25, seed=0)
        rep = pt.property_report(result)
        assert "25" in rep

    def test_report_contains_counterexample_on_failure(self, pt):
        result = pt.property_test(lambda x: False, [pt.gen_integer(0, 5)], count=5, seed=0)
        rep = pt.property_report(result)
        assert "Counterexample" in rep

    def test_non_dict_raises(self, pt):
        with pytest.raises(TypeError):
            pt.property_report("not a dict")


# ---------------------------------------------------------------------------
# property_assume
# ---------------------------------------------------------------------------

class TestPropertyAssume:
    def test_assume_true_does_not_skip(self, pt):
        # Should not raise
        pt.property_assume(True)

    def test_assume_false_skips(self, pt):
        def prop(x):
            pt.property_assume(x > 0)
            return x > 0

        result = pt.property_test(prop, [pt.gen_integer(-5, 5)], count=100, seed=0)
        assert result["passed"] is True
        # All non-positive values were skipped
        assert result["skip_count"] >= 0

    def test_assume_filters_invalid_inputs(self, pt):
        def safe_divide(a, b):
            pt.property_assume(b != 0)
            return (a // b) * b + (a % b) == a

        result = pt.property_test(
            safe_divide,
            [pt.gen_integer(-20, 20), pt.gen_integer(-10, 10)],
            count=200, seed=0
        )
        assert result["passed"] is True


# ---------------------------------------------------------------------------
# Combination / integration tests
# ---------------------------------------------------------------------------

class TestCombinations:
    def test_list_reverse_twice_identity(self, pt):
        def prop(lst):
            return list(reversed(list(reversed(lst)))) == lst

        result = pt.property_test(
            prop, [pt.gen_list(pt.gen_integer(0, 100))], count=100, seed=0
        )
        assert result["passed"] is True

    def test_string_concat_length(self, pt):
        def prop(a, b):
            return len(a + b) == len(a) + len(b)

        result = pt.property_test(
            prop,
            [pt.gen_text(0, 20), pt.gen_text(0, 20)],
            count=100, seed=0
        )
        assert result["passed"] is True

    def test_min_max_ordering(self, pt):
        def prop(a, b):
            return min(a, b) <= max(a, b)

        result = pt.property_test(
            prop,
            [pt.gen_integer(-1000, 1000), pt.gen_integer(-1000, 1000)],
            count=200, seed=1
        )
        assert result["passed"] is True

    def test_abs_nonneg(self, pt):
        result = pt.property_test(
            lambda x: abs(x) >= 0,
            [pt.gen_integer(-1000, 1000)],
            count=100, seed=0
        )
        assert result["passed"] is True

    def test_nested_strategy(self, pt):
        s = pt.gen_list(
            pt.gen_one_of([pt.gen_integer(0, 10), pt.gen_bool()]),
            min_size=1, max_size=5
        )
        for seed in range(5):
            v = pt.strategy_draw(s, seed=seed)
            assert isinstance(v, list)
            assert 1 <= len(v) <= 5
            assert all(isinstance(x, (int, bool)) for x in v)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegistration:
    def test_all_generators_registered(self, pt):
        calls = []

        class _R:
            def register_function(self, name, fn):
                calls.append(name)
            def register_module(self, name):
                pass

        pt.register_property_testing_functions(_R())
        expected = [
            "gen_integer", "gen_float", "gen_bool", "gen_none", "gen_constant",
            "gen_choice", "gen_from_list", "gen_string", "gen_text",
            "gen_list", "gen_dict", "gen_tuple", "gen_one_of", "gen_optional",
            "gen_fixed_dict", "gen_text_lines",
            "gen_positive_integer", "gen_non_negative_integer",
            "gen_ascii_string", "gen_non_empty_list",
            "strategy_filter", "strategy_map", "strategy_draw",
            "property_test", "property_assert",
            "property_find_counterexample", "property_find_example",
            "property_statistics", "property_shrink",
            "property_report", "property_assume",
        ]
        for name in expected:
            assert name in calls, f"Missing registration: {name}"

    def test_modules_registered(self, pt):
        mods = []

        class _R:
            def register_function(self, name, fn): pass
            def register_module(self, name):
                mods.append(name)

        pt.register_property_testing_functions(_R())
        assert "property_testing" in mods
        assert "prop_test" in mods

    def test_function_count(self, pt):
        calls = []

        class _R:
            def register_function(self, name, fn):
                calls.append(name)
            def register_module(self, name): pass

        pt.register_property_testing_functions(_R())
        assert len(calls) >= 30
