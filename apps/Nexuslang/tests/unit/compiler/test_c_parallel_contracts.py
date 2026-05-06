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
)


def test_c_parallel_for_lowers_to_sequential_loop():
    ast = Program([
        ParallelForLoop(
            "item",
            ListExpression([Literal("integer", 1), Literal("integer", 2)]),
            [PrintStatement(Identifier("item"))],
        )
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "parallel-for lowered to sequential loop" in c_code
    assert "for (int _i = 0; _i < 2; _i++)" in c_code


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
