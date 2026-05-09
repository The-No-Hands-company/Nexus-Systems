"""LSP diagnostics coverage for ownership/borrow conflict shaping."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def _ownership_diags(code: str):
    diagnostics = _provider().get_diagnostics("file:///ownership_conflict.nxl", code, check_imports=False)
    return [d for d in diagnostics if "Ownership error:" in d.get("message", "")]


def test_borrow_conflict_message_is_shaped_for_lsp():
    code = """
set x to 10
set b to borrow x
set y to move x
"""

    diagnostics = _ownership_diags(code)

    assert diagnostics, "Expected ownership diagnostic for move while borrowed"
    diag = diagnostics[0]
    assert diag.get("code") == "E201"
    assert diag.get("source") == "nlpl"
    assert "Ownership error:" in diag.get("message", "")
    assert "Type error:" not in diag.get("message", "")


def test_borrow_conflict_includes_related_ranges():
    code = """
set x to 10
set b to borrow x
set y to move x
"""

    diagnostics = _ownership_diags(code)
    assert diagnostics, "Expected ownership diagnostic for move while borrowed"

    related = diagnostics[0].get("relatedInformation")
    assert isinstance(related, list)
    assert related, "Expected relatedInformation ranges for borrow conflict"

    related_messages = [entry.get("message", "") for entry in related]
    assert any("borrow" in msg.lower() for msg in related_messages)

    for entry in related:
        location = entry.get("location", {})
        uri = location.get("uri", "")
        rng = location.get("range", {})
        assert uri == "file:///ownership_conflict.nxl"
        assert "start" in rng and "end" in rng


def test_ownership_diagnostic_suggests_memory_fixes():
    code = """
set x to 0
set m to borrow mutable x
set n to borrow x
"""

    diagnostics = _ownership_diags(code)
    assert diagnostics, "Expected ownership diagnostic for mutable/immutable borrow conflict"

    fixes = diagnostics[0].get("data", {}).get("fixes", [])
    assert fixes, "Expected ownership quick-fix guidance"
    assert any("borrow" in fix.lower() or "move" in fix.lower() for fix in fixes)
