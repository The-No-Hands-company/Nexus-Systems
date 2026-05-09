"""LLVM backend coverage for sizeof lowering edge-cases."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def test_llvm_sizeof_expression_target_uses_inferred_type_size():
    ast = _parse(
        """
function main returns Integer
    set numbers to [1, 2, 3]
    set size_numbers to sizeof numbers
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)

    assert "sizeof expression target" in llvm_ir
    # numbers lowers to pointer-like storage in LLVM path, so sizeof should be pointer size.
    assert "add i64 8, 0  ; sizeof expression target" in llvm_ir


def test_llvm_sizeof_sub_byte_type_rounds_up_to_one_byte():
    ast = _parse(
        """
function main returns Integer
    set flag to true
    set size_flag to sizeof [flag]
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)

    # i1 element size must round up to 1 byte, not 0.
    assert "add i64 1, 0  ; sizeof array literal" in llvm_ir
