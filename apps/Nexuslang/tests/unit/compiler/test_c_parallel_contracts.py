"""C backend coverage for parallel-for and contract/assertion lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.ast import (
    Program,
    ParallelForLoop,
    ListExpression,
    Literal,
    PrintStatement,
    Identifier,
    RequireStatement,
    ExpectStatement,
    TryCatch,
    RaiseStatement,
    TryCatchBlock,
    Block,
    VariableDeclaration,
    BinaryOperation,
    FunctionDefinition,
    Parameter,
)


def _make_c(ast: Program) -> str:
    return CCodeGenerator(target="c").generate(ast)


def test_c_parallel_for_lowers_to_parallel_runtime_call():
    ast = Program([
        ParallelForLoop(
            "item",
            ListExpression([Literal("integer", 1), Literal("integer", 2)]),
            [PrintStatement(Identifier("item"))],
        )
    ])

    c_code = _make_c(ast)

    assert "extern void nxl_parallel_for_i64(int64_t* data, int64_t count, void (*body)(int64_t), int64_t workers);" in c_code
    assert "static void __nxl_parallel_body_0(int64_t __nxl_iter_value);" in c_code
    assert "static void __nxl_parallel_body_0(int64_t __nxl_iter_value) {" in c_code
    assert "nxl_parallel_for_i64(__nxl_parallel_data_0, (int64_t)2, __nxl_parallel_body_0, 0);" in c_code
    assert "parallel-for lowered to sequential loop" not in c_code


def test_c_parallel_for_identifier_without_known_size_falls_back_to_sequential_loop():
    ast = Program([
        ParallelForLoop(
            "item",
            Identifier("unknown_items"),
            [PrintStatement(Identifier("item"))],
        )
    ])

    c_code = _make_c(ast)

    assert "parallel-for lowered to sequential loop" in c_code
    assert "for (int _i = 0; _i < 0; _i++) {" in c_code


# ---------------------------------------------------------------------------
# Capture-transport tests (new in Wave 4 extension)
# ---------------------------------------------------------------------------

def test_c_parallel_for_with_outer_local_uses_ctx_transport():
    """Parallel body referencing an outer local must use nxl_parallel_for_ctx_i64."""
    ast = Program([
        VariableDeclaration("threshold", Literal("integer", 10)),
        ParallelForLoop(
            "item",
            ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
            [PrintStatement(Identifier("threshold"))],
        )
    ])

    c_code = _make_c(ast)

    # Should use context-transport runtime call, not direct call.
    assert "nxl_parallel_for_ctx_i64(" in c_code
    assert "nxl_parallel_for_i64(" not in c_code
    # Context struct typedef must be emitted.
    assert "__nxl_ctx_t_0" in c_code
    # Callback must accept void* context argument.
    assert "static void __nxl_parallel_body_0(int64_t __nxl_iter_value, void* __nxl_ctx_raw);" in c_code
    assert "static void __nxl_parallel_body_0(int64_t __nxl_iter_value, void* __nxl_ctx_raw) {" in c_code
    # Captured variable must be unpacked inside the callback.
    assert "__nxl_ctx->threshold" in c_code
    # Context instance initialised from outer local.
    assert "threshold" in c_code
    # No sequential fallback.
    assert "parallel-for lowered to sequential loop" not in c_code


def test_c_parallel_for_with_multiple_captures_emits_all_fields():
    """All captured outer locals are included in the context struct."""
    ast = Program([
        VariableDeclaration("scale", Literal("integer", 2)),
        VariableDeclaration("offset", Literal("integer", 100)),
        ParallelForLoop(
            "n",
            ListExpression([Literal("integer", 1), Literal("integer", 2)]),
            [
                PrintStatement(
                    BinaryOperation(
                        BinaryOperation(Identifier("n"), "*", Identifier("scale")),
                        "+",
                        Identifier("offset"),
                    )
                )
            ],
        )
    ])

    c_code = _make_c(ast)

    # Context struct must contain both captured fields.
    assert "__nxl_ctx_t_0" in c_code
    assert "scale" in c_code
    assert "offset" in c_code
    assert "nxl_parallel_for_ctx_i64(" in c_code
    assert "__nxl_ctx->scale" in c_code
    assert "__nxl_ctx->offset" in c_code


def test_c_parallel_for_no_capture_does_not_emit_ctx():
    """Parallel body without outer locals must NOT emit ctx struct or ctx runtime call."""
    ast = Program([
        ParallelForLoop(
            "x",
            ListExpression([Literal("integer", 5), Literal("integer", 6)]),
            [PrintStatement(Identifier("x"))],
        )
    ])

    c_code = _make_c(ast)

    assert "nxl_parallel_for_ctx_i64" not in c_code
    assert "__nxl_ctx_t_0" not in c_code
    assert "nxl_parallel_for_i64(" in c_code


def test_c_parallel_for_ctx_extern_declaration_emitted():
    """nxl_parallel_for_ctx_i64 extern declaration is present when ctx path is taken."""
    ast = Program([
        VariableDeclaration("factor", Literal("integer", 3)),
        ParallelForLoop(
            "v",
            ListExpression([Literal("integer", 10)]),
            [PrintStatement(Identifier("factor"))],
        )
    ])

    c_code = _make_c(ast)

    assert (
        "extern void nxl_parallel_for_ctx_i64("
        "int64_t* data, int64_t count, "
        "void (*body)(int64_t, void*), void* ctx, int64_t workers);"
    ) in c_code


def test_c_contracts_and_expect_emit_abort_guards():
    ast = Program([
        RequireStatement(Literal("boolean", False), Literal("string", "require failed")),
        ExpectStatement(Literal("integer", 1), "equal", Literal("integer", 2)),
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "if (!(false))" in c_code
    assert "fprintf(stderr, \"%s\\n\"" in c_code
    assert "exit(1);" in c_code


def test_c_try_catch_and_raise_emit_setjmp_longjmp_flow():
    ast = Program([
        TryCatch(
            try_block=[RaiseStatement(message=Literal("string", "boom"))],
            catch_block=[PrintStatement(Identifier("err"))],
            exception_var="err",
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "#include <setjmp.h>" in c_code
    assert "if (setjmp(" in c_code
    assert "longjmp(" in c_code
    assert "nxl_current_exception_message" in c_code
    assert "const char* err = nxl_current_exception_message ? nxl_current_exception_message : \"Error occurred\";" in c_code


def test_c_uncaught_raise_emits_abort_path():
    ast = Program([
        RaiseStatement(message=Literal("string", "fatal")),
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "fprintf(stderr, \"%s\\n\", (const char*)(\"fatal\"));" in c_code
    assert "exit(1);" in c_code


def test_c_nested_try_catch_multiple_rethrows_emit_nested_longjmp_paths():
    ast = Program([
        TryCatch(
            try_block=[
                TryCatch(
                    try_block=[RaiseStatement(message=Literal("string", "inner"))],
                    catch_block=[RaiseStatement(message=Literal("string", "rethrow1"))],
                    exception_var="inner_err",
                )
            ],
            catch_block=[
                TryCatchBlock(
                    try_block=Block([RaiseStatement(message=Literal("string", "rethrow2"))]),
                    catch_block=Block([PrintStatement(Identifier("outer_err2"))]),
                    exception_var="outer_err2",
                )
            ],
            exception_var="outer_err",
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert c_code.count("if (setjmp(") >= 3
    assert c_code.count("longjmp(") >= 3
    assert "const char* outer_err = nxl_current_exception_message ? nxl_current_exception_message : \"Error occurred\";" in c_code
    assert "const char* outer_err2 = nxl_current_exception_message ? nxl_current_exception_message : \"Error occurred\";" in c_code
