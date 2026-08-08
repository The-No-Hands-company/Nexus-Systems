"""Focused coverage tests for nexuslang.lsp.__main__ entrypoint behavior."""

import importlib

import pytest

lsp_main = importlib.import_module("nexuslang.lsp.__main__")


class DummyServer:
    def __init__(self, behavior=None):
        self.behavior = behavior
        self.started = False

    def start(self):
        self.started = True
        if self.behavior is not None:
            raise self.behavior


def test_main_stdio_mode_starts_server(monkeypatch):
    server = DummyServer()

    monkeypatch.setattr(lsp_main, "NexusLangLanguageServer", lambda: server)
    monkeypatch.setattr(lsp_main.sys, "argv", ["prog"])

    lsp_main.main()

    assert server.started is True


def test_main_tcp_mode_exits_with_error(monkeypatch, capsys):
    monkeypatch.setattr(lsp_main, "NexusLangLanguageServer", lambda: DummyServer())
    monkeypatch.setattr(lsp_main.sys, "argv", ["prog", "--tcp"])

    with pytest.raises(SystemExit) as exc:
        lsp_main.main()

    assert exc.value.code == 1
    assert "not yet implemented" in capsys.readouterr().err


def test_main_keyboard_interrupt_exits_zero(monkeypatch):
    server = DummyServer(behavior=KeyboardInterrupt())

    monkeypatch.setattr(lsp_main, "NexusLangLanguageServer", lambda: server)
    monkeypatch.setattr(lsp_main.sys, "argv", ["prog", "--debug"])

    with pytest.raises(SystemExit) as exc:
        lsp_main.main()

    assert exc.value.code == 0


def test_main_recoverable_exception_exits_one_and_prints_error(monkeypatch, capsys):
    server = DummyServer(behavior=RuntimeError("boom"))

    monkeypatch.setattr(lsp_main, "NexusLangLanguageServer", lambda: server)
    monkeypatch.setattr(lsp_main.sys, "argv", ["prog"])

    with pytest.raises(SystemExit) as exc:
        lsp_main.main()

    assert exc.value.code == 1
    assert "Error: boom" in capsys.readouterr().err
