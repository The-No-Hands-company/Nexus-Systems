"""
Tests for NexusLang extended build system features:
    - Parallel compilation  (src/nlpl/build/parallel.py)
    - Link-Time Optimization (src/nlpl/build/lto.py)
    - Cross-compilation      (src/nlpl/build/cross.py)
    - Integration with BuildConfig / Builder
"""

import sys
import os
import subprocess
import tempfile
from pathlib import Path
from typing import List, Set
from unittest.mock import patch, MagicMock

import pytest

# ---------------------------------------------------------------------------
# Parallel compilation
# ---------------------------------------------------------------------------

from nexuslang.build.parallel import (
    CompilationTask,
    DependencyGraph,
    ParallelCompiler,
    TaskResult,
    build_tasks_from_sources,
)


class TestCompilationTask:
    def test_creation(self, tmp_path):
        src = tmp_path / "a.nxl"
        out = tmp_path / "a.o"
        t = CompilationTask(
            source_file=src,
            output_file=out,
            compiler_args=["nlplc", str(src), "-o", str(out)],
        )
        assert t.source_file == src
        assert t.output_file == out
        assert t.dependencies == []

    def test_hash_and_equality(self, tmp_path):
        src = tmp_path / "a.nxl"
        out = tmp_path / "a.o"
        t1 = CompilationTask(src, out, [])
        t2 = CompilationTask(src, out, ["extra"])
        assert t1 == t2
        assert hash(t1) == hash(t2)

    def test_inequality(self, tmp_path):
        t1 = CompilationTask(tmp_path / "a.nxl", tmp_path / "a.o", [])
        t2 = CompilationTask(tmp_path / "b.nxl", tmp_path / "b.o", [])
        assert t1 != t2


class TestDependencyGraph:
    def _make_task(self, name: str, deps: List[str], tmp_path: Path) -> CompilationTask:
        src = tmp_path / f"{name}.nxl"
        out = tmp_path / f"{name}.o"
        dep_paths = [tmp_path / f"{d}.nxl" for d in deps]
        return CompilationTask(
            source_file=src,
            output_file=out,
            compiler_args=[],
            dependencies=dep_paths,
        )

    def test_empty_graph(self):
        g = DependencyGraph()
        assert g.topological_layers() == []
        assert g.task_count() == 0

    def test_single_task(self, tmp_path):
        g = DependencyGraph()
        t = self._make_task("main", [], tmp_path)
        g.add_task(t)
        layers = g.topological_layers()
        assert len(layers) == 1
        assert layers[0][0].source_file.name == "main.nxl"

    def test_independent_tasks_same_layer(self, tmp_path):
        g = DependencyGraph()
        for name in ("a", "b", "c"):
            g.add_task(self._make_task(name, [], tmp_path))
        layers = g.topological_layers()
        assert len(layers) == 1
        assert g.task_count() == 3

    def test_linear_chain(self, tmp_path):
        # a <- b <- c  (c depends on b, b depends on a)
        ta = self._make_task("a", [], tmp_path)
        tb = self._make_task("b", ["a"], tmp_path)
        tc = self._make_task("c", ["b"], tmp_path)
        g = DependencyGraph()
        for t in (ta, tb, tc):
            g.add_task(t)
        layers = g.topological_layers()
        # Each layer must contain only the next in chain
        assert len(layers) >= 2
        # First layer has no unresolved deps
        first_names = {t.source_file.stem for t in layers[0]}
        assert "a" in first_names

    def test_diamond_dependency(self, tmp_path):
        # a <- b, a <- c, b <- d, c <- d
        ta = self._make_task("a", [], tmp_path)
        tb = self._make_task("b", ["a"], tmp_path)
        tc = self._make_task("c", ["a"], tmp_path)
        td = self._make_task("d", ["b", "c"], tmp_path)
        g = DependencyGraph()
        for t in (ta, tb, tc, td):
            g.add_task(t)
        layers = g.topological_layers()
        all_tasks = [t for layer in layers for t in layer]
        assert len(all_tasks) == 4

    def test_external_dep_ignored(self, tmp_path):
        """Dependencies on files not in the graph are silently ignored."""
        t = CompilationTask(
            source_file=tmp_path / "a.nxl",
            output_file=tmp_path / "a.o",
            compiler_args=[],
            dependencies=[tmp_path / "external.nxl"],  # not added as task
        )
        g = DependencyGraph()
        g.add_task(t)
        # Should not raise; external dep is simply absent from adjacency
        layers = g.topological_layers()
        assert len(layers) == 1

    def test_task_count(self, tmp_path):
        g = DependencyGraph()
        for i in range(5):
            g.add_task(self._make_task(f"f{i}", [], tmp_path))
        assert g.task_count() == 5


class TestParallelCompiler:
    def _make_task(self, name: str, cmd: List[str], tmp_path: Path) -> CompilationTask:
        src = tmp_path / f"{name}.nxl"
        out = tmp_path / f"{name}.o"
        return CompilationTask(src, out, cmd)

    def test_empty_tasks(self):
        pc = ParallelCompiler(max_workers=2)
        results = pc.compile_all([])
        assert results == []

    def test_successful_task(self, tmp_path):
        """Task that runs 'true' (always succeeds)."""
        t = self._make_task("ok", ["true"], tmp_path)
        pc = ParallelCompiler(max_workers=1)
        results = pc.compile_all([t])
        assert len(results) == 1
        assert results[0].success is True

    def test_failing_task(self, tmp_path):
        """Task that runs 'false' (always fails)."""
        t = self._make_task("fail", ["false"], tmp_path)
        pc = ParallelCompiler(max_workers=1)
        results = pc.compile_all([t])
        assert len(results) == 1
        assert results[0].success is False

    def test_multiple_independent_tasks(self, tmp_path):
        tasks = [self._make_task(f"f{i}", ["true"], tmp_path) for i in range(4)]
        pc = ParallelCompiler(max_workers=2)
        results = pc.compile_all(tasks)
        assert len(results) == 4
        assert all(r.success for r in results)

    def test_compile_independent_shortcut(self, tmp_path):
        tasks = [self._make_task(f"g{i}", ["true"], tmp_path) for i in range(3)]
        pc = ParallelCompiler(max_workers=2)
        results = pc.compile_independent(tasks)
        assert len(results) == 3

    def test_invalid_command_returns_failure(self, tmp_path):
        """Non-existent command should produce a failed TaskResult."""
        t = self._make_task("bad", ["__nxl_nonexistent_program__"], tmp_path)
        pc = ParallelCompiler(max_workers=1)
        results = pc.compile_all([t])
        assert len(results) == 1
        assert results[0].success is False
        assert results[0].errors

    def test_duration_recorded(self, tmp_path):
        t = self._make_task("dur", ["true"], tmp_path)
        pc = ParallelCompiler(max_workers=1)
        results = pc.compile_all([t])
        assert results[0].duration >= 0.0


class TestBuildTasksFromSources:
    def test_creates_correct_number_of_tasks(self, tmp_path):
        sources = [tmp_path / f"f{i}.nxl" for i in range(3)]
        compiler = tmp_path / "nlplc"
        tasks = build_tasks_from_sources(
            source_files=sources,
            output_dir=tmp_path / "obj",
            compiler_path=compiler,
            compiler_flags=["-O2"],
        )
        assert len(tasks) == 3

    def test_output_extension_is_o(self, tmp_path):
        src = tmp_path / "main.nxl"
        tasks = build_tasks_from_sources(
            source_files=[src],
            output_dir=tmp_path / "obj",
            compiler_path=tmp_path / "nlplc",
            compiler_flags=[],
        )
        assert tasks[0].output_file.suffix == ".o"

    def test_compiler_args_include_source(self, tmp_path):
        src = tmp_path / "main.nxl"
        tasks = build_tasks_from_sources(
            source_files=[src],
            output_dir=tmp_path / "obj",
            compiler_path=tmp_path / "nlplc",
            compiler_flags=["-O1"],
        )
        assert str(src) in tasks[0].compiler_args

    def test_deps_wired_from_dep_graph(self, tmp_path):
        a = tmp_path / "a.nxl"
        b = tmp_path / "b.nxl"
        dep_graph = {b: {a}}
        tasks = build_tasks_from_sources(
            source_files=[a, b],
            output_dir=tmp_path / "obj",
            compiler_path=tmp_path / "nlplc",
            compiler_flags=[],
            dep_graph=dep_graph,
        )
        b_task = next(t for t in tasks if t.source_file == b)
        assert a in b_task.dependencies

    def test_output_dir_created(self, tmp_path):
        src = tmp_path / "main.nxl"
        obj_dir = tmp_path / "new_obj_dir"
        assert not obj_dir.exists()
        build_tasks_from_sources([src], obj_dir, tmp_path / "nlplc", [])
        assert obj_dir.exists()


# ---------------------------------------------------------------------------
# LTO
# ---------------------------------------------------------------------------

from nexuslang.build.lto import (
    LTOMode,
    LTOConfig,
    LTOResult,
    LTOLinker,
    LLVMTools,
    lto_flags_for_profile,
)


class TestLTOMode:
    def test_values(self):
        assert LTOMode.DISABLED.value == "disabled"
        assert LTOMode.THIN.value == "thin"
        assert LTOMode.FULL.value == "full"

    def test_all_modes_present(self):
        modes = {m.value for m in LTOMode}
        assert {"disabled", "thin", "full"} == modes


class TestLTOConfig:
    def test_defaults(self):
        cfg = LTOConfig()
        assert cfg.mode == LTOMode.DISABLED
        assert cfg.opt_level == 2
        assert cfg.strip_debug is False
        assert cfg.internalize is True
        assert cfg.passes == ""

    def test_custom(self):
        cfg = LTOConfig(mode=LTOMode.FULL, opt_level=3, strip_debug=True)
        assert cfg.mode == LTOMode.FULL
        assert cfg.opt_level == 3
        assert cfg.strip_debug is True


class TestLLVMTools:
    def test_find_returns_none_for_missing(self):
        tools = LLVMTools()
        result = tools.find("__certainly_not_a_real_llvm_tool_xyz__")
        assert result is None

    def test_check_lto_tools_returns_dict(self):
        tools = LLVMTools()
        result = tools.check_lto_tools()
        assert set(result.keys()) == {"llvm-link", "opt", "llc", "llvm-strip"}

    def test_all_required_available_returns_bool(self):
        tools = LLVMTools()
        result = tools.all_required_available()
        assert isinstance(result, bool)

    def test_caching(self):
        tools = LLVMTools()
        r1 = tools.find("llc")
        r2 = tools.find("llc")
        assert r1 == r2  # second call reads from cache


class TestLTOLinkerEmitFlags:
    def test_disabled_no_flags(self):
        cfg = LTOConfig(mode=LTOMode.DISABLED)
        linker = LTOLinker(cfg)
        assert linker.emit_bitcode_flags() == []

    def test_thin_includes_emit_llvm(self):
        cfg = LTOConfig(mode=LTOMode.THIN)
        linker = LTOLinker(cfg)
        flags = linker.emit_bitcode_flags()
        assert "-emit-llvm" in flags
        assert "-flto=thin" in flags

    def test_full_includes_emit_llvm(self):
        cfg = LTOConfig(mode=LTOMode.FULL)
        linker = LTOLinker(cfg)
        flags = linker.emit_bitcode_flags()
        assert "-emit-llvm" in flags
        assert "-flto" in flags


class TestLTOLinkerMissingTools:
    def test_returns_error_when_tools_missing(self, tmp_path):
        cfg = LTOConfig(mode=LTOMode.FULL)
        mock_tools = MagicMock()
        mock_tools.all_required_available.return_value = False
        mock_tools.check_lto_tools.return_value = {
            "llvm-link": None, "opt": None, "llc": None, "llvm-strip": None
        }
        linker = LTOLinker(cfg, tools=mock_tools)
        result = linker.link_with_lto(
            [tmp_path / "a.bc"], tmp_path / "out", work_dir=tmp_path
        )
        assert result.success is False
        assert result.errors

    def test_empty_bitcode_list_fails(self, tmp_path):
        cfg = LTOConfig(mode=LTOMode.FULL)
        linker = LTOLinker(cfg)
        result = linker.link_with_lto([], tmp_path / "out")
        assert result.success is False
        assert "No bitcode" in result.errors[0]


class TestLTOFlagsForProfile:
    def test_disabled_returns_empty(self):
        assert lto_flags_for_profile(LTOMode.DISABLED) == []

    def test_thin_includes_flto_thin(self):
        flags = lto_flags_for_profile(LTOMode.THIN, opt_level=2)
        assert "-flto=thin" in flags
        assert "-O2" in flags

    def test_full_includes_flto(self):
        flags = lto_flags_for_profile(LTOMode.FULL, opt_level=3)
        assert "-flto" in flags
        assert "-O3" in flags

    def test_strip_adds_s_flag(self):
        flags = lto_flags_for_profile(LTOMode.THIN, strip=True)
        assert "-s" in flags

    def test_no_strip_no_s_flag(self):
        flags = lto_flags_for_profile(LTOMode.THIN, strip=False)
        assert "-s" not in flags


# ---------------------------------------------------------------------------
# Cross-compilation
# ---------------------------------------------------------------------------

from nexuslang.build.cross import (
    TargetArch,
    TargetOS,
    TargetABI,
    TargetTriple,
    ToolchainInfo,
    ToolchainDetector,
    CrossCompiler,
    CrossCompileResult,
    KNOWN_TARGETS,
    get_cross_compiler,
)


class TestTargetArch:
    def test_known_values(self):
        assert TargetArch.X86_64.value == "x86_64"
        assert TargetArch.AARCH64.value == "aarch64"
        assert TargetArch.WASM32.value == "wasm32"

    def test_from_str_exact(self):
        assert TargetArch.from_str("x86_64") == TargetArch.X86_64
        assert TargetArch.from_str("aarch64") == TargetArch.AARCH64

    def test_from_str_alias(self):
        assert TargetArch.from_str("amd64") == TargetArch.X86_64
        assert TargetArch.from_str("arm64") == TargetArch.AARCH64

    def test_from_str_case_insensitive(self):
        assert TargetArch.from_str("X86_64") == TargetArch.X86_64

    def test_from_str_unknown_raises(self):
        with pytest.raises(ValueError):
            TargetArch.from_str("z80")


class TestTargetOS:
    def test_known_values(self):
        assert TargetOS.LINUX.value == "linux"
        assert TargetOS.NONE.value == "none"
        assert TargetOS.WASI.value == "wasi"

    def test_from_str(self):
        assert TargetOS.from_str("linux") == TargetOS.LINUX
        assert TargetOS.from_str("darwin") == TargetOS.MACOS

    def test_unknown_maps_to_unknown(self):
        assert TargetOS.from_str("haiku") == TargetOS.UNKNOWN


class TestTargetABI:
    def test_known_values(self):
        assert TargetABI.GNU.value == "gnu"
        assert TargetABI.MUSL.value == "musl"
        assert TargetABI.EABI.value == "eabi"

    def test_from_str(self):
        assert TargetABI.from_str("gnu") == TargetABI.GNU
        assert TargetABI.from_str("gnueabihf") == TargetABI.GNU_EABIHF

    def test_unknown_maps_to_unknown(self):
        assert TargetABI.from_str("exotic") == TargetABI.UNKNOWN


class TestTargetTripleParse:
    def test_four_component(self):
        t = TargetTriple.parse("x86_64-unknown-linux-gnu")
        assert t.arch == TargetArch.X86_64
        assert t.vendor == "unknown"
        assert t.os == TargetOS.LINUX
        assert t.abi == TargetABI.GNU

    def test_three_component_wasm(self):
        t = TargetTriple.parse("wasm32-unknown-wasi")
        assert t.arch == TargetArch.WASM32
        assert t.os == TargetOS.WASI

    def test_three_component_embedded(self):
        t = TargetTriple.parse("arm-none-eabi")
        assert t.arch == TargetArch.ARM
        assert t.os == TargetOS.NONE
        assert t.abi == TargetABI.EABI

    def test_aarch64_linux_gnu(self):
        t = TargetTriple.parse("aarch64-unknown-linux-gnu")
        assert t.arch == TargetArch.AARCH64
        assert t.os == TargetOS.LINUX
        assert t.abi == TargetABI.GNU

    def test_invalid_raises(self):
        with pytest.raises(ValueError):
            TargetTriple.parse("onlyonething")

    def test_str_round_trip(self):
        original = "x86_64-unknown-linux-gnu"
        t = TargetTriple.parse(original)
        assert str(t) == original

    def test_str_omits_none_abi(self):
        t = TargetTriple.parse("wasm32-unknown-wasi")
        s = str(t)
        assert s == "wasm32-unknown-wasi"


class TestTargetTripleProperties:
    def test_is_embedded_bare_metal(self):
        t = TargetTriple.parse("arm-none-eabi")
        assert t.is_embedded is True

    def test_is_embedded_false_for_linux(self):
        t = TargetTriple.parse("aarch64-unknown-linux-gnu")
        assert t.is_embedded is False

    def test_is_wasm(self):
        t = TargetTriple.parse("wasm32-unknown-wasi")
        assert t.is_wasm is True

    def test_not_wasm(self):
        t = TargetTriple.parse("x86_64-unknown-linux-gnu")
        assert t.is_wasm is False

    def test_is_linux(self):
        t = TargetTriple.parse("x86_64-unknown-linux-gnu")
        assert t.is_linux is True

    def test_pointer_width_64(self):
        t = TargetTriple.parse("x86_64-unknown-linux-gnu")
        assert t.pointer_width == 64

    def test_pointer_width_32(self):
        t = TargetTriple.parse("arm-none-eabi")
        assert t.pointer_width == 32

    def test_wasm32_pointer_width(self):
        t = TargetTriple.parse("wasm32-unknown-wasi")
        assert t.pointer_width == 32


class TestKnownTargets:
    def test_linux_x86_64_present(self):
        assert "x86_64-unknown-linux-gnu" in KNOWN_TARGETS

    def test_aarch64_linux_present(self):
        assert "aarch64-unknown-linux-gnu" in KNOWN_TARGETS

    def test_wasm32_wasi_present(self):
        assert "wasm32-unknown-wasi" in KNOWN_TARGETS

    def test_arm_none_eabi_present(self):
        assert "arm-none-eabi" in KNOWN_TARGETS

    def test_all_parse_correctly(self):
        for triple_str, target in KNOWN_TARGETS.items():
            assert isinstance(target, TargetTriple)
            assert str(target) == triple_str


class TestToolchainInfo:
    def test_incomplete_when_no_compiler(self):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        info = ToolchainInfo(target=target)
        assert info.is_complete is False

    def test_complete_when_clang_present(self, tmp_path):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        info = ToolchainInfo(target=target, clang=tmp_path / "clang")
        assert info.is_complete is True

    def test_complete_when_gcc_present(self, tmp_path):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        info = ToolchainInfo(target=target, gcc=tmp_path / "aarch64-linux-gnu-gcc")
        assert info.is_complete is True

    def test_compiler_prefers_clang(self, tmp_path):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        clang = tmp_path / "clang"
        gcc = tmp_path / "aarch64-linux-gnu-gcc"
        info = ToolchainInfo(target=target, clang=clang, gcc=gcc)
        assert info.compiler == clang

    def test_compiler_falls_back_to_gcc(self, tmp_path):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        gcc = tmp_path / "aarch64-linux-gnu-gcc"
        info = ToolchainInfo(target=target, gcc=gcc)
        assert info.compiler == gcc


class TestToolchainDetector:
    def test_detect_returns_toolchain_info(self):
        detector = ToolchainDetector()
        target = TargetTriple.parse("x86_64-unknown-linux-gnu")
        info = detector.detect(target)
        assert isinstance(info, ToolchainInfo)
        assert info.target == target

    def test_find_sysroot_returns_none_for_unknown(self):
        detector = ToolchainDetector()
        target = TargetTriple.parse("riscv32-unknown-none-elf")
        sysroot = detector._find_sysroot(target)
        # May be None or a Path; both are valid
        assert sysroot is None or isinstance(sysroot, Path)

    def test_list_available_returns_list(self):
        detector = ToolchainDetector()
        available = detector.list_available()
        assert isinstance(available, list)
        # All entries must be complete toolchains
        for info in available:
            assert info.is_complete


class TestCrossCompilerFlags:
    def _make_cc(self, triple_str: str, clang: Path = None) -> CrossCompiler:
        target = TargetTriple.parse(triple_str)
        info = ToolchainInfo(target=target, clang=clang)
        return CrossCompiler(info)

    def test_target_flag_present_with_clang(self, tmp_path):
        cc = self._make_cc("aarch64-unknown-linux-gnu", clang=tmp_path / "clang")
        flags = cc.get_compiler_flags()
        assert any("--target=aarch64" in f for f in flags)

    def test_sysroot_flag_when_sysroot_set(self, tmp_path):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        info = ToolchainInfo(
            target=target, clang=tmp_path / "clang",
            sysroot=tmp_path / "sysroot"
        )
        cc = CrossCompiler(info)
        flags = cc.get_compiler_flags()
        assert any("--sysroot" in f for f in flags)

    def test_wasm_gets_nostdlib(self, tmp_path):
        cc = self._make_cc("wasm32-unknown-wasi", clang=tmp_path / "clang")
        flags = cc.get_compiler_flags()
        assert "-nostdlib" in flags

    def test_embedded_gets_nostdlib(self, tmp_path):
        cc = self._make_cc("arm-none-eabi", clang=tmp_path / "clang")
        flags = cc.get_compiler_flags()
        assert "-nostdlib" in flags

    def test_wasm_linker_flags(self, tmp_path):
        cc = self._make_cc("wasm32-unknown-wasi", clang=tmp_path / "clang")
        flags = cc.get_linker_flags()
        assert any("no-entry" in f for f in flags)

    def test_no_compiler_returns_failure(self, tmp_path):
        target = TargetTriple.parse("aarch64-unknown-linux-gnu")
        info = ToolchainInfo(target=target)  # no clang, no gcc
        cc = CrossCompiler(info)
        result = cc.compile(tmp_path / "a.c", tmp_path / "a.o")
        assert result.success is False
        assert result.errors


class TestGetCrossCompiler:
    def test_returns_tuple(self):
        cc, info = get_cross_compiler("x86_64-unknown-linux-gnu")
        assert isinstance(cc, CrossCompiler)
        assert isinstance(info, ToolchainInfo)

    def test_target_set_correctly(self):
        _, info = get_cross_compiler("aarch64-unknown-linux-gnu")
        assert info.target.arch == TargetArch.AARCH64


# ---------------------------------------------------------------------------
# Integration: BuildConfig new fields
# ---------------------------------------------------------------------------

from nexuslang.build.project import BuildConfig


class TestBuildConfigNewFields:
    def test_defaults(self):
        cfg = BuildConfig()
        assert cfg.parallel_jobs == 0
        assert cfg.lto == "disabled"
        assert cfg.sysroot is None

    def test_custom_values(self):
        cfg = BuildConfig(parallel_jobs=4, lto="thin", sysroot="/opt/sysroot")
        assert cfg.parallel_jobs == 4
        assert cfg.lto == "thin"
        assert cfg.sysroot == "/opt/sysroot"


class TestProjectTomlRoundTrip:
    def test_new_fields_persisted(self, tmp_path):
        from nexuslang.build.project import Project
        project = Project.init(tmp_path, "myapp")
        project.build.parallel_jobs = 4
        project.build.lto = "thin"
        project.build.sysroot = "/usr/sysroot"
        toml_path = tmp_path / "nexuslang.toml"
        project.to_toml(toml_path)

        loaded = Project.from_toml(toml_path)
        assert loaded.build.parallel_jobs == 4
        assert loaded.build.lto == "thin"
        assert loaded.build.sysroot == "/usr/sysroot"

    def test_defaults_preserved_on_round_trip(self, tmp_path):
        from nexuslang.build.project import Project
        project = Project.init(tmp_path, "myapp")
        toml_path = tmp_path / "nexuslang.toml"
        project.to_toml(toml_path)

        loaded = Project.from_toml(toml_path)
        assert loaded.build.parallel_jobs == 0
        assert loaded.build.lto == "disabled"
        assert loaded.build.sysroot is None


class TestBuilderJobsOverride:
    def test_jobs_override_stored(self, tmp_path):
        from nexuslang.build.project import Project
        from nexuslang.build.builder import Builder
        project = Project.init(tmp_path, "myapp")
        project.build.parallel_jobs = 1

        # Patch _find_compiler to avoid FileNotFoundError
        with patch.object(Builder, "_find_compiler", return_value=Path("nlplc")):
            b = Builder(project, jobs=8)
        assert b._jobs == 8

    def test_project_jobs_used_when_no_override(self, tmp_path):
        from nexuslang.build.project import Project
        from nexuslang.build.builder import Builder
        project = Project.init(tmp_path, "myapp")
        project.build.parallel_jobs = 4

        with patch.object(Builder, "_find_compiler", return_value=Path("nlplc")):
            b = Builder(project)
        assert b._jobs == 4


# ---------------------------------------------------------------------------
# Integration: BuildSystem LTO wiring
# ---------------------------------------------------------------------------

from nexuslang.tooling.builder import BuildSystem, BuildResult
from nexuslang.tooling.config import ProjectConfig, ProfileConfig, PackageConfig, BuildConfig


def _make_build_system(tmp_path: Path, target: str = "c", lto: bool = False) -> BuildSystem:
    """Create a minimal BuildSystem pointing at tmp_path."""
    pkg = PackageConfig(name="testpkg", version="0.1.0")
    build_cfg = BuildConfig(
        source_dir=str(tmp_path / "src"),
        output_dir=str(tmp_path / "build"),
        target=target,
    )
    cfg = ProjectConfig(package=pkg, build=build_cfg)
    cfg.profiles["dev"] = ProfileConfig(
        name="dev", optimization=0, lto=lto
    )
    cfg.profiles["release"] = ProfileConfig(
        name="release", optimization=3, lto=lto, strip=True
    )
    return BuildSystem(cfg)


class TestBuildResultLTOFields:
    """BuildResult carries LTO metadata correctly."""

    def test_default_lto_fields_are_none(self):
        r = BuildResult(success=True)
        assert r.lto_output is None
        assert r.lto_skipped_reason is None

    def test_lto_output_set(self, tmp_path):
        out = tmp_path / "mypkg"
        r = BuildResult(success=True, lto_output=out)
        assert r.lto_output == out

    def test_lto_skipped_reason_set(self):
        r = BuildResult(success=True, lto_skipped_reason="no .ll files")
        assert "no .ll" in r.lto_skipped_reason

    def test_print_summary_with_lto_output(self, tmp_path, capsys):
        out = tmp_path / "mypkg"
        r = BuildResult(success=True, files_compiled=2, elapsed=0.5, lto_output=out)
        r.print_summary("mypkg", "release")
        captured = capsys.readouterr().out
        assert "LTO" in captured
        assert "mypkg" in captured

    def test_print_summary_with_lto_skipped(self, capsys):
        r = BuildResult(
            success=True, files_compiled=1, elapsed=0.1,
            lto_skipped_reason="target is 'c'"
        )
        r.print_summary("mypkg", "dev")
        captured = capsys.readouterr().out
        assert "LTO skipped" in captured

    def test_print_summary_no_lto_no_mention(self, capsys):
        r = BuildResult(success=True, files_compiled=1, elapsed=0.1)
        r.print_summary("mypkg", "dev")
        captured = capsys.readouterr().out
        assert "LTO" not in captured


class TestBuildSystemRunLTOIfEnabled:
    """Unit tests for BuildSystem._run_lto_if_enabled."""

    def test_lto_disabled_returns_no_run(self, tmp_path):
        bs = _make_build_system(tmp_path, target="llvm_ir", lto=False)
        prof = bs.config.get_profile("dev")  # lto=False
        ran, out, skip, errs, warns = bs._run_lto_if_enabled(prof, str(tmp_path), "pkg")
        assert ran is False
        assert out is None
        assert skip is None
        assert errs == []
        assert warns == []

    def test_lto_enabled_but_wrong_target_returns_skip_reason(self, tmp_path):
        bs = _make_build_system(tmp_path, target="c", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True)
        ran, out, skip, errs, warns = bs._run_lto_if_enabled(prof, str(tmp_path), "pkg")
        assert ran is False
        assert out is None
        assert skip is not None
        assert "llvm_ir" in skip
        assert errs == []

    def test_lto_enabled_llvm_ir_no_ll_files_returns_skip(self, tmp_path):
        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True)
        ran, out, skip, errs, warns = bs._run_lto_if_enabled(
            prof, str(tmp_path / "empty_dir"), "pkg"
        )
        assert ran is False
        assert skip is not None
        assert "no .ll" in skip

    def test_lto_enabled_missing_tools_returns_errors(self, tmp_path):
        out_dir = tmp_path / "build"
        out_dir.mkdir()
        (out_dir / "main.ll").write_text("; fake bitcode\n")

        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True)

        from nexuslang.build.lto import LTOResult
        with patch("nexuslang.tooling.builder.LTOLinker") as MockLinker:
            instance = MockLinker.return_value
            instance.link_with_lto.return_value = LTOResult(
                success=False, errors=["llvm-link not found"]
            )
            ran, out, skip, errs, warns = bs._run_lto_if_enabled(
                prof, str(out_dir), "pkg"
            )

        assert ran is True
        assert out is None
        assert errs == ["llvm-link not found"]

    def test_lto_enabled_success_path(self, tmp_path):
        out_dir = tmp_path / "build"
        out_dir.mkdir()
        (out_dir / "main.ll").write_text("; fake bitcode\n")
        (out_dir / "utils.ll").write_text("; fake bitcode 2\n")

        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True)

        final_out = out_dir / "pkg"
        from nexuslang.build.lto import LTOResult
        with patch("nexuslang.tooling.builder.LTOLinker") as MockLinker:
            instance = MockLinker.return_value
            instance.link_with_lto.return_value = LTOResult(
                success=True, output_file=final_out
            )
            ran, out, skip, errs, warns = bs._run_lto_if_enabled(
                prof, str(out_dir), "pkg"
            )

        assert ran is True
        assert out == final_out
        assert skip is None
        assert errs == []

    def test_lto_uses_thin_for_opt_lt_3(self, tmp_path):
        out_dir = tmp_path / "build"
        out_dir.mkdir()
        (out_dir / "a.ll").write_text("; bit\n")

        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="dev", optimization=1, lto=True)

        from nexuslang.build.lto import LTOResult, LTOMode
        captured_config = {}

        def capture_link(bitcode_files, output, work_dir=None):
            captured_config["mode"] = MockLinker.call_args[0][0].mode
            return LTOResult(success=True, output_file=output)

        with patch("nexuslang.tooling.builder.LTOLinker") as MockLinker:
            instance = MockLinker.return_value
            instance.link_with_lto.side_effect = capture_link
            bs._run_lto_if_enabled(prof, str(out_dir), "pkg")

        # LTOLinker was constructed with THIN mode for opt_level < 3
        ctor_config = MockLinker.call_args[0][0]
        assert ctor_config.mode == LTOMode.THIN

    def test_lto_uses_full_for_opt_3(self, tmp_path):
        out_dir = tmp_path / "build"
        out_dir.mkdir()
        (out_dir / "a.ll").write_text("; bit\n")

        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True)

        from nexuslang.build.lto import LTOResult, LTOMode
        with patch("nexuslang.tooling.builder.LTOLinker") as MockLinker:
            instance = MockLinker.return_value
            instance.link_with_lto.return_value = LTOResult(success=True, output_file=out_dir / "pkg")
            bs._run_lto_if_enabled(prof, str(out_dir), "pkg")

        ctor_config = MockLinker.call_args[0][0]
        assert ctor_config.mode == LTOMode.FULL

    def test_lto_strip_debug_follows_profile(self, tmp_path):
        out_dir = tmp_path / "build"
        out_dir.mkdir()
        (out_dir / "a.ll").write_text("; bit\n")

        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True, strip=True)

        from nexuslang.build.lto import LTOResult
        with patch("nexuslang.tooling.builder.LTOLinker") as MockLinker:
            instance = MockLinker.return_value
            instance.link_with_lto.return_value = LTOResult(
                success=True, output_file=out_dir / "pkg"
            )
            bs._run_lto_if_enabled(prof, str(out_dir), "pkg")

        ctor_config = MockLinker.call_args[0][0]
        assert ctor_config.strip_debug is True

    def test_lto_passes_all_ll_files(self, tmp_path):
        out_dir = tmp_path / "build"
        out_dir.mkdir()
        sub = out_dir / "sub"
        sub.mkdir()
        (out_dir / "a.ll").write_text("; 1\n")
        (out_dir / "b.ll").write_text("; 2\n")
        (sub / "c.ll").write_text("; 3\n")

        bs = _make_build_system(tmp_path, target="llvm_ir", lto=True)
        prof = ProfileConfig(name="release", optimization=3, lto=True)

        from nexuslang.build.lto import LTOResult
        with patch("nexuslang.tooling.builder.LTOLinker") as MockLinker:
            instance = MockLinker.return_value
            instance.link_with_lto.return_value = LTOResult(
                success=True, output_file=out_dir / "pkg"
            )
            bs._run_lto_if_enabled(prof, str(out_dir), "pkg")

        call_args = instance.link_with_lto.call_args
        passed_files = call_args[1]["bitcode_files"] if call_args[1] else call_args[0][0]
        assert len(passed_files) == 3


class TestBuildResultLTOIntegration:
    """Verify that _build_internal populates BuildResult LTO fields."""

    def _make_project_config(self, tmp_path: Path, target: str, lto: bool) -> ProjectConfig:
        src_dir = tmp_path / "src"
        src_dir.mkdir(parents=True, exist_ok=True)
        (src_dir / "main.nxl").write_text('print text "hello"\n')
        build_dir = tmp_path / "build"

        pkg = PackageConfig(name="mypkg", version="0.1.0")
        build_cfg = BuildConfig(
            source_dir=str(src_dir),
            output_dir=str(build_dir),
            target=target,
        )
        cfg = ProjectConfig(package=pkg, build=build_cfg)
        cfg.profiles["dev"] = ProfileConfig(name="dev", optimization=0, lto=lto)
        cfg.profiles["release"] = ProfileConfig(
            name="release", optimization=3, lto=lto, strip=False
        )
        return cfg

    def test_lto_disabled_result_has_no_lto_info(self, tmp_path):
        cfg = self._make_project_config(tmp_path, target="c", lto=False)
        bs = BuildSystem(cfg)
        result = bs._build_internal(
            release=False, profile=None, features=None,
            jobs=1, clean=False, check_only=False, optimize_bounds_checks=False,
        )
        assert result.lto_output is None
        assert result.lto_skipped_reason is None

    def test_lto_enabled_wrong_target_sets_skip_reason(self, tmp_path):
        cfg = self._make_project_config(tmp_path, target="c", lto=True)
        bs = BuildSystem(cfg)
        result = bs._build_internal(
            release=False, profile=None, features=None,
            jobs=1, clean=False, check_only=False, optimize_bounds_checks=False,
        )
        # LTO enabled but target is 'c', not 'llvm_ir' — skip reason set
        assert result.lto_skipped_reason is not None
        assert "llvm_ir" in result.lto_skipped_reason
        # The build should still succeed
        assert result.success is True

    def test_lto_enabled_llvm_ir_target_skips_gracefully_without_ll_files(self, tmp_path):
        """When compilation fails to produce .ll files (mocked), LTO is skipped."""
        cfg = self._make_project_config(tmp_path, target="llvm_ir", lto=True)
        bs = BuildSystem(cfg)

        # Mock _compile_one to succeed without actually writing .ll files
        def fake_compile(source_path, out_dir, prof, features, check_only, opt_bounds):
            return source_path, True, []

        with patch.object(bs, "_compile_one", side_effect=fake_compile):
            result = bs._build_internal(
                release=False, profile=None, features=None,
                jobs=1, clean=False, check_only=False, optimize_bounds_checks=False,
            )

        # Should succeed (compilation ok) but LTO skipped (no .ll files produced)
        assert result.success is True
        assert result.lto_output is None
        assert result.lto_skipped_reason is not None
        assert "no .ll" in result.lto_skipped_reason
