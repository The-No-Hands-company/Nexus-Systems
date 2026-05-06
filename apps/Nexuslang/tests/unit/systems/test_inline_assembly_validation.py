"""Typechecker validation tests for inline assembly operands and clobbers."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.parser.ast import Identifier, InlineAssembly, Literal, Program, VariableDeclaration
from nexuslang.typesystem.typechecker import TypeChecker


def _check_program(*statements):
    checker = TypeChecker()
    checker.check_program(Program(list(statements)))
    return checker.errors


def test_inline_assembly_accepts_valid_constraints_and_clobbers():
    errors = _check_program(
        VariableDeclaration("x", Literal("integer", 7)),
        VariableDeclaration("out", Literal("integer", 0)),
        InlineAssembly(
            asm_code=["nop"],
            inputs=[("\"r\"", Identifier("x"))],
            outputs=[("\"=r\"", Identifier("out"))],
            clobbers=["\"memory\"", "\"cc\"", "\"rax\""],
        ),
    )

    assert errors == []


def test_inline_assembly_rejects_invalid_input_constraint():
    errors = _check_program(
        VariableDeclaration("x", Literal("integer", 1)),
        InlineAssembly(
            asm_code=["nop"],
            inputs=[("\"bad constraint\"", Identifier("x"))],
        ),
    )

    assert any("Invalid inline assembly input constraint" in err for err in errors)


def test_inline_assembly_rejects_output_constraint_without_write_marker():
    errors = _check_program(
        VariableDeclaration("out", Literal("integer", 0)),
        InlineAssembly(
            asm_code=["nop"],
            outputs=[("\"r\"", Identifier("out"))],
        ),
    )

    assert any("Invalid inline assembly output constraint" in err for err in errors)


def test_inline_assembly_rejects_non_identifier_output_operand():
    errors = _check_program(
        InlineAssembly(
            asm_code=["nop"],
            outputs=[("\"=r\"", Literal("integer", 1))],
        ),
    )

    assert any("output operand must be an identifier" in err for err in errors)


def test_inline_assembly_rejects_invalid_and_duplicate_clobbers():
    errors = _check_program(
        InlineAssembly(
            asm_code=["nop"],
            clobbers=["\"rax!\"", "\"memory\"", "\"memory\""],
        ),
    )

    assert any("Invalid inline assembly clobber" in err for err in errors)
    assert any("Duplicate inline assembly clobber" in err for err in errors)
