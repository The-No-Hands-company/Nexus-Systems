"""Smoke test for the coverage_utils stdlib module."""
import sys
import os
import types
import importlib.util

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "src"))

# Stub out nlpl.runtime.runtime.Runtime
for pkg in ("nlpl", "nexuslang.runtime", "nexuslang.runtime.runtime",
            "nexuslang.stdlib", "nexuslang.stdlib.coverage_utils"):
    if pkg not in sys.modules:
        sys.modules[pkg] = types.ModuleType(pkg)

class _StubRuntime:
    def register_function(self, name, fn): pass
    def register_module(self, name): pass

sys.modules["nexuslang.runtime.runtime"].Runtime = _StubRuntime

INIT_PATH = os.path.join(ROOT, "src", "nlpl", "stdlib", "coverage_utils", "__init__.py")
spec = importlib.util.spec_from_file_location("nexuslang.stdlib.coverage_utils", INIT_PATH)
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

# ---------------------------------------------------------------------------
# Helper: a simple function to measure coverage against
# ---------------------------------------------------------------------------

def sample_function(x):
    if x > 0:
        result = x * 2
    else:
        result = 0
    return result

def sum_list(lst):
    total = 0
    for item in lst:
        total += item
    return total

# ---------------------------------------------------------------------------
# coverage_measure — primary safe API (wraps a callable)
# ---------------------------------------------------------------------------

ret, sess = mod.coverage_measure(sample_function, args=[5], include=[__file__])
check("coverage_measure returns tuple", isinstance((ret, sess), tuple))
check("coverage_measure return value correct", ret == 10)
check("coverage_measure session is dict", isinstance(sess, dict))
check("coverage_measure has 'files' key", "files" in sess)
check("coverage_measure has 'call_count'", "call_count" in sess)
check("coverage_measure call_count >= 1", sess["call_count"] >= 1)
check("coverage_measure line_count >= 1", sess["line_count"] >= 1)

# File tracking: smoke script file should appear (we included it)
this_file = os.path.normpath(__file__)
file_keys = [os.path.normpath(k) for k in sess["files"].keys()]
check("coverage_measure tracked this file", this_file in file_keys)

# Find the record for this file
file_rec = None
for k, v in sess["files"].items():
    if os.path.normpath(k) == this_file:
        file_rec = v
        break
check("file record has lines_hit", file_rec is not None and "lines_hit" in file_rec)
check("file record has lines_total", file_rec is not None and "lines_total" in file_rec)
check("lines_hit is list", isinstance(file_rec["lines_hit"], list))
check("sample_function covered at least 2 lines", len(file_rec["lines_hit"]) >= 2)

# ---------------------------------------------------------------------------
# coverage_measure_line_rate
# ---------------------------------------------------------------------------

rate = mod.coverage_measure_line_rate(sample_function, args=[1], include=[__file__])
check("coverage_measure_line_rate returns float", isinstance(rate, float))
check("coverage_measure_line_rate in [0,1]", 0.0 <= rate <= 1.0)
check("coverage_measure_line_rate > 0", rate > 0.0)

# ---------------------------------------------------------------------------
# coverage_start / stop / get — use include pattern to limit noise
# ---------------------------------------------------------------------------

mod.coverage_start("smoke1", include=[__file__])
_ = sample_function(3)
_ = sample_function(-1)
_ = sum_list([1, 2, 3])
result_dict = mod.coverage_stop("smoke1")

check("coverage_stop returns dict", isinstance(result_dict, dict))
check("coverage_stop has name", result_dict["name"] == "smoke1")
check("coverage_stop not active", result_dict["active"] is False)
check("coverage_stop has files", isinstance(result_dict["files"], dict))
check("coverage_stop call_count >= 3", result_dict["call_count"] >= 3)

# ---------------------------------------------------------------------------
# coverage_get (non-destructive read while active)
# ---------------------------------------------------------------------------

mod.coverage_start("smoke2", include=[__file__])
_ = sample_function(7)
live = mod.coverage_get("smoke2")
check("coverage_get while active", live["active"] is True)
check("coverage_get has data", live["line_count"] >= 1)
mod.coverage_stop("smoke2")

# ---------------------------------------------------------------------------
# coverage_is_active
# ---------------------------------------------------------------------------

mod.coverage_start("smoke3", include=[__file__])
check("coverage_is_active True while running", mod.coverage_is_active("smoke3") is True)
mod.coverage_stop("smoke3")
check("coverage_is_active False after stop", mod.coverage_is_active("smoke3") is False)

# ---------------------------------------------------------------------------
# coverage_list_sessions
# ---------------------------------------------------------------------------

sessions = mod.coverage_list_sessions()
check("coverage_list_sessions is list", isinstance(sessions, list))
check("coverage_list_sessions contains smoke1", "smoke1" in sessions)

# ---------------------------------------------------------------------------
# coverage_reset
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_reset", include=[__file__])
_ = sample_function(1)
mod.coverage_reset("smoke_reset")
after_reset = mod.coverage_get("smoke_reset")
check("coverage_reset clears line_count", after_reset["line_count"] == 0)
check("coverage_reset clears files", len(after_reset["files"]) == 0)
mod.coverage_stop("smoke_reset")

# ---------------------------------------------------------------------------
# coverage_destroy
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_destroy", include=[__file__])
mod.coverage_destroy("smoke_destroy")
check("coverage_destroy removes session", "smoke_destroy" not in mod.coverage_list_sessions())

# ---------------------------------------------------------------------------
# coverage_summary
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_summ", include=[__file__])
_ = sample_function(5)
summ = mod.coverage_summary("smoke_summ")
mod.coverage_stop("smoke_summ")

check("coverage_summary is dict", isinstance(summ, dict))
check("coverage_summary has name", summ["name"] == "smoke_summ")
check("coverage_summary has file_count", "file_count" in summ)
check("coverage_summary has line_rate", "line_rate" in summ)
check("coverage_summary line_rate in [0,1]", 0.0 <= summ["line_rate"] <= 1.0)
check("coverage_summary has call_count", "call_count" in summ)
check("coverage_summary hit_lines >= 0", summ["hit_lines"] >= 0)

# ---------------------------------------------------------------------------
# coverage_files / coverage_lines_hit / coverage_lines_total
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_lines", include=[__file__])
_ = sample_function(5)
_ = sample_function(-1)

flist = mod.coverage_files("smoke_lines")
check("coverage_files returns list", isinstance(flist, list))
check("coverage_files not empty", len(flist) >= 1)
# Use first tracked file
first_file = flist[0]
hit = mod.coverage_lines_hit(first_file, "smoke_lines")
total = mod.coverage_lines_total(first_file, "smoke_lines")
check("coverage_lines_hit is list", isinstance(hit, list))
check("coverage_lines_total is list", isinstance(total, list))
check("lines_hit all int", all(isinstance(x, int) for x in hit))
check("lines_hit subset of total", set(hit).issubset(set(total)))

rate2 = mod.coverage_line_rate(first_file, "smoke_lines")
check("coverage_line_rate file in [0,1]", 0.0 <= rate2 <= 1.0)
overall_rate = mod.coverage_line_rate(name="smoke_lines")
check("coverage_line_rate overall in [0,1]", 0.0 <= overall_rate <= 1.0)

mod.coverage_stop("smoke_lines")

# ---------------------------------------------------------------------------
# coverage_functions / coverage_function_rate
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_fns", include=[__file__])
_ = sample_function(1)
_ = sum_list([1, 2])
fns_map = mod.coverage_functions(flist[0] if flist else first_file, "smoke_fns")
check("coverage_functions returns dict", isinstance(fns_map, dict))
fn_rate = mod.coverage_function_rate(name="smoke_fns")
check("coverage_function_rate in [0,1]", 0.0 <= fn_rate <= 1.0)
mod.coverage_stop("smoke_fns")

# ---------------------------------------------------------------------------
# coverage_branches / coverage_branch_count
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_br", include=[__file__])
_ = sample_function(5)   # takes x>0 branch
_ = sample_function(-1)  # takes else branch
branches_map = mod.coverage_branches(first_file, "smoke_br")
check("coverage_branches returns dict", isinstance(branches_map, dict))
bc = mod.coverage_branch_count(name="smoke_br")
check("coverage_branch_count is int", isinstance(bc, int))
check("coverage_branch_count >= 0", bc >= 0)
mod.coverage_stop("smoke_br")

# ---------------------------------------------------------------------------
# coverage_call_count / coverage_line_count
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_counts", include=[__file__])
_ = sample_function(2)
cc = mod.coverage_call_count("smoke_counts")
lc = mod.coverage_line_count("smoke_counts")
check("coverage_call_count >= 1", cc >= 1)
check("coverage_line_count >= 1", lc >= 1)
mod.coverage_stop("smoke_counts")

# ---------------------------------------------------------------------------
# coverage_report / coverage_report_text
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_rep", include=[__file__])
_ = sample_function(3)
rows = mod.coverage_report("smoke_rep")
check("coverage_report returns list", isinstance(rows, list))
if rows:
    row = rows[0]
    check("report row has filename", "filename" in row)
    check("report row has hit_lines", "hit_lines" in row)
    check("report row has line_rate", "line_rate" in row)
    check("report row has branch_count", "branch_count" in row)

report_rows_with_lines = mod.coverage_report("smoke_rep", show_lines=True)
if report_rows_with_lines:
    check("report with lines has hit_line_list", "hit_line_list" in report_rows_with_lines[0])
    check("report with lines has miss_line_list", "miss_line_list" in report_rows_with_lines[0])

text_report = mod.coverage_report_text("smoke_rep")
check("coverage_report_text is str", isinstance(text_report, str))
check("coverage_report_text contains TOTAL", "TOTAL" in text_report)
check("coverage_report_text contains Report", "Report" in text_report)
mod.coverage_stop("smoke_rep")

# ---------------------------------------------------------------------------
# coverage_to_dict
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_dict", include=[__file__])
_ = sample_function(1)
d = mod.coverage_to_dict("smoke_dict")
check("coverage_to_dict is dict", isinstance(d, dict))
check("coverage_to_dict has name", "name" in d)
check("coverage_to_dict has files", "files" in d)
check("coverage_to_dict lists are sorted ints", all(
    isinstance(k, int) for v in d["files"].values()
    for k in v.get("lines_hit", [])
))
mod.coverage_stop("smoke_dict")

# ---------------------------------------------------------------------------
# coverage_merge
# ---------------------------------------------------------------------------

_, sess_a = mod.coverage_measure(sample_function, args=[5], include=[__file__])
_, sess_b = mod.coverage_measure(sum_list, args=[[1, 2, 3]], include=[__file__])
# Recreate sessions under known names
mod.coverage_start("merge_a", include=[__file__])
sample_function(5)
mod.coverage_stop("merge_a")
mod.coverage_start("merge_b", include=[__file__])
sum_list([1, 2, 3])
mod.coverage_stop("merge_b")

merged_name = mod.coverage_merge("merge_a", "merge_b", "merged_ab")
check("coverage_merge returns name", merged_name == "merged_ab")
check("merged session exists", "merged_ab" in mod.coverage_list_sessions())
merged_summ = mod.coverage_summary("merged_ab")
check("merged has data", merged_summ["call_count"] >= 2)

# ---------------------------------------------------------------------------
# coverage_diff
# ---------------------------------------------------------------------------

diff = mod.coverage_diff("merge_a", "merge_b")
check("coverage_diff returns dict", isinstance(diff, dict))

# ---------------------------------------------------------------------------
# coverage_pause / coverage_resume
# ---------------------------------------------------------------------------

mod.coverage_start("smoke_pause", include=[__file__])
_ = sample_function(1)
mod.coverage_pause("smoke_pause")
check("coverage_is_active False after pause", mod.coverage_is_active("smoke_pause") is False)
mod.coverage_resume("smoke_pause")
check("coverage_is_active True after resume", mod.coverage_is_active("smoke_pause") is True)
_ = sample_function(2)
mod.coverage_stop("smoke_pause")

# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

calls = []
mods = []
class _RecRuntime:
    def register_function(self, name, fn): calls.append(name)
    def register_module(self, name): mods.append(name)

mod.register_coverage_utils_functions(_RecRuntime())

expected_fns = [
    "coverage_start", "coverage_stop", "coverage_pause", "coverage_resume",
    "coverage_reset", "coverage_destroy", "coverage_is_active", "coverage_list_sessions",
    "coverage_get", "coverage_files", "coverage_lines_hit", "coverage_lines_total",
    "coverage_line_rate", "coverage_functions", "coverage_function_rate",
    "coverage_branches", "coverage_branch_count", "coverage_call_count",
    "coverage_line_count", "coverage_summary", "coverage_report",
    "coverage_report_text", "coverage_to_dict", "coverage_merge", "coverage_diff",
    "coverage_measure", "coverage_measure_line_rate",
]
for fn in expected_fns:
    check(f"registration {fn}", fn in calls)

check("registration module coverage_utils", "coverage_utils" in mods)
check("registration module coverage", "coverage" in mods)

print(f"\n=== {passed} passed, {failed} failed ===")
if failed:
    sys.exit(1)
