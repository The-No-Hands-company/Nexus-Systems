"""Tooling diagnostics coverage for inline-assembly specific diagnostics metadata."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def test_inline_asm_invalid_input_constraint_includes_fix_suggestions():
    code = """
set x to 1
asm
    code
        "nop"
    inputs "bad constraint": x
end
"""

    diagnostics = _provider().get_diagnostics("file:///asm_invalid_input.nxl", code, check_imports=False)

    asm_diags = [d for d in diagnostics if "inline assembly input constraint" in d.get("message", "").lower()]
    assert asm_diags, "Expected inline-assembly constraint diagnostic"

    diag = asm_diags[0]
    assert diag.get("code") == "E201"
    assert diag.get("data", {}).get("category") == "systems"
    fixes = diag.get("data", {}).get("fixes", [])
    assert any("gcc-style input constraint" in f.lower() for f in fixes)


def test_inline_asm_duplicate_clobber_includes_fix_suggestions():
    code = """
asm
    code
        "nop"
    clobbers "memory", "memory"
end
"""

    diagnostics = _provider().get_diagnostics("file:///asm_duplicate_clobber.nxl", code, check_imports=False)

    clobber_diags = [d for d in diagnostics if "duplicate inline assembly clobber" in d.get("message", "").lower()]
    assert clobber_diags, "Expected duplicate clobber diagnostic"

    fixes = clobber_diags[0].get("data", {}).get("fixes", [])
    assert any("remove duplicate clobber" in f.lower() for f in fixes)
