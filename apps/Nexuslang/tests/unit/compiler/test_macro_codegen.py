"""Compiler backend coverage for macro definition/expansion lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def test_llvm_macro_expansion_emits_expanded_body():
    ast = _parse(
        """
macro GREET
    print text "hello from macro"
end

function main returns Integer
    expand GREET
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)

    assert "hello from macro" in llvm_ir
    assert "call i32 (i8*, ...) @printf" in llvm_ir


def test_c_macro_expansion_emits_expanded_block():
    ast = _parse(
        """
macro GREET
    print text "hello from macro"
end

function main returns Integer
    expand GREET
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "hello from macro" in c_code
    assert "printf(" in c_code
    assert "{" in c_code and "}" in c_code


def test_macro_expansion_with_arguments_substitutes_identifiers():
    ast = _parse(
        """
macro SAY with msg
    print text msg
end

function main returns Integer
    expand SAY with msg "greetings"
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)
    c_code = CCodeGenerator(target="c").generate(ast)

    assert "greetings" in llvm_ir
    assert "greetings" in c_code


def test_nested_macro_expansion_is_lowered_in_both_backends():
    ast = _parse(
        """
macro INNER with msg
    print text msg
end

macro OUTER with value
    expand INNER with msg value
end

function main returns Integer
    expand OUTER with value "nested macro"
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)
    c_code = CCodeGenerator(target="c").generate(ast)

    assert "nested macro" in llvm_ir
    assert "nested macro" in c_code


def test_macro_hygiene_renames_local_collisions_in_both_backends():
    ast = _parse(
        """
macro SHOW_LOCAL with value
    set temp to value
    print text temp
end

function main returns Integer
    set temp to 100
    expand SHOW_LOCAL with value 7
    print text temp
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)
    c_code = CCodeGenerator(target="c").generate(ast)

    assert "__macro_SHOW_LOCAL_" in llvm_ir
    assert "__macro_SHOW_LOCAL_" in c_code
