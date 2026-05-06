# Persistent AST Cache Prototype Plan (Draft)

## Goals

- Optional cross-session AST cache to reduce cold-start latency for LSP/CLI.
- Safe invalidation via content hash and mtime; never serve stale AST.
- Minimal complexity; opt-in flag; easy rollback.

## Approach

- **Storage format**: gzipped JSON or pickle of cache entries; include source hash, mtime, memory size, timestamps.
- **Location**: per-user cache dir (e.g., `~/.nlpl/cache/ast/`); allow override via env/flag.
- **Keying**: absolute path + SHA-256 of content; mtime recorded to short-circuit hash when unchanged.
- **Lifecycle**:
  - On shutdown: serialize current in-memory cache (bounded by max_entries/memory).
  - On startup: load metadata, validate hash/mtime, hydrate entries lazily (load-on-demand) to limit startup cost.
- **Limits**: reuse existing max_entries/max_memory; add disk quota (e.g., 100MB default) with LRU eviction on write.
- **Opt-in controls**: CLI/LSP flag `--persistent-cache`; env `NLPL_PERSISTENT_CACHE=1`.
- **Safety**: if deserialization fails, discard file; never block startup >100ms (enforce timeout, async load optional later).

## Minimal Data Schema

```json
{
  "version": 1,
  "entries": [
    {
      "file_path": "/abs/path/file.nxl",
      "source_hash": "sha256...",
      "mtime": 1234567890.0,
      "memory_size": 12345,
      "ast": "<serialized>"
    }
  ]
}
```

## Integration Steps

1) Add persistence module: read/write cache file, LRU eviction by disk quota.
2) Hook into `get_global_cache` to load metadata on first use when enabled.
3) On put/evict, mark dirty; on shutdown/atexit, flush to disk (best-effort).
4) Validation: on load, compare mtime and hash to current file before reuse; else skip.
5) CLI/LSP config: wire flags/env to enable; default off.

## Benchmarks

- Measure cold-start parse of 5 representative files with and without persistent cache.
- Target: cold-start saved >50% on repeated sessions when files unchanged.

## Tests

- Unit: serialize/deserialize round-trip; hash/mtime invalidation; disk quota eviction.
- Integration: start server, enable persistent cache, restart, verify cache hits on second run.
- Failure cases: corrupted cache file should be ignored without crash.

## Risks / Mitigations

- **Stale AST**: mitigate with hash+mtime validation.
- **Large disk use**: enforce quota + LRU.
- **Startup delay**: lazy load entries; cap load time.

## Rollback Plan

- Keep feature behind flag; default off.
- If issues found, disable flag or delete cache directory.
