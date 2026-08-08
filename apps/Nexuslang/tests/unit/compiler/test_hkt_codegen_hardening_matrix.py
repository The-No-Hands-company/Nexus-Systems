"""HKT backend lowering hardening matrix for C and LLVM.

These tests ensure kind annotations attached to generic parameters do not
regress specialization/lowering behavior in either backend.
"""

import copy
import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    ArrowKindAnnotation,
    FunctionCall,
    FunctionDefinition,
    Identifier,
    Literal,
    Parameter,
    Program,
    ReturnStatement,
    StarKindAnnotation,
    VariableDeclaration,
)


def _star_kind():
    return StarKindAnnotation()


def _star_to_star_kind():
    return ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation())


def _star_to_star_to_star_kind():
    return ArrowKindAnnotation(StarKindAnnotation(), ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation()))


BACKENDS = ["llvm", "c"]


def _emit(backend: str, program: Program) -> str:
    if backend == "llvm":
        return LLVMIRGenerator().generate(program)
    if backend == "c":
        return CCodeGenerator(target="c").generate(program)
    raise ValueError(f"Unknown backend: {backend}")


KIND_SPECIALIZATION_CASES = [
    (
        "star_explicit",
        FunctionDefinition(
            name="identity",
            parameters=[Parameter("value", "T")],
            body=[ReturnStatement(Identifier("value"))],
            return_type="T",
            type_parameters=["T"],
            type_param_kinds={"T": _star_kind()},
        ),
        FunctionCall("identity", [Literal("integer", 42)], type_arguments=["Integer"]),
        "identity_Integer",
    ),
    (
        "star_inferred",
        FunctionDefinition(
            name="identity",
            parameters=[Parameter("value", "T")],
            body=[ReturnStatement(Identifier("value"))],
            return_type="T",
            type_parameters=["T"],
            type_param_kinds={"T": _star_kind()},
        ),
        FunctionCall("identity", [Literal("string", "hello")]),
        "identity_String",
    ),
    (
        "star_to_star_explicit",
        FunctionDefinition(
            name="const_value",
            parameters=[Parameter("value", "Integer")],
            body=[ReturnStatement(Identifier("value"))],
            return_type="Integer",
            type_parameters=["F"],
            type_param_kinds={"F": _star_to_star_kind()},
        ),
        FunctionCall("const_value", [Literal("integer", 7)], type_arguments=["List"]),
        "const_value_List",
    ),
    (
        "star_to_star_to_star_explicit",
        FunctionDefinition(
            name="const_value2",
            parameters=[Parameter("value", "Integer")],
            body=[ReturnStatement(Identifier("value"))],
            return_type="Integer",
            type_parameters=["F"],
            type_param_kinds={"F": _star_to_star_to_star_kind()},
        ),
        FunctionCall("const_value2", [Literal("integer", 9)], type_arguments=["Dictionary"]),
        "const_value2_Dictionary",
    ),
]


@pytest.mark.parametrize("backend", BACKENDS)
@pytest.mark.parametrize("_case_name,template_fn,call_expr,specialized_name", KIND_SPECIALIZATION_CASES)
def test_kind_annotated_generic_specialization_matrix(
    backend: str,
    _case_name: str,
    template_fn: FunctionDefinition,
    call_expr: FunctionCall,
    specialized_name: str,
):
    # Rebuild fresh AST nodes per run to avoid accidental state carry-over.
    fn = copy.deepcopy(template_fn)
    call = copy.deepcopy(call_expr)

    program = Program([
        fn,
        VariableDeclaration("out", call),
    ])

    output = _emit(backend, program)

    if backend == "llvm":
        assert f"@{specialized_name}" in output
        assert "Unknown generic function" not in output
    else:
        assert specialized_name in output
        assert "Unknown generic function" not in output
