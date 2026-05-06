"""
Tests for dynamic invocation in the NexusLang stdlib reflection module.

Groups:
    TestReflectInvoke       -- reflect_invoke: call a named method on an Object
    TestReflectInvokeSafe   -- reflect_invoke_safe: fallback on failure
    TestReflectCall         -- reflect_call: call any registered runtime function by name
    TestDynamicInvokeReg    -- registration check for all three functions
"""

import sys
import os

_PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "..")
)
if _PROJECT_ROOT not in sys.path:
    sys.path.insert(0, _PROJECT_ROOT)

import pytest

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_runtime():
    from nexuslang.runtime.runtime import Runtime
    from nexuslang.stdlib import register_stdlib
    rt = Runtime()
    register_stdlib(rt)
    return rt


def _make_object(class_name, properties=None, methods=None):
    from nexuslang.runtime.runtime import Object
    obj = Object(class_name, properties or {})
    if methods:
        for name, func in methods.items():
            obj.add_method(name, func)
    return obj


# Simple method callables (obj is the self/receiver, ignored for pure logic)
def _greet(self, name="World"):
    return f"Hello, {name}!"


def _add(self, a, b):
    return a + b


def _get_x(self):
    return self.properties.get("x", 0)


def _set_x(self, value):
    self.properties["x"] = value
    return True


def _raises(self):
    raise ValueError("intentional error")


def _returns_none(self):
    return None


def _returns_list(self, n):
    return list(range(n))


# ---------------------------------------------------------------------------
# 1. reflect_invoke
# ---------------------------------------------------------------------------

class TestReflectInvoke:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_invoke
        self.rt = _make_runtime()
        self.reflect_invoke = lambda obj, name, args=None: reflect_invoke(
            self.rt, obj, name, args
        )
        self.obj = _make_object(
            "Calculator",
            properties={"x": 10},
            methods={
                "greet": _greet,
                "add": _add,
                "get_x": _get_x,
                "set_x": _set_x,
                "raises": _raises,
                "returns_none": _returns_none,
                "returns_list": _returns_list,
            },
        )

    # --- successful invocations ---

    def test_invoke_no_args(self):
        result = self.reflect_invoke(self.obj, "greet", [])
        assert result == "Hello, World!"

    def test_invoke_with_one_arg(self):
        result = self.reflect_invoke(self.obj, "greet", ["NexusLang"])
        assert result == "Hello, NexusLang!"

    def test_invoke_with_two_args(self):
        result = self.reflect_invoke(self.obj, "add", [3, 4])
        assert result == 7

    def test_invoke_negative_args(self):
        result = self.reflect_invoke(self.obj, "add", [-5, 3])
        assert result == -2

    def test_invoke_reads_object_property(self):
        result = self.reflect_invoke(self.obj, "get_x", [])
        assert result == 10

    def test_invoke_mutates_object_property(self):
        self.reflect_invoke(self.obj, "set_x", [99])
        assert self.obj.properties["x"] == 99

    def test_invoke_returns_none(self):
        result = self.reflect_invoke(self.obj, "returns_none", [])
        assert result is None

    def test_invoke_returns_list(self):
        result = self.reflect_invoke(self.obj, "returns_list", [4])
        assert result == [0, 1, 2, 3]

    # --- args=None treated as empty list ---

    def test_invoke_args_none_treated_as_empty(self):
        result = self.reflect_invoke(self.obj, "greet", None)
        assert result == "Hello, World!"

    # --- error cases ---

    def test_invoke_unknown_method_returns_error_dict(self):
        result = self.reflect_invoke(self.obj, "nonexistent", [])
        assert isinstance(result, dict)
        assert "error" in result
        assert "nonexistent" in result["error"]

    def test_invoke_error_dict_mentions_available_methods(self):
        result = self.reflect_invoke(self.obj, "bogus", [])
        assert "Available" in result["error"]

    def test_invoke_on_primitive_returns_error_dict(self):
        result = self.reflect_invoke(42, "greet", [])
        assert isinstance(result, dict)
        assert "error" in result

    def test_invoke_on_string_returns_error_dict(self):
        result = self.reflect_invoke("hello", "greet", [])
        assert isinstance(result, dict)
        assert "error" in result

    def test_invoke_on_none_returns_error_dict(self):
        result = self.reflect_invoke(None, "greet", [])
        assert isinstance(result, dict)
        assert "error" in result

    def test_invoke_on_list_returns_error_dict(self):
        result = self.reflect_invoke([1, 2, 3], "greet", [])
        assert isinstance(result, dict)
        assert "error" in result

    def test_invoke_method_raises_returns_error_dict(self):
        result = self.reflect_invoke(self.obj, "raises", [])
        assert isinstance(result, dict)
        assert "error" in result
        assert "ValueError" in result["error"] or "intentional" in result["error"]

    def test_invoke_error_dict_mentions_class_name_for_missing(self):
        result = self.reflect_invoke(self.obj, "missing_method", [])
        assert "Calculator" in result["error"]


# ---------------------------------------------------------------------------
# 2. reflect_invoke_safe
# ---------------------------------------------------------------------------

class TestReflectInvokeSafe:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_invoke_safe
        self.rt = _make_runtime()
        self.reflect_invoke_safe = lambda obj, name, args=None, default=None: reflect_invoke_safe(
            self.rt, obj, name, args, default
        )
        self.obj = _make_object(
            "Widget",
            methods={
                "compute": lambda self, x: x * 2,
                "raises": _raises,
            },
        )

    def test_safe_returns_method_result_on_success(self):
        result = self.reflect_invoke_safe(self.obj, "compute", [7])
        assert result == 14

    def test_safe_returns_none_default_for_missing_method(self):
        result = self.reflect_invoke_safe(self.obj, "nonexistent", [])
        assert result is None

    def test_safe_returns_custom_default_for_missing_method(self):
        result = self.reflect_invoke_safe(self.obj, "nonexistent", [], default="FALLBACK")
        assert result == "FALLBACK"

    def test_safe_returns_default_when_method_raises(self):
        result = self.reflect_invoke_safe(self.obj, "raises", [], default="SAFE")
        assert result == "SAFE"

    def test_safe_returns_default_for_non_object(self):
        result = self.reflect_invoke_safe(42, "compute", [7], default=-1)
        assert result == -1

    def test_safe_default_can_be_zero(self):
        result = self.reflect_invoke_safe(self.obj, "missing", [], default=0)
        assert result == 0

    def test_safe_default_can_be_false(self):
        result = self.reflect_invoke_safe(self.obj, "missing", [], default=False)
        assert result is False

    def test_safe_args_none_treated_as_empty(self):
        result = self.reflect_invoke_safe(self.obj, "nonexistent", None, default="X")
        assert result == "X"


# ---------------------------------------------------------------------------
# 3. reflect_call
# ---------------------------------------------------------------------------

class TestReflectCall:
    def setup_method(self):
        from nexuslang.stdlib.reflection import reflect_call
        self.rt = _make_runtime()
        self.reflect_call = lambda name, args=None: reflect_call(
            self.rt, name, args
        )
        # Register a few test functions
        self.rt.register_function("double", lambda x: x * 2)
        self.rt.register_function("add_two", lambda a, b: a + b)
        self.rt.register_function("get_greeting", lambda: "Hello from runtime!")
        self.rt.register_function("explodes", lambda: 1 / 0)

    def test_call_registered_function_no_args(self):
        result = self.reflect_call("get_greeting", [])
        assert result == "Hello from runtime!"

    def test_call_registered_function_one_arg(self):
        result = self.reflect_call("double", [21])
        assert result == 42

    def test_call_registered_function_two_args(self):
        result = self.reflect_call("add_two", [10, 32])
        assert result == 42

    def test_call_stdlib_function(self):
        # sqrt is always registered via math/stdlib
        result = self.reflect_call("sqrt", [4.0])
        assert abs(result - 2.0) < 1e-9

    def test_call_args_none_treated_as_empty(self):
        result = self.reflect_call("get_greeting", None)
        assert result == "Hello from runtime!"

    def test_call_unknown_function_returns_error_dict(self):
        result = self.reflect_call("does_not_exist", [])
        assert isinstance(result, dict)
        assert "error" in result

    def test_call_error_dict_mentions_function_name(self):
        result = self.reflect_call("does_not_exist", [])
        assert "does_not_exist" in result["error"]

    def test_call_error_dict_mentions_count(self):
        result = self.reflect_call("does_not_exist", [])
        # Check the error message contains a count or registration info
        assert "registered" in result["error"]

    def test_call_raising_function_returns_error_dict(self):
        result = self.reflect_call("explodes", [])
        assert isinstance(result, dict)
        assert "error" in result
        assert "ZeroDivisionError" in result["error"] or "explodes" in result["error"]


# ---------------------------------------------------------------------------
# 4. Registration check
# ---------------------------------------------------------------------------

class TestDynamicInvokeRegistration:
    def test_reflect_invoke_registered(self):
        rt = _make_runtime()
        assert "reflect_invoke" in rt.functions

    def test_reflect_invoke_safe_registered(self):
        rt = _make_runtime()
        assert "reflect_invoke_safe" in rt.functions

    def test_reflect_call_registered(self):
        rt = _make_runtime()
        assert "reflect_call" in rt.functions

    def test_reflect_invoke_callable_via_runtime(self):
        rt = _make_runtime()
        obj = _make_object("X", methods={"ping": lambda self: "pong"})
        fn = rt.functions["reflect_invoke"]
        result = fn(obj, "ping", [])
        assert result == "pong"

    def test_reflect_call_callable_via_runtime(self):
        rt = _make_runtime()
        rt.register_function("my_fn", lambda: 99)
        fn = rt.functions["reflect_call"]
        result = fn("my_fn", [])
        assert result == 99
