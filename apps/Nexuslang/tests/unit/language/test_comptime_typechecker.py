"""Typechecker coverage for comptime statements."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import BinaryOperation, ComptimeAssert, ComptimeConst, ComptimeExpression, Identifier, Literal, Program, VariableDeclaration
from nexuslang.typesystem.typechecker import TypeChecker


def _check_program(*statements):
    checker = TypeChecker()
    checker.check_program(Program(list(statements)))
    return checker.errors


def test_comptime_expression_typechecks_inner_expression():
    errors = _check_program(
        VariableDeclaration("x", Literal("integer", 4)),
        ComptimeExpression(BinaryOperation(Identifier("x"), "+", Literal("integer", 2))),
    )

    assert errors == []


def test_comptime_const_binds_name_for_following_statements():
    errors = _check_program(
        ComptimeConst("BASE", Literal("integer", 10)),
        VariableDeclaration("result", BinaryOperation(Identifier("BASE"), "+", Literal("integer", 5))),
    )

    assert errors == []


def test_comptime_assert_requires_boolean_condition():
    errors = _check_program(ComptimeAssert(Literal("integer", 1)))

    assert any("comptime assert condition must be boolean" in error for error in errors)


def test_comptime_assert_requires_string_message_when_present():
    errors = _check_program(
        ComptimeAssert(Literal("boolean", True), message_expr=Literal("integer", 99))
    )

    assert any("Comptime assert message must be a string" in error for error in errors)
