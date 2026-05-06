"""Tests for nlpllint CLI configuration support."""

import json
import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src"))

from nexuslang.cli import nlpllint


def _write_nxl_file(path: Path) -> None:
    path.write_text(
        """
# TODO: style marker
set value to 42
print number value
""".lstrip(),
        encoding="utf-8",
    )


def test_nlpllint_config_enables_strict_mode_with_style_issue(tmp_path, monkeypatch, capsys):
    source_file = tmp_path / "sample.nxl"
    config_file = tmp_path / "nlpllint.toml"
    _write_nxl_file(source_file)

    config_file.write_text(
        """
mode = "strict"
json = true
""".lstrip(),
        encoding="utf-8",
    )

    monkeypatch.setattr(
        sys,
        "argv",
        ["nlpllint", str(source_file), "--config", str(config_file)],
    )

    exit_code = nlpllint.main()
    assert exit_code is None

    out = capsys.readouterr().out
    payload = json.loads(out)
    issues = payload["files"][0]["issues"]
    assert any(issue["code"] == "S018" for issue in issues)


def test_nlpllint_config_can_disable_security_checker(tmp_path, monkeypatch, capsys):
    source_file = tmp_path / "security_check.nxl"
    source_file.write_text(
        """
set cmd to "echo hello"
shell execute cmd
""".lstrip(),
        encoding="utf-8",
    )

    config_file = tmp_path / "nlpllint.json"
    config_file.write_text(
        json.dumps(
            {
                "json": True,
                "analyzer": {
                    "security": False,
                },
            }
        ),
        encoding="utf-8",
    )

    monkeypatch.setattr(
        sys,
        "argv",
        ["nlpllint", str(source_file), "--config", str(config_file)],
    )

    exit_code = nlpllint.main()
    assert exit_code is None

    out = capsys.readouterr().out
    payload = json.loads(out)
    issues = payload["files"][0]["issues"]
    assert not any(issue["category"] == "security" for issue in issues)


def test_nlpllint_invalid_config_path_returns_error(tmp_path, monkeypatch, capsys):
    source_file = tmp_path / "sample.nxl"
    _write_nxl_file(source_file)

    missing_config = tmp_path / "missing.toml"

    monkeypatch.setattr(
        sys,
        "argv",
        ["nlpllint", str(source_file), "--config", str(missing_config)],
    )

    exit_code = nlpllint.main()
    assert exit_code == 1

    err = capsys.readouterr().err
    assert "Error loading config" in err


def test_nlpllint_rejects_unknown_root_config_key(tmp_path, monkeypatch, capsys):
    source_file = tmp_path / "sample.nxl"
    _write_nxl_file(source_file)

    config_file = tmp_path / "nlpllint.toml"
    config_file.write_text(
        """
unknown_option = true
""".lstrip(),
        encoding="utf-8",
    )

    monkeypatch.setattr(
        sys,
        "argv",
        ["nlpllint", str(source_file), "--config", str(config_file)],
    )

    exit_code = nlpllint.main()
    assert exit_code == 1

    err = capsys.readouterr().err
    assert "Unknown config key(s)" in err


def test_nlpllint_rejects_invalid_mode_value(tmp_path, monkeypatch, capsys):
    source_file = tmp_path / "sample.nxl"
    _write_nxl_file(source_file)

    config_file = tmp_path / "nlpllint.toml"
    config_file.write_text(
        """
mode = "aggressive"
""".lstrip(),
        encoding="utf-8",
    )

    monkeypatch.setattr(
        sys,
        "argv",
        ["nlpllint", str(source_file), "--config", str(config_file)],
    )

    exit_code = nlpllint.main()
    assert exit_code == 1

    err = capsys.readouterr().err
    assert "mode' must be one of" in err


def test_nlpllint_rejects_invalid_analyzer_value_type(tmp_path, monkeypatch, capsys):
    source_file = tmp_path / "sample.nxl"
    _write_nxl_file(source_file)

    config_file = tmp_path / "nlpllint.json"
    config_file.write_text(
        json.dumps(
            {
                "analyzer": {
                    "security": "yes",
                }
            }
        ),
        encoding="utf-8",
    )

    monkeypatch.setattr(
        sys,
        "argv",
        ["nlpllint", str(source_file), "--config", str(config_file)],
    )

    exit_code = nlpllint.main()
    assert exit_code == 1

    err = capsys.readouterr().err
    assert "Analyzer config key 'security' must be a boolean" in err
