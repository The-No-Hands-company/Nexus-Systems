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


def test_close_non_channel_reports_invalid_operation():
    code = """
set target to 42
close target
"""

    diagnostics = _provider().get_diagnostics("file:///channel_close_invalid_target.nxl", code)

    assert diagnostics, "Expected at least one diagnostic for invalid close target"
    assert any(d.get("code") == "E201" for d in diagnostics)
    assert any("Cannot close non-channel" in d.get("message", "") for d in diagnostics)


def test_close_channel_does_not_report_non_channel_error():
    code = """
set ch to create channel
close ch
"""

    diagnostics = _provider().get_diagnostics("file:///channel_close_valid_target.nxl", code)

    invalid_close = [
        d for d in diagnostics
        if d.get("code") == "E201" and "Cannot close non-channel" in d.get("message", "")
    ]
    assert not invalid_close, "Did not expect non-channel close diagnostics for channel variable"


def test_send_after_close_reports_warning():
    code = """
set ch to create channel
close ch
send 1 to ch
"""

    diagnostics = _provider().get_diagnostics("file:///channel_send_after_close.nxl", code)

    assert diagnostics, "Expected diagnostics for send after close"
    closed_send = [d for d in diagnostics if "Potential send to closed channel" in d.get("message", "")]
    assert closed_send

    fixes = closed_send[0].get("data", {}).get("fixes", [])
    assert any("Recreate channel before send" in fix for fix in fixes)
    assert any("Move this send before close" in fix for fix in fixes)


def test_receive_after_close_reports_warning():
    code = """
set ch to create channel
close ch
set value to receive from ch
"""

    diagnostics = _provider().get_diagnostics("file:///channel_receive_after_close.nxl", code)

    assert diagnostics, "Expected diagnostics for receive after close"
    closed_receive = [d for d in diagnostics if "Potential receive from closed channel" in d.get("message", "")]
    assert closed_receive

    fixes = closed_receive[0].get("data", {}).get("fixes", [])
    assert any("Recreate channel before receive" in fix for fix in fixes)
    assert any("Move close ch after this receive" in fix for fix in fixes)


def test_channel_reassignment_clears_closed_state():
    code = """
set ch to create channel
close ch
set ch to create channel
send 1 to ch
"""

    diagnostics = _provider().get_diagnostics("file:///channel_reopen_after_close.nxl", code)

    after_close = [
        d for d in diagnostics
        if "Potential send to closed channel" in d.get("message", "")
    ]
    assert not after_close, "Did not expect closed-channel warning after reassignment"
