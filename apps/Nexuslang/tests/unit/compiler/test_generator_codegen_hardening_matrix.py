"""Generator codegen hardening matrix for C and LLVM backends.

This suite intentionally uses a broad parameter matrix to guard generator
lowering behavior across source forms, transforms, and predicates.
"""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    BinaryOperation,
    ForLoop,
    GeneratorExpression,
    Identifier,
    ListExpression,
    Literal,
    PrintStatement,
    Program,
    VariableDeclaration,
)
from nexuslang.parser.lexer import TokenType


def _build_transform(kind: str):
    if kind == "identity":
        return Identifier("x")
    if kind == "add1":
        return BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 1))
    if kind == "add10":
        return BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 10))
    if kind == "add100":
        return BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 100))
    raise ValueError(f"Unknown transform kind: {kind}")


def _build_transform_llvm(kind: str):
    if kind == "identity":
        return Identifier("x")
    if kind == "add1":
        return BinaryOperation(Identifier("x"), "+", Literal("integer", 1))
    if kind == "add10":
        return BinaryOperation(Identifier("x"), "+", Literal("integer", 10))
    if kind == "add100":
        return BinaryOperation(Identifier("x"), "+", Literal("integer", 100))
    raise ValueError(f"Unknown transform kind: {kind}")


def _build_condition(kind: str):
    if kind == "none":
        return None
    if kind == "truthy":
        return Identifier("x")
    if kind == "gt1":
        return BinaryOperation(Identifier("x"), TokenType.GREATER_THAN, Literal("integer", 1))
    if kind == "eq2":
        return BinaryOperation(Identifier("x"), TokenType.EQUAL_TO, Literal("integer", 2))
    raise ValueError(f"Unknown condition kind: {kind}")


def _build_condition_llvm(kind: str):
    if kind == "none":
        return None
    if kind == "truthy":
        return Identifier("x")
    if kind == "gt1":
        return BinaryOperation(Identifier("x"), ">", Literal("integer", 1))
    if kind == "eq2":
        return BinaryOperation(Identifier("x"), "==", Literal("integer", 2))
    raise ValueError(f"Unknown condition kind: {kind}")


def _source_expr(source_kind: str):
    if source_kind == "list_literal":
        return ListExpression([
            Literal("integer", 1),
            Literal("integer", 2),
            Literal("integer", 3),
            Literal("integer", 4),
        ])
    if source_kind == "identifier":
        return Identifier("numbers")
    raise ValueError(f"Unknown source kind: {source_kind}")


def _source_decl_if_needed(source_kind: str):
    if source_kind != "identifier":
        return []
    return [
        VariableDeclaration(
            "numbers",
            ListExpression([
                Literal("integer", 1),
                Literal("integer", 2),
                Literal("integer", 3),
                Literal("integer", 4),
            ]),
        ),
        VariableDeclaration("numbers_length", Literal("integer", 4)),
    ]


def _generator_expr(source_kind: str, transform_kind: str, condition_kind: str):
    return GeneratorExpression(
        _build_transform(transform_kind),
        Identifier("x"),
        _source_expr(source_kind),
        _build_condition(condition_kind),
    )


def _generator_expr_llvm(source_kind: str, transform_kind: str, condition_kind: str):
    return GeneratorExpression(
        _build_transform_llvm(transform_kind),
        Identifier("x"),
        _source_expr(source_kind),
        _build_condition_llvm(condition_kind),
    )


SOURCE_KINDS = ["list_literal", "identifier"]
SOURCE_KINDS_LLVM = ["identifier"]
TRANSFORM_KINDS = ["identity", "add1", "add10", "add100"]
CONDITION_KINDS = ["none", "truthy", "gt1", "eq2"]


@pytest.mark.parametrize("source_kind", SOURCE_KINDS)
@pytest.mark.parametrize("transform_kind", TRANSFORM_KINDS)
@pytest.mark.parametrize("condition_kind", CONDITION_KINDS)
def test_c_foreach_generator_matrix(source_kind: str, transform_kind: str, condition_kind: str):
    ast = Program(
        _source_decl_if_needed(source_kind)
        + [
            ForLoop(
                "item",
                _generator_expr(source_kind, transform_kind, condition_kind),
                [PrintStatement(Identifier("item"))],
            )
        ]
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "For each loop over generator expression (materialized)" in c_code
    assert "Unhandled expression: GeneratorExpression" not in c_code
    assert "__nxl_gen_values_" in c_code
    assert "for (int _i = 0; _i < __nxl_gen_count_" in c_code


@pytest.mark.parametrize("source_kind", SOURCE_KINDS_LLVM)
@pytest.mark.parametrize("transform_kind", TRANSFORM_KINDS)
@pytest.mark.parametrize("condition_kind", CONDITION_KINDS)
def test_llvm_generator_variable_matrix(source_kind: str, transform_kind: str, condition_kind: str):
    ast = Program(
        _source_decl_if_needed(source_kind)
        + [
            VariableDeclaration(
                "gen",
                _generator_expr_llvm(source_kind, transform_kind, condition_kind),
            ),
            ForLoop(
                "item",
                Identifier("gen"),
                [PrintStatement(Identifier("item"))],
            ),
        ]
    )

    llvm_ir = LLVMIRGenerator().generate(ast)

    assert "define i1 @nxl_generator_has_next(i8* %gen)" in llvm_ir
    assert "define i64 @nxl_generator_next(i8* %gen)" in llvm_ir
    assert "call i8* @malloc(i64 56)" in llvm_ir
    assert "call i1 @nxl_generator_has_next(i8*" in llvm_ir
    assert "call i64 @nxl_generator_next(i8*" in llvm_ir


@pytest.mark.parametrize("source_kind", SOURCE_KINDS)
@pytest.mark.parametrize("condition_kind", CONDITION_KINDS)
def test_c_generator_value_expression_variable_lowering(source_kind: str, condition_kind: str):
    ast = Program(
        _source_decl_if_needed(source_kind)
        + [
            VariableDeclaration(
                "gen_values",
                _generator_expr(source_kind, "add10", condition_kind),
            ),
        ]
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "__nxl_gen_values_" in c_code
    assert "__nxl_gen_count_" in c_code
    assert "int* gen_values = __nxl_gen_values_" in c_code or "gen_values = __nxl_gen_values_" in c_code
    assert "int gen_values_length = __nxl_gen_count_" in c_code or "gen_values_length = __nxl_gen_count_" in c_code


@pytest.mark.parametrize("source_kind", SOURCE_KINDS)
def test_c_generator_value_expression_assignment_updates_length_metadata(source_kind: str):
    ast = Program(
        _source_decl_if_needed(source_kind)
        + [
            VariableDeclaration("gen_values", ListExpression([Literal("integer", 9)])),
            VariableDeclaration("gen_values_length", Literal("integer", 1)),
            VariableDeclaration(
                "gen_values",
                _generator_expr(source_kind, "identity", "none"),
            ),
        ]
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "gen_values = __nxl_gen_values_" in c_code
    assert "gen_values_length = __nxl_gen_count_" in c_code
