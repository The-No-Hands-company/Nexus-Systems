"""Integration tests for CLI lint flag precedence over manifest defaults."""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src"))

import nexuslang.cli.__init__ as cli


def _write_manifest(path: Path, lint_on_build: bool) -> None:
    path.write_text(
        (
            '[package]\n'
            'name = "lint-precedence"\n'
            'version = "0.1.0"\n\n'
            '[build]\n'
            'source_dir = "src"\n'
            'output_dir = "build"\n'
            f'lint_on_build = {str(lint_on_build).lower()}\n'
        ),
        encoding="utf-8",
    )


def _write_source(root: Path) -> None:
    src = root / "src"
    src.mkdir(parents=True, exist_ok=True)
    (src / "main.nxl").write_text("set x to 1\n", encoding="utf-8")


def _run_cli_build(monkeypatch, argv, captured):
    class FakeBuildSystem:
        def __init__(self, config):
            self.config = config

        def build(self, **kwargs):
            captured.update(kwargs)
            return True

    monkeypatch.setattr(cli, "BuildSystem", FakeBuildSystem)
    monkeypatch.setattr(sys, "argv", argv)

    with pytest.raises(SystemExit) as excinfo:
        cli.main()

    assert excinfo.value.code == 0


def test_cli_lint_flag_overrides_manifest_default_false(monkeypatch, tmp_path):
    _write_manifest(tmp_path / "nexuslang.toml", lint_on_build=False)
    _write_source(tmp_path)
    monkeypatch.chdir(tmp_path)

    captured = {}
    _run_cli_build(monkeypatch, ["nexuslang", "build", "--lint"], captured)

    assert captured.get("lint") is True


def test_cli_no_lint_flag_overrides_manifest_default_true(monkeypatch, tmp_path):
    _write_manifest(tmp_path / "nexuslang.toml", lint_on_build=True)
    _write_source(tmp_path)
    monkeypatch.chdir(tmp_path)

    captured = {}
    _run_cli_build(monkeypatch, ["nexuslang", "build", "--no-lint"], captured)

    assert captured.get("lint") is False
