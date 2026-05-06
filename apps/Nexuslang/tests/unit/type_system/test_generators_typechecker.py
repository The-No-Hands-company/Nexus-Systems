"""TypeChecker coverage tests for generator and yield expressions."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import (
    FunctionDefinition,
    GeneratorExpression,
    Identifier,
    ListExpression,
    Literal,
    Program,
    ReturnStatement,
    VariableDeclaration,
    YieldExpression,
)
from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.typesystem.types import INTEGER_TYPE, ListType


def test_typechecker_infers_generator_expression_as_list_type():
    ast = Program([
        VariableDeclaration(
            "numbers",
            ListExpression([
                Literal("integer", 1),
                Literal("integer", 2),
                Literal("integer", 3),
            ]),
        ),
        VariableDeclaration(
            "gen",
            GeneratorExpression(
                Identifier("x"),
                Identifier("x"),
                Identifier("numbers"),
                None,
            ),
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []
    gen_type = checker.env.get_variable_type("gen")
    assert isinstance(gen_type, ListType)
    assert gen_type.element_type == INTEGER_TYPE


def test_typechecker_rejects_non_boolean_generator_condition():
    ast = Program([
        VariableDeclaration(
            "numbers",
            ListExpression([
                Literal("integer", 1),
                Literal("integer", 2),
                Literal("integer", 3),
            ]),
        ),
        VariableDeclaration(
            "gen",
            GeneratorExpression(
                Identifier("x"),
                Identifier("x"),
                Identifier("numbers"),
                Literal("string", "no"),
            ),
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("Generator condition must be boolean" in err for err in errors)


def test_typechecker_rejects_yield_outside_function():
    ast = Program([
        YieldExpression(Literal("integer", 1)),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("'yield' can only be used inside a function" in err for err in errors)


def test_typechecker_checks_yield_against_function_return_type():
    ast = Program([
        FunctionDefinition(
            name="bad_yield",
            parameters=[],
            body=[YieldExpression(Literal("string", "oops"))],
            return_type="Integer",
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("generator function" in err.lower() for err in errors)


def test_typechecker_accepts_generator_function_with_list_return_type():
    ast = Program([
        FunctionDefinition(
            name="count_up",
            parameters=[],
            body=[YieldExpression(Literal("integer", 1))],
            return_type="List<Integer>",
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []
    fn_type = checker.env.get_function_type("count_up")
    assert isinstance(fn_type.return_type, ListType)
    assert fn_type.return_type.element_type == INTEGER_TYPE


def test_typechecker_rejects_generator_function_returning_value():
    ast = Program([
        FunctionDefinition(
            name="bad_return",
            parameters=[],
            body=[
                YieldExpression(Literal("integer", 1)),
                ReturnStatement(Literal("integer", 2)),
            ],
            return_type="List<Integer>",
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("generator function" in err.lower() and "return a value" in err.lower() for err in errors)


def test_typechecker_inferrs_generator_function_signature_from_yield_type():
    ast = Program([
        FunctionDefinition(
            name="gen_numbers",
            parameters=[],
            body=[YieldExpression(Literal("integer", 1))],
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert errors == []
    fn_type = checker.env.get_function_type("gen_numbers")
    assert isinstance(fn_type.return_type, ListType)
    assert fn_type.return_type.element_type == INTEGER_TYPE


def test_typechecker_rejects_incompatible_yield_types_in_same_generator_function():
    ast = Program([
        FunctionDefinition(
            name="mixed_gen",
            parameters=[],
            body=[
                YieldExpression(Literal("integer", 1)),
                YieldExpression(Literal("string", "oops")),
            ],
        ),
    ])
    checker = TypeChecker()
    errors = checker.check_program(ast)

    assert any("incompatible yield types" in err.lower() for err in errors)
