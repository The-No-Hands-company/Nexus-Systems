"""Regression tests for LLVM inference fail-fast behavior."""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    BinaryOperation,
    FunctionCall,
    FunctionDefinition,
    Identifier,
    IndexExpression,
    IndexAssignment,
    Literal,
    MemberAccess,
    MemberAssignment,
    Parameter,
    Program,
    ReturnStatement,
    VariableDeclaration,
)


def test_llvm_map_unknown_nxl_type_fails_fast():
    generator = LLVMIRGenerator()

    with pytest.raises(RuntimeError, match="Unable to map NexusLang type"):
        generator._map_nxl_type_to_llvm("CompletelyUnknownType")


def test_llvm_generic_call_without_type_information_fails_fast():
    generator = LLVMIRGenerator()
    ast = Program([
        FunctionDefinition(
            name="identity",
            parameters=[Parameter("x", "T")],
            body=[ReturnStatement(Identifier("x"))],
            return_type="T",
            type_parameters=["T"],
        ),
        VariableDeclaration("result", FunctionCall("identity", [])),
    ])

    with pytest.raises(RuntimeError, match="without sufficient type information to infer type arguments"):
        generator.generate(ast)


def test_llvm_index_expression_non_pointer_base_fails_fast():
    generator = LLVMIRGenerator()

    expr = IndexExpression(Literal("integer", 42), Literal("integer", 0))
    with pytest.raises(RuntimeError, match="IndexExpression requires pointer-like array type"):
        generator._infer_expression_type(expr)


def test_llvm_generate_identifier_unknown_symbol_fails_fast():
    generator = LLVMIRGenerator()

    with pytest.raises(RuntimeError, match="Unknown identifier 'missing_symbol' during LLVM generation"):
        generator._generate_identifier(Identifier("missing_symbol"))


def test_llvm_generate_expression_unknown_ast_node_fails_fast():
    generator = LLVMIRGenerator()

    class UnsupportedExpression:
        pass

    with pytest.raises(RuntimeError, match="Unsupported expression type during LLVM generation"):
        generator._generate_expression(UnsupportedExpression())


def test_llvm_load_named_variable_unknown_symbol_fails_fast():
    generator = LLVMIRGenerator()

    with pytest.raises(RuntimeError, match="Unknown variable 'ghost' in value load"):
        generator._load_named_variable("ghost")


def test_llvm_move_expression_missing_name_fails_fast():
    generator = LLVMIRGenerator()

    class BrokenMoveExpression:
        var_name = None

    with pytest.raises(RuntimeError, match="MoveExpression missing source variable name"):
        generator._generate_move_expression(BrokenMoveExpression())


def test_llvm_borrow_expression_missing_name_fails_fast():
    generator = LLVMIRGenerator()

    class BrokenBorrowExpression:
        var_name = None

    with pytest.raises(RuntimeError, match="BorrowExpression missing source variable name"):
        generator._generate_borrow_expression(BrokenBorrowExpression())


def test_llvm_index_assignment_unknown_array_variable_fails_fast():
    generator = LLVMIRGenerator()

    bad_assignment = IndexAssignment(
        IndexExpression(Identifier("missing_arr"), Literal("integer", 0)),
        Literal("integer", 1),
    )

    with pytest.raises(RuntimeError, match="Unknown array variable 'missing_arr' in index assignment"):
        generator._generate_index_assignment(bad_assignment)


def test_llvm_member_assignment_unknown_object_variable_fails_fast():
    generator = LLVMIRGenerator()

    bad_assignment = MemberAssignment(
        MemberAccess(Identifier("missing_obj"), "field"),
        Literal("integer", 1),
    )

    with pytest.raises(RuntimeError, match="Unknown object variable 'missing_obj' in member assignment"):
        generator._generate_member_assignment(bad_assignment)


def test_llvm_binary_operation_unknown_operator_fails_fast():
    generator = LLVMIRGenerator()

    bad_expr = BinaryOperation(Literal("integer", 1), "???", Literal("integer", 2))
    with pytest.raises(RuntimeError, match="Unsupported integer binary operator"):
        generator._generate_binary_operation(bad_expr)
