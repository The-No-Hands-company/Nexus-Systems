"""Regression tests for parser diagnostic precision and recovery hints."""

import pytest

from nexuslang.errors import NxlSyntaxError
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(source: str):
    tokens = Lexer(source).tokenize()
    return Parser(tokens, source).parse()


def test_memory_allocation_with_value_requires_expression_diagnostic():
    source = "allocate a new Integer in memory with value and name it buffer\n"
    with pytest.raises(NxlSyntaxError) as exc:
        _parse(source)

    message = str(exc.value)
    assert "Expected expression after 'with value'" in message
    assert "Expected:" in message
    assert "Got:" in message


def test_unexpected_token_diagnostic_includes_expected_and_got():
    source = "set x to )\n"
    with pytest.raises(NxlSyntaxError) as exc:
        _parse(source)

    message = str(exc.value)
    assert "Syntax Error" in message
    assert "Expected:" in message
    assert "Got:" in message
