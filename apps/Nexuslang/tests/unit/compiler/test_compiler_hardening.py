import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler import CompilationTarget, Compiler
from nexuslang.compiler.header_parser import CHeaderParser
from nexuslang.compiler.linker import invoke_linker
from nexuslang.parser.ast import Program


def test_compiler_compile_propagates_generator_runtime_errors(tmp_path):
    compiler = Compiler()

    class BrokenGenerator:
        def __init__(self, _target):
            self.required_libraries = set()

        def generate(self, _ast):
            raise RuntimeError("generator bug")

    compiler.generators[CompilationTarget.C] = BrokenGenerator

    with pytest.raises(RuntimeError, match="generator bug"):
        compiler.compile(Program([]), CompilationTarget.C, str(tmp_path / "out.c"))


def test_compiler_compile_returns_failure_on_output_write_oserror(monkeypatch, tmp_path):
    compiler = Compiler()

    class StableGenerator:
        def __init__(self, _target):
            self.required_libraries = {"m"}

        def generate(self, _ast):
            return "int main(void) { return 0; }"

    compiler.generators[CompilationTarget.C] = StableGenerator

    def failing_open(*args, **kwargs):
        raise OSError("disk full")

    monkeypatch.setattr("builtins.open", failing_open)

    success, libraries = compiler.compile(Program([]), CompilationTarget.C, str(tmp_path / "out.c"))

    assert success is False
    assert libraries == set()


def test_compiler_link_with_system_compiler_propagates_runtime_errors(monkeypatch, tmp_path):
    compiler = Compiler()

    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/gcc" if name in {"gcc", "clang"} else None)

    def broken_run(*args, **kwargs):
        raise RuntimeError("unexpected subprocess bug")

    monkeypatch.setattr("subprocess.run", broken_run)

    with pytest.raises(RuntimeError, match="unexpected subprocess bug"):
        compiler._link_with_system_compiler(str(tmp_path / "in.c"), str(tmp_path / "out"), CompilationTarget.C)


def test_compiler_assemble_and_link_propagates_runtime_errors(monkeypatch, tmp_path):
    compiler = Compiler()

    monkeypatch.setattr("shutil.which", lambda name: "/usr/bin/nasm" if name == "nasm" else None)

    def broken_run(*args, **kwargs):
        raise RuntimeError("assembler bug")

    monkeypatch.setattr("subprocess.run", broken_run)

    with pytest.raises(RuntimeError, match="assembler bug"):
        compiler._assemble_and_link(str(tmp_path / "in.asm"), str(tmp_path / "out"))


def test_header_parser_returns_false_on_read_oserror(monkeypatch):
    parser = CHeaderParser()

    monkeypatch.setattr("os.path.exists", lambda _path: True)

    def failing_open(*args, **kwargs):
        raise OSError("read failed")

    monkeypatch.setattr("builtins.open", failing_open)

    assert parser.parse_header("test.h") is False


def test_header_parser_propagates_runtime_errors_from_extractors(tmp_path, monkeypatch):
    parser = CHeaderParser()
    header = tmp_path / "sample.h"
    header.write_text("int add(int a, int b);", encoding="utf-8")

    def broken_extract(*args, **kwargs):
        raise RuntimeError("extractor bug")

    monkeypatch.setattr(parser, "_extract_functions", broken_extract)

    with pytest.raises(RuntimeError, match="extractor bug"):
        parser.parse_header(str(header))


def test_invoke_linker_returns_failure_on_oserror(monkeypatch, tmp_path):
    def failing_run(*args, **kwargs):
        raise OSError("spawn failed")

    monkeypatch.setattr("subprocess.run", failing_run)

    result = invoke_linker([str(tmp_path / "input.o")], str(tmp_path / "a.out"), linker_binary="ld")

    assert result.success is False
    assert result.returncode == 1
    assert "spawn failed" in result.stderr


def test_invoke_linker_propagates_runtime_errors(monkeypatch, tmp_path):
    def broken_run(*args, **kwargs):
        raise RuntimeError("linker orchestration bug")

    monkeypatch.setattr("subprocess.run", broken_run)

    with pytest.raises(RuntimeError, match="linker orchestration bug"):
        invoke_linker([str(tmp_path / "input.o")], str(tmp_path / "a.out"), linker_binary="ld")