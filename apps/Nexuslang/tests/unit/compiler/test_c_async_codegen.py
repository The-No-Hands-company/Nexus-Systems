"""C backend regression tests for async coroutine-frame lowering."""

import os
import sys


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler.backends.c_generator import CCodeGenerator
from nexuslang.parser.ast import (
    Program,
    AsyncFunctionDefinition,
    FunctionDefinition,
    Parameter,
    FunctionCall,
    ReturnStatement,
    VariableDeclaration,
    AwaitExpression,
    Literal,
    Identifier,
)


def _c(nodes):
    return CCodeGenerator(target="c").generate(Program(nodes))


def test_c_async_function_emits_coroutine_frame_signature_and_result_store():
    c_code = _c([
        AsyncFunctionDefinition(
            name="fetch",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 7))],
            return_type="Integer",
        ),
    ])

    assert "NXLAsyncFrame* fetch(void)" in c_code
    assert "NXLAsyncFrame* __nxl_async_frame = nxl_async_frame_create();" in c_code
    assert "__nxl_async_frame->result_i64 = (intptr_t)(7);" in c_code
    assert "return __nxl_async_frame;" in c_code


def test_c_await_on_async_task_variable_emits_poll_resume_extract_destroy_sequence():
    c_code = _c([
        AsyncFunctionDefinition(
            name="fetch",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 7))],
            return_type="Integer",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration("task", FunctionCall("fetch", [])),
                VariableDeclaration("value", AwaitExpression(Identifier("task"))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Integer",
        ),
    ])

    assert "NXLAsyncFrame* task = fetch();" in c_code
    assert "while (!_nxl_task->done) { nxl_async_resume(_nxl_task); }" in c_code
    assert "nxl_async_destroy(_nxl_task);" in c_code


def test_c_await_on_async_function_identifier_emits_implicit_async_call():
    c_code = _c([
        AsyncFunctionDefinition(
            name="fetch",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 9))],
            return_type="Integer",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration("value", AwaitExpression(Identifier("fetch"))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Integer",
        ),
    ])

    assert "_nxl_task = fetch();" in c_code
    assert "int value = ({ NXLAsyncFrame* _nxl_task = fetch();" in c_code


def test_c_await_expression_is_null_safe_with_default_result_initialization():
    c_code = _c([
        AsyncFunctionDefinition(
            name="fetch",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 9))],
            return_type="Integer",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration("task", FunctionCall("fetch", [])),
                VariableDeclaration("value", AwaitExpression(Identifier("task"))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Integer",
        ),
    ])

    assert "int _nxl_result = 0; if (_nxl_task) {" in c_code
    assert "while (!_nxl_task->done) { nxl_async_resume(_nxl_task); }" in c_code


def test_c_await_async_call_with_arguments_emits_direct_task_call_and_extract():
    c_code = _c([
        AsyncFunctionDefinition(
            name="add_one",
            parameters=[Parameter("n", "Integer")],
            body=[ReturnStatement(Literal("integer", 1))],
            return_type="Integer",
        ),
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[
                VariableDeclaration("value", AwaitExpression(FunctionCall("add_one", [Literal("integer", 41)]))),
                ReturnStatement(Identifier("value")),
            ],
            return_type="Integer",
        ),
    ])

    assert "_nxl_task = add_one(41);" in c_code
    assert "nxl_async_destroy(_nxl_task);" in c_code
