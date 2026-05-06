# NexusLang Build Tool (`nlpl`)

**Status**: ✅ Complete - Build Tool CLI + Incremental Compilation

---

## Overview

The NexusLang build tool is exposed through the main `nlpl` CLI. It provides project-aware compilation, build orchestration, feature flag management, and profile support through `nexuslang.toml` manifests.

**Key Features**:
- **Manifest-driven builds**: Reads `nexuslang.toml` for project configuration
- **Incremental compilation**: Smart rebuilds only recompile changed files (see [INCREMENTAL_COMPILATION.md](INCREMENTAL_COMPILATION.md))
- **Build profiles**: Support for dev, release, and custom profiles
- **Feature flags**: Enable/disable capabilities at compile time
- **Multiple targets**: Build libraries and multiple binaries
- **Integrated testing**: Run test suites with `nexuslang test`
- **Clean builds**: Remove artifacts with `nexuslang clean`
- **Syntax checking**: Fast error checking without compilation

---

## Installation

The build tool is available through the main `nlpl` command and requires:
- Python 3.8+
- NexusLang runtime/compiler dependencies
- Access to `src/nexuslang/` modules

**Development invocation**:
```bash
PYTHONPATH=src python -m nexuslang.cli --help
```

**Typical installed usage**:
```bash
nlpl --help
```

---

## Commands

### `build` - Compile Project

Build the current project described by `nexuslang.toml`.

**Usage**:
```bash
nlpl build                               # Build with dev profile
nlpl build --release                     # Build with release profile
nlpl build --profile custom              # Build with custom profile
nlpl build --features f1 f2              # Enable specific features
nlpl build --jobs 8                      # Control parallelism
nlpl build --lint --lint-strict          # Run lint as part of the build
nlpl build -O 3 --lto                    # Override optimisation / enable LTO
```

**Options**:
- `--release` - Build with release profile (optimized)
- `--profile NAME` - Build with custom profile
- `--features FEATURE [FEATURE ...]` - Enable named features
- `--jobs N` - Control parallel compilation workers
- `--clean` - Remove cached artefacts before building
- `--optimize-bounds-checks` - Enable compile-time bounds-check elimination
- `--verbose` - Show detailed build information and rebuild reasons
- `--quiet` - Suppress informational output
- `--target TRIPLE` - Cross-compilation target triple
- `-O, --opt-level LEVEL` - Optimisation override: `0`, `1`, `2`, `3`, or `s`
- `--lto` - Enable link-time optimisation
- `--pgo-use FILE` - Use a merged PGO profile
- `--lint`, `--no-lint` - Override `[build].lint_on_build`
- `--lint-strict` - Use strict lint profile during build
- `--lint-errors-only` - Suppress lint warnings
- `--lint-fail-on-warnings` - Promote lint warnings to build-breaking

**Examples**:
```bash
# Development build
nlpl build

# Production-optimized build
nlpl build --release

# Build with specific features
nlpl build --features data-analytics storage-backend

# Build with lint integration enabled
nlpl build --lint --lint-fail-on-warnings
```

**Incremental Compilation**:

By default, `nexuslang build` uses incremental compilation to skip recompiling unchanged files. This dramatically improves build times for large projects.

```bash
# First build - compiles everything
nlpl build --verbose
# Output: "Finished build [profile: dev] (5 compiled, 0 up-to-date)"

# Second build - skips unchanged files
nlpl build --verbose
# Output: "Finished build [profile: dev] (0 compiled, 5 up-to-date)"

# After modifying a file
echo '# Modified' >> src/utils.nxl
nlpl build --verbose
# Output: "Rebuild reason: Dependency utils.nxl changed"
#         "Finished build [profile: dev] (2 compiled, 3 up-to-date)"
```

**Force a clean rebuild**:
```bash
nlpl build --clean
```

See [INCREMENTAL_COMPILATION.md](INCREMENTAL_COMPILATION.md) for detailed documentation on the incremental compilation system.

**Build Artifacts**:
- **Location**: `build/<profile>/`
- **Structure**:
  ```
  build/
    dev/           # Development builds
      binary_name
      binary_name.ll
      binary_name.o
    release/       # Release builds
      binary_name
      ...
    .cache/        # Incremental compilation cache
      build_cache.json
  ```

---

### `clean` - Remove Build Artifacts

Remove all build outputs and intermediate files.

**Usage**:
```bash
nlpl clean
```

**Removes**:
- `build/` directory (all profiles)
- `*.ll` (LLVM IR files in project root)
- `*.o` (Object files in project root)
- `*.bc` (LLVM bitcode files in project root)

---

### `check` - Syntax Check Without Building

Fast syntax checking without compilation. Useful for rapid feedback during development.

**Usage**:
```bash
nlpl check
nlpl check --features f1 f2
```

**Checks**:
- Lexical analysis (tokenization)
- Syntax parsing
- AST generation
- **Does NOT**: Generate code, compile, or link

**Performance**: 10-100x faster than full build for large projects.

---

### `run` - Build and Execute Binary

Build and immediately run a binary target.

**Usage**:
```bash
nlpl run
nlpl run --release
nlpl run --features data-analytics -- input.csv
```

**Options**:
- `--release` - Run release build
- `--features FEATURE [FEATURE ...]` - Enable features
- `-- ARGS` - Arguments to pass to binary (after `--`)

**Examples**:
```bash
# Run default binary (dev profile)
nlpl run

# Run optimized binary
nlpl run --release

# Run with features and arguments
nlpl run --features data-analytics -- input.csv
```

---

### `test` - Run Test Suite

Run all tests in the project.

**Usage**:
```bash
nlpl test
nlpl test --release
nlpl test --features f1 f2
nlpl test parser --jobs 8
```

**Test Discovery**:
- Searches `tests/` directory
- Compiles all `*.nxl` files
- Runs each test binary
- Reports pass/fail status

**Test Output**:
```
Running tests...
Running 5 test(s)...
  Testing test_parser... ok
  Testing test_lexer... ok
  Testing test_runtime... FAILED
  Testing test_stdlib... ok
  Testing test_integration... ok

Test result: 4 passed, 1 failed
```

**Test Conventions**:
- Place tests in `tests/` directory
- Name files `test_*.nxl`
- Exit code 0 = pass, non-zero = fail
- Use `assert` statements or explicit error handling

---

## `nexuslang.toml` Configuration

### Package Metadata

```toml
[package]
name = "my-project"
version = "0.1.0"
authors = ["Developer <dev@example.com>"]
license = "MIT"
description = "Project description"
repository = "https://github.com/user/project"
keywords = ["keyword1", "keyword2"]
```

### Binary Targets

```toml
# Single binary
[[bin]]
name = "my-app"
path = "src/main.nxl"

# Multiple binaries
[[bin]]
name = "processor"
path = "src/bin/processor.nxl"

[[bin]]
name = "analyzer"
path = "src/bin/analyzer.nxl"
required-features = ["data-analytics"]  # Only built if feature enabled
```

### Library Target

```toml
[lib]
name = "my_lib"
path = "src/lib.nxl"
crate-type = ["lib"]  # or ["staticlib", "dylib", "cdylib"]
```

### Build Profiles

```toml
[profile.dev]
opt-level = 0       # No optimization
debug = true        # Include debug symbols
incremental = true  # Faster rebuilds

[profile.release]
opt-level = 3       # Maximum optimization
lto = true          # Link-time optimization
strip = true        # Remove debug symbols
panic = "abort"     # Abort on panic (no unwinding)

[profile.custom]
inherits = "release"
opt-level = 2       # Moderate optimization
```

**Profile Fields**:
- `opt-level`: 0-3 (optimization level)
- `debug`: Include debug information
- `lto`: Link-time optimization (true, false, "thin")
- `strip`: Remove symbols (true, false, "symbols")
- `panic`: Panic strategy ("unwind", "abort")
- `incremental`: Incremental compilation
- `codegen-units`: Parallelization (1-256)

### Feature Flags

**Domain-neutral feature examples**:
```toml
[features]
default = ["text-processing"]
text-processing = []                    # Text/string manipulation
data-analytics = ["nexuslang-math/stats"]    # Statistical analysis
storage-backend = ["dep:nexuslang-database"] # Persistent storage
concurrent-execution = []                # Multi-threading
network-support = ["nexuslang-network"]      # Networking capabilities
serialization = ["json", "binary"]      # Data serialization
compression = []                        # Data compression
encryption = []                         # Cryptographic features
ui-support = []                         # User interface capabilities
scripting = []                          # Embedded scripting
```

**Feature Dependencies**:
- `[]` - No dependencies (just a flag)
- `["other-feature"]` - Depends on another feature
- `["dep:package"]` - Enables optional dependency
- `["package/feature"]` - Enables feature in dependency

**Using Features**:
```bash
# Enable single feature
nlpl build --features data-analytics

# Enable multiple features
nlpl build --features data-analytics storage-backend

# Features are transitive (dependencies enabled automatically)
```

---

## Directory Structure

**Expected Project Layout**:
```
my-project/
   nexuslang.toml            # Project manifest (required)
  src/
      main.nxl                # Default entry point
      utils.nxl               # Additional project sources
  tests/
      test_module1.nxl        # Test files
      test_module2.nxl
  build/                    # Build artifacts (auto-generated)
    dev/
      binary_name
    release/
      binary_name
```

---

## Integration with Compiler

The `nlpl` CLI integrates with the current NexusLang toolchain:

1. **Manifest loading**: Reads `nexuslang.toml` through `ConfigLoader`
2. **Build orchestration**: Delegates to `BuildSystem`
3. **Profiles and features**: Applies CLI overrides for profiles, optimisation, linting, and enabled features

**Development invocation**:
```bash
PYTHONPATH=src python -m nexuslang.cli build
PYTHONPATH=src python -m nexuslang.cli run -- --help
```

---

## Examples

### Example 1: Simple Hello World

**`nexuslang.toml`**:
```toml
[package]
name = "hello"
version = "0.1.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[profile.dev]
opt-level = 0
debug = true

[profile.release]
opt-level = 2
```

**`src/main.nxl`**:
```nexuslang
function main
      print text "Hello from NexusLang!"
end
```

**Build and Run**:
```bash
$ nlpl build
Building hello [profile: dev, features: none]
   Compiling project sources...
Finished build [profile: dev]

$ nlpl run
Hello from NexusLang!
```

---

### Example 2: Data Processing Tool

**`nexuslang.toml`**:
```toml
[package]
name = "data-tool"
version = "1.0.0"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
nexuslang-csv = "1.0"
nexuslang-math = "2.0"

[features]
default = ["text-processing"]
text-processing = []
data-analytics = ["nexuslang-math/statistics"]
storage-backend = []

[profile.release]
opt-level = 3
lto = true
```

**Build Commands**:
```bash
# Build the project
nlpl build

# Build with analytics enabled
nlpl build --features data-analytics

# Production build
nlpl build --release --features data-analytics storage-backend

# Run with features and runtime arguments
nlpl run --features data-analytics -- input.csv
```

---

### Example 3: Library + Binaries

**`nexuslang.toml`**:
```toml
[package]
name = "my-lib"
version = "0.2.0"

[build]
source_dir = "src"
output_dir = "build"

[features]
default = []
performance-optimizations = []

[profile.dev]
opt-level = 0
debug = true

[profile.release]
opt-level = 3
```

**Build Commands**:
```bash
# Build the project
nlpl build

# Run type checking without producing output
nlpl check

# Clean everything
nlpl clean
```

---

## Error Handling

**Common Errors**:

1. **No `nexuslang.toml` found**:
   ```
   error: nexuslang.toml not found. Are you in an NexusLang project root?
   ```
   **Solution**: Create `nexuslang.toml` or run the command from the project root.

2. **Source file not found**:
   ```
   Error: Source file not found: src/main.nxl
   ```
   **Solution**: Create the missing file or fix the source path in `[build]`.

3. **Unknown feature**:
   ```
   Warning: Unknown feature 'invalid-feature'
   ```
   **Solution**: Check feature name spelling in `[features]` section.

4. **Missing feature dependencies**:
   ```
   Warning: feature dependency requirements not satisfied
   ```
   **Solution**: Add `--features data-analytics` to command.

5. **Compilation failed**:
   ```
   Error compiling source.nxl: Parse error: ...
   Build failed
   ```
   **Solution**: Fix syntax errors in source code.

---

## Performance Tips

1. **Use `check` for rapid feedback**: 10-100x faster than full build
   ```bash
   nlpl check
   ```

2. **Incremental builds**: Only recompile changed files
   ```bash
   nlpl build
   ```

3. **Profile-specific builds**: Keep dev and release separate
   ```bash
   nlpl build
   nlpl build --release
   ```

4. **Feature flags**: Disable unused features
   ```toml
   [features]
   default = []  # Minimal by default
   ```

---

## Further Enhancements

Active areas for expansion include:

1. **Richer documentation tooling**:
   - Extract API documentation from project sources
   - Generate publishable reference output

2. **Broader package-management maturity**:
   - Expand registry workflows beyond the current dependency and publish commands
   - Improve lockfile and offline workflows

3. **Additional build orchestration features**:
   - More granular target selection and packaging flows
   - Deeper integration with profiling and release pipelines

---

## Technical Details

### Implementation

**Language**: Python 3.8+  
**Primary implementation**: `src/nexuslang/cli/__init__.py`, `src/nexuslang/tooling/config.py`, `src/nexuslang/tooling/builder.py`  
**Dependencies**:
- `nexuslang.tooling.config` - Manifest parsing
- `nexuslang.tooling.builder` - Build orchestration
- `nexuslang.tooling.workspace` - Workspace operations
- `nexuslang.tooling.dependency_manager` - Dependency add/remove/lock flows

**Architecture**:
```
nlpl CLI
   ├── ConfigLoader.load()    # Manifest parsing
   ├── BuildSystem.build()    # Project builds
   ├── BuildSystem.run()      # Build + execute
   ├── BuildSystem.check()    # Type checking
   ├── BuildSystem.test()     # Test discovery/execution
   ├── cmd_lint()             # Static analysis
   ├── cmd_pgo()              # PGO workflow
   └── workspace/deps commands
```

### Build Context

Each build creates a `BuildContext`:
- Manifest reference
- Build profile (opt level, debug, etc.)
- Enabled features
- Build directory
- Verbose flag

### Feature Resolution

Features are resolved transitively:
```python
requested = ["data-analytics"]
resolved = ["data-analytics", "nexuslang-math/statistics"]  # Includes deps
```

---

## Comparison with Cargo

| Feature | `nlpl` | Cargo |
|---------|--------|-------|
| Manifest format | `nexuslang.toml` | `Cargo.toml` |
| Build profiles | ✅ dev, release, custom | ✅ dev, release, custom |
| Feature flags | ✅ | ✅ |
| Incremental compilation | ✅ | ✅ |
| Test runner | ✅ | ✅ |
| Coverage command | ✅ basic | ✅ advanced |
| Workspace commands | ✅ | ✅ |
| Dependency add/remove/lock | ✅ | ✅ |
| Registry search/publish | ✅ basic | ✅ crates.io |
| Documentation generation | ⏳ partial ecosystem docs | ✅ cargo doc |

---

## See Also

- **Manifest Specification**: `docs/tooling/nexuslang-toml.md`
- **Compiler Guide**: `COMPILER_GUIDE.md`
- **Development Guide**: `docs/contributing/`
- **Build System Architecture**: `docs/contributing/compiler-guide.md`

---

## Status

✅ **Implemented** - Build Tool CLI

**Implemented**:
- ✅ Core commands: build, run, check, clean, test, lint
- ✅ Build profile support (dev, release, custom)
- ✅ Feature flag resolution (transitive dependencies)
- ✅ Incremental compilation
- ✅ Coverage and profiling commands
- ✅ Workspace commands and dependency workflows
- ✅ Integration with the NexusLang build/runtime toolchain
- ✅ Comprehensive error handling
- ✅ Verbose mode for debugging

**Validated**:
- ✅ Build with dev profile
- ✅ Build with release profile
- ✅ Feature flag enabling
- ✅ Clean artifacts
- ✅ Run binaries
- ✅ Check syntax without compilation

**Next Steps**: Continue aligning higher-level package/build documentation with the current CLI surface as the tooling evolves.
