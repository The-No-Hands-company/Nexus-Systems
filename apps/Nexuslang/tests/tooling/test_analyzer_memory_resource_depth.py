"""Deep coverage tests for memory/resource checker advanced diagnostics."""

import os
import sys
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

from nexuslang.parser.ast import (  # noqa: E402
    BinaryOperation,
    DereferenceExpression,
    ForLoop,
    FunctionDefinition,
    FunctionCall,
    Identifier,
    IfStatement,
    IndexExpression,
    ListExpression,
    Literal,
    Program,
    ReturnStatement,
    TryCatch,
    VariableDeclaration,
)
from nexuslang.tooling.analyzer.checks.memory_safety import MemorySafetyChecker  # noqa: E402
from nexuslang.tooling.analyzer.checks.resource_leak import ResourceLeakChecker  # noqa: E402


def _codes(checker, ast, source: str):
    return [i.code for i in checker().check(ast, source, source.splitlines())]


def test_memory_m009_out_of_bounds_index_detection():
    source = "\n".join([
        "set arr to [1, 2]",
        "set x to arr[5]",
    ])
    ast = Program([
        VariableDeclaration("arr", ListExpression([Literal("number", 1), Literal("number", 2)], line_number=1), line_number=1),
        VariableDeclaration(
            "x",
            IndexExpression(Identifier("arr", line_number=2), Literal("number", 5), line_number=2),
            line_number=2,
        ),
    ])

    assert "M009" in _codes(MemorySafetyChecker, ast, source)


def test_memory_m010_null_dereference_detection():
    source = "\n".join([
        "set ptr to null",
        "set x to dereference ptr",
    ])
    ast = Program([
        VariableDeclaration("ptr", Literal("null", None), line_number=1),
        VariableDeclaration("x", DereferenceExpression(Identifier("ptr", line_number=2), line_number=2), line_number=2),
    ])

    assert "M010" in _codes(MemorySafetyChecker, ast, source)


def test_resource_r009_cascade_leak_in_loop():
    source = "\n".join([
        "for each item in items",
        "set f to open with \"file.txt\"",
        "end",
    ])
    loop = ForLoop(
        iterator="item",
        iterable=Identifier("items", line_number=1),
        body=[
            VariableDeclaration(
                "f",
                FunctionCall("open", [Literal("string", "file.txt")], line_number=2),
                line_number=2,
            )
        ],
        line_number=1,
    )
    ast = Program([loop])

    assert "R009" in _codes(ResourceLeakChecker, ast, source)


def test_resource_r010_exception_path_leak():
    source = "\n".join([
        "try",
        "set f to open with \"file.txt\"",
        "catch err",
        "print text err",
        "end",
    ])
    ast = Program([
        TryCatch(
            try_block=[
                VariableDeclaration(
                    "f",
                    FunctionCall("open", [Literal("string", "file.txt")], line_number=2),
                    line_number=2,
                )
            ],
            catch_block=[
                FunctionCall("print", [Identifier("err", line_number=4)], line_number=4)
            ],
            exception_var="err",
            line_number=1,
        )
    ])

    assert "R010" in _codes(ResourceLeakChecker, ast, source)


def test_memory_return_leak_does_not_cross_function_boundaries():
    source = "\n".join([
        "function first returns Integer",
        "set ptr to alloc with 16",
        "return 0",
        "end",
        "function second returns Integer",
        "return 0",
        "end",
    ])

    ast = Program([
        FunctionDefinition(
            "first",
            [],
            body=[
                VariableDeclaration(
                    "ptr",
                    FunctionCall("alloc", [Literal("number", 16)], line_number=2),
                    line_number=2,
                ),
                ReturnStatement(Literal("number", 0), line_number=3),
            ],
            return_type="Integer",
            line_number=1,
        ),
        FunctionDefinition(
            "second",
            [],
            body=[
                ReturnStatement(Literal("number", 0), line_number=6),
            ],
            return_type="Integer",
            line_number=5,
        ),
    ])

    codes = _codes(MemorySafetyChecker, ast, source)
    assert codes.count("M008") == 1


def test_resource_r008_only_for_connection_apis():
    source = "receive from ch"
    ast = Program([
        FunctionCall("receive", [Identifier("ch", line_number=1)], line_number=1),
    ])

    assert "R008" not in _codes(ResourceLeakChecker, ast, source)


def test_resource_r008_connect_without_timeout_still_detected():
    source = "connect to server"
    ast = Program([
        FunctionCall("connect", [Literal("string", "server")], line_number=1),
    ])

    assert "R008" in _codes(ResourceLeakChecker, ast, source)


def test_memory_alias_free_marks_original_as_freed():
    source = "\n".join([
        "set ptr to alloc with 32",
        "set alias to ptr",
        "call dealloc with alias",
        "set value to dereference ptr",
    ])
    ast = Program([
        VariableDeclaration("ptr", FunctionCall("alloc", [Literal("number", 32)], line_number=1), line_number=1),
        VariableDeclaration("alias", Identifier("ptr", line_number=2), line_number=2),
        FunctionCall("dealloc", [Identifier("alias", line_number=3)], line_number=3),
        VariableDeclaration("value", DereferenceExpression(Identifier("ptr", line_number=4), line_number=4), line_number=4),
    ])

    codes = _codes(MemorySafetyChecker, ast, source)
    assert "M002" not in codes
    assert "M006" in codes


def test_memory_use_after_free_detected_in_binary_expression():
    source = "\n".join([
        "set ptr to alloc with 8",
        "call dealloc with ptr",
        "set x to ptr plus 1",
    ])
    ast = Program([
        VariableDeclaration("ptr", FunctionCall("alloc", [Literal("number", 8)], line_number=1), line_number=1),
        FunctionCall("dealloc", [Identifier("ptr", line_number=2)], line_number=2),
        VariableDeclaration(
            "x",
            BinaryOperation(Identifier("ptr", line_number=3), "plus", Literal("number", 1), line_number=3),
            line_number=3,
        ),
    ])

    assert "M006" in _codes(MemorySafetyChecker, ast, source)


def test_resource_r010_not_reported_when_catch_releases_on_all_branches():
    source = "try/catch with full release"
    ast = Program([
        TryCatch(
            try_block=[
                VariableDeclaration(
                    "conn",
                    FunctionCall("open_connection", [Literal("string", "db")], line_number=2),
                    line_number=2,
                )
            ],
            catch_block=[
                IfStatement(
                    condition=Literal("boolean", True),
                    then_block=[FunctionCall("close_connection", [Identifier("conn", line_number=5)], line_number=5)],
                    else_block=[FunctionCall("close_connection", [Identifier("conn", line_number=6)], line_number=6)],
                    line_number=4,
                )
            ],
            exception_var="err",
            line_number=1,
        )
    ])

    assert "R010" not in _codes(ResourceLeakChecker, ast, source)


def test_resource_r010_reported_when_catch_release_is_branch_dependent():
    source = "try/catch with partial release"
    ast = Program([
        TryCatch(
            try_block=[
                VariableDeclaration(
                    "conn",
                    FunctionCall("open_connection", [Literal("string", "db")], line_number=2),
                    line_number=2,
                )
            ],
            catch_block=[
                IfStatement(
                    condition=Literal("boolean", True),
                    then_block=[FunctionCall("close_connection", [Identifier("conn", line_number=5)], line_number=5)],
                    else_block=[],
                    line_number=4,
                )
            ],
            exception_var="err",
            line_number=1,
        )
    ])

    assert "R010" in _codes(ResourceLeakChecker, ast, source)
