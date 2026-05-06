"""Tests for stdlib registration wiring and optional-module resilience."""

import logging

from nexuslang.runtime.runtime import Runtime
from nexuslang import stdlib as stdlib_module


def test_stdlib_registrar_list_has_no_duplicates():
    registrars = stdlib_module._STDLIB_REGISTRARS
    assert len(registrars) == len(set(registrars))


def test_stdlib_module_aliases_have_no_exact_duplicates():
    module_aliases = stdlib_module._STDLIB_MODULES
    assert len(module_aliases) == len(set(module_aliases))


def test_optional_graphics_failure_is_logged_and_non_fatal(monkeypatch, caplog):
    def _boom(_runtime):
        raise RuntimeError("graphics backend unavailable")

    monkeypatch.setattr(stdlib_module, "register_graphics_functions", _boom)

    runtime = Runtime()
    with caplog.at_level(logging.WARNING):
        stdlib_module._register_optional_graphics(runtime)

    assert "Skipping optional graphics registration" in caplog.text
    assert "graphics backend unavailable" in caplog.text
