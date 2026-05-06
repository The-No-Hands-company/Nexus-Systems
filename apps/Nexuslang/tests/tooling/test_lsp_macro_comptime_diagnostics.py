"""Tooling diagnostics coverage for macro/comptime misuse cases."""

from pathlib import Path
from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def _fixture_text(name: str) -> str:
    fixture = (
        Path(__file__).resolve().parent.parent.parent
        / "test_programs"
        / "regression"
        / "metaprogramming"
        / name
    )
    return fixture.read_text(encoding="utf-8")


def test_expand_undefined_macro_reports_diagnostic():
    code = """
expand UNKNOWN_MACRO
"""

    diagnostics = _provider().get_diagnostics("file:///expand_undefined_macro.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for undefined macro expansion"
    assert any(d.get("code") == "E101" for d in diagnostics)
    assert any("undefined macro" in d.get("message", "").lower() for d in diagnostics)


def test_reassign_comptime_const_reports_invalid_operation():
    code = """
comptime const LIMIT is 10
set LIMIT to 20
"""

    diagnostics = _provider().get_diagnostics("file:///comptime_const_reassign.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for comptime const reassignment"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("comptime constant" in d.get("message", "").lower() for d in diagnostics)


def test_expand_defined_macro_does_not_report_undefined_macro_error():
    code = """
macro GREET
    print text "hi"
end

expand GREET
"""

    diagnostics = _provider().get_diagnostics("file:///expand_defined_macro.nxl", code)

    undefined_macro_errors = [
        d for d in diagnostics if d.get("code") == "E101" and "undefined macro" in d.get("message", "").lower()
    ]
    assert not undefined_macro_errors, "Did not expect undefined-macro diagnostics for a defined macro"


def test_invalid_fixture_reports_expected_macro_comptime_diagnostics():
    code = _fixture_text("macro_comptime_regression_invalid.nxl")

    diagnostics = _provider().get_diagnostics("file:///macro_comptime_regression_invalid.nxl", code)

    codes = {d.get("code") for d in diagnostics}
    assert "E101" in codes
    assert "E201" in codes
