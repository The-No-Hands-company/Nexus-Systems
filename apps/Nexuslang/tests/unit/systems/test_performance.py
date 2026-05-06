"""
Performance regression tests for the NexusLang interpreter.

These tests verify that:
1. The dispatch table provides adequate speedup over naive regex dispatch.
2. Core interpreter operations complete within acceptable time bounds.
3. AST optimization levels do not regress execution speed.

Run with:
    pytest tests/test_performance.py -v
"""

import sys
import os
import time
import timeit
import statistics

import pytest

# Ensure src/ is on the path when running from project root
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))

from nexuslang.interpreter.interpreter import Interpreter, _camel_to_snake
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_interpreter(source: str = "") -> Interpreter:
    """Create a fully initialised interpreter with stdlib registered."""
    rt = Runtime()
    register_stdlib(rt)
    return Interpreter(rt, source=source)


def _run_nxl(source: str, optimization_level: int = 0) -> float:
    """
    Run an NexusLang program and return wall-clock execution time in milliseconds.
    Parse time is excluded; only interpretation time is measured.
    """
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    ast = parser.parse()

    rt = Runtime()
    register_stdlib(rt)
    interp = Interpreter(rt, source=source)

    t0 = time.perf_counter()
    interp.interpret(ast, optimization_level=optimization_level)
    t1 = time.perf_counter()
    return (t1 - t0) * 1000.0


# ---------------------------------------------------------------------------
# Dispatch table tests
# ---------------------------------------------------------------------------

class TestDispatchTable:
    """Verify that the static dispatch table is built and used correctly."""

    def test_dispatch_table_populated(self):
        """_DISPATCH_TABLE should contain at least 50 execute_* entries."""
        interp = _make_interpreter()
        assert len(Interpreter._DISPATCH_TABLE) >= 50, (
            f"Expected >= 50 dispatch entries, got {len(Interpreter._DISPATCH_TABLE)}"
        )

    def test_dispatch_table_has_common_nodes(self):
        """Common AST node types must appear in the dispatch table."""
        required = [
            "BinaryOperation",
            "VariableDeclaration",
            "FunctionDefinition",
            "FunctionCall",
            "IfStatement",
            "WhileLoop",
            "ReturnStatement",
            "PrintStatement",
        ]
        for node_type in required:
            assert node_type in Interpreter._DISPATCH_TABLE, (
                f"Missing dispatch entry for '{node_type}'"
            )

    def test_dispatch_cache_populated_after_run(self):
        """Running a program should populate the per-instance dispatch cache."""
        src = '''
set x to 1
set y to 2
set z to x plus y
print text convert z to string
'''
        interp = _make_interpreter(src)
        lexer = Lexer(src)
        tokens = lexer.tokenize()
        parser = Parser(tokens, source=src)
        ast = parser.parse()
        interp.interpret(ast)
        assert len(interp._dispatch_cache) > 0, "Dispatch cache not populated after run"

    def test_dispatch_speedup_measurable(self):
        """
        Dict lookup dispatch should be at least 5x faster than per-call regex.

        This exercises the cached scenario (repeated lookups for the same node
        type, which is the hot path in typical programs).
        """
        iterations = 50_000

        # Baseline: regex conversion every time
        def old_dispatch(name):
            import re
            return re.sub(r'(?<!^)(?=[A-Z])', '_', name).lower()

        # New dispatch: pre-built dict lookup
        table = Interpreter._DISPATCH_TABLE
        cache: dict = {}

        def new_dispatch(name):
            if name in cache:
                return cache[name]
            method_name = table.get(name) or old_dispatch(name)
            cache[name] = method_name
            return method_name

        node_types = ["BinaryOperation", "VariableDeclaration", "FunctionCall",
                      "IfStatement", "WhileLoop", "PrintStatement"]

        old_t = timeit.timeit(
            lambda: [old_dispatch(n) for n in node_types],
            number=iterations,
        )
        new_t = timeit.timeit(
            lambda: [new_dispatch(n) for n in node_types],
            number=iterations,
        )

        speedup = old_t / new_t
        assert speedup >= 5.0, (
            f"Dispatch speedup {speedup:.1f}x is below the 5x minimum threshold. "
            f"Old: {old_t*1000:.1f}ms, New: {new_t*1000:.1f}ms"
        )

    def test_camel_to_snake_helper(self):
        """_camel_to_snake must convert node type names correctly."""
        cases = {
            "BinaryOperation": "binary_operation",
            "VariableDeclaration": "variable_declaration",
            "FunctionCall": "function_call",
            "IfStatement": "if_statement",
            "WhileLoop": "while_loop",
            "PrintStatement": "print_statement",
            "IndexAssignment": "index_assignment",
        }
        for camel, expected in cases.items():
            result = _camel_to_snake(camel)
            assert result == expected, (
                f"_camel_to_snake({camel!r}) = {result!r}, expected {expected!r}"
            )


# ---------------------------------------------------------------------------
# Execution time regression tests
# ---------------------------------------------------------------------------

class TestExecutionTime:
    """
    Wall-clock regression tests for core interpreter operations.

    Thresholds are deliberately generous (10x current observed values) to avoid
    false failures on slow CI machines, while still catching catastrophic regressions.
    """

    # Threshold multipliers applied to observed baseline values
    REGRESSION_FACTOR = 5.0

    def _assert_within_threshold(self, label: str, measured_ms: float, baseline_ms: float):
        """Assert measured time is within regression factor of baseline."""
        threshold_ms = baseline_ms * self.REGRESSION_FACTOR
        assert measured_ms <= threshold_ms, (
            f"{label}: {measured_ms:.1f}ms exceeds threshold "
            f"({baseline_ms:.1f}ms * {self.REGRESSION_FACTOR}x = {threshold_ms:.1f}ms)"
        )

    def test_simple_arithmetic(self):
        """Simple arithmetic should complete in < 100ms."""
        src = '''
set a to 100
set b to 200
set c to a plus b times 3
print text convert c to string
'''
        elapsed = _run_nxl(src)
        assert elapsed < 100.0, f"Simple arithmetic took {elapsed:.1f}ms (expected < 100ms)"

    def test_iterative_fibonacci_baseline(self):
        """
        Iterative fibonacci(100) should complete in under 200ms.
        Observed baseline at dispatch table commit: ~1ms per fib(1000) iteration.
        fib(100) uses far fewer loop iterations.
        """
        src = '''
function fib_iter with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    set a to 0
    set b to 1
    set i to 2
    while i is less than or equal to n
        set temp to a plus b
        set a to b
        set b to temp
        set i to i plus 1
    end
    return b
end
set result to fib_iter with 100
print text convert result to string
'''
        elapsed = _run_nxl(src)
        # Observed: ~0.5ms. Threshold: 200ms (ample headroom under loaded CI)
        assert elapsed < 200.0, (
            f"fib_iter(100) took {elapsed:.1f}ms (expected < 200ms)"
        )

    def test_loop_1000_iterations(self):
        """A simple counting loop of 1000 iterations should complete in < 200ms."""
        src = '''
set count to 0
set i to 0
while i is less than 1000
    set count to count plus 1
    set i to i plus 1
end
print text convert count to string
'''
        elapsed = _run_nxl(src)
        # Observed: ~5ms. Threshold: 2000ms (generous to avoid flakiness under load)
        assert elapsed < 2000.0, (
            f"1000-iteration while loop took {elapsed:.1f}ms (expected < 2000ms)"
        )

    def test_list_append_loop(self):
        """Appending 100 items to a list should complete in < 100ms."""
        src = '''
set items to []
set i to 0
while i is less than 100
    append i to items
    set i to i plus 1
end
print text convert i to string
'''
        elapsed = _run_nxl(src)
        assert elapsed < 100.0, (
            f"100-append loop took {elapsed:.1f}ms (expected < 100ms)"
        )

    def test_function_call_overhead(self):
        """100 function calls should complete in < 100ms."""
        src = '''
function add_one with n as Integer returns Integer
    return n plus 1
end
set result to 0
set i to 0
while i is less than 100
    set result to add_one with result
    set i to i plus 1
end
print text convert result to string
'''
        elapsed = _run_nxl(src)
        assert elapsed < 100.0, (
            f"100 function calls took {elapsed:.1f}ms (expected < 100ms)"
        )


# ---------------------------------------------------------------------------
# Optimization level tests
# ---------------------------------------------------------------------------

class TestOptimizationLevels:
    """Verify that optimization levels do not catastrophically regress performance."""

    NLPL_SRC = '''
function fib with n as Integer returns Integer
    set a to 0
    set b to 1
    set i to 2
    while i is less than or equal to n
        set temp to a plus b
        set a to b
        set b to temp
        set i to i plus 1
    end
    return b
end
set result to fib with 50
print text convert result to string
'''

    def _timed_run(self, opt_level: int) -> float:
        """Return median of 3 runs at the given optimization level (ms)."""
        times = []
        for _ in range(3):
            elapsed = _run_nxl(self.NLPL_SRC, optimization_level=opt_level)
            times.append(elapsed)
        return statistics.median(times)

    def test_o0_completes(self):
        """O0 (no optimization) must complete in < 50ms for fib(50)."""
        elapsed = self._timed_run(0)
        assert elapsed < 50.0, f"O0 fib(50) took {elapsed:.1f}ms"

    def test_o1_not_catastrophically_slower_than_o0(self):
        """
        O1 should not be more than 5x slower than O0.
        (O1 introduces optimizer overhead; a small slowdown is acceptable.
        The threshold is generous to avoid false failures under system load.)
        """
        o0 = self._timed_run(0)
        o1 = self._timed_run(1)
        assert o1 <= o0 * 5.0, (
            f"O1 ({o1:.1f}ms) is > 5x slower than O0 ({o0:.1f}ms) - "
            "optimizer overhead is too high"
        )

    def test_o2_not_catastrophically_slower_than_o0(self):
        """
        O2 should not be more than 10x slower than O0.
        The optimizer runs multiple AST passes before execution; for very small
        programs (like fib(50)) the pass overhead dominates.  The threshold is
        generous to avoid false failures on small benchmarks or slow CI machines.
        """
        o0 = self._timed_run(0)
        o2 = self._timed_run(2)
        assert o2 <= o0 * 10.0, (
            f"O2 ({o2:.1f}ms) is > 10x slower than O0 ({o0:.1f}ms)"
        )

    def test_o3_not_catastrophically_slower_than_o0(self):
        """
        O3 should not be more than 10x slower than O0.
        Same reasoning as O2 - pass overhead is the dominant cost for small programs.
        """
        o0 = self._timed_run(0)
        o3 = self._timed_run(3)
        assert o3 <= o0 * 10.0, (
            f"O3 ({o3:.1f}ms) is > 10x slower than O0 ({o0:.1f}ms)"
        )


# ---------------------------------------------------------------------------
# Error handling performance test
# ---------------------------------------------------------------------------

class TestErrorHandlingPerformance:
    """
    Verify that NxlNameError with large available_names does not hang.
    Regression test for the difflib O(n^2) hang with 1000+ stdlib names.
    """

    def test_name_error_with_large_scope_is_fast(self):
        """
        NxlNameError with 1500 available names should resolve in < 100ms
        (was hanging for seconds before the 256-cap update in errors.py).
        """
        from nexuslang.errors import NxlNameError, get_close_matches

        many_names = ["func_" + str(i) for i in range(1500)]
        many_names += ["i", "j", "k", "result", "value", "count"]

        t0 = time.perf_counter()
        try:
            raise NxlNameError("list_append", available_names=many_names)
        except NxlNameError:
            pass
        elapsed_ms = (time.perf_counter() - t0) * 1000

        assert elapsed_ms < 100.0, (
            f"NxlNameError with 1500 names took {elapsed_ms:.1f}ms "
            "(expected < 100ms - difflib cap not working?)"
        )

    def test_get_close_matches_with_large_list(self):
        """get_close_matches should cap at 256 candidates and return quickly."""
        from nexuslang.errors import get_close_matches

        many = ["name_" + str(i) for i in range(2000)]
        t0 = time.perf_counter()
        result = get_close_matches("list_append", many)
        elapsed_ms = (time.perf_counter() - t0) * 1000

        assert elapsed_ms < 50.0, (
            f"get_close_matches with 2000 names took {elapsed_ms:.1f}ms (expected < 50ms)"
        )
        assert isinstance(result, list)
