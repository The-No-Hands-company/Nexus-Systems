"""Matrix-style hardening coverage for macro expansion across compiler backends."""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(code: str):
    lexer = Lexer(code)
    tokens = lexer.scan_tokens()
    parser = Parser(tokens)
    return parser.parse()


def _generate(code: str, backend: str) -> str:
    ast = _parse(code)
    if backend == "c":
        return CCodeGenerator(target="c").generate(ast)
    if backend == "llvm":
        return LLVMIRGenerator().generate(ast)
    raise ValueError(f"Unsupported backend {backend}")


VALUE_SPECS = [
    ("int_literal", "", "7", "7"),
    ("string_literal", "", '"payload"', "payload"),
    ("identifier", "set seed to 5", "seed", "seed"),
    ("binary_expr", "", "1 plus 2", None),
]

MACRO_BODY_SPECS = [
    (
        "single_local",
        """
macro EMIT with value
    set temp to value
    print text temp
end
""",
    ),
    (
        "double_local",
        """
macro EMIT with value
    set shadow to value
    set temp to shadow
    print text temp
end
""",
    ),
    (
        "local_copy",
        """
macro EMIT with value
    set mirror to value
    set temp to mirror
    print text temp
end
""",
    ),
]


@pytest.mark.parametrize("backend", ["c", "llvm"])
@pytest.mark.parametrize("value_spec", VALUE_SPECS, ids=[spec[0] for spec in VALUE_SPECS])
@pytest.mark.parametrize("body_spec", MACRO_BODY_SPECS, ids=[spec[0] for spec in MACRO_BODY_SPECS])
def test_macro_expansion_hygiene_matrix(
    backend: str,
    value_spec,
    body_spec,
):
    _, setup_stmt, arg_expr, expected_hint = value_spec
    _, macro_block = body_spec

    setup_block = ""
    if setup_stmt:
        setup_block = f"    {setup_stmt}\n"

    code = (
        f"{macro_block}\n"
        "function main returns Integer\n"
        "    set temp to 99\n"
        f"{setup_block}"
        f"    expand EMIT with value {arg_expr}\n"
        "    print text temp\n"
        "    return 0\n"
        "end\n"
    )

    output = _generate(code, backend)

    # Core expansion + hygiene expectations.
    assert "__macro_EMIT_" in output
    assert "printf" in output

    # Optional hint checks to ensure substitution payload reached backend IR/C.
    if expected_hint is not None:
        assert expected_hint in output


MSG_SPECS = [
    ("msg_literal", "", '"hello-msg"', "hello-msg"),
    ("msg_identifier", 'set msg_seed to "seed-msg"', "msg_seed", "seed-msg"),
    ("msg_numeric", "", "42", None),
]

VAL_SPECS = [
    ("val_literal", "", "3", "3"),
    ("val_identifier", "set delta to 4", "delta", "delta"),
    ("val_binary", "", "1 plus 2", None),
]


@pytest.mark.parametrize("backend", ["c", "llvm"])
@pytest.mark.parametrize("msg_spec", MSG_SPECS, ids=[spec[0] for spec in MSG_SPECS])
@pytest.mark.parametrize("val_spec", VAL_SPECS, ids=[spec[0] for spec in VAL_SPECS])
def test_macro_two_arg_substitution_matrix(backend: str, msg_spec, val_spec):
    _, msg_setup, msg_expr, msg_hint = msg_spec
    _, val_setup, val_expr, val_hint = val_spec

    setup_lines = []
    if msg_setup:
        setup_lines.append(f"    {msg_setup}")
    if val_setup:
        setup_lines.append(f"    {val_setup}")
    setup_block = "\n".join(setup_lines)
    if setup_block:
        setup_block += "\n"

    code = (
        "macro MIX with msg, value\n"
        "    set local to value\n"
        "    print text msg\n"
        "    print text local\n"
        "end\n\n"
        "function main returns Integer\n"
        "    set local to 100\n"
        f"{setup_block}"
        f"    expand MIX with msg {msg_expr}, value {val_expr}\n"
        "    print text local\n"
        "    return 0\n"
        "end\n"
    )

    output = _generate(code, backend)

    # Expansion emitted and local collision names are hygienic.
    assert "__macro_MIX_" in output
    assert "printf" in output

    if msg_hint is not None:
        assert msg_hint in output
    if val_hint is not None:
        assert val_hint in output
