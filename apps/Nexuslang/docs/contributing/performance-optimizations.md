# Performance Optimizations

## Overview

NLPL v1.0+ includes intelligent AST caching and optimized parsing to improve performance, especially for Language Server Protocol (LSP) operations where files are parsed repeatedly.

## AST Cache System

### Features

- **Hash-based Invalidation**: Automatically detects when source code changes
- **LRU Eviction**: Least Recently Used policy keeps frequently accessed files cached
- **Memory Management**: Configurable limits prevent excessive memory usage
- **Thread-safe**: Safe for concurrent access in multi-threaded environments
- **Statistics Tracking**: Monitor cache hits, misses, and evictions

### Performance Metrics

Based on benchmarks with typical NexusLang files (50-70 lines):

| Scenario | Time (uncached) | Time (cached) | Speedup |
|----------|----------------|---------------|---------|
| Initial parse | 2.3-3.8ms | N/A (cold start) | 1x |
| Re-parse (same content) | 2.3-3.8ms | 0.26-0.51ms | **7.6x-9.6x** |
| LSP diagnostics | 3.3ms avg | 0.34ms avg | **9.6x** |

**Key Findings:**
- Average cache speedup: **7.6x**
- Time saved per cache hit: **2.6ms**
- Memory usage: **0.02-0.07MB per file**
- Cache hit rate in LSP: **44%** (realistic editing scenario)

### Usage

#### Basic Usage (Automatic)

The cache is automatically used by the LSP server. No configuration needed:

```python
from nexuslang.parser.cached_parser import parse_with_cache

# Automatically uses global cache
ast = parse_with_cache(file_path, source_code)

# Same file, same content - instant cache hit
ast = parse_with_cache(file_path, source_code)  # ~10x faster

# Same file, different content - cache miss, reparse
ast = parse_with_cache(file_path, modified_source)
```

#### Advanced Usage (Custom Cache)

For fine-grained control:

```python
from nexuslang.parser.ast_cache import ASTCache
from nexuslang.parser.cached_parser import CachedParser

# Create custom cache with limits
cache = ASTCache(
    max_entries=100,      # Max 100 files
    max_memory_mb=50      # Max 50MB memory
)

# Create parser with custom cache
parser = CachedParser(cache=cache, enable_debug=True)

# Parse files
ast = parser.parse(file_path, source_code)

# Check statistics
parser.print_stats()
cache.print_stats()
```

#### Configuration

Configure global cache limits:

```python
from nexuslang.parser.ast_cache import set_cache_limits

# Set limits before first use
set_cache_limits(max_entries=200, max_memory_mb=100)
```

### Cache Statistics

Monitor cache performance:

```python
from nexuslang.parser.ast_cache import get_global_cache

cache = get_global_cache()
stats = cache.get_stats()

print(f"Hit rate: {stats['hit_rate']*100:.1f}%")
print(f"Memory usage: {stats['current_memory_mb']:.2f}MB")
print(f"Evictions: {stats['evictions']}")
```

Example output:
```
AST Cache Statistics:
  Hits: 8
  Misses: 10
  Hit rate: 44.4%
  Entries: 5/100
  Evictions: 0
  Memory: 0.35/50.00MB (0.7%)
```

### Memory Estimation

The cache estimates memory usage based on AST node count:
- **Average**: ~200 bytes per AST node
- **Typical file**: 0.02-0.10MB for 50-100 lines
- **Large file**: 0.4-0.8MB for 1000+ lines

### Eviction Policy

When limits are exceeded, the cache evicts entries using LRU (Least Recently Used):

1. **Count limit**: Evict oldest when `max_entries` exceeded
2. **Memory limit**: Evict oldest when `max_memory_bytes` exceeded
3. **Access tracking**: Recently accessed entries stay longer

### Integration with LSP

The LSP server automatically uses the cache for:
- **textDocument/publishDiagnostics**: Syntax checking on every edit
- **textDocument/completion**: Context-aware completions
- **textDocument/definition**: Jump to definition
- **textDocument/hover**: Type information
- **textDocument/rename**: Symbol renaming

## Parser Performance

### Baseline Metrics

Without caching, the NexusLang parser is already fast:

| Metric | Value |
|--------|-------|
| Lexer speed | ~30,000 tokens/sec |
| Parser speed | ~7,000 lines/sec |
| Typical file (70 lines) | ~9ms total |
| Lexer time | 75% (6-9ms) |
| Parser time | 25% (1-2ms) |

### LSP Responsiveness

With caching, LSP operations are highly responsive:

| Operation | Time (cached) | Target |
|-----------|--------------|--------|
| Diagnostics on edit | 0.34ms | <50ms ✅ |
| Completion request | <1ms | <100ms ✅ |
| Definition lookup | <1ms | <100ms ✅ |
| Hover info | <1ms | <100ms ✅ |

## Incremental Parsing (Future)

**Status**: Planned for future releases

The current cache system provides excellent performance for LSP operations. Future optimizations may include:

1. **Incremental Lexer**: Tokenize only changed regions
2. **Incremental Parser**: Reparse only affected AST subtrees
3. **Change Tracking**: Track modified line ranges
4. **Scope Preservation**: Reuse symbol tables for unchanged regions

**Theoretical Speedup**: 50-100x for single-line edits
**Current Speedup**: 7.6x (already excellent for LSP)

## Benchmarking

### Profile Parser Performance

```bash
python dev_tools/profile_parser.py
```

Output:
```
File: examples/01_basic_concepts.nlpl
========================================
Size: 71 lines, 1,783 characters

Lexer Performance:
- Tokens: 272
- Time: 8.99ms
- Rate: 30,253 tokens/sec

Parser Performance:
- Time: 1.59ms

Total:
- Time: 10.58ms
- Rate: 6,708 lines/sec
```

### Benchmark Cache Performance

```bash
python dev_tools/benchmark_lsp_cache.py
```

Output:
```
LSP PERFORMANCE BENCHMARK - MULTIPLE FILES
======================================================================

Average cache speedup: 7.6x
Average time saved per cache hit: 2.642ms

Per-file results:
  01_basic_concepts.nlpl            9.6x speedup,   2.958ms saved
  test_json.nlpl                    5.6x speedup,   2.327ms saved
```

### Test Cache Correctness

```bash
python dev_tools/test_ast_cache.py
```

Validates:
- Cache hit/miss logic
- Hash-based invalidation
- LRU eviction
- Memory limits
- Statistics tracking

## Best Practices

### For Library Users

1. **Use CachedParser for LSP**: Always use `CachedParser` or `parse_with_cache()` for Language Server operations
2. **Configure limits appropriately**: Set cache limits based on available memory
3. **Monitor statistics**: Track hit rate to ensure cache is effective
4. **Invalidate on external changes**: Call `cache.invalidate(file_path)` when files change outside LSP

### For NexusLang Core Developers

1. **Keep AST nodes lightweight**: Minimize unnecessary attributes
2. **Avoid circular references**: Prevents memory leaks and improves size estimation
3. **Use hash-based keys**: Don't rely on file modification times
4. **Test with large files**: Ensure cache handles 1000+ line files

## Implementation Details

### Cache Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    CachedParser                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │ parse(file_path, source_code)                      │    │
│  │   1. Compute SHA-256 hash of source                │    │
│  │   2. Check cache.get(file_path, source_code)       │    │
│  │   3. If hit: return cached AST (instant)           │    │
│  │   4. If miss: parse with Lexer + Parser            │    │
│  │   5. Store in cache.put(file_path, source, AST)    │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                     ASTCache                                 │
│  ┌────────────────────────────────────────────────────┐    │
│  │ OrderedDict[file_path, CacheEntry]                 │    │
│  │                                                     │    │
│  │ CacheEntry:                                        │    │
│  │   - ast: Parsed AST                                │    │
│  │   - source_hash: SHA-256 of source                 │    │
│  │   - memory_size: Estimated bytes                   │    │
│  │   - access_count, last_access: LRU tracking        │    │
│  └────────────────────────────────────────────────────┘    │
│                                                              │
│  LRU Eviction: When limits exceeded, evict oldest           │
│  Hash Validation: Invalidate if source_hash differs         │
│  Thread-safe: Uses threading.Lock                           │
└─────────────────────────────────────────────────────────────┘
```

### Hash-based Invalidation

The cache uses SHA-256 hashing for content-based invalidation:

```python
import hashlib

def _compute_hash(source_code: str) -> str:
    return hashlib.sha256(source_code.encode('utf-8')).hexdigest()

# Cache hit only if hashes match
cached_hash = entry.source_hash
current_hash = _compute_hash(source_code)
if cached_hash != current_hash:
    # Content changed - cache miss
    return None
```

**Benefits**:
- Detects any source code change (even single character)
- Independent of file modification times
- Works with in-memory strings (no file system dependency)
- Collision probability: ~0 (SHA-256 is cryptographically secure)

### Memory Estimation

AST memory size is estimated by counting nodes:

```python
def _estimate_size(ast) -> int:
    node_count = _count_nodes(ast)
    return node_count * 200  # 200 bytes per node average

def _count_nodes(node) -> int:
    count = 1  # Current node
    for attr in node.__dict__.values():
        if isinstance(attr, list):
            for item in attr:
                if hasattr(item, '__dict__'):
                    count += _count_nodes(item)
        elif hasattr(attr, '__dict__'):
            count += _count_nodes(attr)
    return count
```

**Accuracy**: Within ~20% of actual memory usage

## Troubleshooting

### Low Cache Hit Rate

**Symptom**: Hit rate < 20% in LSP

**Causes**:
- Files changing frequently (expected during active editing)
- Cache limits too small (check memory/entries limits)
- External file modifications not tracked

**Solutions**:
1. Increase `max_entries` and `max_memory_mb`
2. Check if files are being modified externally
3. Monitor evictions - if high, increase limits

### High Memory Usage

**Symptom**: Cache using excessive memory

**Causes**:
- Limits set too high
- Large files cached
- Memory estimation inaccurate

**Solutions**:
1. Reduce `max_memory_mb` limit
2. Check file sizes with `cache.print_stats()`
3. Clear cache periodically with `cache.clear()`

### Cache Not Working

**Symptom**: No speedup observed

**Causes**:
- Not using `CachedParser` or `parse_with_cache()`
- File path inconsistencies (relative vs absolute)
- Debug mode overhead

**Solutions**:
1. Ensure using cached parser APIs
2. Use absolute paths consistently
3. Disable debug mode in production

## Future Enhancements

### Planned Improvements

1. **Incremental Parsing**: Parse only changed regions (50-100x speedup potential)
2. **Persistent Cache**: Save ASTs to disk for cross-session persistence
3. **Distributed Cache**: Share cache across multiple LSP server instances
4. **Adaptive Limits**: Dynamically adjust based on available system memory
5. **Cache Warming**: Pre-parse project files on startup

### Contributing

Performance improvements welcome! See:
- `src/nexuslang/parser/ast_cache.py` - Cache implementation
- `src/nexuslang/parser/cached_parser.py` - Parser wrapper
- `dev_tools/profile_parser.py` - Profiling tool
- `dev_tools/benchmark_lsp_cache.py` - Benchmark tool

## References

- **AST Cache**: `src/nexuslang/parser/ast_cache.py`
- **Cached Parser**: `src/nexuslang/parser/cached_parser.py`
- **LSP Integration**: `src/nexuslang/lsp/diagnostics.py`
- **Profiling Tool**: `dev_tools/profile_parser.py`
- **Benchmark Tool**: `dev_tools/benchmark_lsp_cache.py`
- **Test Suite**: `dev_tools/test_ast_cache.py`
