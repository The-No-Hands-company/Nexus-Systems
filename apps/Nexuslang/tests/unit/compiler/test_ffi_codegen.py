"""Compiler backend FFI lowering hardening tests.

Covers pointer guards, extern type lowering, callback transport, and variadic ABI handling.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    Program,
    ExternFunctionDeclaration,
    ExternTypeDeclaration,
    FunctionDefinition,
    ReturnStatement,
    VariableDeclaration,
    FunctionCall,
    CallbackReference,
    Literal,
    Identifier,
    Parameter,
)


def _c(nodes):
    return CCodeGenerator(target="c").generate(Program(nodes))


def _llvm(nodes):
    return LLVMIRGenerator().generate(Program(nodes))


def test_c_pointer_param_extern_call_uses_runtime_pointer_guard():
    c_code = _c([
        ExternFunctionDeclaration(
            name="puts",
            parameters=[Parameter("s", "Pointer")],
            return_type="Integer",
            library="c",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration("rc", FunctionCall("puts", [Literal("string", "hello")])) ,
                ReturnStatement(Identifier("rc")),
            ],
            return_type="Integer",
        ),
    ])

    assert "nxl_ffi_check_ptr" in c_code
    assert "puts((void*)nxl_ffi_check_ptr((void*)(\"hello\")" in c_code


def test_c_extern_function_pointer_type_and_callback_call_lowering():
    c_code = _c([
        ExternTypeDeclaration(
            name="CompareFunc",
            base_type="function",
            is_function_pointer=True,
            function_signature=(["Integer", "Integer"], "Integer"),
        ),
        ExternFunctionDeclaration(
            name="register_compare",
            parameters=[Parameter("cb", "CompareFunc")],
            return_type="Integer",
            library="c",
        ),
        FunctionDefinition(
            name="compare_values",
            parameters=[Parameter("a", "Integer"), Parameter("b", "Integer")],
            body=[ReturnStatement(Literal("integer", 0))],
            return_type="Integer",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "ok",
                    FunctionCall("register_compare", [CallbackReference("compare_values")]),
                ),
                ReturnStatement(Identifier("ok")),
            ],
            return_type="Integer",
        ),
    ])

    assert "typedef int (*CompareFunc)(int, int);" in c_code
    assert "extern int register_compare(CompareFunc cb);" in c_code
    assert "register_compare((CompareFunc)compare_values)" in c_code


def test_c_variadic_extern_call_promotes_bool_and_float_args():
    c_code = _c([
        ExternFunctionDeclaration(
            name="printf",
            parameters=[Parameter("fmt", "String")],
            return_type="Integer",
            library="c",
            variadic=True,
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "rc",
                    FunctionCall(
                        "printf",
                        [
                            Literal("string", "%d %f\\n"),
                            Literal("boolean", True),
                            Literal("float", 1.25),
                        ],
                    ),
                ),
                ReturnStatement(Identifier("rc")),
            ],
            return_type="Integer",
        ),
    ])

    assert "printf((const char*)" in c_code
    assert "(int)(true)" in c_code
    assert "1.25" in c_code


def test_llvm_callback_reference_lowers_to_i8_bitcast_for_ffi():
    ir = _llvm([
        ExternFunctionDeclaration(
            name="register_compare",
            parameters=[Parameter("cb", "Pointer")],
            return_type="Integer",
            library="c",
        ),
        FunctionDefinition(
            name="compare_values",
            parameters=[Parameter("a", "Integer"), Parameter("b", "Integer")],
            body=[ReturnStatement(Literal("integer", 0))],
            return_type="Integer",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "ok",
                    FunctionCall("register_compare", [CallbackReference("compare_values")]),
                ),
                ReturnStatement(Identifier("ok")),
            ],
            return_type="Integer",
        ),
    ])

    assert "bitcast (i64 (i64, i64)* @compare_values to i8*)" in ir
    assert "call i64 @register_compare(i8*" in ir


def test_llvm_variadic_extern_call_promotes_small_integral_arguments():
    ir = _llvm([
        ExternFunctionDeclaration(
            name="printf",
            parameters=[Parameter("fmt", "String")],
            return_type="Integer",
            library="c",
            variadic=True,
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "rc",
                    FunctionCall(
                        "printf",
                        [
                            Literal("string", "%d\\n"),
                            Literal("boolean", True),
                        ],
                    ),
                ),
                ReturnStatement(Identifier("rc")),
            ],
            return_type="Integer",
        ),
    ])

    assert "sext i1" in ir
    assert "call i32 (i8*, ...) @printf(i8*" in ir
