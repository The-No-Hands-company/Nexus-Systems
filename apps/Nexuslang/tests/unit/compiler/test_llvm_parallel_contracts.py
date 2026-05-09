"""LLVM backend coverage for parallel-for and contract/assertion lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    Program,
    FunctionDefinition,
    ParallelForLoop,
    ListExpression,
    GeneratorExpression,
    Literal,
    PrintStatement,
    Identifier,
    VariableDeclaration,
    BinaryOperation,
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


def test_llvm_parallel_for_allows_read_only_outer_local_capture():
    ast = Program([
        FunctionDefinition(
            name="use_parallel_scale",
            parameters=[],
            body=[
                VariableDeclaration("scale", Literal("integer", 2)),
                ParallelForLoop(
                    "item",
                    ListExpression([Literal("integer", 1), Literal("integer", 2)]),
                    [PrintStatement(BinaryOperation(Identifier("item"), "*", Identifier("scale")))],
                ),
            ],
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "@.parallel_capture.0.scale = internal global i64 zeroinitializer, align 8" in llvm_ir
    assert "define void @__nxl_parallel_body_0(i64 %iter_value)" in llvm_ir
    assert "call void @nxl_parallel_for_i64(" in llvm_ir


def test_llvm_parallel_for_over_inline_identity_generator_uses_parallel_runtime():
    ast = Program([
        VariableDeclaration(
            "numbers",
            ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
        ),
        ParallelForLoop(
            "item",
            GeneratorExpression(
                Identifier("x"),
                Identifier("x"),
                Identifier("numbers"),
                None,
            ),
            [PrintStatement(Identifier("item"))],
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call void @nxl_parallel_for_i64(" in llvm_ir
    assert "define void @__nxl_parallel_body_0(i64 %iter_value)" in llvm_ir


def test_llvm_parallel_for_over_filtered_generator_emits_prefilter_then_parallel():
    """Parallel-for over a filtered generator should emit a prefilter pass rather than falling back."""
    ast = Program([
        FunctionDefinition(
            name="parallel_filtered",
            parameters=[],
            body=[
                VariableDeclaration(
                    "numbers",
                    ListExpression([
                        Literal("integer", -1),
                        Literal("integer", 3),
                        Literal("integer", -2),
                        Literal("integer", 7),
                    ]),
                ),
                ParallelForLoop(
                    "item",
                    GeneratorExpression(
                        Identifier("x"),
                        Identifier("x"),
                        Identifier("numbers"),
                        BinaryOperation(Identifier("x"), ">", Literal("integer", 0)),
                    ),
                    [PrintStatement(Identifier("item"))],
                ),
            ],
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call void @nxl_parallel_for_i64(" in llvm_ir
    assert "nxl_generator_predicate_match" in llvm_ir
    assert "pfilt.cond" in llvm_ir
    assert "pfilt.end" in llvm_ir
    assert "call void @free(i8*" in llvm_ir


def test_llvm_parallel_for_over_arithmetic_generator_uses_source_directly():
    """Parallel-for over an arithmetic-mapped generator should call parallel without materialization."""
    ast = Program([
        VariableDeclaration(
            "numbers",
            ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
        ),
        ParallelForLoop(
            "item",
            GeneratorExpression(
                BinaryOperation(Identifier("x"), "+", Literal("integer", 5)),
                Identifier("x"),
                Identifier("numbers"),
                None,
            ),
            [PrintStatement(Identifier("item"))],
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call void @nxl_parallel_for_i64(" in llvm_ir
    assert "call i8* @malloc(i64 800)" not in llvm_ir


def test_llvm_contracts_and_expect_emit_runtime_panic_guards():
    ast = Program([
        RequireStatement(Literal("boolean", False), Literal("string", "require failed")),
        ExpectStatement(Literal("integer", 1), "equal", Literal("integer", 2)),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "call void @nxl_panic(i8*" in llvm_ir
    assert "contract.fail" in llvm_ir


def test_llvm_expect_supports_string_and_approximate_matchers():
    ast = Program([
        ExpectStatement(Literal("string", "nexuslang"), "contain", Literal("string", "lang")),
        ExpectStatement(Literal("string", "nexuslang"), "start_with", Literal("string", "nexus")),
        ExpectStatement(Literal("string", "nexuslang"), "end_with", Literal("string", "lang")),
        ExpectStatement(
            Literal("float", 3.14159),
            "approximately_equal",
            Literal("float", 3.1416),
            tolerance_expr=Literal("float", 0.001),
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "unsupported expect matcher" not in llvm_ir
    assert "call i8* @strstr(" in llvm_ir
    assert "call i64 @strlen(" in llvm_ir
    assert "call i32 @strcmp(" in llvm_ir
    assert "call double @fabs(" in llvm_ir


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
