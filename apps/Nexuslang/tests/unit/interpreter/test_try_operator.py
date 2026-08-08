"""Focused coverage tests for ? operator runtime behavior."""

from types import SimpleNamespace

import pytest

from nexuslang.interpreter.try_operator import execute_TryExpression
from nexuslang.stdlib.option_result import Result


class DummyInterpreter:
    def __init__(self, result_value):
        self.result_value = result_value
        self.return_value = None

    def execute(self, expression):
        assert expression == "expr"
        return self.result_value


def test_try_expression_unwraps_ok_result():
    interpreter = DummyInterpreter(Result(value=123, is_ok=True))
    node = SimpleNamespace(expression="expr")

    value = execute_TryExpression(interpreter, node)

    assert value == 123
    assert interpreter.return_value is None


def test_try_expression_propagates_err_result_to_return_value():
    err = Result(error="boom", is_ok=False)
    interpreter = DummyInterpreter(err)
    node = SimpleNamespace(expression="expr")

    value = execute_TryExpression(interpreter, node)

    assert value is None
    assert interpreter.return_value is err


def test_try_expression_rejects_non_result_values():
    interpreter = DummyInterpreter("not-a-result")
    node = SimpleNamespace(expression="expr")

    with pytest.raises(RuntimeError, match=r"\? operator can only be used on Result types"):
        execute_TryExpression(interpreter, node)
