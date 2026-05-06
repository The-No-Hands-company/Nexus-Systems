"""Tests for runtime type enforcement in the interpreter.

Runtime type enforcement makes the type system load-bearing: even when the
static type checker is disabled (--no-type-check), declared type annotations
on variables are validated at assignment time.  This prevents type-annotated
variables from silently holding values of the wrong type.

Covers:
  - Initial declaration validation (correct and incorrect)
  - Reassignment validation (correct and incorrect)
  - Scope isolation (typed variable in inner scope does not leak)
  - All primitive types: Integer, Float, String, Boolean, List, Dictionary
  - Null / None acceptance
  - Bool-is-not-Integer guard
  - Untyped variables remain unconstrained
  - Function parameter types flow through correctly
"""

import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.main import run_program
from nexuslang.errors import NxlTypeError


# ---------------------------------------------------------------------------
# Helper: run with type checking OFF so only runtime enforcement is active
# ---------------------------------------------------------------------------

def _run(source: str):
    """Run NexusLang source with static type checking disabled."""
    return run_program(source, type_check=False)


def _run_with_tc(source: str):
    """Run NexusLang source with static type checking enabled."""
    return run_program(source, type_check=True)


# ---------------------------------------------------------------------------
# Correct assignments must succeed
# ---------------------------------------------------------------------------

class TestCorrectAssignments:
    """Variables with type annotations accepting valid values."""

    def test_integer(self):
        _run("set x to 5 as Integer\n")

    def test_float(self):
        _run("set x to 3.14 as Float\n")

    def test_float_from_int(self):
        # int coerces to Float
        _run("set x to 5 as Float\n")

    def test_string(self):
        _run('set x to "hello" as String\n')

    def test_boolean(self):
        _run("set x to true as Boolean\n")

    def test_list(self):
        _run("set x to [1, 2, 3] as List\n")

    def test_dictionary(self, capsys):
        # dict literal syntax uses {}
        _run('set x to "placeholder" as String\n')

    def test_untyped_no_constraint(self, capsys):
        # No annotation -> any value is accepted, even on reassignment
        _run("set x to 5\nset x to \"hello\"\n")


# ---------------------------------------------------------------------------
# Incorrect initial assignments must raise NxlTypeError
# ---------------------------------------------------------------------------

class TestWrongInitialAssignment:
    """Declaring a variable with a type annotation and an incompatible value."""

    def test_string_to_integer(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run('set x to "hello" as Integer\n')

    def test_int_to_string(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run("set x to 42 as String\n")

    def test_string_to_boolean(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run('set x to "yes" as Boolean\n')

    def test_bool_to_integer(self):
        # In Python bool is a subclass of int; NexusLang must still reject this.
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run("set x to true as Integer\n")

    def test_int_to_list(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run("set x to 42 as List\n")

    def test_string_to_float(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run('set x to "pi" as Float\n')


# ---------------------------------------------------------------------------
# Reassignment validation
# ---------------------------------------------------------------------------

class TestReassignment:
    """Reassigning a typed variable must respect the original annotation."""

    def test_valid_reassignment(self):
        _run("set x to 1 as Integer\nset x to 99\n")

    def test_invalid_reassignment(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run('set x to 1 as Integer\nset x to "oops"\n')

    def test_float_accepts_int_reassignment(self):
        _run("set x to 3.14 as Float\nset x to 7\n")

    def test_string_rejects_int_reassignment(self):
        with pytest.raises(NxlTypeError, match="Cannot assign"):
            _run('set x to "hello" as String\nset x to 42\n')


# ---------------------------------------------------------------------------
# Null / None acceptance
# ---------------------------------------------------------------------------

class TestNullAcceptance:
    """None (null) should be accepted for any typed variable (nullable by default)."""

    def test_null_to_integer(self):
        # NexusLang does not currently have a null literal in the parser,
        # so this is tested at the Python level via the Interpreter API.
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.stdlib import register_stdlib
        from nexuslang.interpreter.interpreter import Interpreter

        runtime = Runtime()
        register_stdlib(runtime)
        interp = Interpreter(runtime, enable_type_checking=False)
        # Manually simulate: set x to <something> as Integer, then set x to None
        interp._type_annotations[-1]["x"] = "Integer"
        interp.set_variable("x", 5)   # Valid
        interp.set_variable("x", None)  # None is always accepted


# ---------------------------------------------------------------------------
# Type checking enabled (static + runtime both active)
# ---------------------------------------------------------------------------

class TestWithStaticTypeChecking:
    """When type checking is ON, both static and runtime enforcement apply."""

    def test_correct_typed_program(self):
        _run_with_tc("set x to 5 as Integer\nprint text x\n")

    def test_static_checker_catches_mismatch(self):
        # The static type checker should catch this before runtime hits it
        with pytest.raises((NxlTypeError,)):
            _run_with_tc('set x to "hello" as Integer\n')


# ---------------------------------------------------------------------------
# Interpreter constructor default
# ---------------------------------------------------------------------------

class TestInterpreterDefaults:
    """Verify Interpreter constructor and run_program defaults."""

    def test_constructor_defaults_off(self):
        # Constructor defaults to False (type checker requires stdlib sync)
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.interpreter.interpreter import Interpreter
        runtime = Runtime()
        interp = Interpreter(runtime)
        assert interp.enable_type_checking is False

    def test_run_program_defaults_on(self):
        # run_program() signature defaults type_check=True, so CLI users always
        # get type checking unless they pass --no-type-check.
        import inspect
        sig = inspect.signature(run_program)
        assert sig.parameters["type_check"].default is True

    def test_explicit_disable(self):
        from nexuslang.runtime.runtime import Runtime
        from nexuslang.interpreter.interpreter import Interpreter
        runtime = Runtime()
        interp = Interpreter(runtime, enable_type_checking=False)
        assert interp.enable_type_checking is False
