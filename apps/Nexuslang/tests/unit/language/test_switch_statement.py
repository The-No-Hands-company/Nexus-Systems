"""Tests for switch/case/fallthrough statement semantics.

Covers:
- Parser: switch with literal cases, default case, fallthrough
- Interpreter: basic dispatch, no-match default, fallthrough chain
- Typechecker: case type compatibility, duplicate case detection,
               unreachable case flagging
"""

import pytest
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import SwitchStatement, SwitchCase
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.typesystem.typechecker import TypeChecker, TypeCheckError


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _parse(source: str):
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


def _run(source: str):
    ast = _parse(source)
    runtime = Runtime()
    interp = Interpreter(runtime)
    output = []
    runtime.register_function("print", lambda *args: output.append(str(args[0]) if args else ""))
    interp.interpret(ast)
    return output


def _typecheck(source: str):
    ast = _parse(source)
    tc = TypeChecker()
    errors = tc.check_program(ast)
    return errors


# ---------------------------------------------------------------------------
# Parser tests
# ---------------------------------------------------------------------------

class TestSwitchParser:
    def test_switch_basic_parsed(self):
        ast = _parse("""
switch 1
    case 1
        set x to 10
""")
        stmts = ast.statements
        assert any(isinstance(s, SwitchStatement) for s in stmts)

    def test_switch_cases_count(self):
        ast = _parse("""
switch value
    case 1
        set a to 1
    case 2
        set a to 2
    case 3
        set a to 3
""")
        sw = next(s for s in ast.statements if isinstance(s, SwitchStatement))
        assert len(sw.cases) == 3

    def test_switch_default_case(self):
        ast = _parse("""
switch value
    case 1
        set a to 1
    default
        set a to 99
""")
        sw = next(s for s in ast.statements if isinstance(s, SwitchStatement))
        assert sw.default_case is not None
        assert len(sw.default_case) > 0

    def test_switch_case_has_value_and_body(self):
        ast = _parse("""
switch x
    case 42
        set result to "found"
""")
        sw = next(s for s in ast.statements if isinstance(s, SwitchStatement))
        case = sw.cases[0]
        assert isinstance(case, SwitchCase)
        assert case.value is not None
        assert len(case.body) > 0


# ---------------------------------------------------------------------------
# Interpreter tests
# ---------------------------------------------------------------------------

class TestSwitchInterpreter:
    def test_matching_case_executes(self):
        source = """
set x to 2
set result to 0
switch x
    case 1
        set result to 100
    case 2
        set result to 200
    case 3
        set result to 300
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == 200

    def test_no_match_default_executes(self):
        source = """
set x to 99
set result to 0
switch x
    case 1
        set result to 1
    case 2
        set result to 2
    default
        set result to 999
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == 999

    def test_no_match_no_default_returns_none(self):
        source = """
set x to 7
set result to 0
switch x
    case 1
        set result to 1
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == 0

    def test_first_matching_case_wins(self):
        source = """
set x to 1
set result to 0
switch x
    case 1
        set result to 10
    case 1
        set result to 20
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == 10

    def test_switch_on_string(self):
        source = """
set color to "red"
set code to 0
switch color
    case "red"
        set code to 1
    case "green"
        set code to 2
    case "blue"
        set code to 3
    default
        set code to 0
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("code") == 1

    def test_switch_expression_evaluated(self):
        source = """
set x to 1 plus 1
set result to 0
switch x
    case 1
        set result to 1
    case 2
        set result to 2
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == 2

    def test_switch_fallthrough_to_next_case(self):
        source = """
set x to 1
set result to ""
switch x
    case 1
        set result to result plus "A"
        fallthrough
    case 2
        set result to result plus "B"
    case 3
        set result to result plus "C"
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == "AB"

    def test_switch_fallthrough_chain_multiple(self):
        source = """
set x to 1
set result to ""
switch x
    case 1
        set result to result plus "A"
        fallthrough
    case 2
        set result to result plus "B"
        fallthrough
    case 3
        set result to result plus "C"
    case 4
        set result to result plus "D"
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == "ABC"

    def test_switch_fallthrough_into_default(self):
        source = """
set x to 2
set result to ""
switch x
    case 1
        set result to result plus "one"
    case 2
        set result to result plus "two"
        fallthrough
    default
        set result to result plus "-default"
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == "two-default"

    def test_switch_body_creates_no_new_scope_leak(self):
        source = """
set x to 5
switch x
    case 5
        set local_var to 42
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("local_var") == 42

    def test_switch_with_zero_value(self):
        source = """
set x to 0
set result to -1
switch x
    case 0
        set result to 0
    case 1
        set result to 1
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == 0

    def test_switch_with_boolean(self):
        source = """
set flag to true
set result to "unknown"
switch flag
    case true
        set result to "yes"
    case false
        set result to "no"
"""
        ast = _parse(source)
        runtime = Runtime()
        interp = Interpreter(runtime)
        interp.interpret(ast)
        assert interp.get_variable("result") == "yes"


# ---------------------------------------------------------------------------
# Typechecker tests
# ---------------------------------------------------------------------------

class TestSwitchTypechecker:
    def test_switch_integer_cases_no_error(self):
        source = """
set x to 1
switch x
    case 1
        set y to 10
    case 2
        set y to 20
    default
        set y to 0
"""
        errors = _typecheck(source)
        assert errors == [], f"Unexpected type errors: {errors}"

    def test_switch_string_cases_no_error(self):
        source = """
set label to "hello"
switch label
    case "hello"
        set result to 1
    case "world"
        set result to 2
"""
        errors = _typecheck(source)
        assert errors == [], f"Unexpected type errors: {errors}"

    def test_switch_duplicate_case_flagged(self):
        source = """
set x to 1
switch x
    case 1
        set y to 10
    case 1
        set y to 20
"""
        errors = _typecheck(source)
        assert any("duplicate" in e.lower() for e in errors), (
            "Expected duplicate-case error, got: " + str(errors)
        )

    def test_switch_case_type_mismatch_flagged(self):
        # Type mismatch detection requires the typechecker to resolve the switch
        # expression type precisely. Use an annotated declaration via a cast
        # approach or rely on a fresh environment where x was set to an int.
        # For now, we verify that a switch with an integer expression and a string
        # case value raises an error from the typechecker when types are known.
        errors = _typecheck("""
set x to 5
switch x
    case "hello"
        set y to 1
""")
        assert any("incompatible" in e.lower() or "type" in e.lower() for e in errors), (
            "Expected type-mismatch error, got: " + str(errors)
        )

    def test_switch_with_fallthrough_no_typechecker_error(self):
        source = """
set x to 1
switch x
    case 1
        set y to 1
        fallthrough
    case 2
        set y to 2
"""
        errors = _typecheck(source)
        assert errors == [], f"Unexpected type errors: {errors}"

    def test_switch_default_only_no_error(self):
        source = """
set x to 5
switch x
    default
        set y to 0
"""
        errors = _typecheck(source)
        assert errors == [], f"Unexpected type errors: {errors}"
