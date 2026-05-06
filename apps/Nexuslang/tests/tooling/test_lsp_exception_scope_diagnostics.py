"""Diagnostics coverage for exception-scope misuse and unreachable catch hints."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def test_catch_variable_usage_outside_catch_is_reported():
    provider = _provider()
    source = """
try
    set value to 1
catch err
    print text err
end
print text err
"""

    diagnostics = provider._check_exception_scope_and_unreachable_catch(source)

    assert any("only defined inside its catch block" in d["message"] for d in diagnostics)
    leaking = [d for d in diagnostics if "only defined inside its catch block" in d["message"]]
    assert leaking[0]["code"] == "E100"
    assert leaking[0]["source"] == "nlpl"


def test_return_before_catch_reports_likely_unreachable_catch():
    provider = _provider()
    source = """
function main returns Integer
    try
        return 1
    catch err
        print text err
    end
end
"""

    diagnostics = provider._check_exception_scope_and_unreachable_catch(source)

    assert any("Likely unreachable catch block" in d["message"] for d in diagnostics)
    unreachable = [d for d in diagnostics if "Likely unreachable catch block" in d["message"]]
    assert unreachable[0]["code"] == "E309"
    assert unreachable[0]["source"] == "nlpl"
