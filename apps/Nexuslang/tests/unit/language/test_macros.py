"""Language-level macro/comptime fixture regression coverage."""

import os
import sys
from pathlib import Path


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import ComptimeAssert, ComptimeConst, FunctionDefinition, MacroDefinition
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.typechecker import TypeChecker


FIXTURE_PATH = (
    Path(__file__).resolve().parents[3]
    / "test_programs"
    / "integration"
    / "features"
    / "test_macro_comptime_regression.nxl"
)


def _parse_fixture():
    source = FIXTURE_PATH.read_text(encoding="utf-8")
    tokens = Lexer(source).scan_tokens()
    return Parser(tokens).parse()


def test_macro_comptime_regression_fixture_parses_key_nodes():
    ast = _parse_fixture()

    assert any(isinstance(stmt, MacroDefinition) for stmt in ast.statements)
    assert any(isinstance(stmt, ComptimeConst) for stmt in ast.statements)
    assert any(isinstance(stmt, ComptimeAssert) for stmt in ast.statements)
    assert any(isinstance(stmt, FunctionDefinition) and stmt.name == "main" for stmt in ast.statements)


def test_macro_comptime_regression_fixture_typechecks_cleanly():
    ast = _parse_fixture()
    checker = TypeChecker()

    checker.check_program(ast)

    assert checker.errors == []
