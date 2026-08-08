"""Regression tests for backend lowering of test framework AST blocks."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.cpp_generator import CppCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    AfterEachBlock,
    BeforeEachBlock,
    DescribeBlock,
    Identifier,
    ItBlock,
    Literal,
    ParameterizedTestBlock,
    PrintStatement,
    Program,
    TestBlock,
    VariableDeclaration,
)


def _sample_program() -> Program:
    return Program(
        [
            DescribeBlock(
                "backend suite",
                [
                    BeforeEachBlock(
                        [VariableDeclaration("setup_flag", Literal("integer", 1))]
                    ),
                    ItBlock(
                        "emits body",
                        [
                            VariableDeclaration("it_value", Identifier("setup_flag")),
                            PrintStatement(Literal("string", "it body"), print_type="text"),
                        ],
                    ),
                    TestBlock(
                        "named test",
                        [
                            VariableDeclaration("test_value", Literal("integer", 2)),
                            PrintStatement(Literal("string", "test body"), print_type="text"),
                        ],
                    ),
                    AfterEachBlock(
                        [VariableDeclaration("teardown_flag", Literal("integer", 0))]
                    ),
                    ParameterizedTestBlock(
                        name="param test",
                        params=["x"],
                        cases=[[Literal("integer", 7)], [Literal("integer", 11)]],
                        body=[
                            VariableDeclaration("copied", Identifier("x")),
                        ],
                    ),
                ],
            )
        ]
    )


def test_c_codegen_lowers_testing_blocks_with_markers_and_body():
    c_code = CCodeGenerator(target="c").generate(_sample_program())

    assert "/* describe: backend suite */" in c_code
    assert "/* before each */" in c_code
    assert "/* it: emits body */" in c_code
    assert "/* test: named test */" in c_code
    assert "/* after each */" in c_code
    assert "/* parameterized test: param test */" in c_code
    assert "it body" in c_code
    assert "test body" in c_code
    assert "int x = 7;" in c_code
    assert "int x = 11;" in c_code
    assert "int copied = x;" in c_code


def test_llvm_codegen_lowers_testing_blocks_with_markers_and_body():
    ir = LLVMIRGenerator().generate(_sample_program())

    assert "; describe: backend suite" in ir
    assert "; before each" in ir
    assert "; it: emits body" in ir
    assert "; test: named test" in ir
    assert "; after each" in ir
    assert "; parameterized test: param test" in ir
    assert "it body" in ir
    assert "test body" in ir
    assert "store i64" in ir or "store i32" in ir
    assert "copied" in ir


def test_cpp_codegen_inherits_testing_block_lowering():
    cpp_code = CppCodeGenerator(target="cpp").generate(_sample_program())

    assert "/* describe: backend suite */" in cpp_code
    assert "/* before each */" in cpp_code
    assert "/* it: emits body */" in cpp_code
    assert "/* test: named test */" in cpp_code
    assert "/* after each */" in cpp_code
    assert "/* parameterized test: param test */" in cpp_code
    assert "it body" in cpp_code
    assert "test body" in cpp_code
    assert "int x = 7;" in cpp_code
    assert "auto copied = x;" in cpp_code
