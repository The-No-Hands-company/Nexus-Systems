"""Regression tests for LLVM lambda and closure signature lowering."""

import os
import re
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    BinaryOperation,
    FunctionCall,
    FunctionDefinition,
    Identifier,
    LambdaExpression,
    Literal,
    Parameter,
    Program,
    ReturnStatement,
    VariableDeclaration,
)


def _llvm(nodes):
    return LLVMIRGenerator().generate(Program(nodes))


def test_llvm_global_lambda_call_uses_exact_float_signature():
    ir = _llvm([
        VariableDeclaration(
            "adder",
            LambdaExpression(
                [Parameter("x", "Float"), Parameter("y", "Float")],
                BinaryOperation(Identifier("x"), "+", Identifier("y")),
                return_type="Float",
            ),
        ),
        VariableDeclaration(
            "result",
            FunctionCall("adder", [Literal("float", 1.25), Literal("float", 2.75)]),
        ),
    ])

    assert re.search(r"bitcast i8\* %\d+ to double \(double, double\)\*", ir)
    assert re.search(r"call double %\d+\(double [^,]+, double [^)]+\)", ir)


def test_llvm_local_capturing_lambda_call_uses_exact_signature_with_env():
    ir = _llvm([
        FunctionDefinition(
            name="apply_scale",
            parameters=[],
            return_type="Float",
            body=[
                VariableDeclaration("scale", Literal("float", 2.0)),
                VariableDeclaration(
                    "mul",
                    LambdaExpression(
                        [Parameter("value", "Float")],
                        BinaryOperation(Identifier("value"), "*", Identifier("scale")),
                        return_type="Float",
                    ),
                ),
                ReturnStatement(FunctionCall("mul", [Literal("float", 3.5)])),
            ],
        )
    ])

    assert re.search(r"bitcast i8\* %\d+ to double \(i8\*, double\)\*", ir)
    assert re.search(r"call double %\d+\(i8\* %\d+, double [^)]+\)", ir)
