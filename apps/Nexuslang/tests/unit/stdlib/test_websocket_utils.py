import asyncio
import importlib
import os
import sys
import warnings

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.stdlib import websocket_utils


class _DummyLoop:
    def __init__(self, *, result=None, side_effect=None):
        self.result = result
        self.side_effect = side_effect
        self.awaitables = []

    def run_until_complete(self, awaitable):
        self.awaitables.append(awaitable)
        try:
            if self.side_effect is not None:
                raise self.side_effect
            return self.result
        finally:
            awaitable.close()


class _DummyRuntime:
    def __init__(self):
        self.functions = {}

    def register_function(self, name, func):
        self.functions[name] = func


def setup_function():
    websocket_utils._ws_connections.clear()
    websocket_utils._ws_servers.clear()


def test_ws_send_returns_false_for_recoverable_websocket_failures(caplog):
    loop = _DummyLoop(side_effect=RuntimeError("socket closed"))
    websocket_utils._ws_connections[1] = {"ws": object(), "loop": loop}

    with caplog.at_level("WARNING"):
        result = websocket_utils.ws_send(1, "hello")

    assert result is False
    assert "WebSocket send failed for connection 1" in caplog.text


def test_ws_send_propagates_connection_state_corruption():
    websocket_utils._ws_connections[1] = {"ws": object()}

    with pytest.raises(KeyError, match="loop"):
        websocket_utils.ws_send(1, "hello")


def test_ws_close_returns_false_for_recoverable_close_failures(caplog):
    loop = _DummyLoop(side_effect=RuntimeError("close failed"))
    websocket_utils._ws_connections[2] = {"ws": object(), "loop": loop}

    with caplog.at_level("WARNING"):
        result = websocket_utils.ws_close(2)

    assert result is False
    assert 2 in websocket_utils._ws_connections
    assert "WebSocket close failed for connection 2" in caplog.text


def test_ws_is_open_uses_modern_state_model():
    state = websocket_utils.State.OPEN if websocket_utils.State is not None else "OPEN"
    websocket = type("Socket", (), {"state": state})()
    websocket_utils._ws_connections[3] = {"ws": websocket, "loop": _DummyLoop()}

    assert websocket_utils.ws_is_open(3) is True


@pytest.mark.skipif(not websocket_utils.HAS_WEBSOCKETS, reason="websockets not installed")
def test_ws_start_server_uses_modern_handler_signature(monkeypatch):
    observed = {}

    async def fake_serve(handler, host, port):
        observed["host"] = host
        observed["port"] = port
        observed["argcount"] = handler.__code__.co_argcount

        class _FakeSocket:
            def __init__(self):
                self.sent = []

            async def __aiter__(self):
                yield "ping"

            async def send(self, message):
                self.sent.append(message)
                observed["response"] = message

        await handler(_FakeSocket())
        return "server"

    monkeypatch.setattr(websocket_utils.websockets, "serve", fake_serve)

    result = asyncio.run(
        websocket_utils._ws_start_server("127.0.0.1", 9000, lambda message: f"echo:{message}")
    )

    assert result == "server"
    assert observed == {
        "host": "127.0.0.1",
        "port": 9000,
        "argcount": 1,
        "response": "echo:ping",
    }


def test_register_websocket_functions_registers_full_fallback_surface(monkeypatch):
    runtime = _DummyRuntime()
    monkeypatch.setattr(websocket_utils, "HAS_WEBSOCKETS", False)

    websocket_utils.register_websocket_functions(runtime)

    expected = {
        "ws_connect",
        "ws_send",
        "ws_send_json",
        "ws_receive",
        "ws_receive_json",
        "ws_close",
        "ws_is_open",
        "ws_start_server",
        "ws_stop_server",
        "ws_server_info",
    }
    assert expected == set(runtime.functions)
    for func in runtime.functions.values():
        with pytest.raises(ImportError, match="websockets is not installed"):
            func()


def test_import_uses_non_deprecated_websockets_entry_points():
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always", DeprecationWarning)
        reloaded = importlib.reload(websocket_utils)

    assert reloaded.HAS_WEBSOCKETS in {True, False}
    assert not [warning for warning in caught if issubclass(warning.category, DeprecationWarning)]