"""Compiler backend coverage for comptime lowering."""

import os
import sys
import pytest


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


def test_llvm_comptime_const_is_collected_as_global_and_initialized_before_main():
    ast = _parse(
        """
comptime const BASE is 10
function add_limit with n as Integer returns Integer
    return n plus BASE
end

function main returns Integer
    print text add_limit with 5
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)

    assert "@BASE = global i64 0" in llvm_ir
    assert "store i64 10, i64* @BASE" in llvm_ir
    assert "invoke i64 @nxl_main()" in llvm_ir
    assert llvm_ir.index("store i64 10, i64* @BASE") < llvm_ir.index("invoke i64 @nxl_main()")


def test_llvm_comptime_assert_constant_true_emits_no_runtime_panic():
    ast = _parse(
        """
comptime assert 2 times 3 is equal to 6
function main returns Integer
    return 0
end
"""
    )

    llvm_ir = LLVMIRGenerator().generate(ast)

    assert "Compile-time assertion failed" not in llvm_ir


def test_c_comptime_const_uses_top_level_init_and_static_storage():
    ast = _parse(
        """
comptime const BASE is 10
function add_limit with n as Integer returns Integer
    return n plus BASE
end

function main returns Integer
    print text add_limit with 5
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "static void __nxl_top_level_init(void);" in c_code
    assert "static int BASE;" in c_code
    assert "__nxl_top_level_init();" in c_code
    assert "static void __nxl_top_level_init(void) {" in c_code
    assert "BASE = 10;" in c_code


def test_c_comptime_assert_lowers_to_guard():
    ast = _parse(
        """
function main returns Integer
    set value to 5
    comptime assert value is greater than 0 message "value must be positive"
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "value must be positive" in c_code
    assert "fprintf(stderr" in c_code
    assert "exit(1);" in c_code


def test_llvm_comptime_assert_constant_false_uses_folded_const_message():
    ast = _parse(
        """
comptime const DETAIL is "details"
comptime assert 1 is equal to 2 message "prefix: " plus DETAIL
"""
    )

    with pytest.raises(RuntimeError, match="Compile-time assertion failed: prefix: details"):
        LLVMIRGenerator().generate(ast)


def test_c_comptime_assert_constant_false_uses_folded_const_message():
    ast = _parse(
        """
comptime const DETAIL is "details"
comptime assert 1 is equal to 2 message "prefix: " plus DETAIL
"""
    )

    with pytest.raises(RuntimeError, match="Compile-time assertion failed: prefix: details"):
        CCodeGenerator(target="c").generate(ast)


def test_llvm_comptime_assert_non_foldable_message_falls_back_to_default_error():
    ast = _parse(
        """
function dynamic_message with n as Integer returns String
    return "n=" plus n
end

comptime assert 1 is equal to 2 message dynamic_message with 7
"""
    )

    with pytest.raises(RuntimeError, match="Compile-time assertion failed"):
        LLVMIRGenerator().generate(ast)


def test_c_top_level_variable_state_materialized_for_user_main():
    ast = _parse(
        """
set base to 10
function add_limit with n as Integer returns Integer
    return n plus base
end

function main returns Integer
    print text add_limit with 5
    return 0
end
"""
    )

    c_code = CCodeGenerator(target="c").generate(ast)

    assert "static int base;" in c_code
    assert "static void __nxl_top_level_init(void)" in c_code
    assert "base = 10;" in c_code
    assert "__nxl_top_level_init();" in c_code
