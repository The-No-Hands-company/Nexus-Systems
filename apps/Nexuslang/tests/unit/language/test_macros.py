"""Language-level macro/comptime fixture regression coverage."""

import os
import sys
from pathlib import Path


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import ComptimeAssert, ComptimeConst, FunctionDefinition, MacroDefinition
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.typechecker import TypeChecker


FIXTURES_DIR = (
    Path(__file__).resolve().parents[3]
    / "test_programs"
    / "integration"
    / "features"
)


VALID_FIXTURE_FILES = [
    "test_macro_comptime_regression.nxl",
    "test_macro_comptime_hygiene_regression.nxl",
]

INVALID_FIXTURE_FILE = "test_macro_comptime_invalid_regression.nxl"


def _parse_fixture(filename):
    source = (FIXTURES_DIR / filename).read_text(encoding="utf-8")
    tokens = Lexer(source).scan_tokens()
    return Parser(tokens).parse()


def test_macro_comptime_regression_fixtures_parse_key_nodes():
    for filename in VALID_FIXTURE_FILES:
        ast = _parse_fixture(filename)

        assert any(isinstance(stmt, MacroDefinition) for stmt in ast.statements)
        assert any(isinstance(stmt, ComptimeConst) for stmt in ast.statements)
        assert any(isinstance(stmt, ComptimeAssert) for stmt in ast.statements)
        assert any(isinstance(stmt, FunctionDefinition) and stmt.name == "main" for stmt in ast.statements)


def test_macro_comptime_regression_fixture_typechecks_cleanly():
    for filename in VALID_FIXTURE_FILES:
        ast = _parse_fixture(filename)
        checker = TypeChecker()

        checker.check_program(ast)

        assert checker.errors == []


def test_macro_comptime_invalid_regression_fixture_surfaces_type_errors():
    ast = _parse_fixture(INVALID_FIXTURE_FILE)
    checker = TypeChecker()

    checker.check_program(ast)

    assert checker.errors != []
    assert any("comptime assert condition must be boolean" in error for error in checker.errors)
