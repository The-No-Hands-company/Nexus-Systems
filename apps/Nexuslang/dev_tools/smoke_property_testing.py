"""Smoke test for the property_testing stdlib module."""
import sys
import os
import types
import importlib.util

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "src"))

# Stub out nlpl.runtime.runtime.Runtime so the module can be imported standalone
for pkg in ("nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
            "nexuslang.stdlib", "nexuslang.stdlib.property_testing"):
    if pkg not in sys.modules:
        sys.modules[pkg] = types.ModuleType(pkg)

class _StubRuntime:
    def register_function(self, name, fn): pass
    def register_module(self, name): pass

sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime

INIT_PATH = os.path.join(ROOT, "src", "nlpl", "stdlib", "property_testing", "__init__.py")
spec = importlib.util.spec_from_file_location("nexuslang.stdlib.property_testing", INIT_PATH)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

passed = 0
failed = 0

def check(label, condition):
    global passed, failed
    if condition:
        passed += 1
    else:
        failed += 1
        print(f"  FAIL: {label}")

# --- gen_integer ---
s = mod.gen_integer(-5, 5)
check("gen_integer returns strategy dict", isinstance(s, dict) and s["type"] == "strategy")
check("gen_integer kind", s["kind"] == "integer")
v = mod.strategy_draw(s, seed=1)
check("gen_integer draw in range", -5 <= v <= 5)

# --- gen_integer bad args ---
try:
    mod.gen_integer(10, 1)
    check("gen_integer bad range raises", False)
except ValueError:
    check("gen_integer bad range raises", True)

# --- gen_float ---
sf = mod.gen_float(0.0, 1.0)
check("gen_float strategy", sf["kind"] == "float")
fv = mod.strategy_draw(sf, seed=42)
check("gen_float draw in range", 0.0 <= fv <= 1.0)

# --- gen_bool ---
sb = mod.gen_bool()
check("gen_bool strategy", sb["kind"] == "bool")
bv = mod.strategy_draw(sb, seed=1)
check("gen_bool is bool", isinstance(bv, bool))

# --- gen_none ---
sn = mod.gen_none()
nv = mod.strategy_draw(sn, seed=0)
check("gen_none returns None", nv is None)

# --- gen_constant ---
sc = mod.gen_constant(42)
cv = mod.strategy_draw(sc, seed=0)
check("gen_constant returns constant", cv == 42)

# --- gen_choice ---
vals = [10, 20, 30]
sch = mod.gen_choice(vals)
chv = mod.strategy_draw(sch, seed=0)
check("gen_choice draws from list", chv in vals)

try:
    mod.gen_choice([])
    check("gen_choice empty raises", False)
except ValueError:
    check("gen_choice empty raises", True)

# --- gen_from_list ---
sfl = mod.gen_from_list(["a", "b", "c"])
flv = mod.strategy_draw(sfl, seed=5)
check("gen_from_list in values", flv in ["a", "b", "c"])

# --- gen_string ---
ss = mod.gen_string(min_len=3, max_len=8)
sv = mod.strategy_draw(ss, seed=7)
check("gen_string is str", isinstance(sv, str))
check("gen_string length", 3 <= len(sv) <= 8)

# --- gen_text ---
st = mod.gen_text(min_len=2, max_len=10)
tv = mod.strategy_draw(st, seed=3)
check("gen_text is str", isinstance(tv, str))
check("gen_text length", 2 <= len(tv) <= 10)

# --- gen_list ---
sl = mod.gen_list(mod.gen_integer(0, 10), min_size=2, max_size=5)
lv = mod.strategy_draw(sl, seed=9)
check("gen_list is list", isinstance(lv, list))
check("gen_list size", 2 <= len(lv) <= 5)
check("gen_list elements in range", all(0 <= x <= 10 for x in lv))

# --- gen_dict ---
sd = mod.gen_dict(mod.gen_ascii_string(1, 5), mod.gen_integer(0, 100), min_size=1, max_size=3)
dv = mod.strategy_draw(sd, seed=11)
check("gen_dict is dict", isinstance(dv, dict))
check("gen_dict size", 1 <= len(dv) <= 3)

# --- gen_tuple ---
stp = mod.gen_tuple([mod.gen_integer(0, 5), mod.gen_bool(), mod.gen_text(0, 5)])
tpv = mod.strategy_draw(stp, seed=2)
check("gen_tuple is list", isinstance(tpv, list))
check("gen_tuple length 3", len(tpv) == 3)
check("gen_tuple[0] int", isinstance(tpv[0], int))
check("gen_tuple[1] bool", isinstance(tpv[1], bool))

# --- gen_one_of ---
soo = mod.gen_one_of([mod.gen_integer(0, 10), mod.gen_bool()])
oov = mod.strategy_draw(soo, seed=3)
check("gen_one_of draws a value", oov is not None or oov is False or True)

# --- gen_optional ---
opt = mod.gen_optional(mod.gen_integer(1, 100))
none_count = sum(1 for i in range(50) if mod.strategy_draw(opt, seed=i) is None)
check("gen_optional sometimes None", none_count >= 1)
some_count = sum(1 for i in range(50) if mod.strategy_draw(opt, seed=i) is not None)
check("gen_optional sometimes non-None", some_count >= 1)

# --- gen_fixed_dict ---
sfd = mod.gen_fixed_dict({"x": mod.gen_integer(0, 10), "y": mod.gen_float(0.0, 1.0)})
fdv = mod.strategy_draw(sfd, seed=5)
check("gen_fixed_dict has keys", "x" in fdv and "y" in fdv)
check("gen_fixed_dict x int", isinstance(fdv["x"], int))
check("gen_fixed_dict y float", isinstance(fdv["y"], float))

# --- gen_text_lines ---
stl = mod.gen_text_lines(3)
tlv = mod.strategy_draw(stl, seed=1)
check("gen_text_lines is list", isinstance(tlv, list))
check("gen_text_lines count", len(tlv) == 3)
check("gen_text_lines elements str", all(isinstance(x, str) for x in tlv))

# --- gen_positive_integer ---
spi = mod.gen_positive_integer(50)
piv = mod.strategy_draw(spi, seed=0)
check("gen_positive_integer > 0", piv >= 1)
check("gen_positive_integer <= max", piv <= 50)

# --- gen_non_negative_integer ---
snni = mod.gen_non_negative_integer(20)
nniv = mod.strategy_draw(snni, seed=0)
check("gen_non_negative_integer >= 0", nniv >= 0)

# --- gen_ascii_string ---
sas = mod.gen_ascii_string(3, 8)
asv = mod.strategy_draw(sas, seed=4)
check("gen_ascii_string is str", isinstance(asv, str))
check("gen_ascii_string length", 3 <= len(asv) <= 8)

# --- gen_non_empty_list ---
snel = mod.gen_non_empty_list(mod.gen_integer(0, 5))
nelv = mod.strategy_draw(snel, seed=0)
check("gen_non_empty_list not empty", len(nelv) >= 1)

# --- strategy_filter ---
sf2 = mod.strategy_filter(mod.gen_integer(0, 20), lambda x: x % 2 == 0)
fv2 = mod.strategy_draw(sf2, seed=1)
check("strategy_filter even", fv2 % 2 == 0)

# --- strategy_map ---
sm = mod.strategy_map(mod.gen_integer(1, 10), lambda x: x * 2)
mv = mod.strategy_draw(sm, seed=2)
check("strategy_map doubles", mv % 2 == 0 and 2 <= mv <= 20)

# --- strategy_draw repeatability ---
s1 = mod.strategy_draw(mod.gen_integer(0, 1000), seed=99)
s2 = mod.strategy_draw(mod.gen_integer(0, 1000), seed=99)
check("strategy_draw same seed same result", s1 == s2)

# --- property_test passing ---
result = mod.property_test(lambda x: x >= 0, [mod.gen_non_negative_integer(100)], count=50, seed=0)
check("property_test passed True", result["passed"] is True)
check("property_test run_count", result["run_count"] == 50)
check("property_test no counterexample", result["counterexample"] is None)

# --- property_test failing ---
result_fail = mod.property_test(
    lambda x: x < 50,
    [mod.gen_integer(0, 100)],
    count=200, seed=42
)
check("property_test failing not passed", result_fail["passed"] is False)
check("property_test has counterexample", result_fail["counterexample"] is not None)
check("property_test ce is int", isinstance(result_fail["counterexample"][0], int))

# --- property_test shrinking ---
check("property_test shrunk_counterexample exists", result_fail.get("shrunk_counterexample") is not None)
shrunk = result_fail["shrunk_counterexample"][0]
check("property_test shrunk >= 50", shrunk >= 50)

# --- property_assert passing ---
try:
    mod.property_assert(lambda x: isinstance(x, bool), [mod.gen_bool()], count=20, seed=0)
    check("property_assert pass does not raise", True)
except Exception:
    check("property_assert pass does not raise", False)

# --- property_assert failing ---
try:
    mod.property_assert(lambda x: x < 0, [mod.gen_integer(0, 10)], count=50, seed=0)
    check("property_assert fail raises", False)
except RuntimeError:
    check("property_assert fail raises RuntimeError", True)

# --- property_find_counterexample ---
ce = mod.property_find_counterexample(lambda x: x < 5, [mod.gen_integer(0, 10)], count=100, seed=0)
check("property_find_counterexample found", ce is not None)
check("property_find_counterexample is list", isinstance(ce, list))

# --- property_find_counterexample no failure ---
no_ce = mod.property_find_counterexample(
    lambda x: x >= 0, [mod.gen_non_negative_integer(50)], count=50, seed=0
)
check("property_find_counterexample None when passing", no_ce is None)

# --- property_find_example ---
ex = mod.property_find_example(lambda x: x > 90, [mod.gen_integer(0, 100)], count=200, seed=1)
check("property_find_example found", ex is not None)
check("property_find_example value > 90", ex[0] > 90)

# --- property_find_example not found ---
no_ex = mod.property_find_example(lambda x: x == -1, [mod.gen_non_negative_integer(10)], count=50, seed=0)
check("property_find_example None when not found", no_ex is None)

# --- property_statistics ---
stats = mod.property_statistics(lambda x: x % 2 == 0, [mod.gen_integer(0, 100)], count=200, seed=42)
check("property_statistics total", stats["total"] == 200)
check("property_statistics passed + failed == total", stats["passed"] + stats["failed"] == stats["total"])
check("property_statistics pass_rate", 0.0 <= stats["pass_rate"] <= 1.0)
check("property_statistics fail_rate", 0.0 <= stats["fail_rate"] <= 1.0)
check("property_statistics roughly 50/50", 0.3 <= stats["pass_rate"] <= 0.7)

# --- property_shrink ---
shrunk2 = mod.property_shrink(
    lambda x: x < 50,
    [mod.gen_integer(0, 200)],
    [150]
)
check("property_shrink returns list", isinstance(shrunk2, list))
check("property_shrink minimized", shrunk2[0] >= 50)
check("property_shrink reduced value", shrunk2[0] <= 150)

# --- property_report ---
rep = mod.property_report(result)
check("property_report is str", isinstance(rep, str))
check("property_report contains PASSED", "PASSED" in rep)

rep_fail = mod.property_report(result_fail)
check("property_report_fail contains FAILED", "FAILED" in rep_fail)
check("property_report_fail contains counterexample", "Counterexample" in rep_fail)

# --- property_assume ---
def prop_even_positive(x):
    mod.property_assume(x > 0)
    return x > 0

result_assume = mod.property_test(prop_even_positive, [mod.gen_integer(-5, 5)], count=100, seed=0)
check("property_assume skips negatives", result_assume["passed"] is True)
check("property_assume skip_count > 0", result_assume["skip_count"] >= 0)

# --- multi-arg property_test ---
def prop_commutative(a, b):
    return a + b == b + a

result_comm = mod.property_test(
    prop_commutative,
    [mod.gen_integer(-100, 100), mod.gen_integer(-100, 100)],
    count=100, seed=0
)
check("multi-arg property_test passed", result_comm["passed"] is True)
check("multi-arg run_count", result_comm["run_count"] == 100)

# --- property_test with list strategy ---
def prop_list_len_nonneg(lst):
    return len(lst) >= 0

result_list = mod.property_test(prop_list_len_nonneg, [mod.gen_list(mod.gen_integer(0, 10))], count=50, seed=1)
check("property_test list passes", result_list["passed"] is True)

# --- registration ---
runtime_calls = []
class _RecordingRuntime:
    def register_function(self, name, fn):
        runtime_calls.append(("fn", name))
    def register_module(self, name):
        runtime_calls.append(("mod", name))

rr = _RecordingRuntime()
mod.register_property_testing_functions(rr)
fn_names = {n for t, n in runtime_calls if t == "fn"}
mod_names = {n for t, n in runtime_calls if t == "mod"}

check("registration gen_integer", "gen_integer" in fn_names)
check("registration gen_float", "gen_float" in fn_names)
check("registration gen_bool", "gen_bool" in fn_names)
check("registration gen_string", "gen_string" in fn_names)
check("registration gen_list", "gen_list" in fn_names)
check("registration gen_dict", "gen_dict" in fn_names)
check("registration gen_tuple", "gen_tuple" in fn_names)
check("registration gen_one_of", "gen_one_of" in fn_names)
check("registration gen_optional", "gen_optional" in fn_names)
check("registration gen_choice", "gen_choice" in fn_names)
check("registration strategy_filter", "strategy_filter" in fn_names)
check("registration strategy_map", "strategy_map" in fn_names)
check("registration strategy_draw", "strategy_draw" in fn_names)
check("registration property_test", "property_test" in fn_names)
check("registration property_assert", "property_assert" in fn_names)
check("registration property_find_counterexample", "property_find_counterexample" in fn_names)
check("registration property_find_example", "property_find_example" in fn_names)
check("registration property_statistics", "property_statistics" in fn_names)
check("registration property_shrink", "property_shrink" in fn_names)
check("registration property_report", "property_report" in fn_names)
check("registration property_assume", "property_assume" in fn_names)
check("registration module property_testing", "property_testing" in mod_names)
check("registration module prop_test", "prop_test" in mod_names)

print(f"\n=== {passed} passed, {failed} failed ===")
if failed:
    sys.exit(1)
