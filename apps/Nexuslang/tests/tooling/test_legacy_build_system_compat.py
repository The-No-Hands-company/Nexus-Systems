"""Regression tests for the legacy build_system compatibility builder."""

from pathlib import Path

import pytest

from nexuslang.build_system.builder import BuildConfig, Builder
from nexuslang.build_system.project import Project
from nexuslang.compiler import CompilationTarget
from nexuslang.parser.ast import Program


def _make_legacy_project(tmp_path: Path) -> Project:
    return Project.init(tmp_path, "legacy-app")


def test_legacy_builder_executable_targets_delegate_to_compiler_facade(monkeypatch, tmp_path):
    project = _make_legacy_project(tmp_path)
    builder = Builder(project, BuildConfig(incremental=False))
    captured = {}

    def fake_parse(source_path):
        captured["parsed"] = source_path
        return Program([])

    class FakeCompiler:
        def compile_and_link(self, ast, target, output_file):
            captured["ast"] = ast
            captured["target"] = target
            captured["output"] = output_file
            Path(output_file).write_text("binary")
            return True

    monkeypatch.setattr(builder, "_parse_source", fake_parse)
    monkeypatch.setattr(builder, "_create_compiler", lambda **kwargs: FakeCompiler())

    assert builder.build_target("main") is True
    assert captured["parsed"] == project.root_path / "src/main.nxl"
    assert captured["target"] == CompilationTarget.C
    assert captured["output"] == str(project.get_output_dir() / "main")


def test_legacy_builder_module_targets_delegate_to_maintained_llvm_ir_path(monkeypatch, tmp_path):
    project = _make_legacy_project(tmp_path)
    project.config.targets["main"].target_type = "module"
    builder = Builder(project, BuildConfig(incremental=False))
    captured = {}

    class FakeCompiler:
        def compile(self, ast, target, output_file):
            captured["ast"] = ast
            captured["target"] = target
            captured["output"] = output_file
            Path(output_file).write_text("; module ir")
            return True, set()

    monkeypatch.setattr(builder, "_parse_source", lambda source_path: Program([]))
    monkeypatch.setattr(builder, "_create_compiler", lambda **kwargs: FakeCompiler())

    assert builder.build_target("main") is True
    assert captured["target"] == CompilationTarget.LLVM_IR
    assert captured["output"] == str(project.get_output_dir() / "main.ll")


def test_legacy_builder_library_targets_fail_fast_with_explicit_error(monkeypatch, tmp_path):
    project = _make_legacy_project(tmp_path)
    project.config.targets["main"].target_type = "library"
    builder = Builder(project, BuildConfig(incremental=False))

    monkeypatch.setattr(builder, "_parse_source", lambda source_path: Program([]))

    with pytest.raises(NotImplementedError, match="library builds"):
        builder.build_target("main")