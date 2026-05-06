"""Tooling diagnostics coverage for channel type errors."""

from unittest.mock import MagicMock

from nexuslang.lsp.diagnostics import DiagnosticsProvider


def _provider() -> DiagnosticsProvider:
    return DiagnosticsProvider(server=MagicMock())


def test_channel_payload_mismatch_reports_type_diagnostic():
    code = """
set ch to create channel
send 1 to ch
send "oops" to ch
"""

    diagnostics = _provider().get_diagnostics("file:///channel_mismatch.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for mismatched channel payload"
    assert any(d.get("code") == "E200" for d in diagnostics)
    assert any("Cannot send value of type" in d.get("message", "") for d in diagnostics)


def test_typed_channel_function_param_mismatch_reports_type_diagnostic():
    code = """
function push_bad with ch as Channel<Integer>
    send "oops" to ch
end
"""

    diagnostics = _provider().get_diagnostics("file:///channel_param_mismatch.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for typed channel mismatch"
    assert any(d.get("code") == "E200" for d in diagnostics)
    assert any("Cannot send value of type" in d.get("message", "") for d in diagnostics)


def test_send_to_non_channel_reports_invalid_operation():
    code = """
set target to 42
send 1 to target
"""

    diagnostics = _provider().get_diagnostics("file:///channel_send_invalid_target.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for invalid send target"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("Cannot send to non-channel" in d.get("message", "") for d in diagnostics)


def test_receive_from_non_channel_reports_invalid_operation():
    code = """
set src to "oops"
set value to receive from src
"""

    diagnostics = _provider().get_diagnostics("file:///channel_receive_invalid_source.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for invalid receive source"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("Cannot receive from non-channel" in d.get("message", "") for d in diagnostics)
