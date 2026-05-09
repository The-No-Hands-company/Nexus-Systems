"""
C backend round-trip integration tests: NexusLang source -> C source -> native binary -> execute.

Each test compiles a NexusLang program to C via CCodeGenerator, compiles the C source with
gcc/clang together with the NexusLang runtime C file, runs the resulting binary, and checks
stdout against expected output.

Requirements
------------
- gcc or clang installed and on PATH
- pthread available (standard on Linux/macOS)

Tests are automatically *skipped* when no C compiler is found so CI environments without
a native toolchain do not fail.
"""

import os
import shutil
import subprocess
import sys
import tempfile

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../src"))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import (
    Program,
    ParallelForLoop,
    ForLoop,
    GeneratorExpression,
    ListExpression,
    Literal,
    PrintStatement,
    Identifier,
    VariableDeclaration,
    BinaryOperation,
    FunctionDefinition,
    Parameter,
)


# ---------------------------------------------------------------------------
# Fixtures / helpers
# ---------------------------------------------------------------------------

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
_RUNTIME_C = os.path.join(
    _REPO_ROOT, "src", "nexuslang", "stdlib_native", "src", "nxl_runtime.c"
)
_RUNTIME_H_DIR = os.path.join(
    _REPO_ROOT, "src", "nexuslang", "stdlib_native", "include"
)


def _c_compiler() -> str | None:
    """Return the first available C compiler or None."""
    for cc in ("gcc", "clang", "cc"):
        if shutil.which(cc):
            return cc
    return None


skip_no_cc = pytest.mark.skipif(
    _c_compiler() is None or not os.path.isfile(_RUNTIME_C),
    reason="C compiler or NexusLang runtime source not available",
)


class CompilationFailed(Exception):
    pass


def compile_c_source_and_run(c_source: str, *, timeout: int = 15) -> tuple[int, str, str]:
    """Compile a C source string together with nxl_runtime.c and execute it.

    Returns ``(returncode, stdout, stderr)``.
    Raises ``CompilationFailed`` if compilation fails.
    """
    cc = _c_compiler()
    if cc is None:
        raise CompilationFailed("No C compiler found")

    with tempfile.TemporaryDirectory() as tmpdir:
        src_path = os.path.join(tmpdir, "program.c")
        exe_path = os.path.join(tmpdir, "program")

        with open(src_path, "w") as fh:
            fh.write(c_source)

        cmd = [
            cc,
            "-O0", "-g",
            f"-I{_RUNTIME_H_DIR}",
            src_path,
            _RUNTIME_C,
            "-o", exe_path,
            "-lpthread", "-lm",
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise CompilationFailed(
                f"Compilation failed:\nCMD: {' '.join(cmd)}\n"
                f"STDERR:\n{result.stderr}\n"
                f"SOURCE:\n{c_source}"
            )

        run_result = subprocess.run(
            [exe_path], capture_output=True, text=True, timeout=timeout
        )
        return run_result.returncode, run_result.stdout, run_result.stderr


def compile_nlpl_to_c_and_run(source: str, *, timeout: int = 15) -> tuple[int, str, str]:
    """Full pipeline: NexusLang source -> CCodeGenerator -> compile -> execute."""
    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()
    c_source = CCodeGenerator(target="c").generate(ast)
    return compile_c_source_and_run(c_source, timeout=timeout)


def ast_to_c_and_run(ast: Program, *, timeout: int = 15) -> tuple[int, str, str]:
    """AST -> CCodeGenerator -> compile -> execute."""
    c_source = CCodeGenerator(target="c").generate(ast)
    return compile_c_source_and_run(c_source, timeout=timeout)


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

@skip_no_cc
class TestCBackendRoundtrip:
    """Each method compiles a NexusLang program via the C backend and checks native output."""

    # --- basic output -------------------------------------------------------

    def test_hello_world(self):
        src = """
function main returns Integer
    print text "hello world"
    return 0
end
"""
        rc, out, _ = compile_nlpl_to_c_and_run(src)
        assert rc == 0
        assert "hello world" in out

    def test_print_integer(self):
        src = """
function main returns Integer
    print text 42
    return 0
end
"""
        rc, out, _ = compile_nlpl_to_c_and_run(src)
        assert rc == 0
        assert "42" in out

    # --- parallel-for without captures (direct path) -----------------------

    def test_parallel_for_no_capture_executes(self):
        """Parallel-for body runs for each element; all elements must appear in output."""
        ast = Program([
            ParallelForLoop(
                "item",
                ListExpression([
                    Literal("integer", 10),
                    Literal("integer", 20),
                    Literal("integer", 30),
                ]),
                [PrintStatement(Identifier("item"))],
            )
        ])
        c_source = CCodeGenerator(target="c").generate(ast)
        # Confirm direct (no-ctx) path was chosen
        assert "nxl_parallel_for_i64(" in c_source
        assert "nxl_parallel_for_ctx_i64" not in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        lines = [l.strip() for l in out.strip().splitlines()]
        assert set(lines) == {"10", "20", "30"}, f"Unexpected output: {out!r}"

    def test_parallel_for_single_element_no_capture(self):
        """Single-element parallel-for still runs the body exactly once."""
        ast = Program([
            ParallelForLoop(
                "v",
                ListExpression([Literal("integer", 99)]),
                [PrintStatement(Identifier("v"))],
            )
        ])
        c_source = CCodeGenerator(target="c").generate(ast)
        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        assert "99" in out

    # --- parallel-for with captures (ctx transport path) -------------------

    def test_parallel_for_single_capture_executes(self):
        """Parallel body captures one outer int; ctx transport must deliver correct value."""
        ast = Program([
            VariableDeclaration("threshold", Literal("integer", 42)),
            ParallelForLoop(
                "item",
                ListExpression([
                    Literal("integer", 1),
                    Literal("integer", 2),
                ]),
                [PrintStatement(Identifier("threshold"))],
            )
        ])
        c_source = CCodeGenerator(target="c").generate(ast)
        # Confirm ctx path was chosen
        assert "nxl_parallel_for_ctx_i64(" in c_source
        assert "__nxl_ctx_t_0" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        # Two iterations both printing 42 (order may vary under parallel execution).
        values = [l.strip() for l in out.strip().splitlines()]
        assert len(values) == 2, f"Expected 2 output lines, got: {out!r}"
        assert all(v == "42" for v in values), f"Unexpected capture values: {out!r}"

    def test_parallel_for_two_captures_executes(self):
        """Parallel body captures two outer ints via ctx struct transport."""
        ast = Program([
            VariableDeclaration("scale", Literal("integer", 3)),
            VariableDeclaration("offset", Literal("integer", 10)),
            ParallelForLoop(
                "n",
                ListExpression([Literal("integer", 1), Literal("integer", 2)]),
                [
                    PrintStatement(
                        BinaryOperation(
                            BinaryOperation(Identifier("n"), TokenType.TIMES, Identifier("scale")),
                            TokenType.PLUS,
                            Identifier("offset"),
                        )
                    )
                ],
            )
        ])
        c_source = CCodeGenerator(target="c").generate(ast)
        assert "nxl_parallel_for_ctx_i64(" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        # n=1: 1*3+10 = 13; n=2: 2*3+10 = 16
        values = sorted(l.strip() for l in out.strip().splitlines())
        assert values == ["13", "16"], f"Unexpected output: {out!r}"

    # --- parallel-for inside NexusLang function with captured args ---------

    def test_parallel_for_captures_function_parameter(self):
        """Parallel body inside a function captures a function parameter via ctx.

        Uses AST construction directly to bypass named-parameter call-site
        parsing (a separate compiler subsystem not under test here).
        """
        # Build: function print_times(base) { parallel for each x in [1,2,3]: print base }
        # Then call it as main()->print_times(7)
        from nexuslang.parser.ast import FunctionCall, ReturnStatement

        loop = ParallelForLoop(
            "x",
            ListExpression([
                Literal("integer", 1),
                Literal("integer", 2),
                Literal("integer", 3),
            ]),
            [PrintStatement(Identifier("base"))],
        )
        fn = FunctionDefinition(
            name="print_times",
            parameters=[Parameter("base", "Integer")],
            body=[loop, ReturnStatement(Literal("integer", 0))],
            return_type="Integer",
        )
        main_fn = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration("_ignored", FunctionCall("print_times", [Literal("integer", 7)])),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )
        ast = Program([fn, main_fn])

        c_source = CCodeGenerator(target="c").generate(ast)
        # Confirm ctx transport is used (base is a param captured inside the parallel body)
        assert "nxl_parallel_for_ctx_i64(" in c_source
        assert "__nxl_ctx->base" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        # Three iterations each printing 7
        values = [l.strip() for l in out.strip().splitlines()]
        assert len(values) == 3, f"Expected 3 lines, got: {out!r}"
        assert all(v == "7" for v in values), f"Unexpected output: {out!r}"

    # --- generator-expression foreach materialization ----------------------

    def test_foreach_over_filtered_mapped_generator_expression_executes(self):
        """C backend should execute for-each over generator expressions via materialization."""
        ast = Program([
            ForLoop(
                "item",
                GeneratorExpression(
                    BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 10)),
                    Identifier("x"),
                    ListExpression([
                        Literal("integer", 1),
                        Literal("integer", 2),
                        Literal("integer", 3),
                    ]),
                    BinaryOperation(Identifier("x"), TokenType.GREATER_THAN, Literal("integer", 1)),
                ),
                [PrintStatement(Identifier("item"))],
            )
        ])

        c_source = CCodeGenerator(target="c").generate(ast)
        assert "For each loop over generator expression (materialized)" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        values = [l.strip() for l in out.strip().splitlines()]
        assert values == ["12", "13"], f"Unexpected output: {out!r}"

    def test_foreach_over_generator_identifier_source_local_array_executes(self):
        """Generator expression over local array identifier should execute in C backend."""
        ast = Program([
            VariableDeclaration(
                "numbers",
                ListExpression([
                    Literal("integer", 1),
                    Literal("integer", 2),
                    Literal("integer", 3),
                ]),
            ),
            ForLoop(
                "item",
                GeneratorExpression(
                    BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 5)),
                    Identifier("x"),
                    Identifier("numbers"),
                    None,
                ),
                [PrintStatement(Identifier("item"))],
            )
        ])

        c_source = CCodeGenerator(target="c").generate(ast)
        assert "For each loop over generator expression (materialized)" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        values = [l.strip() for l in out.strip().splitlines()]
        assert values == ["6", "7", "8"], f"Unexpected output: {out!r}"

    def test_foreach_over_generator_identifier_source_with_length_metadata_executes(self):
        """Non-sized identifier sources should lower safely when <name>_length metadata exists."""
        from nexuslang.parser.ast import FunctionCall, ReturnStatement

        emit_fn = FunctionDefinition(
            name="emit_mapped",
            parameters=[
                Parameter("numbers", "Integer[]"),
                Parameter("numbers_length", "Integer"),
            ],
            body=[
                ForLoop(
                    "item",
                    GeneratorExpression(
                        BinaryOperation(Identifier("x"), TokenType.PLUS, Literal("integer", 5)),
                        Identifier("x"),
                        Identifier("numbers"),
                        None,
                    ),
                    [PrintStatement(Identifier("item"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        main_fn = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "numbers",
                    ListExpression([
                        Literal("integer", 1),
                        Literal("integer", 2),
                        Literal("integer", 3),
                    ]),
                ),
                VariableDeclaration(
                    "_ignored",
                    FunctionCall("emit_mapped", [Identifier("numbers"), Literal("integer", 3)]),
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        ast = Program([emit_fn, main_fn])

        c_source = CCodeGenerator(target="c").generate(ast)
        assert "numbers_length" in c_source
        assert "int __nxl_gen_src_bound_" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        values = [l.strip() for l in out.strip().splitlines()]
        assert values == ["6", "7", "8"], f"Unexpected output: {out!r}"

    def test_foreach_over_generator_identifier_source_with_len_alias_executes(self):
        """Generator expression should accept _len metadata alias."""
        from nexuslang.parser.ast import FunctionCall, ReturnStatement

        emit_fn = FunctionDefinition(
            name="process_len",
            parameters=[
                Parameter("data", "Integer[]"),
                Parameter("data_len", "Integer"),
            ],
            body=[
                ForLoop(
                    "val",
                    GeneratorExpression(
                        BinaryOperation(Identifier("val"), TokenType.PLUS, Literal("integer", 10)),
                        Identifier("val"),
                        Identifier("data"),
                        None,
                    ),
                    [PrintStatement(Identifier("val"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        main_fn = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "data",
                    ListExpression([Literal("integer", 1), Literal("integer", 2)]),
                ),
                VariableDeclaration(
                    "_r",
                    FunctionCall("process_len", [Identifier("data"), Literal("integer", 2)]),
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        ast = Program([emit_fn, main_fn])

        c_source = CCodeGenerator(target="c").generate(ast)
        assert "data_len" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        values = [l.strip() for l in out.strip().splitlines()]
        assert values == ["11", "12"], f"Unexpected output: {out!r}"

    def test_foreach_over_generator_identifier_source_with_size_alias_executes(self):
        """Generator expression should accept _size metadata alias."""
        from nexuslang.parser.ast import FunctionCall, ReturnStatement

        emit_fn = FunctionDefinition(
            name="process_size",
            parameters=[
                Parameter("array", "Integer[]"),
                Parameter("array_size", "Integer"),
            ],
            body=[
                ForLoop(
                    "n",
                    GeneratorExpression(
                        BinaryOperation(Identifier("n"), TokenType.PLUS, Literal("integer", 100)),
                        Identifier("n"),
                        Identifier("array"),
                        None,
                    ),
                    [PrintStatement(Identifier("n"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        main_fn = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "array",
                    ListExpression([Literal("integer", 5)]),
                ),
                VariableDeclaration(
                    "_r",
                    FunctionCall("process_size", [Identifier("array"), Literal("integer", 1)]),
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        ast = Program([emit_fn, main_fn])

        c_source = CCodeGenerator(target="c").generate(ast)
        assert "array_size" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        values = [l.strip() for l in out.strip().splitlines()]
        assert values == ["105"], f"Unexpected output: {out!r}"

    def test_foreach_over_generator_identifier_source_with_count_alias_executes(self):
        """Generator expression should accept _count metadata alias."""
        from nexuslang.parser.ast import FunctionCall, ReturnStatement

        emit_fn = FunctionDefinition(
            name="process_count",
            parameters=[
                Parameter("seq", "Integer[]"),
                Parameter("seq_count", "Integer"),
            ],
            body=[
                ForLoop(
                    "item",
                    GeneratorExpression(
                        BinaryOperation(Identifier("item"), TokenType.PLUS, Literal("integer", 20)),
                        Identifier("item"),
                        Identifier("seq"),
                        None,
                    ),
                    [PrintStatement(Identifier("item"))],
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        main_fn = FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration(
                    "seq",
                    ListExpression([Literal("integer", 1), Literal("integer", 2), Literal("integer", 3)]),
                ),
                VariableDeclaration(
                    "_r",
                    FunctionCall("process_count", [Identifier("seq"), Literal("integer", 3)]),
                ),
                ReturnStatement(Literal("integer", 0)),
            ],
            return_type="Integer",
        )

        ast = Program([emit_fn, main_fn])

        c_source = CCodeGenerator(target="c").generate(ast)
        assert "seq_count" in c_source

        rc, out, err = compile_c_source_and_run(c_source)
        assert rc == 0, f"Binary exited with {rc}; stderr={err}"
        values = [l.strip() for l in out.strip().splitlines()]
        assert values == ["21", "22", "23"], f"Unexpected output: {out!r}"
