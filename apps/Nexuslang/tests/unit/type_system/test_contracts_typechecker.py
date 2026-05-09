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


def test_typechecker_expect_contain_validates_container_and_expected_types():
    ast = Program([
        # Invalid: integer has no containment semantics
        ExpectStatement(Literal("integer", 7), "contain", Literal("integer", 1)),
        # Invalid: string containment expects a string expected value
        ExpectStatement(Literal("string", "hello"), "contain", Literal("integer", 1)),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("expect contain requires a string/list/dictionary" in err for err in errors)
    assert any("expect contain on strings requires a string expected value" in err for err in errors)


def test_typechecker_expect_have_length_requires_length_capable_and_integer_expected():
    ast = Program([
        # Invalid actual type
        ExpectStatement(Literal("boolean", True), "have_length", Literal("integer", 1)),
        # Invalid expected type
        ExpectStatement(Literal("string", "abc"), "have_length", Literal("string", "3")),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("expect have_length requires a string/list/dictionary" in err for err in errors)
    assert any("expect have_length requires an integer expected value" in err for err in errors)


def test_typechecker_expect_start_with_and_be_empty_validate_actual_types():
    ast = Program([
        ExpectStatement(Literal("integer", 5), "start_with", Literal("integer", 1)),
        ExpectStatement(Literal("float", 3.14), "be_empty"),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("expect start_with requires a string/list actual value" in err for err in errors)
    assert any("expect be_empty requires a string/list/dictionary actual value" in err for err in errors)


def test_typechecker_expect_be_of_type_requires_string_type_name():
    ast = Program([
        ExpectStatement(Literal("integer", 42), "be_of_type", Literal("integer", 7)),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("expect be_of_type requires a string type name" in err for err in errors)


def test_typechecker_accepts_extended_expect_matchers_when_well_typed():
    ast = Program([
        ExpectStatement(Literal("string", "hello"), "contain", Literal("string", "ell")),
        ExpectStatement(Literal("string", "hello"), "have_length", Literal("integer", 5)),
        ExpectStatement(Literal("string", "hello"), "start_with", Literal("string", "he")),
        ExpectStatement(Literal("string", "hello"), "end_with", Literal("string", "lo")),
        ExpectStatement(Literal("string", ""), "be_empty"),
        ExpectStatement(Literal("integer", 5), "be_of_type", Literal("string", "integer")),
        ExpectStatement(Literal("integer", 1), "raise_error"),
    ])

    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []
