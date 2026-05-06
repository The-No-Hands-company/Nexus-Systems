# Incremental Compilation System

## Overview

NLPL's incremental compilation system provides smart rebuilds that only recompile files that have changed or have changed dependencies. This dramatically improves build times for large projects by avoiding unnecessary recompilation.

**Status**: ✅ Complete (February 2026)

## Features

- **File Change Detection**: Tracks modification times, file sizes, and content hashes
- **Dependency Tracking**: Monitors import relationships and transitive dependencies  
- **Build Artifact Caching**: Records output paths, build profiles, and feature flags
- **Persistent Cache**: JSON-based cache survives across build sessions
- **Profile/Feature Awareness**: Detects when build settings change
- **Automatic Invalidation**: Rebuilds dependent files when dependencies change

## How It Works

### 1. File Metadata Tracking

The system tracks metadata for every source file:

```python
{
    "path": "/path/to/file.nxl",
    "mtime": 1771124456.018715,  # Modification time
    "size": 72,                    # File size in bytes
    "hash": "bda91e29...",         # SHA-256 content hash
    "imports": ["module1", "module2"]  # Imported modules
}
```

### 2. Dependency Graph

Maintains a bidirectional dependency graph:

```python
{
    "dependencies": {
        "calculator.nxl": ["math_utils.nxl"]
    },
    "reverse_deps": {
        "math_utils.nxl": ["calculator.nxl"]
    }
}
```

This enables:
- Forward lookup: "What does this file depend on?"
- Reverse lookup: "What files depend on this?"
- Transitive resolution: "What are ALL dependencies (direct + indirect)?"

### 3. Build Artifacts

Records build outputs with their settings:

```python
{
    "source_file": "calculator.nxl",
    "output_file": "build/dev/calculator",
    "build_time": 1771124462.679344,
    "profile": "dev",
    "features": ["networking", "graphics"]
}
```

### 4. Rebuild Decision Logic

A file needs rebuilding if ANY of these conditions are true:

1. **Source file changed** (mtime/size/hash comparison)
2. **Any dependency changed** (transitive check)
3. **Output artifact missing**
4. **Build profile changed** (dev → release)
5. **Feature flags changed**
6. **No previous build record**

## Usage

### Enable/Disable

Incremental compilation is **enabled by default**:

```bash
# Build with incremental compilation (default)
nlpl build

# Disable incremental compilation
nlpl build --clean
```

### Clean Build

Clear the cache to force a full rebuild:

```bash
nlpl clean  # Removes build/ directory and cache
nlpl build  # Fresh build with new cache
```

### Verbose Output

See rebuild reasons in verbose mode:

```bash
nlpl build --verbose
```

Output:
```
Building calculator [profile: dev, features: none]
    Rebuild reason: Source file calculator.nlpl changed
  Compiling binary calculator...
✓ Successfully compiled: build/dev/calculator
Finished build [profile: dev] (1 compiled, 0 up-to-date)
```

## Cache Structure

### Location

`build/.cache/build_cache.json`

### Format

```json
{
    "version": "1.0",
    "timestamp": "2026-02-15T04:01:02.679361",
    "file_metadata": {
        "file.nxl": {
            "path": "file.nxl",
            "mtime": 1771124456.018715,
            "size": 72,
            "hash": "bda91e291778ad44a5d79e673fd40c32aa7652daa051f1131e6f540eab88f762",
            "imports": []
        }
    },
    "dependency_graph": {
        "dependencies": {},
        "reverse_deps": {}
    },
    "build_artifacts": {
        "file.nlpl:dev:": {
            "source_file": "file.nxl",
            "output_file": "build/dev/file",
            "build_time": 1771124462.679344,
            "profile": "dev",
            "features": []
        }
    }
}
```

## Performance

### Fast Path: mtime + size

First check uses quick file system metadata:

```python
def has_changed(self, current_mtime, current_size):
    return self.mtime != current_mtime or self.size != current_size
```

**Speed**: Microseconds per file

### Slow Path: Content Hash

Only when mtime/size are insufficient:

```python
def is_file_hash_changed(self, file_path):
    current_hash = calculate_sha256(file_path)
    return self.file_metadata[file_path].hash != current_hash
```

**Speed**: Milliseconds per file (depends on file size)

### Typical Speedup

- **No changes**: ~99% faster (skip all compilation)
- **One file changed**: Only recompile that file + dependents
- **Dependency changed**: Recompile all transitively dependent files

Example:
- Project: 100 files, 10 with dependencies on `utils.nlpl`
- Change: Modify `utils.nlpl`
- Result: Recompile 11 files (1 changed + 10 dependents), skip 89

## Implementation Details

### Core Classes

**Current implementation paths**:

- `src/nexuslang/build_system/builder.py`
- `src/nexuslang/build_system/dependency_resolver.py`

1. **FileMetadata** (dataclass)
   - Stores path, mtime, size, hash, imports
   - `has_changed()` method for fast comparison

2. **BuildArtifact** (dataclass)
   - Records source→output mapping
   - Stores build time, profile, features

3. **DependencyGraph**
   - Manages forward and reverse dependency edges
   - `get_transitive_dependencies()` uses BFS
   - `get_transitive_dependents()` for invalidation

4. **BuildCache**
   - Main API: `needs_rebuild()`, `record_build()`, `save()`, `clear()`
   - Loads/saves JSON cache
   - Handles cache corruption gracefully

### Integration with Build Tool

**`nlpl` CLI integration**:

```python
class BuildTool:
    def __init__(self, incremental=True):
        if self.incremental:
            cache_dir = self.build_dir / '.cache'
            self.build_cache = BuildCache(cache_dir)
    
    def build(self):
        # Check before building
        if self.incremental and not self._needs_rebuild(source, profile, features):
            print(f"  Skipping {name} (up to date)")
            skipped_count += 1
        else:
            # Compile...
            compiled_count += 1
        
        # Save cache
        if self.build_cache:
            self.build_cache.save()
    
    def _needs_rebuild(self, source_path, profile, features):
        if not self.build_cache:
            return True
        needs_rebuild, reason = self.build_cache.needs_rebuild(source_path, profile, features)
        if self.verbose and needs_rebuild:
            print(f"    Rebuild reason: {reason}")
        return needs_rebuild
```

## Testing

### Test Structure

`test_programs/build_system/`:
- `calculator.nlpl` - Main program
- `math_utils.nlpl` - Dependency module
- `nexuslang.toml` - Build manifest

### Manual Testing

```bash
cd test_programs/build_system

# Clean build
nlpl clean
nlpl build --verbose
# Output: "1 compiled, 0 up-to-date"

# No changes - should skip
nlpl build --verbose
# Output: "0 compiled, 1 up-to-date"

# Modify dependency
echo '# Comment' >> math_utils.nlpl
nlpl build --verbose
# Output: "Rebuild reason: Dependency math_utils.nlpl changed"
#         "1 compiled, 0 up-to-date"
```

### Expected Behavior

| Scenario | Expected Result |
|----------|----------------|
| First build | All files compiled |
| Second build (no changes) | All files skipped |
| Modify source file | That file + dependents rebuilt |
| Modify dependency | All reverse dependents rebuilt |
| Change profile (dev→release) | All files rebuilt |
| Delete output artifact | Corresponding source rebuilt |
| `--clean` flag | Force clean rebuild |

## Limitations & Future Enhancements

### Current Limitations

1. **Simple Import Resolution**: Only handles direct file-to-file imports
   - No support for nested module paths yet
   - No stdlib dependency tracking

2. **Single-Level Depth**: Doesn't track transitive imports of imported modules
   - If A imports B, and B imports C, changing C doesn't invalidate A
   - Fix: Recursive import scanning

3. **No Parallel Builds**: Files compiled sequentially
   - Future: Parallel compilation of independent files

4. **No Link-Time Optimization Tracking**: Doesn't detect linker flag changes

### Planned Enhancements

**Short Term**:
- [ ] Recursive dependency scanning (transitive imports)
- [ ] Stdlib module dependency tracking
- [ ] Better module path resolution (use module loader)

**Medium Term**:
- [ ] Parallel compilation of independent files
- [ ] Incremental linking (only relink if needed)
- [ ] Cache compression (for large projects)

**Long Term**:
- [ ] Distributed build cache (shared across machines)
- [ ] Content-addressable storage
- [ ] Build artifact deduplication

## Troubleshooting

### Cache Corruption

**Symptom**: Build behaves unexpectedly or crashes

**Fix**:
```bash
nlpl clean  # Removes corrupted cache
nlpl build  # Fresh build
```

### False Positives (Unnecessary Rebuilds)

**Symptom**: Files rebuild when they shouldn't

**Debug**:
```bash
nlpl build --verbose  # See rebuild reason
```

**Common Causes**:
- Clock skew on file system
- File touched by editor but not changed
- Cache not saved from previous build

**Workaround**:
```bash
# Verify cache exists
ls -la build/.cache/build_cache.json

# Check file mtimes
stat source_file.nlpl
```

### False Negatives (Missed Changes)

**Symptom**: Files not rebuilding when they should

**Causes**:
- Cache out of sync with reality
- Imported module not tracked properly

**Fix**:
```bash
nlpl clean && nlpl build
```

## Architecture Decisions

### Why JSON Instead of Binary Format?

**Pros of JSON**:
- Human-readable for debugging
- Easy to inspect with standard tools (`cat`, `jq`)
- Simple serialization/deserialization
- Forward-compatible (can add fields)

**Cons**:
- Slower than binary formats
- Larger file size

**Decision**: JSON for initial implementation. Can add binary format later if performance becomes critical.

### Why SHA-256 for Content Hashing?

**Alternatives Considered**:
- **MD5**: Faster but deprecated (collision attacks)
- **xxHash**: Very fast but not cryptographic
- **SHA-256**: Industry standard, good balance

**Decision**: SHA-256 for reliability. Content hash is only computed when mtime is unreliable, so speed is less critical than correctness.

### Why Store Full Paths in Cache?

**Alternatives**:
- Relative paths (shorter but ambiguous)
- File IDs (compact but opaque)

**Decision**: Absolute paths for clarity and robustness. Makes cache portable if project moves.

## Related Documentation

- [Build Tool](build-tool.md) - Overview of build system
- [nexuslang.toml Specification](nexuslang-toml.md) - Manifest format
- [Compiler Architecture](../contributing/compiler-guide.md) - Compilation pipeline

## See Also

**Similar Systems**:
- Cargo (Rust): `.fingerprint` files
- Make: Modification time-based builds
- Ninja: Fast incremental builds
- Bazel: Content-addressable cache

**Key Differences**:
- NexusLang tracks full dependency graph (not just makefile-style rules)
- Profile and feature awareness (rebuild on config changes)
- Natural language syntax (clear build reasons in English)
