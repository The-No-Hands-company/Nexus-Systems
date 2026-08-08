"""Hardening tests for stdlib subprocess utilities."""

import subprocess
import pytest

from nexuslang.stdlib.subprocess_utils import run_command, run_command_list


def test_run_command_timeout_is_recoverable():
    result = run_command("python -c \"import time; time.sleep(0.2)\"", timeout=0.01)
    assert result.returncode == -1
    assert "timed out" in result.stderr


def test_run_command_internal_runtime_error_propagates(monkeypatch):
    def broken_run(*args, **kwargs):
        raise RuntimeError("internal subprocess invariant failure")

    monkeypatch.setattr(subprocess, "run", broken_run)

    with pytest.raises(RuntimeError, match="internal subprocess invariant failure"):
        run_command("echo hello")


def test_run_command_list_internal_runtime_error_propagates(monkeypatch):
    def broken_run(*args, **kwargs):
        raise RuntimeError("internal subprocess invariant failure")

    monkeypatch.setattr(subprocess, "run", broken_run)

    with pytest.raises(RuntimeError, match="internal subprocess invariant failure"):
        run_command_list(["echo", "hello"])
