"""Regression tests for LLVM function-pointer call lowering."""

import os
import re
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    AddressOfExpression,
    BinaryOperation,
    DereferenceExpression,
    FunctionCall,
    FunctionDefinition,
    Identifier,
    Literal,
    Parameter,
    Program,
    ReturnStatement,
    VariableDeclaration,
)


def _llvm(nodes):
    return LLVMIRGenerator().generate(Program(nodes))


def _build_float_add_function() -> FunctionDefinition:
    return FunctionDefinition(
        name="addf",
        parameters=[
            Parameter("x", "Float"),
            Parameter("y", "Float"),
        ],
        body=[
            ReturnStatement(BinaryOperation(Identifier("x"), "+", Identifier("y"))),
        ],
        return_type="Float",
    )


def test_llvm_function_pointer_variable_call_uses_resolved_signature():
    ir = _llvm([
        _build_float_add_function(),
        VariableDeclaration("fp", AddressOfExpression(Identifier("addf"))),
        VariableDeclaration(
            "result",
            FunctionCall("fp", [Literal("float", 1.5), Literal("float", 2.5)]),
        ),
    ])

    assert re.search(r"bitcast i8\* %\d+ to double \(double, double\)\*", ir)
    assert re.search(r"call double %\d+\(double [^,]+, double [^)]+\)", ir)


def test_llvm_indirect_dereference_call_uses_resolved_signature():
    ir = _llvm([
        _build_float_add_function(),
        VariableDeclaration("fp", AddressOfExpression(Identifier("addf"))),
        VariableDeclaration(
            "result",
            FunctionCall(
                DereferenceExpression(Identifier("fp")),
                [Literal("float", 4.0), Literal("float", 5.0)],
            ),
        ),
    ])

    assert re.search(r"bitcast i8\* %\d+ to double \(double, double\)\*", ir)
    assert re.search(r"call double %\d+\(double [^,]+, double [^)]+\)", ir)


def test_llvm_indirect_call_without_signature_fails_fast():
    generator = LLVMIRGenerator()
    ast = Program([
        VariableDeclaration("raw", Literal("integer", 0)),
        VariableDeclaration(
            "result",
            FunctionCall(DereferenceExpression(Identifier("raw")), [Literal("integer", 1)]),
        ),
    ])

    with pytest.raises(RuntimeError, match="Unable to resolve indirect function pointer signature"):
        generator.generate(ast)
