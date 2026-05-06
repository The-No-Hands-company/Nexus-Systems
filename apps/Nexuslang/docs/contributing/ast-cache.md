# AST Cache Quick Reference

## Quick Start

```python
from nexuslang.parser.cached_parser import parse_with_cache

# Parse with automatic caching
ast = parse_with_cache('file.nxl', source_code)

# Second call with same content - instant (cache hit)
ast = parse_with_cache('file.nxl', source_code)  # ~10x faster
```

## Common Use Cases

### 1. LSP Server (Automatic)

Already integrated in `src/nexuslang/lsp/diagnostics.py`. No changes needed.

### 2. Custom Parser

```python
from nexuslang.parser.cached_parser import CachedParser

parser = CachedParser()
ast = parser.parse('file.nxl', source_code)
```

### 3. Configure Cache Limits

```python
from nexuslang.parser.ast_cache import set_cache_limits

# Before first use
set_cache_limits(max_entries=200, max_memory_mb=100)
```

### 4. Check Cache Statistics

```python
from nexuslang.parser.ast_cache import get_global_cache

cache = get_global_cache()
cache.print_stats()
```

Output:
```
AST Cache Statistics:
  Hits: 8
  Misses: 10
  Hit rate: 44.4%
  Entries: 5/100
  Evictions: 0
  Memory: 0.35/50.00MB (0.7%)
```

### 5. Manually Invalidate Cache

```python
cache.invalidate('file.nxl')  # Force reparse next time
```

### 6. Clear All Cache

```python
cache.clear()  # Remove all cached ASTs
```

## Performance

- **Speedup**: 7.6x-9.6x for cache hits
- **Memory**: 0.02-0.09MB per typical file
- **LSP latency**: 0.34ms (cached) vs 3.3ms (uncached)

## Testing

```bash
# Test cache correctness
python dev_tools/test_ast_cache.py

# Benchmark performance
python dev_tools/benchmark_lsp_cache.py

# Profile parser
python dev_tools/profile_parser.py
```

## Documentation

Full documentation: `docs/contributing/performance-optimizations.md`
