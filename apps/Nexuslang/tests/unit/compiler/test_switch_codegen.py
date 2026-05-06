"""Backend switch lowering coverage for LLVM and C generators."""

import re
import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import (
    Program,
    VariableDeclaration,
    Literal,
    Identifier,
    SwitchStatement,
    SwitchCase,
    PrintStatement,
    FallthroughStatement,
    WhileLoop,
    BreakStatement,
    ContinueStatement,
    BinaryOperation,
)


def _sample_switch_program():
    return Program([
        VariableDeclaration("x", Literal("integer", 1)),
        SwitchStatement(
            expression=Identifier("x"),
            cases=[
                SwitchCase(
                    value=Literal("integer", 1),
                    body=[
                        PrintStatement(Literal("string", "one")),
                        FallthroughStatement(),
                    ],
                ),
                SwitchCase(
                    value=Literal("integer", 2),
                    body=[
                        PrintStatement(Literal("string", "two")),
                    ],
                ),
            ],
            default_case=[PrintStatement(Literal("string", "other"))],
        ),
    ])


def test_c_switch_lowers_cases_default_and_fallthrough():
    ast = _sample_switch_program()

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "switch (x) {" in c_code
    assert "case 1:" in c_code
    assert "case 2:" in c_code
    assert "default:" in c_code
    assert "/* fallthrough */" in c_code


def test_llvm_switch_lowers_to_switch_instruction_and_labels():
    ast = _sample_switch_program()

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    assert "switch i64" in llvm_ir
    assert "switch.case." in llvm_ir
    assert "switch.default" in llvm_ir
    assert "switch.end" in llvm_ir


def test_llvm_switch_break_inside_loop_targets_switch_end_not_loop_end():
    ast = Program([
        VariableDeclaration("x", Literal("integer", 1)),
        WhileLoop(
            condition=BinaryOperation(Identifier("x"), "is less than", Literal("integer", 3)),
            body=[
                SwitchStatement(
                    expression=Identifier("x"),
                    cases=[
                        SwitchCase(
                            value=Literal("integer", 1),
                            body=[BreakStatement()],
                        ),
                    ],
                    default_case=[PrintStatement(Literal("string", "d"))],
                ),
                BreakStatement(),
            ],
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    match = re.search(r"(switch\.case\.0[^:]*:)(.*?)(switch\.default[^:]*:)", llvm_ir, re.S)
    assert match is not None, "Expected switch case block in generated LLVM IR"
    case_block = match.group(2)

    assert re.search(r"br label %switch\.end", case_block), "break in switch case should branch to switch.end"
    assert not re.search(r"br label %while\.end", case_block), "break in switch case must not branch to while.end"


def test_llvm_switch_fallthrough_from_last_case_branches_into_default():
    ast = Program([
        VariableDeclaration("x", Literal("integer", 1)),
        SwitchStatement(
            expression=Identifier("x"),
            cases=[
                SwitchCase(
                    value=Literal("integer", 1),
                    body=[FallthroughStatement()],
                ),
            ],
            default_case=[PrintStatement(Literal("string", "other"))],
        ),
    ])

    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)

    default_match = re.search(r"switch i64 .*?, label %([^\s]+) \[", llvm_ir)
    assert default_match is not None, "Expected LLVM switch header with default label"
    default_label = default_match.group(1)

    assert re.search(rf"; fallthrough to next case\s+br label %{re.escape(default_label)}", llvm_ir), (
        "fallthrough from last case should branch into default"
    )


def test_c_labeled_loop_break_and_continue_lower_to_goto_targets():
    ast = Program([
        VariableDeclaration("x", Literal("integer", 1)),
        WhileLoop(
            condition=BinaryOperation(Identifier("x"), "is less than", Literal("integer", 3)),
            body=[
                ContinueStatement(label="outer"),
                BreakStatement(label="outer"),
            ],
            label="outer",
        ),
    ])

    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)

    assert "goto __nxl_loop_continue_" in c_code
    assert "goto __nxl_loop_break_" in c_code
    assert "__nxl_loop_continue_" in c_code
    assert "__nxl_loop_break_" in c_code
