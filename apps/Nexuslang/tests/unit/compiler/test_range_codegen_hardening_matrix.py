"""Range expression backend lowering hardening matrix for C and LLVM.

These tests verify that parser-produced range call expressions lower with stable
call signatures in both backends.
"""

import os
import re
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    FunctionCall,
    FunctionDefinition,
    Literal,
    Parameter,
    Program,
    ReturnStatement,
    VariableDeclaration,
)
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


BACKENDS = ["llvm", "c"]


RANGE_LOWERING_CASES = [
    ("ascending_two_arg", "range(1, 10)", "(1, 10)", ["1", "10"]),
    ("ascending_step", "range(1, 10, 2)", "(1, 10, 2)", ["1", "10", "2"]),
    ("descending_step", "range(10, 0, -1)", "(10, 0, -1)", ["10", "0", "-1"]),
]


def _parse_range_assignment(expr: str) -> VariableDeclaration:
    source = f"set out to {expr}\n"
    ast = Parser(Lexer(source).tokenize(), source=source).parse()
    stmt = ast.statements[0]
    assert isinstance(stmt, VariableDeclaration)
    assert isinstance(stmt.value, FunctionCall)
    assert stmt.value.name == "range"
    return stmt


def _build_program(call_stmt: VariableDeclaration) -> Program:
    arg_count = len(call_stmt.value.arguments)
    param_names = ["start", "stop", "step"][:arg_count]
    params = [Parameter(name, "Integer") for name in param_names]
    range_stub = FunctionDefinition(
        name="range",
        parameters=params,
        body=[ReturnStatement(Literal("integer", 0))],
        return_type="Integer",
    )
    return Program([range_stub, call_stmt])


def _emit(backend: str, program: Program) -> str:
    if backend == "llvm":
        return LLVMIRGenerator().generate(program)
    if backend == "c":
        return CCodeGenerator(target="c").generate(program)
    raise ValueError(f"Unknown backend: {backend}")


def _extract_c_range_call_signature(output: str) -> str:
    match = re.search(r"out\s*=\s*range\((.*)\)\s*;", output)
    assert match is not None, "Expected lowered C assignment call to range(...)"
    signature = re.sub(r"\s+", "", match.group(1))
    return signature.replace("(", "").replace(")", "")


def _extract_llvm_range_call_signature(output: str) -> str:
    match = re.search(r"call\s+i64\s+@range\(([^)]*)\)", output)
    assert match is not None, "Expected lowered LLVM call to @range(...)"
    return re.sub(r"\s+", "", match.group(1))


def _expected_c_signature(literals: list[str]) -> str:
    return ",".join(literals)


def _expected_llvm_signature(literals: list[str]) -> str:
    return ",".join(f"i64{literal}" for literal in literals)


@pytest.mark.parametrize("backend", BACKENDS)
@pytest.mark.parametrize("_name,direct_expr,tuple_expr,arg_literals", RANGE_LOWERING_CASES)
def test_range_call_lowering_matrix_preserves_call_shape(
    backend: str,
    _name: str,
    direct_expr: str,
    tuple_expr: str,
    arg_literals: list[str],
):
    direct_call_stmt = _parse_range_assignment(direct_expr)
    tuple_call_stmt = _parse_range_assignment(tuple_expr)

    direct_output = _emit(backend, _build_program(direct_call_stmt))
    tuple_output = _emit(backend, _build_program(tuple_call_stmt))

    if backend == "llvm":
        direct_sig = _extract_llvm_range_call_signature(direct_output)
        tuple_sig = _extract_llvm_range_call_signature(tuple_output)
        expected = _expected_llvm_signature(arg_literals)
    else:
        direct_sig = _extract_c_range_call_signature(direct_output)
        tuple_sig = _extract_c_range_call_signature(tuple_output)
        expected = _expected_c_signature(arg_literals)

    assert direct_sig == expected
    assert tuple_sig == expected
    assert direct_sig == tuple_sig
