# Build System Test Programs

This directory contains test programs for validating the NexusLang build system's incremental compilation capabilities.

## Contents

### Test Programs

**calculator.nlpl**
- Main program that imports `math_utils` module
- Demonstrates dependency tracking
- Used to test that modifying dependencies triggers rebuilds

**math_utils.nlpl**
- Utility module with math functions
- Used as a dependency of calculator.nlpl
- Tests dependency change detection

### Configuration

**nlpl.toml**
- Manifest for the test project
- Defines binary target
- Uses dev profile with debug symbols

## Testing Incremental Compilation

### Initial Build
```bash
python3 ../../dev_tools/nxl_build.py clean
python3 ../../dev_tools/nxl_build.py --verbose build
```
Expected output: "1 compiled, 0 up-to-date"

### Rebuild Without Changes
```bash
python3 ../../dev_tools/nxl_build.py --verbose build
```
Expected output: "0 compiled, 1 up-to-date"

### Modify Source File
```bash
echo '# Comment' >> calculator.nlpl
python3 ../../dev_tools/nxl_build.py --verbose build
```
Expected output: "Rebuild reason: Source file calculator.nlpl changed"

### Modify Dependency
```bash
echo '# Comment' >> math_utils.nlpl
python3 ../../dev_tools/nxl_build.py --verbose build
```
Expected output: "Rebuild reason: Dependency math_utils.nlpl changed"

### Force Full Rebuild
```bash
python3 ../../dev_tools/nxl_build.py --no-incremental build
```
Expected: Always rebuilds regardless of cache

## Build Artifacts

After building, the following structure is created:

```
build/
  dev/
    calculator           # Executable binary
    calculator.ll        # LLVM IR
    calculator.o         # Object file
  .cache/
    build_cache.json     # Incremental compilation cache
```

## Cache Inspection

View the build cache:
```bash
cat build/.cache/build_cache.json | python3 -m json.tool
```

The cache contains:
- **file_metadata**: Modification times, sizes, hashes, imports
- **dependency_graph**: Forward and reverse dependencies
- **build_artifacts**: Source→output mappings with profiles

## Expected Cache Structure

```json
{
    "version": "1.0",
    "timestamp": "2026-02-15T04:01:02.679361",
    "file_metadata": {
        "/path/to/calculator.nxl": {
            "path": "/path/to/calculator.nxl",
            "mtime": 1771124456.018715,
            "size": 128,
            "hash": "25339fcf910...",
            "imports": ["/path/to/math_utils.nxl"]
        },
        "/path/to/math_utils.nxl": {
            "path": "/path/to/math_utils.nxl",
            "mtime": 1771124456.018715,
            "size": 200,
            "hash": "bda91e2917...",
            "imports": []
        }
    },
    "dependency_graph": {
        "dependencies": {
            "/path/to/calculator.nxl": ["/path/to/math_utils.nxl"]
        },
        "reverse_deps": {
            "/path/to/math_utils.nxl": ["/path/to/calculator.nxl"]
        }
    },
    "build_artifacts": {
        "/path/to/calculator.nlpl:dev:": {
            "source_file": "/path/to/calculator.nxl",
            "output_file": "/path/to/build/dev/calculator",
            "build_time": 1771124462.679344,
            "profile": "dev",
            "features": []
        }
    }
}
```

## Cleanup

Remove all build artifacts:
```bash
python3 ../../dev_tools/nxl_build.py clean
```

This removes:
- `build/` directory
- All compiled binaries
- LLVM IR files
- Object files
- Build cache

## See Also

- [BUILD_TOOL_GUIDE.md](../../docs/build_system/BUILD_TOOL_GUIDE.md) - Complete build tool documentation
- [INCREMENTAL_COMPILATION.md](../../docs/build_system/INCREMENTAL_COMPILATION.md) - Incremental compilation architecture
- [NLPL_TOML_SPECIFICATION.md](../../docs/build_system/NLPL_TOML_SPECIFICATION.md) - Manifest format reference
