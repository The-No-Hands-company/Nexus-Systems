"""Compiler backend baseline coverage for YieldExpression lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import Program, VariableDeclaration, YieldExpression, Literal
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.compiler.backends.c_generator import CCodeGenerator


def test_llvm_lowers_yield_expression_value():
    ast = Program([
        VariableDeclaration("x", YieldExpression(Literal("integer", 7)))
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "store i64 7" in llvm_ir


def test_c_lowers_yield_expression_value():
    ast = Program([
        VariableDeclaration("x", YieldExpression(Literal("integer", 7)))
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "int x = 7;" in c_code
