"""Tests for the result_utils stdlib module.

Covers all 38 registered functions:
  Result constructors  : result_ok, result_err
  Result predicates    : result_is_ok, result_is_err
  Result extraction    : result_unwrap, result_unwrap_err, result_unwrap_or,
                         result_unwrap_or_else, result_expect
  Result transforms    : result_map, result_map_err, result_and_then,
                         result_or_else
  Result conversions   : result_to_option, result_ok_or
  Option constructors  : option_some, option_none
  Option predicates    : option_is_some, option_is_none
  Option extraction    : option_unwrap, option_unwrap_or, option_unwrap_or_else,
                         option_expect
  Option transforms    : option_map, option_and_then, option_or, option_or_else,
                         option_filter, option_flatten
  Option conversions   : option_to_result
  Error chain          : error_new, error_wrap, error_chain, error_root,
                         error_message, error_code, error_context, error_causes
"""

import importlib.util
import os
import sys
import types

import pytest

# ---------------------------------------------------------------------------
# Fixture: load result_utils in isolation
# ---------------------------------------------------------------------------

_HERE = os.path.dirname(os.path.abspath(__file__))
_RU_INIT = os.path.join(
    _HERE, "..", "..", "..", "src", "nlpl", "stdlib", "result_utils", "__init__.py"
)


@pytest.fixture(scope="module")
def ru():
    """Return the result_utils module, loaded in isolation."""
    _pkgs = (
        "nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
        "nexuslang.stdlib", "nexuslang.stdlib.result_utils",
    )
    # Save originals so we can restore after tests
    _originals = {pkg: sys.modules.get(pkg) for pkg in _pkgs}
    _had_runtime_cls = hasattr(sys.modules.get("nexuslang.runtime.runtime", object()), "Runtime")
    _orig_runtime_cls = getattr(sys.modules.get("nexuslang.runtime.runtime"), "Runtime", None) if _had_runtime_cls else None

    class _StubRuntime:
        def register_function(self, name, fn):
            pass
        def register_module(self, name):
            pass

    for pkg in _pkgs:
        if pkg not in sys.modules:
            sys.modules[pkg] = types.ModuleType(pkg)

    sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime

    spec = importlib.util.spec_from_file_location(
        "nexuslang.stdlib.result_utils", os.path.abspath(_RU_INIT)
    )
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
# Shared factories (functions, not fixtures, so they can be called freely)
# ---------------------------------------------------------------------------

def _ok(ru, v=42):
    return ru.result_ok(v)


def _err(ru, e="oops"):
    return ru.result_err(e)


def _some(ru, v="hello"):
    return ru.option_some(v)


def _none(ru):
    return ru.option_none()


# ===========================================================================
# result_ok / result_err
# ===========================================================================


class TestResultConstructors:
    def test_ok_tag(self, ru):
        assert _ok(ru)["tag"] == "ok"

    def test_ok_value(self, ru):
        assert _ok(ru, 99)["value"] == 99

    def test_ok_none_value(self, ru):
        assert ru.result_ok(None)["value"] is None

    def test_ok_list_value(self, ru):
        r = ru.result_ok([1, 2, 3])
        assert r["value"] == [1, 2, 3]

    def test_err_tag(self, ru):
        assert _err(ru)["tag"] == "err"

    def test_err_error_string(self, ru):
        assert _err(ru, "bad")["error"] == "bad"

    def test_err_error_dict(self, ru):
        e = {"message": "disk full", "code": "E_IO", "context": {}, "cause": None}
        r = ru.result_err(e)
        assert r["error"]["message"] == "disk full"

    def test_ok_dict_is_mutable(self, ru):
        r = _ok(ru, 1)
        r["extra"] = True
        assert r["extra"] is True


# ===========================================================================
# result_is_ok / result_is_err
# ===========================================================================


class TestResultPredicates:
    def test_is_ok_true(self, ru):
        assert ru.result_is_ok(_ok(ru)) is True

    def test_is_ok_false(self, ru):
        assert ru.result_is_ok(_err(ru)) is False

    def test_is_err_true(self, ru):
        assert ru.result_is_err(_err(ru)) is True

    def test_is_err_false(self, ru):
        assert ru.result_is_err(_ok(ru)) is False

    def test_invalid_input_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_is_ok({"tag": "unknown"})

    def test_non_dict_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_is_err("not a result")


# ===========================================================================
# result_unwrap
# ===========================================================================


class TestResultUnwrap:
    def test_unwrap_ok(self, ru):
        assert ru.result_unwrap(_ok(ru, 7)) == 7

    def test_unwrap_err_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_unwrap(_err(ru))

    def test_unwrap_err_message_included(self, ru):
        with pytest.raises(RuntimeError, match="oops"):
            ru.result_unwrap(_err(ru, "oops"))

    def test_unwrap_err_dict_error(self, ru):
        e = ru.error_new("disk full", "E_IO")
        with pytest.raises(RuntimeError, match="disk full"):
            ru.result_unwrap(ru.result_err(e))


class TestResultUnwrapErr:
    def test_unwrap_err_ok_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_unwrap_err(_ok(ru))

    def test_unwrap_err_string(self, ru):
        assert ru.result_unwrap_err(_err(ru, "bad")) == "bad"

    def test_unwrap_err_dict(self, ru):
        e = ru.error_new("x")
        r = ru.result_err(e)
        assert ru.result_unwrap_err(r)["message"] == "x"


class TestResultUnwrapOr:
    def test_ok_returns_value(self, ru):
        assert ru.result_unwrap_or(_ok(ru, 3), 99) == 3

    def test_err_returns_default(self, ru):
        assert ru.result_unwrap_or(_err(ru), 99) == 99

    def test_default_can_be_none(self, ru):
        assert ru.result_unwrap_or(_err(ru), None) is None


class TestResultUnwrapOrElse:
    def test_ok_returns_value(self, ru):
        assert ru.result_unwrap_or_else(_ok(ru, 5), lambda: 0) == 5

    def test_err_calls_fn(self, ru):
        assert ru.result_unwrap_or_else(_err(ru), lambda: 77) == 77

    def test_fn_not_called_for_ok(self, ru):
        calls = []
        ru.result_unwrap_or_else(_ok(ru), lambda: calls.append(1) or 0)
        assert calls == []


class TestResultExpect:
    def test_ok_returns_value(self, ru):
        assert ru.result_expect(_ok(ru, "x"), "msg") == "x"

    def test_err_raises_with_msg(self, ru):
        with pytest.raises(RuntimeError, match="custom message"):
            ru.result_expect(_err(ru), "custom message")


# ===========================================================================
# result_map / result_map_err
# ===========================================================================


class TestResultMap:
    def test_ok_applies_fn(self, ru):
        r = ru.result_map(_ok(ru, 3), lambda v: v * 10)
        assert r["tag"] == "ok" and r["value"] == 30

    def test_err_passthrough(self, ru):
        r = ru.result_map(_err(ru, "e"), lambda v: v * 10)
        assert r["tag"] == "err" and r["error"] == "e"

    def test_fn_receives_value(self, ru):
        received = []
        ru.result_map(_ok(ru, "hi"), lambda v: received.append(v) or v)
        assert received == ["hi"]

    def test_ok_returns_new_dict(self, ru):
        original = _ok(ru, 1)
        mapped = ru.result_map(original, lambda v: v + 1)
        assert mapped is not original


class TestResultMapErr:
    def test_err_applies_fn(self, ru):
        r = ru.result_map_err(_err(ru, "bad"), lambda e: f"wrapped:{e}")
        assert "wrapped:bad" in r["error"]

    def test_ok_passthrough(self, ru):
        r = ru.result_map_err(_ok(ru, 5), lambda e: "x")
        assert r["tag"] == "ok" and r["value"] == 5

    def test_fn_not_called_for_ok(self, ru):
        calls = []
        ru.result_map_err(_ok(ru), lambda e: calls.append(e))
        assert calls == []


# ===========================================================================
# result_and_then / result_or_else
# ===========================================================================


class TestResultAndThen:
    def test_ok_calls_fn(self, ru):
        r = ru.result_and_then(_ok(ru, 3), lambda v: ru.result_ok(v + 1))
        assert r["value"] == 4

    def test_err_passthrough(self, ru):
        r = ru.result_and_then(_err(ru, "e"), lambda v: ru.result_ok(v))
        assert r["tag"] == "err"

    def test_ok_fn_returns_err(self, ru):
        r = ru.result_and_then(_ok(ru, 0), lambda v: ru.result_err("rejected"))
        assert r["tag"] == "err" and r["error"] == "rejected"

    def test_fn_must_return_result(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_and_then(_ok(ru, 1), lambda v: 42)


class TestResultOrElse:
    def test_ok_passthrough(self, ru):
        r = ru.result_or_else(_ok(ru, 7), lambda e: ru.result_ok(0))
        assert r["value"] == 7

    def test_err_calls_fn(self, ru):
        r = ru.result_or_else(_err(ru, "e"), lambda e: ru.result_ok(99))
        assert r["value"] == 99

    def test_err_fn_can_return_err(self, ru):
        r = ru.result_or_else(_err(ru, "a"), lambda e: ru.result_err("b"))
        assert r["error"] == "b"

    def test_fn_must_return_result(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_or_else(_err(ru), lambda e: "not a result")


# ===========================================================================
# result_to_option / result_ok_or
# ===========================================================================


class TestResultToOption:
    def test_ok_becomes_some(self, ru):
        o = ru.result_to_option(_ok(ru, 5))
        assert o["tag"] == "some" and o["value"] == 5

    def test_err_becomes_none(self, ru):
        o = ru.result_to_option(_err(ru))
        assert o["tag"] == "none"


class TestResultOkOr:
    def test_some_becomes_ok(self, ru):
        r = ru.result_ok_or(_some(ru, "x"), "e")
        assert r["tag"] == "ok" and r["value"] == "x"

    def test_none_becomes_err(self, ru):
        r = ru.result_ok_or(_none(ru), "err_val")
        assert r["tag"] == "err" and r["error"] == "err_val"

    def test_invalid_input_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.result_ok_or({"tag": "ok"}, "e")  # looks like Result, not Option


# ===========================================================================
# option_some / option_none
# ===========================================================================


class TestOptionConstructors:
    def test_some_tag(self, ru):
        assert _some(ru)["tag"] == "some"

    def test_some_value(self, ru):
        assert ru.option_some(42)["value"] == 42

    def test_some_value_none(self, ru):
        assert ru.option_some(None)["value"] is None

    def test_some_nested(self, ru):
        inner = ru.option_some(1)
        outer = ru.option_some(inner)
        assert outer["value"]["tag"] == "some"

    def test_none_tag(self, ru):
        assert _none(ru)["tag"] == "none"

    def test_none_has_no_value_key(self, ru):
        assert "value" not in _none(ru)


# ===========================================================================
# option_is_some / option_is_none
# ===========================================================================


class TestOptionPredicates:
    def test_is_some_true(self, ru):
        assert ru.option_is_some(_some(ru)) is True

    def test_is_some_false(self, ru):
        assert ru.option_is_some(_none(ru)) is False

    def test_is_none_true(self, ru):
        assert ru.option_is_none(_none(ru)) is True

    def test_is_none_false(self, ru):
        assert ru.option_is_none(_some(ru)) is False

    def test_invalid_input_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.option_is_some({"tag": "ok"})


# ===========================================================================
# option_unwrap variants
# ===========================================================================


class TestOptionUnwrap:
    def test_unwrap_some(self, ru):
        assert ru.option_unwrap(_some(ru, 9)) == 9

    def test_unwrap_none_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.option_unwrap(_none(ru))


class TestOptionUnwrapOr:
    def test_some_returns_value(self, ru):
        assert ru.option_unwrap_or(_some(ru, 3), 0) == 3

    def test_none_returns_default(self, ru):
        assert ru.option_unwrap_or(_none(ru), 0) == 0

    def test_default_zero(self, ru):
        assert ru.option_unwrap_or(_none(ru), 0) == 0


class TestOptionUnwrapOrElse:
    def test_some_returns_value(self, ru):
        assert ru.option_unwrap_or_else(_some(ru, "x"), lambda: "y") == "x"

    def test_none_calls_fn(self, ru):
        assert ru.option_unwrap_or_else(_none(ru), lambda: "y") == "y"

    def test_fn_not_called_for_some(self, ru):
        calls = []
        ru.option_unwrap_or_else(_some(ru), lambda: calls.append(1) or "")
        assert calls == []


class TestOptionExpect:
    def test_some_returns_value(self, ru):
        assert ru.option_expect(_some(ru, "v"), "gone") == "v"

    def test_none_raises_with_msg(self, ru):
        with pytest.raises(RuntimeError, match="not found"):
            ru.option_expect(_none(ru), "not found")


# ===========================================================================
# option_map / option_and_then
# ===========================================================================


class TestOptionMap:
    def test_some_applies_fn(self, ru):
        o = ru.option_map(_some(ru, "hi"), str.upper)
        assert o["value"] == "HI"

    def test_none_passthrough(self, ru):
        o = ru.option_map(_none(ru), str.upper)
        assert o["tag"] == "none"

    def test_returns_new_dict(self, ru):
        original = _some(ru, 1)
        mapped = ru.option_map(original, lambda v: v + 1)
        assert mapped is not original


class TestOptionAndThen:
    def test_some_flatmaps(self, ru):
        o = ru.option_and_then(_some(ru, 3), lambda v: ru.option_some(v * 2))
        assert o["value"] == 6

    def test_none_passthrough(self, ru):
        o = ru.option_and_then(_none(ru), lambda v: ru.option_some(v))
        assert o["tag"] == "none"

    def test_some_fn_returns_none(self, ru):
        o = ru.option_and_then(_some(ru, 0), lambda v: _none(ru))
        assert o["tag"] == "none"

    def test_fn_must_return_option(self, ru):
        with pytest.raises(RuntimeError):
            ru.option_and_then(_some(ru, 1), lambda v: 42)


# ===========================================================================
# option_or / option_or_else
# ===========================================================================


class TestOptionOr:
    def test_some_returns_self(self, ru):
        a = _some(ru, "a")
        b = _some(ru, "b")
        assert ru.option_or(a, b)["value"] == "a"

    def test_none_returns_other(self, ru):
        b = _some(ru, "b")
        assert ru.option_or(_none(ru), b)["value"] == "b"

    def test_none_or_none(self, ru):
        assert ru.option_or(_none(ru), _none(ru))["tag"] == "none"


class TestOptionOrElse:
    def test_some_returns_self(self, ru):
        s = _some(ru, "x")
        assert ru.option_or_else(s, lambda: _some(ru, "y"))["value"] == "x"

    def test_none_calls_fn(self, ru):
        assert ru.option_or_else(_none(ru), lambda: _some(ru, "y"))["value"] == "y"

    def test_fn_not_called_for_some(self, ru):
        calls = []
        ru.option_or_else(_some(ru), lambda: calls.append(1) or _none(ru))
        assert calls == []

    def test_fn_must_return_option(self, ru):
        with pytest.raises(RuntimeError):
            ru.option_or_else(_none(ru), lambda: "not an option")


# ===========================================================================
# option_filter / option_flatten
# ===========================================================================


class TestOptionFilter:
    def test_pass_predicate(self, ru):
        o = ru.option_filter(_some(ru, 5), lambda v: v > 0)
        assert o["tag"] == "some" and o["value"] == 5

    def test_fail_predicate_returns_none(self, ru):
        o = ru.option_filter(_some(ru, -1), lambda v: v > 0)
        assert o["tag"] == "none"

    def test_none_passthrough(self, ru):
        o = ru.option_filter(_none(ru), lambda v: True)
        assert o["tag"] == "none"

    def test_truthy_falsy(self, ru):
        o = ru.option_filter(_some(ru, 0), lambda v: v)
        assert o["tag"] == "none"


class TestOptionFlatten:
    def test_some_some_to_some(self, ru):
        inner = ru.option_some(7)
        outer = ru.option_some(inner)
        assert ru.option_flatten(outer)["value"] == 7

    def test_some_none_to_none(self, ru):
        outer = ru.option_some(_none(ru))
        assert ru.option_flatten(outer)["tag"] == "none"

    def test_none_to_none(self, ru):
        assert ru.option_flatten(_none(ru))["tag"] == "none"

    def test_inner_not_option_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.option_flatten(ru.option_some(42))  # inner is int, not Option


# ===========================================================================
# option_to_result
# ===========================================================================


class TestOptionToResult:
    def test_some_becomes_ok(self, ru):
        r = ru.option_to_result(_some(ru, "v"), "e")
        assert r["tag"] == "ok" and r["value"] == "v"

    def test_none_becomes_err(self, ru):
        r = ru.option_to_result(_none(ru), "missing")
        assert r["tag"] == "err" and r["error"] == "missing"

    def test_error_can_be_dict(self, ru):
        e = ru.error_new("not found")
        r = ru.option_to_result(_none(ru), e)
        assert r["error"]["message"] == "not found"


# ===========================================================================
# error_new
# ===========================================================================


class TestErrorNew:
    def test_message(self, ru):
        e = ru.error_new("disk full")
        assert e["message"] == "disk full"

    def test_default_code_none(self, ru):
        e = ru.error_new("x")
        assert e["code"] is None

    def test_code_set(self, ru):
        e = ru.error_new("x", code="E_IO")
        assert e["code"] == "E_IO"

    def test_context_default_empty(self, ru):
        e = ru.error_new("x")
        assert e["context"] == {}

    def test_context_set(self, ru):
        e = ru.error_new("x", context={"path": "/tmp"})
        assert e["context"]["path"] == "/tmp"

    def test_cause_none(self, ru):
        assert ru.error_new("x")["cause"] is None

    def test_message_coerced_to_str(self, ru):
        e = ru.error_new(404)
        assert e["message"] == "404"


# ===========================================================================
# error_wrap
# ===========================================================================


class TestErrorWrap:
    def test_message(self, ru):
        inner = ru.error_new("inner")
        w = ru.error_wrap(inner, "outer")
        assert w["message"] == "outer"

    def test_cause_is_inner(self, ru):
        inner = ru.error_new("inner")
        w = ru.error_wrap(inner, "outer")
        assert w["cause"]["message"] == "inner"

    def test_code_is_none(self, ru):
        inner = ru.error_new("x")
        assert ru.error_wrap(inner, "y")["code"] is None

    def test_wraps_plain_string(self, ru):
        w = ru.error_wrap("plain", "outer")
        assert w["cause"]["message"] == "plain"

    def test_double_wrap(self, ru):
        e1 = ru.error_new("root")
        e2 = ru.error_wrap(e1, "mid")
        e3 = ru.error_wrap(e2, "top")
        assert e3["cause"]["cause"]["message"] == "root"


# ===========================================================================
# error_chain
# ===========================================================================


class TestErrorChain:
    def test_single_element(self, ru):
        c = ru.error_chain(["only"])
        assert c["message"] == "only" and c["cause"] is None

    def test_three_elements_order(self, ru):
        c = ru.error_chain(["top", "mid", "root"])
        assert c["message"] == "top"
        assert c["cause"]["message"] == "mid"
        assert c["cause"]["cause"]["message"] == "root"

    def test_root_has_no_cause(self, ru):
        c = ru.error_chain(["a", "b"])
        assert c["cause"]["cause"] is None

    def test_accepts_dicts(self, ru):
        e = ru.error_new("structured", code="E_X")
        c = ru.error_chain([e, "plain"])
        assert c["code"] == "E_X"

    def test_empty_list_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.error_chain([])


# ===========================================================================
# error_root
# ===========================================================================


class TestErrorRoot:
    def test_single_is_root(self, ru):
        e = ru.error_new("only")
        assert ru.error_root(e)["message"] == "only"

    def test_finds_root(self, ru):
        c = ru.error_chain(["top", "mid", "root"])
        assert ru.error_root(c)["message"] == "root"

    def test_invalid_input_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.error_root("not an error")


# ===========================================================================
# error_message / error_code / error_context / error_causes
# ===========================================================================


class TestErrorAccessors:
    def test_error_message_dict(self, ru):
        e = ru.error_new("hello")
        assert ru.error_message(e) == "hello"

    def test_error_message_string(self, ru):
        assert ru.error_message("plain string") == "plain string"

    def test_error_message_non_dict(self, ru):
        assert ru.error_message(42) == "42"

    def test_error_code_present(self, ru):
        e = ru.error_new("x", code="E_FAIL")
        assert ru.error_code(e) == "E_FAIL"

    def test_error_code_none(self, ru):
        e = ru.error_new("x")
        assert ru.error_code(e) is None

    def test_error_code_non_dict(self, ru):
        assert ru.error_code("string") is None

    def test_error_context_present(self, ru):
        e = ru.error_new("x", context={"file": "f.txt", "line": 42})
        ctx = ru.error_context(e)
        assert ctx["file"] == "f.txt" and ctx["line"] == 42

    def test_error_context_empty(self, ru):
        e = ru.error_new("x")
        assert ru.error_context(e) == {}

    def test_error_context_non_dict(self, ru):
        assert ru.error_context("string") == {}


class TestErrorCauses:
    def test_single_error(self, ru):
        e = ru.error_new("only")
        c = ru.error_causes(e)
        assert len(c) == 1 and c[0]["message"] == "only"

    def test_chain_order(self, ru):
        c = ru.error_chain(["a", "b", "c"])
        causes = ru.error_causes(c)
        assert [x["message"] for x in causes] == ["a", "b", "c"]

    def test_invalid_input_raises(self, ru):
        with pytest.raises(RuntimeError):
            ru.error_causes("not an error")


# ===========================================================================
# Round-trip: Result ↔ Option ↔ error chain
# ===========================================================================


class TestRoundTrips:
    def test_ok_to_option_and_back(self, ru):
        original = ru.result_ok("data")
        opt = ru.result_to_option(original)
        back = ru.result_ok_or(opt, "lost")
        assert back["value"] == "data"

    def test_err_to_option_and_back_as_err(self, ru):
        original = ru.result_err("fail")
        opt = ru.result_to_option(original)
        back = ru.result_ok_or(opt, "fallback")
        assert back["error"] == "fallback"

    def test_option_none_to_result_err_chain(self, ru):
        e = ru.error_new("not found", code="E_404")
        r = ru.option_to_result(_none(ru), e)
        wrapped = ru.result_err(ru.error_wrap(ru.result_unwrap_err(r), "request failed"))
        assert ru.error_root(ru.result_unwrap_err(wrapped))["code"] == "E_404"

    def test_map_chain_ok(self, ru):
        r = ru.result_ok(2)
        r = ru.result_map(r, lambda v: v * 3)
        r = ru.result_map(r, lambda v: v + 1)
        assert ru.result_unwrap(r) == 7

    def test_and_then_chain_short_circuits(self, ru):
        r = ru.result_ok(10)
        r = ru.result_and_then(r, lambda v: ru.result_err("stop"))
        r = ru.result_and_then(r, lambda v: ru.result_ok(v + 1))
        assert r["tag"] == "err" and r["error"] == "stop"


# ===========================================================================
# Registration
# ===========================================================================


class TestRegistration:
    def test_count(self, ru):
        names = []

        class _M:
            def register_function(self, name, fn):
                names.append(name)
            def register_module(self, name):
                pass

        ru.register_result_utils_functions(_M())
        assert len(names) == 38

    def test_no_duplicates(self, ru):
        names = []

        class _M:
            def register_function(self, name, fn):
                names.append(name)
            def register_module(self, name):
                pass

        ru.register_result_utils_functions(_M())
        assert len(names) == len(set(names))

    def test_all_result_and_option_and_error_prefix(self, ru):
        names = []

        class _M:
            def register_function(self, name, fn):
                names.append(name)
            def register_module(self, name):
                pass

        ru.register_result_utils_functions(_M())
        unexpected = [
            n for n in names
            if not (n.startswith("result_") or n.startswith("option_") or n.startswith("error_"))
        ]
        assert unexpected == []
