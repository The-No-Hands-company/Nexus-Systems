"""Range expression and range-loop parser hardening matrix.

Matrix goals:
- Keep range-based `for ... from ... to ... [by ...]` parsing stable.
- Ensure tuple range syntax `(start, stop[, step])` lowers to `range(...)` call.
- Ensure direct `range(start, stop[, step])` call syntax parses equivalently.
"""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.parser.ast import ForLoop, FunctionCall, VariableDeclaration
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(source: str):
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


RANGE_CALL_CASES = [
    ("range(0, 5)", 2),
    ("range(1, 10)", 2),
    ("range(1, 10, 2)", 3),
    ("range(10, 0, -1)", 3),
]


TUPLE_RANGE_CASES = [
    ("(0, 5)", 2),
    ("(1, 10)", 2),
    ("(1, 10, 2)", 3),
    ("(10, 0, -1)", 3),
]


@pytest.mark.parametrize("expr, arg_count", RANGE_CALL_CASES)
def test_range_call_expression_matrix(expr: str, arg_count: int):
    source = f"set r to {expr}\n"
    ast = _parse(source)

    stmt = ast.statements[0]
    assert isinstance(stmt, VariableDeclaration)
    assert isinstance(stmt.value, FunctionCall)
    assert stmt.value.name == "range"
    assert len(stmt.value.arguments) == arg_count


@pytest.mark.parametrize("expr, arg_count", TUPLE_RANGE_CASES)
def test_tuple_range_expression_matrix(expr: str, arg_count: int):
    source = f"set r to {expr}\n"
    ast = _parse(source)

    stmt = ast.statements[0]
    assert isinstance(stmt, VariableDeclaration)
    assert isinstance(stmt.value, FunctionCall)
    assert stmt.value.name == "range"
    assert len(stmt.value.arguments) == arg_count


RANGE_LOOP_CASES = [
    ("for i from 0 to 5\n    print text i\nend\n", False),
    ("for i from 1 to 10 by 2\n    print text i\nend\n", True),
    ("for i from 10 to 0 by -1\n    print text i\nend\n", True),
]


@pytest.mark.parametrize("source, has_step", RANGE_LOOP_CASES)
def test_range_for_loop_matrix(source: str, has_step: bool):
    ast = _parse(source)

    stmt = ast.statements[0]
    assert isinstance(stmt, ForLoop)
    assert stmt.start is not None
    assert stmt.end is not None
    if has_step:
        assert stmt.step is not None
    else:
        assert stmt.step is None


def test_range_for_loop_by_without_expression_is_syntax_error():
    source = "for i from 0 to 10 by\n    print text i\nend\n"
    with pytest.raises(Exception):
        _parse(source)
