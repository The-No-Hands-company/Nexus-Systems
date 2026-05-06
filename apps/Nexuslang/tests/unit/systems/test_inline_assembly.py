"""Inline assembly interpreter policy tests."""

import os
import sys

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../src")))

from nexuslang.errors import NxlRuntimeError
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


def _parse(source: str):
    tokens = Lexer(source).tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


def _make_interpreter(*, enable_type_checking=False, inline_asm_policy=None):
    runtime = Runtime()
    register_stdlib(runtime)
    if inline_asm_policy is not None:
        runtime.inline_asm_policy = inline_asm_policy
    return Interpreter(runtime, enable_type_checking=enable_type_checking)


class TestInlineAssemblyInterpreterPolicy:
    ASM_SRC = 'asm\n    code\n        "nop"\nend\n'

    def test_strict_mode_raises_runtime_error(self):
        ast = _parse(self.ASM_SRC)
        interpreter = _make_interpreter(enable_type_checking=True)

        with pytest.raises(NxlRuntimeError, match="Inline assembly is not executed in interpreter mode"):
            interpreter.interpret(ast)

    def test_warn_policy_skips_and_continues_program(self, capsys):
        source = (
            'set x to 1\n'
            'asm\n'
            '    code\n'
            '        "nop"\n'
            'end\n'
            'set y to x plus 1\n'
        )
        ast = _parse(source)
        interpreter = _make_interpreter(inline_asm_policy="warn")

        interpreter.interpret(ast)

        assert interpreter.get_variable("y") == 2
        captured = capsys.readouterr()
        assert "Inline assembly is only fully supported in compiled mode" in captured.out

    def test_skip_policy_is_silent(self, capsys):
        source = (
            'set x to 10\n'
            'asm\n'
            '    code\n'
            '        "nop"\n'
            'end\n'
            'set y to x\n'
        )
        ast = _parse(source)
        interpreter = _make_interpreter(inline_asm_policy="skip")

        interpreter.interpret(ast)

        assert interpreter.get_variable("y") == 10
        captured = capsys.readouterr()
        assert "Inline assembly" not in captured.out

    def test_invalid_policy_raises_runtime_error(self):
        ast = _parse(self.ASM_SRC)
        interpreter = _make_interpreter(inline_asm_policy="invalid")

        with pytest.raises(NxlRuntimeError, match="Invalid inline_asm_policy"):
            interpreter.interpret(ast)
