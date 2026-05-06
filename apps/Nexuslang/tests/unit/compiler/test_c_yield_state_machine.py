"""C backend tests for function-body yield state-machine lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.ast import FunctionDefinition, Literal, Parameter, Program, VariableDeclaration, Identifier, YieldExpression, BinaryOperation, ReturnStatement


def test_c_function_body_yield_lowers_with_spilled_params_and_locals():
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

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "static int __yield_state_gen_values = 0;" in c_code
    assert "static int __yield_param_gen_values_start;" in c_code
    assert "static int __yield_local_gen_values_current;" in c_code
    assert "static int __yield_local_gen_values_delta;" in c_code
    assert "switch (__yield_state_gen_values)" in c_code
    assert "yield_state_0:" in c_code
    assert "yield_state_1:" in c_code
    assert "yield_state_2:" in c_code
    assert "__yield_param_gen_values_start = start;" in c_code
    assert "return __yield_local_gen_values_current;" in c_code
