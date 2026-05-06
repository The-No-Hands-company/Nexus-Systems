"""LLVM backend tests for function-body yield suspension-state lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import Program, FunctionDefinition, YieldExpression, Literal, Parameter, VariableDeclaration, Identifier, BinaryOperation, ReturnStatement


def test_llvm_function_body_yield_lowers_to_state_machine():
    ast = Program([
        FunctionDefinition(
            name="gen_values",
            parameters=[],
            body=[
                YieldExpression(Literal("integer", 10)),
                YieldExpression(Literal("integer", 20)),
            ],
            return_type="Integer",
        )
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "@.yield_state.gen_values = internal global i64 0" in llvm_ir
    assert "define i64 @gen_values()" in llvm_ir
    assert "switch i64" in llvm_ir
    assert "yield.state.0" in llvm_ir
    assert "yield.state.1" in llvm_ir
    assert "yield.state.2" in llvm_ir
    assert "store i64 1, i64* @.yield_state.gen_values" in llvm_ir
    assert "store i64 2, i64* @.yield_state.gen_values" in llvm_ir


def test_llvm_function_body_yield_spills_params_and_locals_across_states():
    ast = Program([
        FunctionDefinition(
            name="gen_values",
            parameters=[Parameter("start", "Integer")],
            body=[
                VariableDeclaration("current", Identifier("start")),
                VariableDeclaration("delta", Literal("integer", 3)),
                YieldExpression(Identifier("current")),
                VariableDeclaration("current", BinaryOperation(Identifier("current"), "+", Identifier("delta"))),
                YieldExpression(Identifier("current")),
                ReturnStatement(BinaryOperation(Identifier("current"), "+", Identifier("delta"))),
            ],
            return_type="Integer",
        )
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "@.yield_param.gen_values.start = internal global i64 0" in llvm_ir
    assert "@.yield_spill.gen_values.current = internal global i64 0" in llvm_ir
    assert "@.yield_spill.gen_values.delta = internal global i64 0" in llvm_ir
    assert "store i64 %start, i64* @.yield_param.gen_values.start" in llvm_ir
    assert "load i64, i64* @.yield_param.gen_values.start" in llvm_ir
    assert "load i64, i64* @.yield_spill.gen_values.current" in llvm_ir
    assert "load i64, i64* @.yield_spill.gen_values.delta" in llvm_ir
    assert "store i64 -1, i64* @.yield_state.gen_values" in llvm_ir
