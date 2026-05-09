"""
Compiler round-trip integration tests: NexusLang source -> LLVM IR -> native binary -> execute.

Each test compiles a small NexusLang program all the way to a native executable (via the
LLVMIRGenerator + LLVM/clang tool chain), runs it, and checks stdout against the
expected output.

Requirements
------------
- LLVM tools (llc, opt) installed and on PATH
- clang installed and on PATH

Tests are automatically *skipped* when the required tools are absent so CI
environments without LLVM installed do not fail.
"""

import os
import subprocess
import sys
import tempfile

import pytest

# Ensure the src/ tree is importable regardless of how pytest is invoked.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import MatchExpression, OptionPattern, ResultPattern
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


# ---------------------------------------------------------------------------
# Fixtures / helpers
# ---------------------------------------------------------------------------

def _tools_available() -> bool:
    """Return True when llc, opt AND clang are all on PATH."""
    import shutil
    return all(shutil.which(t) for t in ("llc", "opt", "clang"))


skip_no_llvm = pytest.mark.skipif(
    not _tools_available(),
    reason="LLVM tools (llc, opt, clang) not available on PATH"
)


def compile_and_run(source: str) -> tuple[int, str, str]:
    """Compile *source* to a native binary and run it.

    Returns ``(returncode, stdout, stderr)``.

    Raises ``CompilationFailed`` if any compilation step returns a non-zero
    exit code.
    """
    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()

    gen = LLVMIRGenerator()
    gen.generate(ast, source_file="test.nxl")

    with tempfile.TemporaryDirectory() as tmpdir:
        exe = os.path.join(tmpdir, "test_prog")

        # LLVMIRGenerator.compile_to_executable() drives opt -> llc -> clang.
        # It prints progress banners; suppress them so test output is clean.
        import io
        saved = sys.stdout
        sys.stdout = io.StringIO()
        try:
            ok = gen.compile_to_executable(exe, opt_level=0)
        finally:
            sys.stdout = saved

        if not ok or not os.path.exists(exe):
            raise CompilationFailed("compile_to_executable() returned False")

        result = subprocess.run([exe], capture_output=True, text=True, timeout=15)
        return result.returncode, result.stdout, result.stderr


def compile_ast_and_run(ast) -> tuple[int, str, str]:
    """Compile an already-parsed AST to native code and execute it."""
    gen = LLVMIRGenerator()
    gen.generate(ast, source_file="test.nxl")

    with tempfile.TemporaryDirectory() as tmpdir:
        exe = os.path.join(tmpdir, "test_prog")

        import io
        saved = sys.stdout
        sys.stdout = io.StringIO()
        try:
            ok = gen.compile_to_executable(exe, opt_level=0)
        finally:
            sys.stdout = saved

        if not ok or not os.path.exists(exe):
            raise CompilationFailed("compile_to_executable() returned False")

        result = subprocess.run([exe], capture_output=True, text=True, timeout=15)
        return result.returncode, result.stdout, result.stderr


def _parse_program(source: str):
    tokens = Lexer(source).tokenize()
    return Parser(tokens).parse()


def _first_match_expression(ast) -> MatchExpression:
    for stmt in getattr(ast, "statements", []):
        body = getattr(stmt, "body", None) or []
        for body_stmt in body:
            if isinstance(body_stmt, MatchExpression):
                return body_stmt
    raise AssertionError("Expected at least one match expression")


class CompilationFailed(Exception):
    pass


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

@skip_no_llvm
class TestCompilerRoundtrip:
    """Each method compiles a specific NexusLang program and checks the native output."""

    # --- basic output -------------------------------------------------------

    def test_hello_world(self):
        src = """
function main returns Integer
    print text "hello world"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "hello world\n"

    def test_print_integer(self):
        src = """
function main returns Integer
    print text 42
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    # --- arithmetic ---------------------------------------------------------

    def test_integer_multiplication(self):
        src = """
function main returns Integer
    set x to 6
    set y to 7
    set result to x times y
    print text result
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_integer_addition(self):
        src = """
function main returns Integer
    set a to 100
    set b to 23
    print text a plus b
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "123\n"

    def test_integer_subtraction(self):
        src = """
function main returns Integer
    set x to 50
    set y to 8
    print text x minus y
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    # --- control flow -------------------------------------------------------

    def test_if_true_branch(self):
        src = """
function main returns Integer
    set x to 42
    if x is greater than 10
        print text "big"
    else
        print text "small"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "big\n"

    def test_if_false_branch(self):
        src = """
function main returns Integer
    set x to 5
    if x is greater than 10
        print text "big"
    else
        print text "small"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "small\n"

    def test_while_loop(self):
        src = """
function main returns Integer
    set i to 0
    set sum to 0
    while i is less than 5
        set sum to sum plus i
        set i to i plus 1
    end
    print text sum
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "10\n"

    # --- functions ----------------------------------------------------------

    def test_function_call_no_args(self):
        src = """
function greet returns Integer
    print text "hi"
    return 0
end

function main returns Integer
    call greet
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "hi\n"

    def test_function_with_argument(self):
        src = """
function double with n as Integer returns Integer
    return n times 2
end

function main returns Integer
    set result to double with 21
    print text result
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_yielded_function_preserves_spilled_locals_across_calls(self):
        src = """
function generate_values with start as Integer returns Integer
    set current to start
    set delta to 3
    yield current
    set current to current plus delta
    yield current
    return current plus delta
end

function main returns Integer
    print text generate_values with 5
    print text generate_values with 999
    print text generate_values with 123
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "5\n8\n11\n"

    def test_yielded_function_returns_zero_after_exhaustion(self):
        src = """
function emit_once with start as Integer returns Integer
    yield start
end

function main returns Integer
    print text emit_once with 7
    print text emit_once with 99
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "7\n0\n"

    def test_interleaved_yielded_functions_keep_isolated_state(self):
        src = """
function gen_a returns Integer
    yield 1
    yield 2
end

function gen_b returns Integer
    yield 10
    yield 20
end

function main returns Integer
    print text call gen_a
    print text call gen_b
    print text call gen_a
    print text call gen_b
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "1\n10\n2\n20\n"

    def test_top_level_comptime_const_initializes_before_user_main(self):
        src = """
comptime const LIMIT is 5
comptime assert LIMIT is equal to 5

function add_limit with n as Integer returns Integer
    return n plus LIMIT
end

function main returns Integer
    print text add_limit with 3
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "8\n"

    def test_top_level_variable_initializes_before_user_main(self):
        src = """
set base to 10

function add_limit with n as Integer returns Integer
    return n plus base
end

function main returns Integer
    print text add_limit with 5
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "15\n"

    def test_macro_expansion_runs_in_compiled_path(self):
        src = """
macro GREET
    print text "hello from macro"
end

function main returns Integer
    expand GREET
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "hello from macro\n"

    def test_nested_macro_expansion_runs_in_compiled_path(self):
        src = """
macro INNER with msg
    print text msg
end

macro OUTER with value
    expand INNER with msg value
end

function main returns Integer
    expand OUTER with value "nested hello"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "nested hello\n"

    def test_macro_local_name_collision_is_hygienic(self):
        src = """
macro SHOW_LOCAL with value
    set temp to value
    print text temp
end

function main returns Integer
    set temp to 100
    expand SHOW_LOCAL with value 7
    print text temp
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "7\n100\n"

    def test_function_returns_value(self):
        src = """
function add with a as Integer and b as Integer returns Integer
    return a plus b
end

function main returns Integer
    print text add with 10 and 32
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_recursive_fibonacci(self):
        """Recursive fibonacci: validates recursive function calls and proper
        binary-expression parsing for 'fib with n minus 1 plus fib with n minus 2'."""
        src = """
function fib with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    return fib with n minus 1 plus fib with n minus 2
end

function main returns Integer
    set result to fib with 10
    print text result
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "55\n"

    def test_iterative_fibonacci(self):
        src = """
function fib_iter with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    set a to 0
    set b to 1
    set i to 2
    while i is less than or equal to n
        set c to a plus b
        set a to b
        set b to c
        set i to i plus 1
    end
    return b
end

function main returns Integer
    print text fib_iter with 20
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "6765\n"

    # --- return code --------------------------------------------------------

    def test_return_code_zero(self):
        src = """
function main returns Integer
    return 0
end
"""
        rc, _, _ = compile_and_run(src)
        assert rc == 0

    def test_return_code_nonzero(self):
        src = """
function main returns Integer
    return 42
end
"""
        rc, _, _ = compile_and_run(src)
        assert rc == 42


@skip_no_llvm
class TestArithmeticOperations:
    """Extended arithmetic and comparison operations."""

    def test_integer_division(self):
        src = """
function main returns Integer
    set x to 100
    set y to x divided by 4
    print text y
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "25\n"

    def test_modulo(self):
        src = """
function main returns Integer
    set x to 17
    set y to x modulo 5
    print text y
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "2\n"

    def test_combined_arithmetic(self):
        """Tests operator precedence and multi-step arithmetic."""
        src = """
function main returns Integer
    set a to 10
    set b to 3
    set sum to a plus b
    set diff to a minus b
    set prod to a times b
    set quot to a divided by b
    print text sum
    print text diff
    print text prod
    print text quot
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "13\n7\n30\n3\n"

    def test_variable_reassignment(self):
        src = """
function main returns Integer
    set x to 1
    set x to x plus 10
    set x to x times 2
    print text x
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "22\n"


@skip_no_llvm
class TestComparisonOperators:
    """All six comparison operators compiled to native code."""

    def test_less_than_or_equal(self):
        src = """
function main returns Integer
    set x to 10
    if x is less than or equal to 10
        print text "yes"
    else
        print text "no"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "yes\n"

    def test_greater_than_or_equal(self):
        src = """
function main returns Integer
    set x to 10
    if x is greater than or equal to 10
        print text "yes"
    else
        print text "no"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "yes\n"

    def test_not_equal(self):
        src = """
function main returns Integer
    set x to 10
    if x is not equal to 5
        print text "different"
    else
        print text "same"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "different\n"

    def test_equal_to(self):
        src = """
function main returns Integer
    set x to 42
    if x is equal to 42
        print text "match"
    else
        print text "no match"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "match\n"

    def test_string_equality(self):
        src = """
function main returns Integer
    set name to "alice"
    if name is equal to "alice"
        print text "found"
    else
        print text "not found"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "found\n"


@skip_no_llvm
class TestControlFlow:
    """Extended control flow: nested if/else, boolean operators, match."""

    def test_nested_if_else(self):
        src = """
function main returns Integer
    set x to 15
    if x is greater than 20
        print text "big"
    else if x is greater than 10
        print text "medium"
    else
        print text "small"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "medium\n"

    def test_logical_and(self):
        src = """
function main returns Integer
    set x to 10
    if x is greater than 5 and x is less than 20
        print text "in range"
    else
        print text "out of range"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "in range\n"

    def test_logical_or(self):
        src = """
function main returns Integer
    set x to 0
    if x is equal to 5 or x is equal to 0
        print text "found"
    else
        print text "nope"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "found\n"

    def test_match_integer(self):
        src = """
function main returns Integer
    set x to 2
    match x with
        case 1
            print text "one"
        case 2
            print text "two"
        case 3
            print text "three"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "two\n"

    def test_match_identifier_binding_executes_bound_value(self):
        src = """
function main returns Integer
    set x to 9
    match x with
        case n
            print text n
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "9\n"

    def test_match_guard_selects_correct_case(self):
        src = """
function main returns Integer
    set x to 2
    match x with
        case n if n is greater than 3
            print text "big"
        case _
            print text "small"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "small\n"

    # --- contracts / assertions --------------------------------------------

    def test_require_passes_when_condition_true(self):
        src = """
function main returns Integer
    require 5 is greater than 3 message "Should not fail"
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_require_fails_when_condition_false(self):
        src = """
function main returns Integer
    require 1 is equal to 2 message "This contract will fail"
    print text "should not reach"
    return 0
end
"""
        rc, out, err = compile_and_run(src)
        assert rc != 0  # Should exit with error
        # Panic should be called
        assert "panic" in err.lower() or rc != 0

    def test_ensure_passes_when_condition_true(self):
        src = """
function calculate_value returns Integer
    set result to 42
    ensure result is greater than 0 message "Result must be positive"
    return result
end

function main returns Integer
    set value to call calculate_value
    print text value
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_ensure_fails_when_condition_false(self):
        src = """
function calculate_bad returns Integer
    set result to -5
    ensure result is greater than 0 message "Result must be positive"
    return result
end

function main returns Integer
    set value to call calculate_bad
    print text value
    return 0
end
"""
        rc, out, err = compile_and_run(src)
        assert rc != 0  # Should exit with error

    def test_expect_equal_passes(self):
        src = """
function main returns Integer
    expect 2 plus 2 to equal 4
    print text "test passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "test passed\n"

    def test_expect_equal_fails(self):
        src = """
function main returns Integer
    expect 2 plus 2 to equal 5
    print text "should not reach"
    return 0
end
"""
        rc, out, err = compile_and_run(src)
        assert rc != 0

    def test_expect_greater_than_passes(self):
        src = """
function main returns Integer
    expect 10 to be greater than 5
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_expect_less_than_passes(self):
        src = """
function main returns Integer
    expect 3 to be less than 7
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_expect_be_true_passes(self):
        src = """
function main returns Integer
    set condition to 5 is greater than 3
    expect condition to be true
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_expect_be_false_passes(self):
        src = """
function main returns Integer
    set condition to 2 is greater than 5
    expect condition to be false
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_expect_string_contain_passes(self):
        src = """
function main returns Integer
    expect "hello world" to contain "world"
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_expect_string_start_with_passes(self):
        src = """
function main returns Integer
    expect "hello world" to start with "hello"
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_expect_string_end_with_passes(self):
        src = """
function main returns Integer
    expect "hello world" to end with "world"
    print text "passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "passed\n"

    def test_multiple_requires_in_sequence(self):
        src = """
function main returns Integer
    require 1 is greater than 0 message "First"
    require 2 is greater than 0 message "Second"
    require 3 is greater than 0 message "Third"
    print text "all passed"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "all passed\n"

    def test_require_in_function_parameter_validation(self):
        src = """
function divide with numerator as Integer and denominator as Integer returns Integer
    require denominator is not equal to 0 message "Cannot divide by zero"
    set result to numerator divided by denominator
    return result
end

function main returns Integer
    set answer to divide with 10 and 2
    print text answer
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "5\n"

    def test_require_in_function_fails_on_invalid_input(self):
        src = """
function divide with numerator as Integer and denominator as Integer returns Integer
    require denominator is not equal to 0 message "Cannot divide by zero"
    set result to numerator divided by denominator
    return result
end

function main returns Integer
    set answer to divide with 10 and 0
    print text answer
    return 0
end
"""
        rc, out, err = compile_and_run(src)
        assert rc != 0

    # --- exception handling -------------------------------------------------

    def test_try_catch_catches_raised_error_message(self):
        src = """
function main returns Integer
    try
        raise error with message "boom"
    catch err
        print text err
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "boom\n"

    def test_try_catch_continues_execution_after_catch(self):
        src = """
function main returns Integer
    try
        raise error with message "inner"
    catch err
        print text err
    end
    print text "after catch"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "inner\nafter catch\n"

    def test_uncaught_raise_exits_nonzero(self):
        src = """
function main returns Integer
    raise error with message "fatal"
    print text "unreachable"
    return 0
end
"""
        rc, out, err = compile_and_run(src)
        assert rc != 0
        assert "fatal" in out.lower() or "fatal" in err.lower() or "panic" in err.lower() or err.strip() != ""

    def test_nested_try_catch_inner_caught_outer_not_triggered(self):
        src = """
function main returns Integer
    try
        try
            raise error with message "inner"
        catch inner_err
            print text inner_err
        end
        print text "outer continues"
    catch outer_err
        print text "unexpected outer catch"
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "inner\nouter continues\n"


@skip_no_llvm
class TestRuntimeBackedPatternRoundtrip:
    """Executable roundtrip coverage for runtime-backed advanced match patterns.

    These tests intentionally lock current behavior for Option/Result/Tuple/List
    runtime-backed matching in the native pipeline.
    """

    def test_option_pattern_runtime_backed_roundtrip(self):
        src = """
function main returns Integer
    set opt to 7
    match opt with
        case payload
            print text payload
        case _
            print text 0
    end
    return 0
end
"""
        ast = _parse_program(src)
        match_stmt = _first_match_expression(ast)
        match_stmt.cases[0].pattern = OptionPattern("Some", "payload")
        match_stmt.cases[1].pattern = OptionPattern("None", None)

        with pytest.raises(
            RuntimeError,
            match="OptionPattern requires runtime-backed pointer representation",
        ):
            compile_ast_and_run(ast)

    def test_result_pattern_runtime_backed_roundtrip(self):
        src = """
function main returns Integer
    set res to 0
    match res with
        case item
            print text item
        case _
            print text 0
    end
    return 0
end
"""
        ast = _parse_program(src)
        match_stmt = _first_match_expression(ast)
        match_stmt.cases[0].pattern = ResultPattern("Ok", "value")
        match_stmt.cases[1].pattern = ResultPattern("Err", "error_msg")

        with pytest.raises(
            RuntimeError,
            match="ResultPattern requires runtime-backed pointer representation",
        ):
            compile_ast_and_run(ast)

    def test_result_err_pattern_preserves_scalar_payload_roundtrip(self):
        src = """
function main returns Integer
    set res to -7
    match res with
        case item
            print text item
        case _
            print text 0
    end
    return 0
end
"""
        ast = _parse_program(src)
        match_stmt = _first_match_expression(ast)
        match_stmt.cases[0].pattern = ResultPattern("Err", "item")
        match_stmt.cases[1].pattern = ResultPattern("Ok", "value")

        with pytest.raises(
            RuntimeError,
            match="ResultPattern requires runtime-backed pointer representation",
        ):
            compile_ast_and_run(ast)

    def test_tuple_pattern_runtime_backed_roundtrip(self):
        src = """
function main returns Integer
    set pair to [2, 3]
    match pair with
        case (x, y)
            print text x
        case _
            print text 0
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "2\n"

    def test_list_pattern_runtime_backed_roundtrip(self):
        src = """
function main returns Integer
    set nums to [1, 2, 3]
    match nums with
        case [head, ...tail]
            print text head
        case _
            print text 0
    end
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "1\n"

    def test_option_pattern_typed_binding_mixed_payload_lowering(self):
        """LLVM lowering should use typed Option payload extractor fast path when type metadata is present."""
        generator = LLVMIRGenerator()
        pattern = OptionPattern("Some", "payload", binding_type_annotation="Integer")

        generator._generate_option_pattern_binding(pattern, "%opt", "i8*", "  ")
        ir = "\n".join(generator.ir_lines)

        assert "call i64 @NLPL_Optional_get_value_i64(i8* %opt)" in ir
        assert "call i32 @NLPL_Optional_get_value_kind(i8* %opt)" not in ir

    def test_result_pattern_typed_binding_mixed_payload_lowering(self):
        """LLVM lowering should use typed Result payload extractor fast path when type metadata is present."""
        generator = LLVMIRGenerator()
        pattern = ResultPattern("Err", "error_payload", binding_type_annotation="Float")

        generator._generate_result_pattern_binding(pattern, "%res", "i8*", "  ")
        ir = "\n".join(generator.ir_lines)

        assert "call double @NLPL_Result_get_error_f64(i8* %res)" in ir
        assert "call i32 @NLPL_Result_get_error_kind(i8* %res)" not in ir


@skip_no_llvm
class TestOwnershipRoundtrip:
    """Executable ownership semantics coverage in compiled backends."""

    def test_move_transfers_value_and_invalidates_source(self):
        src = """
function main returns Integer
    set value to 41
    set moved to move value
    print text moved
    print text value
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "41\n0\n"

    def test_borrow_reads_source_snapshot(self):
        src = """
function main returns Integer
    set value to 7
    set ref to borrow value
    set value to 9
    print text ref
    print text value
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "7\n9\n"

    def test_drop_borrow_allows_followup_assignment_path(self):
        src = """
function main returns Integer
    set value to 5
    set ref to borrow value
    drop borrow value
    set value to value plus 10
    print text ref
    print text value
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "5\n15\n"

    def test_drop_mutable_borrow_assignment_path(self):
        src = """
function main returns Integer
    set value to 2
    set mut_ref to borrow mutable value
    drop borrow mutable value
    set value to 12
    print text mut_ref
    print text value
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "2\n12\n"


@skip_no_llvm
class TestAsyncCoroutineRoundtrip:
    """Executable async/await coroutine frame coverage via LLVM native roundtrip."""

    def test_async_main_awaits_async_function_and_prints_value(self):
        src = """
async function fetch returns Integer
    return 7
end

async function main returns Integer
    set value to await fetch
    print text value
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "7\n"

    def test_nested_async_await_chain_preserves_result_through_coroutine_frames(self):
        src = """
async function seed returns Integer
    return 41
end

async function compute returns Integer
    set base to await seed
    return base plus 1
end

async function main returns Integer
    set result to await compute
    print text result
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_mixed_sync_async_call_graph_with_multiple_await_points(self):
        src = """
function scale with n as Integer returns Integer
    return n times 2
end

async function add_one with n as Integer returns Integer
    return n plus 1
end

async function main returns Integer
    set base to scale with 20
    set step1 to await add_one with base
    set step2 to await add_one with step1
    print text step2
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_async_error_path_propagates_contract_failure(self):
        src = """
async function fail_async returns Integer
    require false message "async contract failed"
    return 1
end

async function main returns Integer
    set value to await fail_async
    print text value
    return 0
end
"""
        rc, out, err = compile_and_run(src)
        assert rc != 0
        combined = out + err
        assert "async contract failed" in combined


@skip_no_llvm
class TestLoops:
    """Extended loop constructs: break, continue, nested loops."""

    def test_break_in_while(self):
        src = """
function main returns Integer
    set i to 0
    while i is less than 100
        if i is equal to 5
            break
        end
        set i to i plus 1
    end
    print text i
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "5\n"

    def test_continue_in_while(self):
        """Sum 1..10 skipping 3 and 7: 1+2+4+5+6+8+9+10 = 45."""
        src = """
function main returns Integer
    set sum to 0
    set i to 0
    while i is less than 10
        set i to i plus 1
        if i is equal to 3
            continue
        end
        if i is equal to 7
            continue
        end
        set sum to sum plus i
    end
    print text sum
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "45\n"

    def test_nested_loops(self):
        """5x5 nested loop counting iterations."""
        src = """
function main returns Integer
    set count to 0
    set i to 0
    while i is less than 5
        set j to 0
        while j is less than 5
            set count to count plus 1
            set j to j plus 1
        end
        set i to i plus 1
    end
    print text count
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "25\n"

    def test_loop_accumulator(self):
        """Sum of 1+2+...+100 = 5050."""
        src = """
function main returns Integer
    set sum to 0
    set i to 1
    while i is less than or equal to 100
        set sum to sum plus i
        set i to i plus 1
    end
    print text sum
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "5050\n"


@skip_no_llvm
class TestFunctions:
    """Extended function tests: multiple args, mutual recursion, chaining."""

    def test_multiple_arguments(self):
        src = """
function add with a as Integer and b as Integer returns Integer
    return a plus b
end

function main returns Integer
    print text add with 10 and 32
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "42\n"

    def test_chained_calls(self):
        src = """
function increment with n as Integer returns Integer
    return n plus 1
end

function double with n as Integer returns Integer
    return n times 2
end

function main returns Integer
    set a to increment with 4
    set b to double with a
    print text b
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "10\n"

    def test_nested_function_calls(self):
        """Nested call: add_one(square(5)) = 26."""
        src = """
function square with n as Integer returns Integer
    return n times n
end

function add_one with n as Integer returns Integer
    return n plus 1
end

function main returns Integer
    set result to add_one with square with 5
    print text result
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "26\n"

    def test_mutual_recursion(self):
        """is_even/is_odd via mutual recursion."""
        src = """
function is_even with n as Integer returns Integer
    if n is equal to 0
        return 1
    end
    return is_odd with n minus 1
end

function is_odd with n as Integer returns Integer
    if n is equal to 0
        return 0
    end
    return is_even with n minus 1
end

function main returns Integer
    print text is_even with 10
    print text is_odd with 10
    print text is_even with 7
    print text is_odd with 7
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "1\n0\n0\n1\n"

    def test_multiple_return_paths(self):
        """Function with early returns from different branches."""
        src = """
function classify with n as Integer returns Integer
    if n is greater than 100
        return 3
    end
    if n is greater than 10
        return 2
    end
    return 1
end

function main returns Integer
    print text classify with 5
    print text classify with 50
    print text classify with 500
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "1\n2\n3\n"


@skip_no_llvm
class TestStrings:
    """String operations in compiled output."""

    def test_string_concatenation(self):
        src = """
function main returns Integer
    set greeting to "hello" plus " world"
    print text greeting
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "hello world\n"

    def test_multiple_print_statements(self):
        src = """
function main returns Integer
    print text "a"
    print text "b"
    print text "c"
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "a\nb\nc\n"


@skip_no_llvm
class TestArrays:
    """Array creation, indexing, and iteration."""

    def test_array_literal_indexing(self):
        src = """
function main returns Integer
    set arr to [10, 20, 30, 40, 50]
    print text arr[0]
    print text arr[2]
    print text arr[4]
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "10\n30\n50\n"

    def test_array_sum_loop(self):
        """Iterate over array by index and sum elements."""
        src = """
function main returns Integer
    set arr to [10, 20, 30, 40, 50]
    set sum to 0
    set i to 0
    while i is less than 5
        set sum to sum plus arr[i]
        set i to i plus 1
    end
    print text sum
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "150\n"


@skip_no_llvm
class TestAlgorithms:
    """Real-world algorithms compiled end-to-end."""

    def test_gcd_euclidean(self):
        """Greatest common divisor using Euclidean algorithm."""
        src = """
function gcd with a as Integer and b as Integer returns Integer
    while b is not equal to 0
        set temp to b
        set b to a modulo b
        set a to temp
    end
    return a
end

function main returns Integer
    print text gcd with 48 and 18
    print text gcd with 100 and 75
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "6\n25\n"

    def test_factorial_iterative(self):
        """Iterative factorial: 10! = 3628800."""
        src = """
function factorial with n as Integer returns Integer
    set result to 1
    set i to 2
    while i is less than or equal to n
        set result to result times i
        set i to i plus 1
    end
    return result
end

function main returns Integer
    print text factorial with 10
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "3628800\n"

    def test_power_function(self):
        """Iterative exponentiation: 2^10 = 1024."""
        src = """
function int_pow with base as Integer and exp as Integer returns Integer
    set result to 1
    set i to 0
    while i is less than exp
        set result to result times base
        set i to i plus 1
    end
    return result
end

function main returns Integer
    print text int_pow with 2 and 10
    print text int_pow with 3 and 5
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "1024\n243\n"

    def test_collatz_steps(self):
        """Count Collatz steps from 27 to 1 (should be 111)."""
        src = """
function collatz_steps with n as Integer returns Integer
    set steps to 0
    while n is not equal to 1
        if n modulo 2 is equal to 0
            set n to n divided by 2
        else
            set n to n times 3 plus 1
        end
        set steps to steps plus 1
    end
    return steps
end

function main returns Integer
    print text collatz_steps with 27
    return 0
end
"""
        rc, out, _ = compile_and_run(src)
        assert rc == 0
        assert out == "111\n"


@skip_no_llvm
class TestOptimizationLevels:
    """Verify that -O3 produces correct output (not just that it compiles)."""

    def test_fibonacci_O3_correctness(self):
        """Fibonacci at -O3 must produce the same result as -O0."""
        src = """
function fib with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    set a to 0
    set b to 1
    set i to 2
    while i is less than or equal to n
        set c to a plus b
        set a to b
        set b to c
        set i to i plus 1
    end
    return b
end

function main returns Integer
    print text fib with 30
    return 0
end
"""
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens).parse()

        gen_o0 = LLVMIRGenerator()
        gen_o0.generate(ast, source_file="test.nxl")

        gen_o3 = LLVMIRGenerator()
        gen_o3.generate(ast, source_file="test.nxl")

        import tempfile, io
        with tempfile.TemporaryDirectory() as d:
            # Compile at O0
            exe_o0 = os.path.join(d, "test_o0")
            old = sys.stdout; sys.stdout = io.StringIO()
            try:
                ok0 = gen_o0.compile_to_executable(exe_o0, opt_level=0)
            finally:
                sys.stdout = old
            assert ok0

            # Compile at O3
            exe_o3 = os.path.join(d, "test_o3")
            sys.stdout = io.StringIO()
            try:
                ok3 = gen_o3.compile_to_executable(exe_o3, opt_level=3)
            finally:
                sys.stdout = old

            assert ok3

            r0 = subprocess.run([exe_o0], capture_output=True, text=True, timeout=15)
            r3 = subprocess.run([exe_o3], capture_output=True, text=True, timeout=15)

            assert r0.stdout == r3.stdout == "832040\n"
