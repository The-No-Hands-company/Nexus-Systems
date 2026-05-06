"""LLVM backend coverage for parallel-for and contract/assertion lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    Program,
    ParallelForLoop,
    ListExpression,
    Literal,
    PrintStatement,
    Identifier,
    RequireStatement,
    ExpectStatement,
    TryCatchBlock,
    TryCatch,
    RaiseStatement,
    Block,
)


def test_llvm_parallel_for_is_lowered_to_parallel_runtime_call():
    ast = Program([
        ParallelForLoop(
            "item",
            ListExpression([Literal("integer", 1), Literal("integer", 2)]),
            [PrintStatement(Identifier("item"))],
        )
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "define i32 @main(" in llvm_ir
    assert "declare void @nxl_parallel_for_i64(i64*, i64, void (i64)*, i64)" in llvm_ir
    assert "define void @__nxl_parallel_body_0(i64 %iter_value)" in llvm_ir
    assert "call void @nxl_parallel_for_i64(" in llvm_ir


def test_llvm_contracts_and_expect_emit_runtime_panic_guards():
    ast = Program([
        RequireStatement(Literal("boolean", False), Literal("string", "require failed")),
        ExpectStatement(Literal("integer", 1), "equal", Literal("integer", 2)),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call void @nxl_panic(i8*" in llvm_ir
    assert "contract.fail" in llvm_ir


def test_llvm_try_catch_block_lowers_with_landingpad_and_raise():
    ast = Program([
        TryCatchBlock(
            try_block=Block([
                RaiseStatement(message=Literal("string", "boom")),
            ]),
            catch_block=Block([
                PrintStatement(Identifier("err")),
            ]),
            exception_var="err",
        )
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "landingpad { i8*, i32 }" in llvm_ir
    assert "call i8* @__cxa_begin_catch(i8*" in llvm_ir
    assert "store i8*" in llvm_ir
    assert "@__nxl_throw" in llvm_ir or "@__cxa_throw" in llvm_ir


def test_llvm_nested_try_catch_multiple_rethrows_emit_nested_landingpads():
    ast = Program([
        TryCatch(
            try_block=[
                TryCatchBlock(
                    try_block=Block([RaiseStatement(message=Literal("string", "inner"))]),
                    catch_block=Block([RaiseStatement(message=Literal("string", "rethrow1"))]),
                    exception_var="inner_err",
                )
            ],
            catch_block=[
                TryCatch(
                    try_block=[RaiseStatement(message=Literal("string", "rethrow2"))],
                    catch_block=[PrintStatement(Identifier("outer_err2"))],
                    exception_var="outer_err2",
                )
            ],
            exception_var="outer_err",
        )
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert llvm_ir.count("landingpad { i8*, i32 }") >= 3
    assert llvm_ir.count("@__nxl_throw") >= 3 or llvm_ir.count("@__cxa_throw") >= 3
    assert llvm_ir.count("@__cxa_begin_catch") >= 3


def test_llvm_uncaught_raise_uses_throw_helper_and_unreachable():
    ast = Program([
        RaiseStatement(message=Literal("string", "fatal")),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call void @__nxl_throw(i8*" in llvm_ir
    assert "unreachable" in llvm_ir


def test_llvm_raise_without_message_uses_default_error_text():
    ast = Program([
        RaiseStatement(),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "Error raised" in llvm_ir
    assert "call void @__nxl_throw(i8*" in llvm_ir
