"""TypeChecker coverage for async/await task typing behavior."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.parser.ast import (
    Program,
    AsyncFunctionDefinition,
    AwaitExpression,
    FunctionCall,
    Identifier,
    Literal,
    ReturnStatement,
    VariableDeclaration,
)
from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.typesystem.types import ANY_TYPE, AwaitableType, INTEGER_TYPE, get_type_by_name


def test_async_function_call_is_typed_as_awaitable_and_await_unwraps_payload():
    program = Program([
        AsyncFunctionDefinition(
            name="fetch",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 7))],
            return_type="Integer",
        ),
        AsyncFunctionDefinition(
            name="wrapper",
            parameters=[],
            body=[
                VariableDeclaration("task", FunctionCall("fetch", [])),
                VariableDeclaration("value", AwaitExpression(Identifier("task"))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Integer",
        ),
    ])

    checker = TypeChecker(enable_ownership_passes=False)
    errors = checker.check_program(program)

    assert errors == []
    fetch_type = checker.env.get_function_type("fetch")
    assert isinstance(fetch_type.return_type, AwaitableType)
    assert fetch_type.return_type.payload_type == INTEGER_TYPE


def test_await_reports_error_for_non_awaitable_identifier_operand():
    program = Program([
        AsyncFunctionDefinition(
            name="bad",
            parameters=[],
            body=[
                VariableDeclaration("x", Literal("integer", 1)),
                VariableDeclaration("y", AwaitExpression(Identifier("x"))),
                ReturnStatement(Identifier("y")),
            ],
            return_type="Integer",
        ),
    ])

    checker = TypeChecker(enable_ownership_passes=False)
    errors = checker.check_program(program)

    assert any("await expects a task or awaitable value" in err for err in errors)


def test_nested_await_reports_when_outer_await_operand_is_not_awaitable():
    program = Program([
        AsyncFunctionDefinition(
            name="fetch",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 7))],
            return_type="Integer",
        ),
        AsyncFunctionDefinition(
            name="nested",
            parameters=[],
            body=[
                VariableDeclaration(
                    "value",
                    AwaitExpression(AwaitExpression(FunctionCall("fetch", []))),
                ),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Integer",
        ),
    ])

    checker = TypeChecker(enable_ownership_passes=False)
    errors = checker.check_program(program)

    assert any("await expects a task or awaitable value" in err for err in errors)


def test_get_type_by_name_exact_awaitable_alias_returns_awaitable_type():
    awaitable_type = get_type_by_name("Awaitable")

    assert isinstance(awaitable_type, AwaitableType)
    assert awaitable_type.payload_type == ANY_TYPE


def test_get_type_by_name_task_and_promise_aliases_return_awaitable_type():
    task_type = get_type_by_name("Task")
    promise_type = get_type_by_name("Promise")

    assert isinstance(task_type, AwaitableType)
    assert task_type.payload_type == ANY_TYPE
    assert isinstance(promise_type, AwaitableType)
    assert promise_type.payload_type == ANY_TYPE
