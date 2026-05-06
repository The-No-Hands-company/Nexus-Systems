"""Tooling diagnostics coverage for unsafe blocks and FFI extern signatures."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def test_unsafe_requires_do_keyword():
    code = """
unsafe
    set x to 1
end
"""

    diagnostics = _provider().get_diagnostics("file:///unsafe_missing_do.nxl", code, check_imports=False)

    unsafe_diags = [d for d in diagnostics if "unsafe block must use 'unsafe do'" in d.get("message", "").lower()]
    assert unsafe_diags, "Expected unsafe opener diagnostic"
    assert unsafe_diags[0].get("code") == "E201"
    assert unsafe_diags[0].get("data", {}).get("category") == "ffi"


def test_unsafe_block_must_be_closed_with_end():
    code = """
unsafe do
    set x to 1
"""

    diagnostics = _provider().get_diagnostics("file:///unsafe_unclosed.nxl", code, check_imports=False)

    unclosed = [d for d in diagnostics if "not closed with 'end'" in d.get("message", "").lower()]
    assert unclosed, "Expected unclosed unsafe block diagnostic"
    fixes = unclosed[0].get("data", {}).get("fixes", [])
    assert any("closing 'end'" in f.lower() for f in fixes)


def test_extern_function_requires_library_and_parameter_types():
    code = """
extern function puts with text returns Integer
"""

    diagnostics = _provider().get_diagnostics("file:///extern_missing_parts.nxl", code, check_imports=False)

    lib_diag = [d for d in diagnostics if "must specify source library" in d.get("message", "").lower()]
    param_diag = [d for d in diagnostics if "must declare type using 'as <type>'" in d.get("message", "").lower()]

    assert lib_diag, "Expected source library diagnostic"
    assert param_diag, "Expected typed parameter diagnostic"
    assert lib_diag[0].get("data", {}).get("category") == "ffi"


def test_extern_calling_convention_is_validated():
    code = """
extern function demo returns Integer from library "demo" calling convention weirdcc
"""

    diagnostics = _provider().get_diagnostics("file:///extern_bad_callconv.nxl", code, check_imports=False)

    callconv_diags = [d for d in diagnostics if "unsupported calling convention" in d.get("message", "").lower()]
    assert callconv_diags, "Expected unsupported calling convention diagnostic"

    fixes = callconv_diags[0].get("data", {}).get("fixes", [])
    assert any("supported calling conventions" in f.lower() for f in fixes)
