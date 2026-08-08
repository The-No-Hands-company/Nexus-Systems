"""Focused coverage tests for AST cache and cached parser wrappers."""

from pathlib import Path

from nexuslang.parser.ast_cache import ASTCache, get_global_cache, set_cache_limits
from nexuslang.parser.cached_parser import CachedParser, parse_with_cache


VALID_SOURCE = "set value to 1\n"


def test_ast_cache_put_get_invalidate_and_clear(tmp_path):
    cache = ASTCache(max_entries=2, max_memory_mb=0.1)
    file_path = str((tmp_path / "file_a.nxl").resolve())
    ast = {"program": ["node"]}

    assert cache.get(file_path, VALID_SOURCE) is None

    cache.put(file_path, VALID_SOURCE, ast)
    assert cache.get(file_path, VALID_SOURCE) == ast

    assert cache.invalidate(file_path) is True
    assert cache.invalidate(file_path) is False
    assert cache.get(file_path, VALID_SOURCE) is None

    cache.put(file_path, VALID_SOURCE, ast)
    cache.clear()
    stats = cache.get_stats()
    assert stats["entries"] == 0


def test_ast_cache_hash_invalidation_and_lru_eviction(tmp_path):
    cache = ASTCache(max_entries=1, max_memory_mb=5)
    a = str((tmp_path / "a.nxl").resolve())
    b = str((tmp_path / "b.nxl").resolve())

    cache.put(a, VALID_SOURCE, {"a": 1})
    assert cache.get(a, VALID_SOURCE) == {"a": 1}

    # Source hash mismatch invalidates old entry.
    assert cache.get(a, "set value to 2\n") is None

    cache.put(a, "set value to 2\n", {"a": 2})
    cache.put(b, VALID_SOURCE, {"b": 1})

    stats = cache.get_stats()
    assert stats["entries"] == 1
    assert stats["evictions"] >= 1


def test_global_cache_singleton_and_limit_configuration():
    first = get_global_cache()
    second = get_global_cache()
    assert first is second

    set_cache_limits(max_entries=7, max_memory_mb=1.5)
    configured = get_global_cache()
    assert configured.max_entries == 7
    assert configured.max_memory_bytes == int(1.5 * 1024 * 1024)


def test_cached_parser_hits_misses_parse_from_file_and_clear(tmp_path):
    file_path = tmp_path / "sample.nxl"
    file_path.write_text(VALID_SOURCE, encoding="utf-8")

    cache = ASTCache(max_entries=10, max_memory_mb=5)
    parser = CachedParser(cache=cache, enable_debug=False)

    ast_first = parser.parse(str(file_path), VALID_SOURCE)
    ast_second = parser.parse(str(file_path), VALID_SOURCE)
    assert ast_first is ast_second

    ast_changed = parser.parse(str(file_path), "set value to 2\n")
    assert ast_changed is not None

    from_file = parser.parse_from_file(str(file_path))
    assert from_file is not None

    parser.invalidate(str(file_path))
    parser.clear_cache()

    stats = parser.get_stats()
    assert stats["parser"]["total_parses"] >= 4
    assert stats["parser"]["cache_hits"] >= 1
    assert stats["parser"]["cache_misses"] >= 1


def test_parse_with_cache_convenience_function(tmp_path):
    file_path = Path(tmp_path / "convenience.nxl")

    ast = parse_with_cache(str(file_path), VALID_SOURCE, debug=False)

    assert ast is not None
