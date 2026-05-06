"""TypeChecker coverage tests for contract/assertion statements."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import Program, Literal, RequireStatement, ExpectStatement
from nexuslang.typesystem.typechecker import TypeChecker


def test_typechecker_accepts_well_typed_require_and_expect():
    ast = Program([
        RequireStatement(Literal("boolean", True), Literal("string", "must hold")),
        ExpectStatement(Literal("integer", 4), "greater_than", Literal("integer", 2)),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []


def test_typechecker_reports_contract_and_expect_type_errors():
    ast = Program([
        RequireStatement(Literal("integer", 1), Literal("integer", 99)),
        ExpectStatement(Literal("string", "a"), "greater_than", Literal("integer", 1)),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Require condition must be a boolean" in err for err in errors)
    assert any("Require message must be a string" in err for err in errors)
    assert any("requires numeric operands" in err for err in errors)
