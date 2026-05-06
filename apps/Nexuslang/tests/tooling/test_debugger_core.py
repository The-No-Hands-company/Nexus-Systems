"""
Debugger Core Tests
====================

Tests for the NexusLang core debugger: breakpoint management, step execution,
call stack tracking, variable inspection, and state machine transitions.
"""

import os
import sys
import threading
import time
from pathlib import Path
from unittest.mock import MagicMock, patch, call

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from nexuslang.debugger.debugger import Debugger, DebuggerState, Breakpoint, CallFrame


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_debugger(interactive: bool = False) -> tuple:
    """Return a (debugger, mock_interpreter) pair."""
    interpreter = MagicMock()
    interpreter.current_scope = [{}]
    interpreter.get_variable = MagicMock(return_value=None)
    interpreter.set_variable = MagicMock()
    dbg = Debugger(interpreter, interactive=interactive)
    return dbg, interpreter


# ---------------------------------------------------------------------------
# Breakpoint Management
# ---------------------------------------------------------------------------


class TestBreakpointManagement:
    def test_add_breakpoint_returns_object(self):
        dbg, _ = _make_debugger()
        bp = dbg.add_breakpoint("test.nxl", 10)
        assert isinstance(bp, Breakpoint)
        assert bp.file == os.path.normpath("test.nxl")
        assert bp.line == 10
        assert bp.enabled is True
        assert bp.hit_count == 0

    def test_add_breakpoint_with_condition(self):
        dbg, _ = _make_debugger()
        bp = dbg.add_breakpoint("file.nxl", 5, condition="x > 0")
        assert bp.condition == "x > 0"

    def test_add_temporary_breakpoint(self):
        dbg, _ = _make_debugger()
        bp = dbg.add_breakpoint("file.nxl", 3, temp=True)
        assert bp.temp is True

    def test_duplicate_breakpoint_overwrites(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 10)
        dbg.add_breakpoint("file.nxl", 10, condition="y == 1")
        assert len(dbg.list_breakpoints()) == 1
        assert dbg.list_breakpoints()[0].condition == "y == 1"

    def test_remove_existing_breakpoint(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 10)
        removed = dbg.remove_breakpoint("file.nxl", 10)
        assert removed is True
        assert len(dbg.list_breakpoints()) == 0

    def test_remove_nonexistent_breakpoint(self):
        dbg, _ = _make_debugger()
        removed = dbg.remove_breakpoint("file.nxl", 99)
        assert removed is False

    def test_toggle_breakpoint_disables_then_enables(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 10)
        toggled = dbg.toggle_breakpoint("file.nxl", 10)
        assert toggled is True
        assert dbg.list_breakpoints()[0].enabled is False
        dbg.toggle_breakpoint("file.nxl", 10)
        assert dbg.list_breakpoints()[0].enabled is True

    def test_clear_breakpoints_specific_file(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("a.nxl", 1)
        dbg.add_breakpoint("a.nxl", 2)
        dbg.add_breakpoint("b.nxl", 5)
        dbg.clear_breakpoints("a.nxl")
        remaining = [bp.file for bp in dbg.list_breakpoints()]
        assert all("b.nxl" in f for f in remaining)
        assert len(remaining) == 1

    def test_clear_all_breakpoints(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("a.nxl", 1)
        dbg.add_breakpoint("b.nxl", 2)
        dbg.clear_breakpoints()
        assert dbg.list_breakpoints() == []

    def test_list_breakpoints_sorted(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("b.nxl", 20)
        dbg.add_breakpoint("a.nxl", 5)
        dbg.add_breakpoint("a.nxl", 1)
        bps = dbg.list_breakpoints()
        files = [bp.file for bp in bps]
        lines = [bp.line for bp in bps]
        assert files == sorted(files)
        # Within same file, lines should be ascending
        a_lines = [bp.line for bp in bps if "a.nxl" in bp.file]
        assert a_lines == sorted(a_lines)

    def test_path_normalisation(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("./dir/../file.nxl", 1)
        assert len(dbg.list_breakpoints()) == 1
        bp = dbg.list_breakpoints()[0]
        assert ".." not in bp.file

    def test_temporary_breakpoint_removed_after_check(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 5, temp=True)
        bp = dbg._check_breakpoint("file.nxl", 5)
        assert bp is not None
        assert len(dbg.list_breakpoints()) == 0  # Auto-removed

    def test_disabled_breakpoint_not_triggered(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 5)
        dbg.toggle_breakpoint("file.nxl", 5)  # Disable
        bp = dbg._check_breakpoint("file.nxl", 5)
        assert bp is None


# ---------------------------------------------------------------------------
# State Transitions
# ---------------------------------------------------------------------------


class TestStateTransitions:
    def test_initial_state_is_running(self):
        dbg, _ = _make_debugger()
        assert dbg.state == DebuggerState.RUNNING

    def test_step_into_sets_stepping_state(self):
        dbg, _ = _make_debugger()
        dbg.step_into()
        assert dbg.state == DebuggerState.STEPPING

    def test_step_over_sets_step_over_state(self):
        dbg, _ = _make_debugger()
        dbg.step_over()
        assert dbg.state == DebuggerState.STEP_OVER

    def test_step_out_sets_step_out_state(self):
        dbg, _ = _make_debugger()
        dbg.step_out()
        assert dbg.state == DebuggerState.STEP_OUT

    def test_continue_sets_running_state(self):
        dbg, _ = _make_debugger()
        dbg.step_into()
        dbg.continue_execution()
        assert dbg.state == DebuggerState.RUNNING

    def test_continue_clears_target_depth(self):
        dbg, _ = _make_debugger()
        dbg.step_over()
        dbg.continue_execution()
        assert dbg.target_depth is None


# ---------------------------------------------------------------------------
# Should Pause Logic
# ---------------------------------------------------------------------------


class TestShouldPause:
    def test_pause_at_breakpoint(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 10)
        assert dbg._should_pause("file.nxl", 10) is True

    def test_no_pause_without_breakpoint_in_running_state(self):
        dbg, _ = _make_debugger()
        assert dbg._should_pause("file.nxl", 5) is False

    def test_pause_in_stepping_state(self):
        dbg, _ = _make_debugger()
        dbg.state = DebuggerState.STEPPING
        assert dbg._should_pause("file.nxl", 99) is True

    def test_step_over_pauses_at_same_depth(self):
        dbg, _ = _make_debugger()
        dbg.step_depth = 2
        dbg.state = DebuggerState.STEP_OVER
        dbg.target_depth = 2
        assert dbg._should_pause("file.nxl", 1) is True

    def test_step_over_does_not_pause_in_deeper_frame(self):
        dbg, _ = _make_debugger()
        dbg.step_depth = 3
        dbg.state = DebuggerState.STEP_OVER
        dbg.target_depth = 2
        assert dbg._should_pause("file.nxl", 1) is False

    def test_step_out_pauses_at_shallower_depth(self):
        dbg, _ = _make_debugger()
        dbg.step_depth = 1
        dbg.state = DebuggerState.STEP_OUT
        dbg.target_depth = 2
        assert dbg._should_pause("file.nxl", 1) is True

    def test_step_out_does_not_pause_in_same_frame(self):
        dbg, _ = _make_debugger()
        dbg.step_depth = 2
        dbg.state = DebuggerState.STEP_OUT
        dbg.target_depth = 2
        assert dbg._should_pause("file.nxl", 1) is False


# ---------------------------------------------------------------------------
# Call Stack
# ---------------------------------------------------------------------------


class TestCallStack:
    def test_initial_call_stack_empty(self):
        dbg, _ = _make_debugger()
        assert dbg.call_stack == []
        assert dbg.current_frame() is None

    def test_push_frame(self):
        dbg, _ = _make_debugger()
        dbg.push_frame("my_func", "file.nxl", 10, {"x": 1})
        frame = dbg.current_frame()
        assert frame is not None
        assert frame.function_name == "my_func"
        assert frame.file == "file.nxl"
        assert frame.line == 10
        assert frame.local_vars == {"x": 1}

    def test_push_frame_increments_depth(self):
        dbg, _ = _make_debugger()
        assert dbg.step_depth == 0
        dbg.push_frame("f", "f.nxl", 1, {})
        assert dbg.step_depth == 1
        dbg.push_frame("g", "f.nxl", 2, {})
        assert dbg.step_depth == 2

    def test_pop_frame(self):
        dbg, _ = _make_debugger()
        dbg.push_frame("f", "f.nxl", 1, {})
        dbg.push_frame("g", "f.nxl", 5, {})
        dbg.pop_frame()
        assert dbg.current_frame().function_name == "f"
        assert dbg.step_depth == 1

    def test_pop_frame_empty_stack_safe(self):
        dbg, _ = _make_debugger()
        dbg.pop_frame()  # Should not raise
        assert dbg.step_depth == 0

    def test_local_vars_copied_on_push(self):
        dbg, _ = _make_debugger()
        original = {"a": 1}
        dbg.push_frame("f", "f.nxl", 1, original)
        original["b"] = 2
        assert "b" not in dbg.current_frame().local_vars

    def test_multiple_frames(self):
        dbg, _ = _make_debugger()
        dbg.push_frame("main", "a.nxl", 1, {})
        dbg.push_frame("helper", "a.nxl", 10, {})
        dbg.push_frame("inner", "a.nxl", 20, {})
        assert len(dbg.call_stack) == 3
        assert dbg.current_frame().function_name == "inner"


# ---------------------------------------------------------------------------
# Variable Inspection
# ---------------------------------------------------------------------------


class TestVariableInspection:
    def test_inspect_variable_delegates_to_interpreter(self):
        dbg, interp = _make_debugger()
        interp.get_variable.return_value = 42
        val = dbg.inspect_variable("x")
        assert val == 42
        interp.get_variable.assert_called_once_with("x")

    def test_inspect_variable_returns_none_on_error(self):
        dbg, interp = _make_debugger()
        interp.get_variable.side_effect = Exception("not found")
        val = dbg.inspect_variable("missing")
        assert val is None

    def test_inspect_all_variables_from_scope(self):
        dbg, interp = _make_debugger()
        interp.current_scope = [{"x": 1, "_private": 2}, {"y": 3}]
        vars_ = dbg.inspect_all_variables()
        assert "x" in vars_
        assert "y" in vars_
        assert "_private" not in vars_  # underscore filtered out

    def test_set_variable_delegates(self):
        dbg, interp = _make_debugger()
        dbg.set_variable("z", 99)
        interp.set_variable.assert_called_once_with("z", 99)


# ---------------------------------------------------------------------------
# Trace Hooks (Integration Points)
# ---------------------------------------------------------------------------


class TestTraceHooks:
    def test_trace_line_increments_total_steps(self):
        dbg, _ = _make_debugger()
        dbg.trace_line("file.nxl", 1)
        dbg.trace_line("file.nxl", 2)
        assert dbg.total_steps == 2

    def test_trace_line_updates_current_location(self):
        dbg, _ = _make_debugger()
        dbg.trace_line("src.nxl", 15)
        assert dbg.current_file == "src.nxl"
        assert dbg.current_line == 15

    def test_trace_line_calls_step_callback(self):
        dbg, _ = _make_debugger()
        on_step = MagicMock()
        dbg.on_step = on_step
        dbg.trace_line("f.nxl", 7)
        on_step.assert_called_once_with("f.nxl", 7)

    def test_trace_call_pushes_frame(self):
        dbg, _ = _make_debugger()
        dbg.trace_call("foo", "a.nxl", 3, {"arg": 1})
        assert dbg.current_frame().function_name == "foo"

    def test_trace_return_pops_frame(self):
        dbg, _ = _make_debugger()
        dbg.trace_call("foo", "a.nxl", 3, {})
        dbg.trace_return("foo", 42)
        assert dbg.current_frame() is None

    def test_trace_exception_calls_callback(self):
        dbg, _ = _make_debugger()
        on_exc = MagicMock()
        dbg.on_exception = on_exc
        exc = RuntimeError("oops")
        dbg.trace_exception(exc)
        on_exc.assert_called_once()
        args = on_exc.call_args[0]
        assert args[0] is exc

    def test_breakpoint_hit_count_increment(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 5)
        dbg._check_breakpoint("file.nxl", 5)
        dbg._check_breakpoint("file.nxl", 5)
        bp = dbg.list_breakpoints()[0]
        assert bp.hit_count == 2

    def test_breakpoints_hit_stat_tracked(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("file.nxl", 2)
        dbg.add_breakpoint("file.nxl", 4)
        dbg._check_breakpoint("file.nxl", 2)
        dbg._check_breakpoint("file.nxl", 4)
        assert dbg.breakpoints_hit == 2


# ---------------------------------------------------------------------------
# Statistics
# ---------------------------------------------------------------------------


class TestStatistics:
    def test_initial_stats_zero(self):
        dbg, _ = _make_debugger()
        assert dbg.total_steps == 0
        assert dbg.breakpoints_hit == 0

    def test_stats_after_execution(self):
        dbg, _ = _make_debugger()
        dbg.add_breakpoint("a.nxl", 1)
        dbg.trace_line("a.nxl", 1)
        dbg.trace_line("a.nxl", 2)
        assert dbg.total_steps == 2
        assert dbg.breakpoints_hit == 1


# ---------------------------------------------------------------------------
# Breakpoint Data Class
# ---------------------------------------------------------------------------


class TestBreakpointDataclass:
    def test_str_representation(self):
        bp = Breakpoint(file="test.nxl", line=10)
        s = str(bp)
        assert "test.nxl" in s
        assert "10" in s
        assert "enabled" in s

    def test_str_with_condition(self):
        bp = Breakpoint(file="f.nxl", line=1, condition="x > 10")
        assert "x > 10" in str(bp)

    def test_str_temporary(self):
        bp = Breakpoint(file="f.nxl", line=1, temp=True)
        assert "temporary" in str(bp).lower()


# ---------------------------------------------------------------------------
# CallFrame Data Class
# ---------------------------------------------------------------------------


class TestCallFrame:
    def test_str_representation(self):
        frame = CallFrame(function_name="greet", file="main.nxl", line=5)
        s = str(frame)
        assert "greet" in s
        assert "main.nxl" in s
        assert "5" in s

    def test_default_local_vars_empty(self):
        frame = CallFrame(function_name="f", file="f.nxl", line=1)
        assert frame.local_vars == {}
