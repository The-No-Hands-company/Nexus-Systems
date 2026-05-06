"""Tooling diagnostics coverage for async/await/spawn misuse cases."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def test_await_outside_async_function_reports_invalid_operation():
    code = """
set result to await task
"""

    diagnostics = _provider().get_diagnostics("file:///await_outside_async.nxl", code)

    assert diagnostics, "Expected diagnostics for await outside async function"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("outside an async function" in d.get("message", "") for d in diagnostics)


def test_await_inside_async_function_does_not_report_scope_error():
    code = """
async function fetch returns Integer
    set result to await task
    return result
end
"""

    diagnostics = _provider().get_diagnostics("file:///await_inside_async.nxl", code)

    scope_errors = [
        d for d in diagnostics
        if d.get("code") == "E201" and "outside an async function" in d.get("message", "")
    ]
    assert not scope_errors, "Did not expect await-outside-async diagnostics inside async function"


def test_bare_spawn_reports_missing_target_diagnostic():
    code = """
spawn
"""

    diagnostics = _provider().get_diagnostics("file:///bare_spawn.nxl", code)

    assert diagnostics, "Expected diagnostics for bare spawn expression"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("missing target" in d.get("message", "").lower() for d in diagnostics)


def test_bare_await_reports_missing_awaitable_diagnostic():
    code = """
async function fetch returns Integer
    await
    return 0
end
"""

    diagnostics = _provider().get_diagnostics("file:///bare_await.nxl", code)

    assert diagnostics, "Expected diagnostics for bare await expression"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("missing a task or awaitable" in d.get("message", "") for d in diagnostics)
