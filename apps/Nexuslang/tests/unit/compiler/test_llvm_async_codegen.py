"""Regression tests for LLVM async/await lowering behavior."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../src'))

from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _generate_llvm(code: str) -> str:
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    ast = parser.parse()
    return LLVMIRGenerator().generate(ast)


def test_await_non_async_target_is_lowered_synchronously():
    """Awaiting a non-async function should not emit coroutine poll loop."""
    code = """function sync_value returns Integer
    return 42
end

async function wrapper returns Integer
    set value to await sync_value
    return value
end
"""

    llvm_ir = _generate_llvm(code)

    assert "sync_value" in llvm_ir
    assert "await.poll." not in llvm_ir


def test_await_async_target_emits_coroutine_poll_loop():
    """Awaiting a known async function should still emit coroutine polling labels."""
    code = """async function fetch_value returns Integer
    return 7
end

async function wrapper returns Integer
    set value to await fetch_value
    return value
end
"""

    llvm_ir = _generate_llvm(code)

    assert "call i8* @fetch_value()" in llvm_ir
    assert "await.poll." in llvm_ir


def test_multiple_awaits_emit_distinct_poll_labels():
    """Multiple awaits in one async body should produce multiple poll suspension labels."""
    code = """async function fetch_a returns Integer
    return 2
end

async function fetch_b returns Integer
    return 3
end

async function wrapper returns Integer
    set a to await fetch_a
    set b to await fetch_b
    return a plus b
end
"""

    llvm_ir = _generate_llvm(code)

    assert llvm_ir.count("await.poll.") >= 2
    assert "call i8* @fetch_a()" in llvm_ir
    assert "call i8* @fetch_b()" in llvm_ir


def test_await_async_call_with_arguments_uses_coroutine_path():
    """Awaiting an async call expression with args should still go through coroutine polling."""
    code = """async function add_one with n as Integer returns Integer
    return n plus 1
end

async function wrapper returns Integer
    set value to await add_one with 41
    return value
end
"""

    llvm_ir = _generate_llvm(code)

    assert "call i8* @add_one(i64 41)" in llvm_ir
    assert "await.poll." in llvm_ir
