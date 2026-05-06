"""
Build System Battle Tests (8.1.4)

Stress-tests and edge-case coverage for:
- incremental.py: FileMetadata, DependencyGraph, BuildCache, extract_imports_from_source
- Cache invalidation: mtime change, hash change, deletion, dependency propagation
- Large workspace simulation (100 source files)
- Manifest edge cases: workspace-only, invalid names, complex deps, target-specific
- Thread-safety of BuildCache under concurrent access

NOTE: These tests do NOT invoke the NexusLang compiler or LLVM; they validate the
build infrastructure layer in isolation.
"""
from __future__ import annotations

import json
import os
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import List
from unittest.mock import patch

import pytest

# Ensure project root is on sys.path
_PROJECT_ROOT = Path(__file__).parent.parent.parent.resolve()
if str(_PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(_PROJECT_ROOT))

from nexuslang.build.incremental import (
    BuildArtifact,
    BuildCache,
    DependencyGraph,
    FileMetadata,
    extract_imports_from_source,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _write_nxl(path: Path, content: str = "print text \"hello\"\n") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return path


def _touch(path: Path) -> None:
    """Bump mtime by writing same content."""
    content = path.read_bytes()
    path.write_bytes(content)
    # Force mtime to differ by at least 1 cycle
    os.utime(str(path), (time.time() + 1, time.time() + 1))


def _build_cache(tmp_path: Path) -> BuildCache:
    cache_dir = tmp_path / ".nxl_cache"
    return BuildCache(cache_dir)


# ===========================================================================
# FileMetadata
# ===========================================================================

class TestFileMetadata:
    def test_has_changed_same_values(self) -> None:
        fm = FileMetadata(path="/a", mtime=1000.0, size=512, hash="abc")
        assert fm.has_changed(1000.0, 512) is False

    def test_has_changed_mtime_differs(self) -> None:
        fm = FileMetadata(path="/a", mtime=1000.0, size=512, hash="abc")
        assert fm.has_changed(1001.0, 512) is True

    def test_has_changed_size_differs(self) -> None:
        fm = FileMetadata(path="/a", mtime=1000.0, size=512, hash="abc")
        assert fm.has_changed(1000.0, 513) is True

    def test_has_changed_both_differ(self) -> None:
        fm = FileMetadata(path="/a", mtime=1000.0, size=512, hash="abc")
        assert fm.has_changed(999.0, 1024) is True

    def test_roundtrip_dict(self) -> None:
        fm = FileMetadata(
            path="/src/main.nxl", mtime=9999.5, size=1024,
            hash="deadbeef", imports=["std.io", "core"]
        )
        d = fm.to_dict()
        restored = FileMetadata.from_dict(d)
        assert restored.path == fm.path
        assert restored.mtime == fm.mtime
        assert restored.size == fm.size
        assert restored.hash == fm.hash
        assert restored.imports == fm.imports

    def test_to_dict_keys(self) -> None:
        fm = FileMetadata(path="/x", mtime=1.0, size=8, hash="zz")
        d = fm.to_dict()
        assert set(d.keys()) >= {"path", "mtime", "size", "hash", "imports"}

    def test_empty_imports_default(self) -> None:
        fm = FileMetadata(path="/x", mtime=1.0, size=8, hash="zz")
        assert fm.imports == []


# ===========================================================================
# BuildArtifact
# ===========================================================================

class TestBuildArtifact:
    def test_roundtrip_dict(self) -> None:
        ba = BuildArtifact(
            source_file="/src/main.nxl",
            output_file="/build/main.o",
            build_time=12345.0,
            profile="release",
            features=["lto", "opt"],
        )
        d = ba.to_dict()
        restored = BuildArtifact.from_dict(d)
        assert restored.source_file == ba.source_file
        assert restored.output_file == ba.output_file
        assert restored.build_time == ba.build_time
        assert restored.profile == ba.profile
        assert restored.features == ba.features

    def test_empty_features_default(self) -> None:
        ba = BuildArtifact(
            source_file="/a", output_file="/b",
            build_time=0.0, profile="dev"
        )
        assert ba.features == []


# ===========================================================================
# DependencyGraph (incremental.py)
# ===========================================================================

class TestIncrementalDependencyGraph:
    def test_add_and_get_dependency(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        assert "b.nxl" in g.get_dependencies("a.nxl")

    def test_get_dependents(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        assert "a.nxl" in g.get_dependents("b.nxl")

    def test_no_self_loop_in_transitive_deps(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        deps = g.get_transitive_dependencies("a.nxl")
        assert "a.nxl" not in deps  # file itself excluded

    def test_transitive_dependencies_two_hops(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        g.add_dependency("b.nxl", "c.nxl")
        trans = g.get_transitive_dependencies("a.nxl")
        assert "b.nxl" in trans
        assert "c.nxl" in trans

    def test_transitive_dependencies_diamond(self) -> None:
        """a -> b, a -> c, b -> d, c -> d (diamond)."""
        g = DependencyGraph()
        g.add_dependency("a", "b")
        g.add_dependency("a", "c")
        g.add_dependency("b", "d")
        g.add_dependency("c", "d")
        trans = g.get_transitive_dependencies("a")
        assert trans == {"b", "c", "d"}

    def test_transitive_dependents_two_hops(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        g.add_dependency("b.nxl", "c.nxl")
        trans = g.get_transitive_dependents("c.nxl")
        assert "b.nxl" in trans
        assert "a.nxl" in trans

    def test_circular_dependency_does_not_infinite_loop(self) -> None:
        """Circular deps must terminate cleanly (visited set prevents loop)."""
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        g.add_dependency("b.nxl", "c.nxl")
        g.add_dependency("c.nxl", "a.nxl")  # cycle
        # Must not raise, must not hang
        trans_deps = g.get_transitive_dependencies("a.nxl")
        trans_deps_b = g.get_transitive_dependents("a.nxl")
        assert isinstance(trans_deps, set)
        assert isinstance(trans_deps_b, set)

    def test_empty_graph_returns_empty_sets(self) -> None:
        g = DependencyGraph()
        assert g.get_dependencies("ghost.nxl") == set()
        assert g.get_dependents("ghost.nxl") == set()
        assert g.get_transitive_dependencies("ghost.nxl") == set()
        assert g.get_transitive_dependents("ghost.nxl") == set()

    def test_multiple_deps_same_source(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a", "b")
        g.add_dependency("a", "c")
        g.add_dependency("a", "d")
        assert len(g.get_dependencies("a")) == 3

    def test_roundtrip_dict(self) -> None:
        g = DependencyGraph()
        g.add_dependency("a.nxl", "b.nxl")
        g.add_dependency("b.nxl", "c.nxl")
        d = g.to_dict()
        restored = DependencyGraph.from_dict(d)
        assert "b.nxl" in restored.get_dependencies("a.nxl")
        assert "c.nxl" in restored.get_dependencies("b.nxl")
        assert "a.nxl" in restored.get_dependents("b.nxl")

    def test_large_chain_terminates(self) -> None:
        """Chain of 200 nodes — transitive deps should work without stack overflow."""
        g = DependencyGraph()
        nodes = [f"mod_{i}.nxl" for i in range(200)]
        for i in range(len(nodes) - 1):
            g.add_dependency(nodes[i], nodes[i + 1])
        trans = g.get_transitive_dependencies(nodes[0])
        assert len(trans) == 199  # all but nodes[0] itself


# ===========================================================================
# BuildCache
# ===========================================================================

class TestBuildCacheBasics:
    def test_new_cache_dir_created(self, tmp_path: Path) -> None:
        cache_dir = tmp_path / "nonexistent" / "cache"
        cache = BuildCache(cache_dir)
        assert cache_dir.exists()

    def test_is_file_changed_for_new_file(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        assert cache.is_file_changed(src) is True  # not in cache yet

    def test_update_then_not_changed(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(src)
        assert cache.is_file_changed(src) is False

    def test_mtime_change_detected(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(src)
        _touch(src)
        assert cache.is_file_changed(src) is True

    def test_size_change_detected(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl", "x")
        cache.update_file_metadata(src)
        src.write_text("x" * 1000, encoding="utf-8")
        assert cache.is_file_changed(src) is True

    def test_file_deletion_detected(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(src)
        src.unlink()
        assert cache.is_file_changed(src) is True

    def test_hash_unchanged_after_identical_write(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl", "exact content")
        cache.update_file_metadata(src)
        # Write the exact same bytes — hash must match
        src.write_text("exact content", encoding="utf-8")
        assert cache.is_file_hash_changed(src) is False

    def test_hash_changed_after_content_change(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl", "original")
        cache.update_file_metadata(src)
        src.write_text("modified", encoding="utf-8")
        assert cache.is_file_hash_changed(src) is True

    def test_update_imports_registered(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        dep = _write_nxl(tmp_path / "utils.nxl")
        cache.update_file_metadata(src, imports=[str(dep)])
        deps = cache.dependency_graph.get_dependencies(str(src))
        assert str(dep) in deps


class TestBuildCacheNeedsRebuild:
    def test_new_file_needs_rebuild(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        needs, reason = cache.needs_rebuild(src)
        assert needs is True
        assert reason  # has explanation

    def test_up_to_date_after_record_build(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        out = tmp_path / "main.o"
        out.write_bytes(b"ELF")
        cache.update_file_metadata(src)
        cache.record_build(src, out, profile="dev")
        needs, reason = cache.needs_rebuild(src, profile="dev")
        assert needs is False
        assert "up to date" in reason.lower()

    def test_profile_change_triggers_rebuild(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        out = tmp_path / "main.o"
        out.write_bytes(b"ELF")
        cache.update_file_metadata(src)
        cache.record_build(src, out, profile="dev")
        needs, reason = cache.needs_rebuild(src, profile="release")
        assert needs is True

    def test_feature_change_triggers_rebuild(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        out = tmp_path / "main.o"
        out.write_bytes(b"ELF")
        cache.update_file_metadata(src)
        cache.record_build(src, out, profile="dev", features=["feat_a"])
        needs, reason = cache.needs_rebuild(src, profile="dev", features=["feat_b"])
        assert needs is True

    def test_missing_output_triggers_rebuild(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        out = tmp_path / "main.o"
        out.write_bytes(b"ELF")
        cache.update_file_metadata(src)
        cache.record_build(src, out, profile="dev")
        out.unlink()  # delete output
        needs, reason = cache.needs_rebuild(src, profile="dev")
        assert needs is True

    def test_dependency_change_triggers_rebuild(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        dep = _write_nxl(tmp_path / "utils.nxl")
        out = tmp_path / "main.o"
        out.write_bytes(b"ELF")
        cache.update_file_metadata(dep)
        cache.update_file_metadata(src, imports=[str(dep)])
        cache.record_build(src, out, profile="dev")
        # Modify dependency
        dep.write_text("changed", encoding="utf-8")
        needs, reason = cache.needs_rebuild(src, profile="dev")
        assert needs is True

    def test_get_files_to_rebuild_empty_when_uptodate(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        files = []
        for i in range(5):
            src = _write_nxl(tmp_path / f"mod_{i}.nxl")
            out = tmp_path / f"mod_{i}.o"
            out.write_bytes(b"ELF")
            cache.update_file_metadata(src)
            cache.record_build(src, out, profile="dev")
            files.append(src)
        to_rebuild = cache.get_files_to_rebuild(files, profile="dev")
        assert len(to_rebuild) == 0

    def test_get_files_to_rebuild_detects_changed(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        files = []
        for i in range(5):
            src = _write_nxl(tmp_path / f"mod_{i}.nxl")
            out = tmp_path / f"mod_{i}.o"
            out.write_bytes(b"ELF")
            cache.update_file_metadata(src)
            cache.record_build(src, out, profile="dev")
            files.append(src)
        # Modify one file
        files[2].write_text("changed content", encoding="utf-8")
        to_rebuild = cache.get_files_to_rebuild(files, profile="dev")
        changed_paths = [p for p, _ in to_rebuild]
        assert files[2] in changed_paths


class TestBuildCacheInvalidation:
    def test_invalidate_returns_file_and_dependents(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        lib = _write_nxl(tmp_path / "lib.nxl")
        main = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(lib)
        cache.update_file_metadata(main, imports=[str(lib)])
        invalids = cache.invalidate_file(lib)
        assert lib in invalids
        assert main in invalids

    def test_invalidate_includes_transitive(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        c = _write_nxl(tmp_path / "c.nxl")
        b = _write_nxl(tmp_path / "b.nxl")
        a = _write_nxl(tmp_path / "a.nxl")
        cache.update_file_metadata(c)
        cache.update_file_metadata(b, imports=[str(c)])
        cache.update_file_metadata(a, imports=[str(b)])
        invalids = cache.invalidate_file(c)
        assert c in invalids
        assert b in invalids
        assert a in invalids

    def test_clear_removes_all(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(src)
        cache.clear()
        assert len(cache.file_metadata) == 0
        assert len(cache.build_artifacts) == 0

    def test_invalidate_leaf_does_not_affect_others(self, tmp_path: Path) -> None:
        """Invalidating a leaf (no dependents) only returns itself."""
        cache = _build_cache(tmp_path)
        a = _write_nxl(tmp_path / "a.nxl")
        b = _write_nxl(tmp_path / "b.nxl")
        cache.update_file_metadata(a)
        cache.update_file_metadata(b)
        invalids = cache.invalidate_file(b)
        assert b in invalids
        assert a not in invalids


class TestBuildCachePersistence:
    def test_save_and_reload(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        out = tmp_path / "main.o"
        out.write_bytes(b"ELF")
        cache.update_file_metadata(src)
        cache.record_build(src, out, profile="dev", features=["a"])
        cache.save()

        # Create fresh cache pointing to same dir
        cache2 = BuildCache(tmp_path / ".nxl_cache")
        assert str(src) in cache2.file_metadata
        artifact_key = f"{src}:dev:a"
        assert artifact_key in cache2.build_artifacts

    def test_cache_file_is_valid_json(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(src)
        cache.save()
        data = json.loads(cache.cache_file.read_text(encoding="utf-8"))
        assert "file_metadata" in data
        assert "dependency_graph" in data
        assert "build_artifacts" in data

    def test_corrupt_cache_file_recovered(self, tmp_path: Path) -> None:
        """A corrupt cache file should not crash — start fresh instead."""
        cache_dir = tmp_path / ".nxl_cache"
        cache_dir.mkdir(parents=True)
        cache_file = cache_dir / "build_cache.json"
        cache_file.write_text("{CORRUPT: not json", encoding="utf-8")
        # Should not raise
        cache = BuildCache(cache_dir)
        assert len(cache.file_metadata) == 0

    def test_empty_cache_file_recovered(self, tmp_path: Path) -> None:
        """An empty cache file should start fresh."""
        cache_dir = tmp_path / ".nxl_cache"
        cache_dir.mkdir(parents=True)
        cache_file = cache_dir / "build_cache.json"
        cache_file.write_bytes(b"")
        cache = BuildCache(cache_dir)
        assert len(cache.file_metadata) == 0

    def test_cache_version_field_written(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        cache.save()
        data = json.loads(cache.cache_file.read_text(encoding="utf-8"))
        assert "version" in data

    def test_clear_removes_cache_file(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        cache.save()
        assert cache.cache_file.exists()
        cache.clear()
        assert not cache.cache_file.exists()


# ===========================================================================
# Large Workspace Simulation
# ===========================================================================

class TestLargeWorkspaceSimulation:
    """Simulate a workspace with 100 source files and complex deps."""

    @pytest.fixture()
    def large_workspace(self, tmp_path: Path):
        """Create 100 .nlpl files with a fan-out dependency structure."""
        # 10 core libs, each imported by 9 leaf modules
        # Layout:
        #   core_0..9.nlpl  (no deps)
        #   leaf_0..89.nlpl  (each imports core_{i//9})
        cache = _build_cache(tmp_path)
        cores = []
        for i in range(10):
            p = _write_nxl(tmp_path / f"core_{i}.nxl", f"# core lib {i}")
            cache.update_file_metadata(p, imports=[])
            cores.append(p)

        leaves = []
        for i in range(90):
            core = cores[i // 9]
            p = _write_nxl(tmp_path / f"leaf_{i}.nxl", f"# leaf {i}")
            cache.update_file_metadata(p, imports=[str(core)])
            leaves.append(p)

        return cache, cores, leaves

    def test_up_to_date_check_100_files(self, tmp_path: Path, large_workspace) -> None:
        cache, cores, leaves = large_workspace
        all_files = cores + leaves
        # Record builds for all
        for f in all_files:
            out = f.with_suffix(".o")
            out.write_bytes(b"ELF")
            cache.record_build(f, out, profile="dev")
        to_rebuild = cache.get_files_to_rebuild(all_files, profile="dev")
        assert len(to_rebuild) == 0

    def test_single_core_change_invalidates_all_dependents(
        self, tmp_path: Path, large_workspace
    ) -> None:
        cache, cores, leaves = large_workspace
        # Invalidate core_0
        invalids = cache.invalidate_file(cores[0])
        # core_0 + leaves 0..8 (9 leaves depend on core_0)
        expected_leaves = [leaves[i] for i in range(9)]
        assert cores[0] in invalids
        for leaf in expected_leaves:
            assert leaf in invalids, f"{leaf.name} should be invalidated"

    def test_large_cache_save_and_reload(self, tmp_path: Path, large_workspace) -> None:
        cache, cores, leaves = large_workspace
        cache.save()
        cache2 = BuildCache(tmp_path / ".nxl_cache")
        assert len(cache2.file_metadata) == 100

    def test_needs_rebuild_after_dep_change_propagates(
        self, tmp_path: Path, large_workspace
    ) -> None:
        cache, cores, leaves = large_workspace
        # Record all builds
        for f in cores + leaves:
            out = f.with_suffix(".o")
            out.write_bytes(b"ELF")
            cache.record_build(f, out, profile="dev")

        # Modify core_5 — leaves 45..53 depend on it
        cores[5].write_text("# modified", encoding="utf-8")

        # Only leaves that depend on core_5 should need rebuild
        affected_leaves = [leaves[i] for i in range(45, 54)]
        to_rebuild = cache.get_files_to_rebuild(affected_leaves, profile="dev")
        to_rebuild_paths = {p for p, _ in to_rebuild}
        for leaf in affected_leaves:
            assert leaf in to_rebuild_paths


# ===========================================================================
# extract_imports_from_source
# ===========================================================================

class TestExtractImports:
    def test_simple_import(self) -> None:
        code = "import std_io\n"
        imports = extract_imports_from_source(code)
        assert "std_io" in imports

    def test_dotted_import(self) -> None:
        code = "import std.collections.list\n"
        imports = extract_imports_from_source(code)
        assert "std.collections.list" in imports

    def test_multiple_imports(self) -> None:
        code = "import mod_a\nimport mod_b\nimport mod_c\n"
        imports = extract_imports_from_source(code)
        assert len(imports) == 3

    def test_indented_import(self) -> None:
        code = "  import some_module\n"
        imports = extract_imports_from_source(code)
        assert "some_module" in imports

    def test_no_imports(self) -> None:
        code = "print text \"hello\"\n"
        imports = extract_imports_from_source(code)
        assert imports == []

    def test_import_not_on_first_word(self) -> None:
        """Lines that don't start with import should be ignored."""
        code = "set x to import_path\n"
        imports = extract_imports_from_source(code)
        assert imports == []

    def test_large_file_with_many_imports(self) -> None:
        lines = [f"import module_{i}\n" for i in range(100)]
        code = "".join(lines)
        imports = extract_imports_from_source(code)
        assert len(imports) == 100

    def test_imports_inside_multiline_code(self) -> None:
        code = """
# Some comment
import core_lib
function main
    print text "hello"
end
import utils
"""
        imports = extract_imports_from_source(code)
        assert "core_lib" in imports
        assert "utils" in imports


# ===========================================================================
# Thread safety (concurrent cache access)
# ===========================================================================

class TestBuildCacheThreadSafety:
    """Verify BuildCache does not crash under concurrent access.

    BuildCache is not expected to be lock-safe (the build system serialises
    calls via a queue), but it must not corrupt data in a way that causes
    IndexError/KeyError panics when multiple threads read simultaneously.
    """

    def test_concurrent_is_file_changed_reads(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        files = []
        for i in range(20):
            p = _write_nxl(tmp_path / f"f_{i}.nxl")
            cache.update_file_metadata(p)
            files.append(p)

        errors: List[Exception] = []

        def reader() -> None:
            try:
                for f in files:
                    cache.is_file_changed(f)
                    cache.is_file_hash_changed(f)
            except Exception as exc:
                errors.append(exc)

        threads = [threading.Thread(target=reader) for _ in range(10)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        assert errors == [], f"Concurrent reads raised: {errors}"

    def test_concurrent_save_calls_do_not_crash(self, tmp_path: Path) -> None:
        cache = _build_cache(tmp_path)
        src = _write_nxl(tmp_path / "main.nxl")
        cache.update_file_metadata(src)

        errors: List[Exception] = []

        def saver() -> None:
            try:
                cache.save()
            except Exception as exc:
                errors.append(exc)

        threads = [threading.Thread(target=saver) for _ in range(5)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        # Must not raise; file may be partial but should be re-loadable or empty
        assert errors == [], f"Concurrent saves raised: {errors}"


# ===========================================================================
# Manifest edge cases
# ===========================================================================

class TestManifestEdgeCases:
    """Test Manifest parsing with unusual / boundary inputs."""

    def _write_toml(self, tmp_path: Path, content: str) -> Path:
        p = tmp_path / "nexuslang.toml"
        p.write_text(content, encoding="utf-8")
        return p

    def test_minimal_valid_manifest(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(tmp_path, '[package]\nname = "hello"\nversion = "0.1.0"\n')
        m = Manifest(p)
        assert m.package is not None
        assert m.package.name == "hello"
        assert m.package.version == "0.1.0"

    def test_missing_name_raises(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(tmp_path, '[package]\nversion = "0.1.0"\n')
        with pytest.raises((ValueError, KeyError)):
            Manifest(p)

    def test_missing_version_raises(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(tmp_path, '[package]\nname = "mylib"\n')
        with pytest.raises((ValueError, KeyError)):
            Manifest(p)

    def test_invalid_package_name_raises(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(
            tmp_path, '[package]\nname = "INVALID NAME!"\nversion = "0.1.0"\n'
        )
        with pytest.raises(ValueError, match="name"):
            Manifest(p)

    def test_invalid_version_format_raises(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(tmp_path, '[package]\nname = "mylib"\nversion = "bad"\n')
        with pytest.raises(ValueError, match="[Vv]ersion"):
            Manifest(p)

    def test_manifest_not_found_raises(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        with pytest.raises(FileNotFoundError):
            Manifest(tmp_path / "nonexistent.toml")

    def test_workspace_only_manifest(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(
            tmp_path,
            '[workspace]\nmembers = ["crate_a", "crate_b"]\n'
        )
        m = Manifest(p)
        assert m.package is None
        assert m.workspace is not None

    def test_simple_dependency_parsed(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        content = (
            '[package]\nname = "app"\nversion = "1.0.0"\n'
            '[dependencies]\nstd = "0.3.0"\n'
        )
        p = self._write_toml(tmp_path, content)
        m = Manifest(p)
        assert "std" in m.dependencies
        assert m.dependencies["std"].version_req == "0.3.0"

    def test_path_dependency_parsed(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        content = (
            '[package]\nname = "app"\nversion = "1.0.0"\n'
            '[dependencies]\nmylib = { path = "../mylib" }\n'
        )
        p = self._write_toml(tmp_path, content)
        m = Manifest(p)
        assert "mylib" in m.dependencies
        assert m.dependencies["mylib"].path == "../mylib"

    def test_dev_and_build_deps_parsed(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        content = (
            '[package]\nname = "app"\nversion = "0.1.0"\n'
            '[dev-dependencies]\ntestlib = "1.0.0"\n'
            '[build-dependencies]\nbuildtool = "0.5.0"\n'
        )
        p = self._write_toml(tmp_path, content)
        m = Manifest(p)
        assert "testlib" in m.dev_dependencies
        assert "buildtool" in m.build_dependencies

    def test_features_parsed(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        content = (
            '[package]\nname = "lib"\nversion = "1.0.0"\n'
            '[features]\ndefault = ["feat_a"]\nfeat_a = []\nfeat_b = ["feat_a"]\n'
        )
        p = self._write_toml(tmp_path, content)
        m = Manifest(p)
        assert "feat_a" in m.features
        assert "feat_b" in m.features

    def test_custom_profiles_parsed(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        content = (
            '[package]\nname = "app"\nversion = "0.1.0"\n'
            '[profile.release]\nopt-level = 3\ndebug = false\n'
        )
        p = self._write_toml(tmp_path, content)
        m = Manifest(p)
        assert "release" in m.profiles
        profile = m.profiles["release"]
        assert profile.opt_level == 3

    def test_missing_both_package_and_workspace_raises(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(tmp_path, '# empty\n[other_section]\nkey = "value"\n')
        with pytest.raises((ValueError, KeyError)):
            Manifest(p)

    def test_optional_package_fields_default(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        p = self._write_toml(
            tmp_path, '[package]\nname = "minimal"\nversion = "0.1.0"\n'
        )
        m = Manifest(p)
        assert m.package.authors == []
        assert m.package.license is None
        assert m.package.description is None

    def test_all_optional_package_fields_parsed(self, tmp_path: Path) -> None:
        from nexuslang.build.manifest import Manifest
        content = (
            '[package]\nname = "full-pkg"\nversion = "2.3.4"\n'
            'authors = ["Alice", "Bob"]\n'
            'license = "MIT"\n'
            'description = "A full package"\n'
            'repository = "https://github.com/example/full-pkg"\n'
            'homepage = "https://example.com"\n'
            'keywords = ["nlpl", "test"]\n'
            'categories = ["utilities"]\n'
        )
        p = self._write_toml(tmp_path, content)
        m = Manifest(p)
        assert m.package.authors == ["Alice", "Bob"]
        assert m.package.license == "MIT"
        assert m.package.description == "A full package"
        assert "nlpl" in m.package.keywords
