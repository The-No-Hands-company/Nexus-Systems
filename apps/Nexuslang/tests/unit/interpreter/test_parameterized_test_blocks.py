from types import SimpleNamespace

import pytest

from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


@pytest.fixture
def interpreter():
    runtime = Runtime()
    register_stdlib(runtime)
    return Interpreter(runtime)


def test_run_parameterized_cases_binds_params_and_runs_setup_teardown(interpreter, monkeypatch):
    node = SimpleNamespace(
        name="parameterized suite",
        params=["left", "right"],
        cases=[[1, 2], [3, 4]],
        body=["body_stmt"],
    )

    setup_stmt = "setup_stmt"
    teardown_stmt = "teardown_stmt"

    seen_pairs = []
    call_order = []

    def _execute(stmt):
        if isinstance(stmt, int):
            return stmt
        if stmt == setup_stmt:
            call_order.append("setup")
            return None
        if stmt == teardown_stmt:
            call_order.append("teardown")
            return None
        if stmt == "body_stmt":
            scope = interpreter.current_scope[-1]
            seen_pairs.append((scope["left"], scope["right"]))
            call_order.append("body")
            return None
        raise AssertionError(f"Unexpected statement: {stmt!r}")

    monkeypatch.setattr(interpreter, "execute", _execute)

    results = interpreter._run_parameterized_cases(node, [setup_stmt], [teardown_stmt])

    assert len(results) == 2
    assert all(r["passed"] for r in results)
    assert seen_pairs == [(1, 2), (3, 4)]
    assert call_order == ["setup", "body", "teardown", "setup", "body", "teardown"]


def test_run_parameterized_cases_assertion_failure_keeps_teardown(interpreter, monkeypatch):
    node = SimpleNamespace(
        name="failing assertion suite",
        params=[],
        cases=[[10]],
        body=["failing_stmt"],
    )

    teardown_calls = []

    def _execute(stmt):
        if isinstance(stmt, int):
            return stmt
        if stmt == "failing_stmt":
            raise AssertionError("expectation failed")
        if stmt == "teardown_stmt":
            teardown_calls.append(True)
            return None
        raise AssertionError(f"Unexpected statement: {stmt!r}")

    monkeypatch.setattr(interpreter, "execute", _execute)

    results = interpreter._run_parameterized_cases(node, [], ["teardown_stmt"])

    assert len(results) == 1
    assert results[0]["passed"] is False
    assert results[0]["error"] == "expectation failed"
    assert teardown_calls == [True]


def test_run_parameterized_cases_exception_path(interpreter, monkeypatch):
    node = SimpleNamespace(
        name="exception suite",
        params=[],
        cases=[[1]],
        body=["boom_stmt"],
    )

    def _execute(stmt):
        if isinstance(stmt, int):
            return stmt
        if stmt == "boom_stmt":
            raise ValueError("boom")
        return None

    monkeypatch.setattr(interpreter, "execute", _execute)

    results = interpreter._run_parameterized_cases(node, [], [])

    assert len(results) == 1
    assert results[0]["passed"] is False
    assert results[0]["error"] == "ValueError: boom"


def test_run_parameterized_cases_reports_teardown_failure(interpreter, monkeypatch):
    node = SimpleNamespace(
        name="teardown failure suite",
        params=[],
        cases=[[1]],
        body=["ok_stmt"],
    )

    def _execute(stmt):
        if isinstance(stmt, int):
            return stmt
        if stmt == "ok_stmt":
            return None
        if stmt == "teardown_stmt":
            raise RuntimeError("cleanup blew up")
        return None

    monkeypatch.setattr(interpreter, "execute", _execute)

    results = interpreter._run_parameterized_cases(node, [], ["teardown_stmt"])

    assert len(results) == 1
    assert results[0]["passed"] is True
    assert results[0]["error"] == "Teardown failed: cleanup blew up"


def test_execute_parameterized_test_block_delegates_and_prints_summary(interpreter, monkeypatch):
    node = SimpleNamespace(name="delegation", params=[], cases=[], body=[])
    expected = [{"name": "delegation (case 1)", "passed": True, "error": None, "duration": 0.0}]

    captured = {}

    monkeypatch.setattr(interpreter, "_run_parameterized_cases", lambda _n, _s, _t: expected)

    def _capture_summary(results, suite_name=None):
        captured["results"] = results
        captured["suite_name"] = suite_name

    monkeypatch.setattr(interpreter, "_print_test_summary", _capture_summary)

    returned = interpreter.execute_parameterized_test_block(node)

    assert returned == expected
    assert captured["results"] == expected
    assert captured["suite_name"] == "delegation"
