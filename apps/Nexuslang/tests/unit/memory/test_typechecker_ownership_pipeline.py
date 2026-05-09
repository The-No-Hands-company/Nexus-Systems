"""Regression tests for TypeChecker ownership/lifetime pipeline boundaries."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.typechecker import TypeChecker


def _parse(source: str):
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


def test_ownership_pipeline_stops_at_borrow_phase_by_default():
    """Borrow phase failure should stop lifetime phase in strict boundary mode."""
    src = """
set x to 10
set b to borrow x
set y to move x

function leak with p as Integer returns borrow Integer with lifetime outer
    return borrow p with lifetime outer
end
"""
    ast = _parse(src)
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=True)
    errors = checker.check_program(ast)

    assert any("[ownership.borrow]" in e for e in errors)
    assert not any("[ownership.lifetime]" in e for e in errors)


def test_ownership_pipeline_can_continue_to_lifetime_phase():
    """When configured, ownership pipeline should report both borrow and lifetime errors."""
    src = """
set x to 10
set b to borrow x
set y to move x

function leak with p as Integer returns borrow Integer with lifetime outer
    return borrow p with lifetime outer
end
"""
    ast = _parse(src)
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=False)
    errors = checker.check_program(ast)

    assert any("[ownership.borrow]" in e for e in errors)
    assert any("[ownership.lifetime]" in e for e in errors)


def test_lifetime_warnings_are_separate_from_errors():
    """Lifetime warnings should be surfaced via TypeChecker.warnings, not errors."""
    src = """
function unused_lt with x as borrow String with lifetime a returns String
    return x
end
"""
    ast = _parse(src)
    checker = TypeChecker(enable_ownership_passes=True, stop_on_ownership_errors=False)
    errors = checker.check_program(ast)

    assert not any("[ownership.lifetime]" in e for e in errors)
    assert any("[ownership.lifetime.warning]" in w for w in checker.warnings)
