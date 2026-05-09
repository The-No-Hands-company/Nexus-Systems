"""Regression tests for LLVM async payload type extraction."""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    AsyncFunctionDefinition,
    AwaitExpression,
    FunctionCall,
    Identifier,
    Literal,
    Program,
    ReturnStatement,
    VariableDeclaration,
)


def _llvm(nodes):
    return LLVMIRGenerator().generate(Program(nodes))


def test_await_async_task_variable_uses_float_payload_type():
    llvm_ir = _llvm([
        AsyncFunctionDefinition(
            name="fetch_value",
            parameters=[],
            body=[ReturnStatement(Literal("float", 7.5))],
            return_type="Float",
        ),
        AsyncFunctionDefinition(
            name="wrapper",
            parameters=[],
            body=[
                VariableDeclaration("task", FunctionCall("fetch_value", [])),
                VariableDeclaration("value", AwaitExpression(Identifier("task"))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Float",
        ),
    ])

    assert "call i8* @fetch_value()" in llvm_ir
    assert re.search(r"bitcast i8\* %\d+ to double\*", llvm_ir)
    assert re.search(r"load double, double\* %\d+", llvm_ir)


def test_await_async_task_variable_uses_pointer_payload_without_integer_fallback():
    llvm_ir = _llvm([
        AsyncFunctionDefinition(
            name="fetch_text",
            parameters=[],
            body=[ReturnStatement(Literal("string", "hello"))],
            return_type="String",
        ),
        AsyncFunctionDefinition(
            name="wrapper",
            parameters=[],
            body=[
                VariableDeclaration("task", FunctionCall("fetch_text", [])),
                VariableDeclaration("value", AwaitExpression(Identifier("task"))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="String",
        ),
    ])

    assert "call i8* @fetch_text()" in llvm_ir
    assert "await.poll." in llvm_ir
    assert re.search(r"store i8\* %\d+, i8\*\* %value, align 8", llvm_ir)
    assert "store i64 %7, i64* %value" not in llvm_ir
