"""Regression tests for the legacy LLVM backend compatibility module."""

import os
import sys
from pathlib import Path

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

from nexuslang.compiler import CompilationTarget, Compiler
from nexuslang.compiler.backends.llvm_generator import LLVMCodeGenerator, LLVMTypeMapper
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.parser.ast import FunctionDefinition, Literal, Program, ReturnStatement


def test_legacy_llvm_codegen_import_path_delegates_to_production_backend():
    ast = Program([
        FunctionDefinition(
            name="main",
            parameters=[],
            body=[ReturnStatement(Literal("integer", 7))],
            return_type="Integer",
        )
    ])

    legacy_ir = LLVMCodeGenerator().generate(ast)
    production_ir = LLVMIRGenerator().generate(ast)

    assert legacy_ir == production_ir
    assert "@main" in legacy_ir


def test_legacy_llvm_type_mapper_uses_production_type_lowering():
    mapper = LLVMTypeMapper()

    assert mapper.map_type(None) == "i64"
    assert mapper.map_type("Integer") == "i64"
    assert mapper.map_type("Float") == "double"
    assert mapper.map_type("String") == "i8*"


def test_legacy_llvm_type_mapper_fails_fast_for_unknown_named_types():
    mapper = LLVMTypeMapper()

    with pytest.raises(RuntimeError, match="Unable to map NexusLang type"):
        mapper.map_type("CompletelyUnknownType")


def test_compiler_registry_uses_production_llvm_backend_for_llvm_target():
    compiler = Compiler()

    assert compiler.generators[CompilationTarget.LLVM_IR] is LLVMIRGenerator


def test_compiler_compile_normalizes_cpp_alias_before_generator_dispatch(tmp_path):
    compiler = Compiler()
    captured = {}

    class FakeCppGenerator:
        def __init__(self, target):
            captured["target"] = target
            self.required_libraries = set()

        def generate(self, ast):
            captured["ast"] = ast
            return "// generated cpp"

    compiler.generators[CompilationTarget.CPP] = FakeCppGenerator

    output_file = tmp_path / "output.cpp"
    success, libraries = compiler.compile(Program([]), "c++", str(output_file))

    assert success is True
    assert libraries == set()
    assert captured["target"] == CompilationTarget.CPP
    assert output_file.read_text() == "// generated cpp"


@pytest.mark.parametrize(
    ("alias", "expected"),
    [("c++", CompilationTarget.CPP), ("cxx", CompilationTarget.CPP), ("llvm", CompilationTarget.LLVM_IR), ("llvm-ir", CompilationTarget.LLVM_IR)],
)
def test_compiler_normalize_target_canonicalizes_supported_aliases(alias, expected):
    assert Compiler.normalize_target(alias) == expected


def test_compile_and_link_normalizes_cpp_alias_for_link_step(monkeypatch, tmp_path):
    compiler = Compiler()
    captured = {}

    def fake_compile(ast, target, output_file):
        captured["compile_target"] = target
        captured["intermediate_file"] = output_file
        Path(output_file).write_text("// generated")
        return True, {"m"}

    def fake_link(source_file, output_file, target, libraries=None):
        captured["link_source"] = source_file
        captured["link_output"] = output_file
        captured["link_target"] = target
        captured["libraries"] = libraries
        return True

    monkeypatch.setattr(compiler, "compile", fake_compile)
    monkeypatch.setattr(compiler, "_link_with_system_compiler", fake_link)

    output_file = tmp_path / "app"
    result = compiler.compile_and_link(Program([]), "c++", str(output_file))

    assert result is True
    assert captured["compile_target"] == CompilationTarget.CPP
    assert captured["link_target"] == CompilationTarget.CPP
    assert captured["intermediate_file"].endswith("app.generated.cpp")
    assert captured["link_source"] == captured["intermediate_file"]
    assert captured["link_output"] == str(output_file)
    assert captured["libraries"] == {"m"}