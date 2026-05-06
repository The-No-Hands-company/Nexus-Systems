"""
Debugger DAP Protocol Tests
=============================

Unit tests for the NLPLDebug Adapter Protocol server.  Tests exercise the
request dispatch logic, response formatting, and event emission — all without
requiring a live subprocess or a real NexusLang program.
"""

import json
import sys
import io
import os
from pathlib import Path
from unittest.mock import MagicMock, patch, call, PropertyMock
from dataclasses import asdict

import pytest

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src"))

from nexuslang.debugger.dap_server import DAPServer, DAPCapabilities
from nexuslang.debugger import dap_server as dap_server_module
from nexuslang.debugger.debugger import Debugger, DebuggerState, Breakpoint, CallFrame


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_server() -> DAPServer:
    """Return a fresh DAPServer with mocked debugger/interpreter."""
    server = DAPServer()
    server.debugger = MagicMock(spec=Debugger)
    server.debugger.state = DebuggerState.RUNNING
    server.debugger.call_stack = []
    server.debugger.inspect_all_variables.return_value = {}
    server.debugger.inspect_variable.return_value = None
    server.interpreter = MagicMock()
    server.interpreter.global_scope = {}
    return server


# ---------------------------------------------------------------------------
# Capabilities
# ---------------------------------------------------------------------------


class TestDAPCapabilities:
    def test_supports_attach_request(self):
        caps = DAPCapabilities()
        assert caps.supportsAttachRequest is True

    def test_supports_conditional_breakpoints(self):
        caps = DAPCapabilities()
        assert caps.supportsConditionalBreakpoints is True

    def test_supports_set_variable(self):
        caps = DAPCapabilities()
        assert caps.supportsSetVariable is True

    def test_supports_terminate_request(self):
        caps = DAPCapabilities()
        assert caps.supportsTerminateRequest is True

    def test_serialisable_to_dict(self):
        caps = asdict(DAPCapabilities())
        assert isinstance(caps, dict)
        assert "supportsConditionalBreakpoints" in caps


# ---------------------------------------------------------------------------
# Message Dispatch
# ---------------------------------------------------------------------------


class TestDAPMessageDispatch:
    def test_unknown_command_returns_error_response(self, capsys):
        server = _make_server()
        responses = []
        server._send_message = lambda m: responses.append(m)

        server.handle_message({
            "type": "request",
            "command": "nonExistentCommand",
            "seq": 1,
            "arguments": {}
        })

        assert len(responses) == 1
        r = responses[0]
        assert r["type"] == "response"
        assert r["success"] is False

    def test_non_request_type_ignored(self):
        server = _make_server()
        responses = []
        server._send_message = lambda m: responses.append(m)
        # Should not raise
        server.handle_message({"type": "event", "event": "whatever"})
        assert responses == []

    def test_response_seq_increments(self):
        server = _make_server()
        sent = []
        server._send_message = lambda m: sent.append(m)

        server.send_response(1, "initialize", success=True, body={})
        server.send_response(2, "threads", success=True, body={"threads": []})

        seqs = [m["seq"] for m in sent]
        assert seqs == sorted(seqs)
        assert len(set(seqs)) == 2  # All unique


# ---------------------------------------------------------------------------
# Initialize Handler
# ---------------------------------------------------------------------------


class TestHandleInitialize:
    def test_returns_capabilities(self):
        server = DAPServer()
        events = []
        server.send_event = lambda e, b=None: events.append(e)

        result = server._handle_initialize(1, {
            "clientID": "test",
            "clientName": "pytest"
        })

        assert "supportsConditionalBreakpoints" in result
        assert "initialized" in events

    def test_stores_client_info(self):
        server = DAPServer()
        server.send_event = MagicMock()
        server._handle_initialize(1, {"clientID": "vscode", "clientName": "VS Code"})
        assert server.client_id == "vscode"
        assert server.client_name == "VS Code"


class TestHandleAttach:
    def test_attach_requires_program_path(self):
        server = DAPServer()

        with pytest.raises(ValueError, match="requires 'program'"):
            server._handle_attach(1, {})

    def test_attach_delegates_to_launch_with_program(self):
        server = DAPServer()

        with patch.object(server, "_handle_launch", return_value={"ok": True}) as launch_mock:
            result = server._handle_attach(1, {"target": "/tmp/sample.nxl"})

        assert result == {"ok": True}
        launch_mock.assert_called_once()
        _seq, attach_args = launch_mock.call_args[0]
        assert attach_args["program"] == "/tmp/sample.nxl"

    def test_attach_by_process_id_binds_registered_debuggee(self):
        dap_server_module._DEBUGGEE_REGISTRY.clear()
        owner = _make_server()
        owner.runtime = MagicMock(name="runtime")
        owner.ast = MagicMock(name="ast")
        owner._register_debuggee(os.getpid())

        server = DAPServer()
        result = server._handle_attach(1, {"processId": os.getpid()})

        assert result == {}
        assert server.runtime is owner.runtime
        assert server.interpreter is owner.interpreter
        assert server.debugger is owner.debugger
        assert server.ast is owner.ast
        assert server._attached_process_id == os.getpid()

    def test_attach_by_process_id_errors_when_missing(self):
        dap_server_module._DEBUGGEE_REGISTRY.clear()
        server = DAPServer()

        with pytest.raises(ValueError, match="No attachable NexusLang debug session"):
            server._handle_attach(1, {"processId": 987654})


# ---------------------------------------------------------------------------
# Threads Handler
# ---------------------------------------------------------------------------


class TestHandleThreads:
    def test_returns_single_main_thread(self):
        server = _make_server()
        result = server._handle_threads(1, {})
        threads = result["threads"]
        assert len(threads) == 1
        assert threads[0]["name"] == "Main Thread"
        assert "id" in threads[0]


# ---------------------------------------------------------------------------
# Breakpoint Handler
# ---------------------------------------------------------------------------


class TestHandleSetBreakpoints:
    def test_sets_breakpoints_in_debugger(self):
        server = _make_server()
        bp_mock = MagicMock(spec=Breakpoint)
        server.debugger.add_breakpoint.return_value = bp_mock

        result = server._handle_setBreakpoints(1, {
            "source": {"path": "/tmp/test.nxl"},
            "breakpoints": [{"line": 5}, {"line": 10}]
        })

        assert len(result["breakpoints"]) == 2
        assert server.debugger.add_breakpoint.call_count == 2

    def test_verified_flag_set(self):
        server = _make_server()
        server.debugger.add_breakpoint.return_value = MagicMock()

        result = server._handle_setBreakpoints(1, {
            "source": {"path": "/tmp/t.nxl"},
            "breakpoints": [{"line": 1}]
        })

        assert result["breakpoints"][0]["verified"] is True

    def test_clears_existing_before_setting(self):
        server = _make_server()
        server.debugger.add_breakpoint.return_value = MagicMock()

        server._handle_setBreakpoints(1, {
            "source": {"path": "/file.nxl"},
            "breakpoints": [{"line": 3}]
        })

        server.debugger.clear_breakpoints.assert_called_once_with("/file.nxl")

    def test_empty_breakpoints_list(self):
        server = _make_server()
        result = server._handle_setBreakpoints(1, {
            "source": {"path": "/t.nxl"},
            "breakpoints": []
        })
        assert result["breakpoints"] == []

    def test_no_source_path_returns_empty(self):
        server = _make_server()
        result = server._handle_setBreakpoints(1, {
            "source": {},
            "breakpoints": [{"line": 1}]
        })
        assert result["breakpoints"] == []

    def test_conditional_breakpoint_passes_condition(self):
        server = _make_server()
        server.debugger.add_breakpoint.return_value = MagicMock()

        server._handle_setBreakpoints(1, {
            "source": {"path": "/t.nxl"},
            "breakpoints": [{"line": 7, "condition": "x > 5"}]
        })

        server.debugger.add_breakpoint.assert_called_once_with(
            "/t.nxl", 7, condition="x > 5"
        )


# ---------------------------------------------------------------------------
# Stack Trace Handler
# ---------------------------------------------------------------------------


class TestHandleStackTrace:
    def test_empty_call_stack(self):
        server = _make_server()
        server.debugger.call_stack = []
        result = server._handle_stackTrace(1, {})
        assert result["stackFrames"] == []
        assert result["totalFrames"] == 0

    def test_stack_frames_formatted(self):
        server = _make_server()
        server.debugger.call_stack = [
            CallFrame("main", "app.nxl", 1),
            CallFrame("helper", "app.nxl", 10),
        ]

        result = server._handle_stackTrace(1, {})
        frames = result["stackFrames"]
        assert len(frames) == 2
        # Top frame (innermost) is first in response
        assert frames[0]["name"] == "helper"
        assert frames[1]["name"] == "main"

    def test_frame_has_required_fields(self):
        server = _make_server()
        server.debugger.call_stack = [CallFrame("f", "x.nxl", 3)]
        result = server._handle_stackTrace(1, {})
        frame = result["stackFrames"][0]
        assert "id" in frame
        assert "name" in frame
        assert "source" in frame
        assert "line" in frame
        assert "column" in frame

    def test_no_debugger_returns_empty(self):
        server = DAPServer()
        result = server._handle_stackTrace(1, {})
        assert result == {"stackFrames": [], "totalFrames": 0}


# ---------------------------------------------------------------------------
# Scopes Handler
# ---------------------------------------------------------------------------


class TestHandleScopes:
    def test_returns_local_and_global_scopes(self):
        server = _make_server()
        result = server._handle_scopes(1, {"frameId": 0})
        names = [s["name"] for s in result["scopes"]]
        assert "Locals" in names
        assert "Globals" in names

    def test_scope_has_variables_reference(self):
        server = _make_server()
        result = server._handle_scopes(1, {"frameId": 0})
        for scope in result["scopes"]:
            assert "variablesReference" in scope
            assert scope["variablesReference"] > 0


# ---------------------------------------------------------------------------
# Variables Handler
# ---------------------------------------------------------------------------


class TestHandleVariables:
    def test_unknown_reference_returns_empty(self):
        server = _make_server()
        result = server._handle_variables(1, {"variablesReference": 9999})
        assert result == {"variables": []}

    def test_local_variables_returned(self):
        server = _make_server()
        server.debugger.inspect_all_variables.return_value = {"x": 42, "name": "Alice"}
        ref = server._create_variable_ref("locals", 0)

        result = server._handle_variables(1, {"variablesReference": ref})
        names = {v["name"] for v in result["variables"]}
        assert "x" in names
        assert "name" in names

    def test_variable_entry_has_required_fields(self):
        server = _make_server()
        server.debugger.inspect_all_variables.return_value = {"count": 3}
        ref = server._create_variable_ref("locals", 0)

        result = server._handle_variables(1, {"variablesReference": ref})
        var = result["variables"][0]
        assert "name" in var
        assert "value" in var
        assert "type" in var
        assert "variablesReference" in var


# ---------------------------------------------------------------------------
# Step / Continue Handlers
# ---------------------------------------------------------------------------


class TestStepHandlers:
    def test_handle_continue(self):
        server = _make_server()
        result = server._handle_continue(1, {})
        server.debugger.continue_execution.assert_called_once()
        assert result["allThreadsContinued"] is True

    def test_handle_next_step_over(self):
        server = _make_server()
        server._handle_next(1, {})
        server.debugger.step_over.assert_called_once()

    def test_handle_stepIn(self):
        server = _make_server()
        server._handle_stepIn(1, {})
        server.debugger.step_into.assert_called_once()

    def test_handle_stepOut(self):
        server = _make_server()
        server._handle_stepOut(1, {})
        server.debugger.step_out.assert_called_once()

    def test_handle_pause(self):
        server = _make_server()
        server._handle_pause(1, {})
        server.debugger.step_into.assert_called_once()


class TestConfigurationDone:
    def test_configuration_done_without_launch_returns_error_response(self):
        server = DAPServer()
        responses = []
        server._send_message = lambda m: responses.append(m)

        server.handle_message({
            "type": "request",
            "command": "configurationDone",
            "seq": 1,
            "arguments": {}
        })

        assert len(responses) == 1
        response = responses[0]
        assert response["type"] == "response"
        assert response["success"] is False
        assert "launch request" in response.get("message", "")

    def test_configuration_done_handles_runtime_error_without_debugger(self):
        server = DAPServer()
        server.interpreter = MagicMock()
        server.interpreter.interpret.side_effect = RuntimeError("runtime boom")
        server.ast = object()
        server.debugger = None

        events = []
        server.send_event = lambda e, b=None: events.append((e, b))

        result = server._handle_configurationDone(1, {})

        assert result == {}
        assert any(name == "terminated" for name, _ in events)


# ---------------------------------------------------------------------------
# Evaluate Handler
# ---------------------------------------------------------------------------


class TestHandleEvaluate:
    def test_returns_variable_value(self):
        server = _make_server()
        server.debugger.inspect_variable.return_value = 99
        result = server._handle_evaluate(1, {"expression": "count"})
        assert result["result"] == "99"

    def test_undefined_variable_shows_placeholder(self):
        server = _make_server()
        server.debugger.inspect_variable.return_value = None
        result = server._handle_evaluate(1, {"expression": "missing"})
        assert "undefined" in result["result"].lower() or "missing" in result["result"]

    def test_no_debugger_returns_error(self):
        server = DAPServer()
        result = server._handle_evaluate(1, {"expression": "x"})
        assert "Error" in result["result"]
        assert result["variablesReference"] == 0

    def test_exception_in_inspect_returns_error(self):
        server = _make_server()
        server.debugger.inspect_variable.side_effect = RuntimeError("boom")
        result = server._handle_evaluate(1, {"expression": "x"})
        assert "Error" in result["result"]


# ---------------------------------------------------------------------------
# Disconnect / Terminate Handlers
# ---------------------------------------------------------------------------


class TestDisconnectTerminate:
    def test_disconnect_sets_finished_state(self):
        server = _make_server()
        server._handle_disconnect(1, {})
        assert server.debugger.state == DebuggerState.FINISHED

    def test_terminate_sends_event(self):
        server = _make_server()
        events = []
        server.send_event = lambda e, b=None: events.append(e)
        server._handle_terminate(1, {})
        assert "terminated" in events
        assert server.debugger.state == DebuggerState.FINISHED


# ---------------------------------------------------------------------------
# DAP Callbacks (on_breakpoint, on_step, on_exception)
# ---------------------------------------------------------------------------


class TestDAPCallbacks:
    def test_on_breakpoint_sends_stopped_event(self):
        server = _make_server()
        events = []
        server.send_event = lambda e, b=None: events.append((e, b))

        bp = Breakpoint(file="t.nxl", line=5)
        server._on_breakpoint(bp, None)

        event_names = [e for e, _ in events]
        assert "stopped" in event_names
        bodies = [b for _, b in events if b]
        reasons = [b.get("reason") for b in bodies]
        assert "breakpoint" in reasons

    def test_on_step_sends_stopped_event_when_stepping(self):
        server = _make_server()
        server.debugger.state = DebuggerState.STEPPING
        events = []
        server.send_event = lambda e, b=None: events.append((e, b))

        server._on_step("t.nxl", 3)

        assert any(e == "stopped" for e, _ in events)

    def test_on_exception_sends_stopped_event(self):
        server = _make_server()
        events = []
        server.send_event = lambda e, b=None: events.append((e, b))

        server._on_exception(RuntimeError("crash"), None)

        event_names = [e for e, _ in events]
        assert "stopped" in event_names
        bodies = [b for _, b in events if b]
        reasons = [b.get("reason") for b in bodies]
        assert "exception" in reasons


# ---------------------------------------------------------------------------
# Variable Reference Counter
# ---------------------------------------------------------------------------


class TestVariableRefCounter:
    def test_refs_are_unique(self):
        server = _make_server()
        r1 = server._create_variable_ref("locals", 0)
        r2 = server._create_variable_ref("globals", 0)
        r3 = server._create_variable_ref("locals", 1)
        assert len({r1, r2, r3}) == 3

    def test_ref_stores_scope_info(self):
        server = _make_server()
        ref = server._create_variable_ref("locals", 2)
        info = server.variable_refs[ref]
        assert info["type"] == "locals"
        assert info["frameId"] == 2
