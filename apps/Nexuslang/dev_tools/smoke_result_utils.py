"""Smoke test for src/nlpl/stdlib/result_utils/__init__.py

Loads the module in isolation (no full stdlib import chain) and verifies
every registered function exists and behaves correctly in basic cases.
"""

import importlib.util
import os
import sys
import types

# ---------------------------------------------------------------------------
# Bootstrap: inject stub modules so the relative import chain resolves
# ---------------------------------------------------------------------------
_MOD_PATH = os.path.join(
    os.path.dirname(__file__),
    "..", "src", "nlpl", "stdlib", "result_utils", "__init__.py",
)
_MOD_PATH = os.path.abspath(_MOD_PATH)

for _pkg in ("nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
             "nexuslang.stdlib", "nexuslang.stdlib.result_utils"):
    if _pkg not in sys.modules:
        sys.modules[_pkg] = types.ModuleType(_pkg)


class _StubRuntime:
    def register_function(self, name, fn):
        pass
    def register_module(self, name):
        pass


sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime

spec = importlib.util.spec_from_file_location("nexuslang.stdlib.result_utils", _MOD_PATH)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

# ---------------------------------------------------------------------------
# Minimal assertion helper
# ---------------------------------------------------------------------------
_passed = 0
_failed = 0


def check(label: str, condition: bool) -> None:
    global _passed, _failed
    if condition:
        print(f"  PASS  {label}")
        _passed += 1
    else:
        print(f"  FAIL  {label}")
        _failed += 1


def raises(label: str, fn) -> None:
    global _passed, _failed
    try:
        fn()
        print(f"  FAIL  {label}  (no exception raised)")
        _failed += 1
    except Exception:
        print(f"  PASS  {label}")
        _passed += 1


# ===========================================================================
# Result tests
# ===========================================================================
print("Result constructors & predicates")
r_ok = m.result_ok(42)
r_err = m.result_err("oops")
check("result_ok tag", r_ok["tag"] == "ok")
check("result_ok value", r_ok["value"] == 42)
check("result_err tag", r_err["tag"] == "err")
check("result_err error", r_err["error"] == "oops")
check("result_is_ok true", m.result_is_ok(r_ok) is True)
check("result_is_ok false", m.result_is_ok(r_err) is False)
check("result_is_err true", m.result_is_err(r_err) is True)
check("result_is_err false", m.result_is_err(r_ok) is False)

print("Result extraction")
check("result_unwrap ok", m.result_unwrap(r_ok) == 42)
raises("result_unwrap err raises", lambda: m.result_unwrap(r_err))
check("result_unwrap_err err", m.result_unwrap_err(r_err) == "oops")
raises("result_unwrap_err ok raises", lambda: m.result_unwrap_err(r_ok))
check("result_unwrap_or ok", m.result_unwrap_or(r_ok, 99) == 42)
check("result_unwrap_or err", m.result_unwrap_or(r_err, 99) == 99)
check("result_unwrap_or_else ok", m.result_unwrap_or_else(r_ok, lambda: 99) == 42)
check("result_unwrap_or_else err", m.result_unwrap_or_else(r_err, lambda: 99) == 99)
check("result_expect ok", m.result_expect(r_ok, "msg") == 42)
raises("result_expect err raises", lambda: m.result_expect(r_err, "failed"))

print("Result transformations")
mapped = m.result_map(r_ok, lambda v: v * 2)
check("result_map ok", mapped["value"] == 84)
check("result_map err passthrough", m.result_map(r_err, lambda v: v * 2)["tag"] == "err")
me = m.result_map_err(r_err, lambda e: f"wrapped: {e}")
check("result_map_err err", "wrapped" in me["error"])
check("result_map_err ok passthrough", m.result_map_err(r_ok, lambda e: "x")["tag"] == "ok")
at = m.result_and_then(r_ok, lambda v: m.result_ok(v + 1))
check("result_and_then ok→ok", at["value"] == 43)
check("result_and_then err passthrough", m.result_and_then(r_err, lambda v: m.result_ok(v))["tag"] == "err")
oe = m.result_or_else(r_err, lambda e: m.result_ok(0))
check("result_or_else err→ok", oe["value"] == 0)
check("result_or_else ok passthrough", m.result_or_else(r_ok, lambda e: m.result_ok(0))["value"] == 42)

print("Result ↔ Option conversions")
opt = m.result_to_option(r_ok)
check("result_to_option ok→some", opt["tag"] == "some" and opt["value"] == 42)
check("result_to_option err→none", m.result_to_option(r_err)["tag"] == "none")
some = m.option_some("hi")
none = m.option_none()
check("result_ok_or some→ok", m.result_ok_or(some, "e")["value"] == "hi")
check("result_ok_or none→err", m.result_ok_or(none, "e")["tag"] == "err")

# ===========================================================================
# Option tests
# ===========================================================================
print("Option constructors & predicates")
check("option_some tag", some["tag"] == "some")
check("option_some value", some["value"] == "hi")
check("option_none tag", none["tag"] == "none")
check("option_is_some true", m.option_is_some(some) is True)
check("option_is_some false", m.option_is_some(none) is False)
check("option_is_none true", m.option_is_none(none) is True)
check("option_is_none false", m.option_is_none(some) is False)

print("Option extraction")
check("option_unwrap some", m.option_unwrap(some) == "hi")
raises("option_unwrap none raises", lambda: m.option_unwrap(none))
check("option_unwrap_or some", m.option_unwrap_or(some, "x") == "hi")
check("option_unwrap_or none", m.option_unwrap_or(none, "x") == "x")
check("option_unwrap_or_else some", m.option_unwrap_or_else(some, lambda: "x") == "hi")
check("option_unwrap_or_else none", m.option_unwrap_or_else(none, lambda: "x") == "x")
check("option_expect some", m.option_expect(some, "msg") == "hi")
raises("option_expect none raises", lambda: m.option_expect(none, "gone"))

print("Option transformations")
om = m.option_map(some, lambda v: v.upper())
check("option_map some", om["value"] == "HI")
check("option_map none passthrough", m.option_map(none, lambda v: v)["tag"] == "none")
at2 = m.option_and_then(some, lambda v: m.option_some(len(v)))
check("option_and_then some→some", at2["value"] == 2)
check("option_and_then none passthrough", m.option_and_then(none, lambda v: m.option_some(v))["tag"] == "none")
check("option_or some", m.option_or(some, m.option_some("y"))["value"] == "hi")
check("option_or none", m.option_or(none, m.option_some("y"))["value"] == "y")
check("option_or_else some", m.option_or_else(some, lambda: m.option_some("y"))["value"] == "hi")
check("option_or_else none", m.option_or_else(none, lambda: m.option_some("y"))["value"] == "y")
check("option_filter pass", m.option_filter(some, lambda v: True)["tag"] == "some")
check("option_filter fail", m.option_filter(some, lambda v: False)["tag"] == "none")
check("option_filter none passthrough", m.option_filter(none, lambda v: True)["tag"] == "none")
inner = m.option_some(m.option_some(7))
check("option_flatten some(some(7))", m.option_flatten(inner)["value"] == 7)
check("option_flatten some(none)", m.option_flatten(m.option_some(none))["tag"] == "none")
check("option_flatten none", m.option_flatten(none)["tag"] == "none")

print("Option ↔ Result conversions")
check("option_to_result some→ok", m.option_to_result(some, "e")["value"] == "hi")
check("option_to_result none→err", m.option_to_result(none, "e")["tag"] == "err")

# ===========================================================================
# Error chain tests
# ===========================================================================
print("Error chain")
e1 = m.error_new("disk full", code="E_IO", context={"path": "/tmp"})
check("error_new message", e1["message"] == "disk full")
check("error_new code", e1["code"] == "E_IO")
check("error_new context", e1["context"]["path"] == "/tmp")
check("error_new cause None", e1["cause"] is None)

e2 = m.error_wrap(e1, "write failed")
check("error_wrap message", e2["message"] == "write failed")
check("error_wrap cause", e2["cause"]["message"] == "disk full")

e3 = m.error_chain(["top", "middle", "root"])
check("error_chain top message", e3["message"] == "top")
check("error_chain middle", e3["cause"]["message"] == "middle")
check("error_chain root", e3["cause"]["cause"]["message"] == "root")
check("error_chain root no cause", e3["cause"]["cause"]["cause"] is None)

root = m.error_root(e3)
check("error_root", root["message"] == "root")
check("error_message dict", m.error_message(e1) == "disk full")
check("error_message str", m.error_message("plain") == "plain")
check("error_code", m.error_code(e1) == "E_IO")
check("error_code none", m.error_code(e2) is None)
check("error_context", m.error_context(e1)["path"] == "/tmp")
causes = m.error_causes(e3)
check("error_causes length", len(causes) == 3)
check("error_causes order", causes[0]["message"] == "top")

# ===========================================================================
# Registration count  
# ===========================================================================
print("Registration")
registered = []


class _MockRuntime:
    def register_function(self, name, fn):
        registered.append(name)
    def register_module(self, name):
        pass


m.register_result_utils_functions(_MockRuntime())
check("registered 38 functions", len(registered) == 38)
check("no duplicates", len(registered) == len(set(registered)))

# ===========================================================================
# Summary
# ===========================================================================
print()
print(f"=== {_passed} passed, {_failed} failed ===")
if _failed:
    sys.exit(1)
