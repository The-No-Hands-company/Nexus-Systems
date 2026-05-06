# `nexuslang.toml` Specification

**Version**: 1.0  
**Date**: February 14, 2026  
**Status**: Draft

## Overview

The `nexuslang.toml` manifest file is the heart of every NexusLang project. It defines project metadata, dependencies, build configuration, and targets.

---

## File Location

- **Project Root**: `nexuslang.toml` must be in the project root directory
- **Workspace Root**: For multi-package workspaces, root `nexuslang.toml` defines package configuration

---

## Basic Structure

```toml
# Project metadata
[package]
name = "my-project"
version = "0.1.0"
authors = ["Your Name <you@example.com>"]
license = "MIT OR Apache-2.0"
description = "A short description of the project"
repository = "https://github.com/user/my-project"
homepage = "https://my-project.io"
documentation = "https://docs.my-project.io"
readme = "README.md"
keywords = ["graphics", "3d", "rendering"]
categories = ["graphics", "game-engines"]
edition = "2026"  # NexusLang language edition

# Dependencies
[dependencies]
nexuslang-std = "1.0"
nexuslang-graphics = { version = "0.5", features = ["opengl", "vulkan"] }
nexuslang-math = { path = "../nexuslang-math" }  # Local dependency

[dev-dependencies]
nexuslang-test-framework = "0.3"
nexuslang-bench = "0.2"

# Build configuration
[build]
target-dir = "target"
incremental = true
parallel = true

# Binary targets
[[bin]]
name = "my-app"
path = "src/main.nxl"

# Library target
[lib]
name = "my-lib"
path = "src/lib.nxl"
crate-type = ["lib", "staticlib", "dylib"]

# Build profiles
[profile.dev]
opt-level = 0
debug = true
debug-assertions = true
overflow-checks = true
lto = false
panic = "unwind"
incremental = true
codegen-units = 256

[profile.release]
opt-level = 3
debug = false
debug-assertions = false
overflow-checks = false
lto = true
panic = "abort"
incremental = false
codegen-units = 16
strip = true

# Platform-specific dependencies
[target.'cfg(unix)'.dependencies]
nexuslang-unix-sockets = "0.2"

[target.'cfg(windows)'.dependencies]
nexuslang-windows-api = "0.4"

# Feature flags
[features]
default = ["std", "alloc"]
std = []
alloc = []
simd = []
opengl = ["nexuslang-graphics/opengl"]
vulkan = ["nexuslang-graphics/vulkan"]

# Workspace configuration (for multi-package projects)
[workspace]
members = [
    "crates/core",
    "crates/graphics",
    "crates/network"
]
exclude = ["examples", "benches"]

# Build scripts
[build-dependencies]
nexuslang-build-utils = "0.1"

# Package metadata
[package.metadata.nexuslang-build]
pre-build = "scripts/pre_build.nxl"
post-build = "scripts/post_build.nxl"

# Documentation configuration
[package.metadata.docs]
all-features = true
rustdoc-args = ["--document-private-items"]
```

---

## Section Details

### 1. `[package]` Section (Required)

Defines core project metadata.

#### Required Fields

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `name` | String | Package name (lowercase, hyphens allowed) | `"nexuslang-graphics"` |
| `version` | String | Semantic version | `"0.1.0"`, `"1.2.3-beta"` |

#### Optional Fields

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `authors` | Array[String] | Authors with optional email | `["Alice <alice@example.com>"]` |
| `license` | String | SPDX license identifier | `"MIT"`, `"Apache-2.0"`, `"MIT OR Apache-2.0"` |
| `description` | String | One-line description | `"Fast 3D graphics library"` |
| `repository` | String | Source repository URL | `"https://github.com/user/repo"` |
| `homepage` | String | Project homepage | `"https://project.io"` |
| `documentation` | String | Documentation URL | `"https://docs.project.io"` |
| `readme` | String | README file path | `"README.md"` |
| `keywords` | Array[String] | Search keywords (max 5) | `["graphics", "3d"]` |
| `categories` | Array[String] | Package categories | `["graphics", "game-engines"]` |
| `edition` | String | NexusLang language edition | `"2026"` |
| `build` | String | Build script path | `"build.nxl"` |

#### Name Constraints

- **Allowed**: lowercase letters, numbers, hyphens (`-`), underscores (`_`)
- **Not allowed**: spaces, uppercase, special characters
- **Reserved**: `nlpl`, `std`, `core`, `alloc`, `test`
- **Examples**:
  - ✅ `my-project`
  - ✅ `nexuslang-graphics`
  - ✅ `fast_parser`
  - ❌ `My Project`
  - ❌ `nlpl` (reserved)

---

### 2. `[dependencies]` Section

Declares runtime dependencies.

#### Dependency Types

**1. Registry Dependency** (from package registry):
```toml
[dependencies]
nexuslang-graphics = "1.0"
nexuslang-math = "0.5.2"
```

**2. Git Dependency**:
```toml
[dependencies]
nexuslang-utils = { git = "https://github.com/user/nexuslang-utils", branch = "main" }
nexuslang-core = { git = "https://github.com/user/nexuslang-core", tag = "v1.2.0" }
nexuslang-experimental = { git = "https://github.com/user/nexuslang-experimental", rev = "abc123" }
```

**3. Path Dependency** (local filesystem):
```toml
[dependencies]
nexuslang-local = { path = "../nexuslang-local" }
my-lib = { path = "./libs/my-lib" }
```

**4. Dependency with Features**:
```toml
[dependencies]
nexuslang-graphics = { version = "1.0", features = ["opengl", "vulkan"] }
nexuslang-network = { version = "0.8", default-features = false, features = ["tls"] }
```

**5. Optional Dependency**:
```toml
[dependencies]
nexuslang-compression = { version = "0.5", optional = true }

[features]
compression = ["nexuslang-compression"]  # Enable with --features compression
```

#### Version Constraints

| Constraint | Meaning | Example Matches |
|------------|---------|-----------------|
| `"1.2.3"` | Exact version | `1.2.3` only |
| `"^1.2.3"` | Compatible with 1.2.3 (SemVer) | `1.2.3`, `1.2.4`, `1.9.0` (not `2.0.0`) |
| `"~1.2.3"` | Reasonably close to 1.2.3 | `1.2.3`, `1.2.4` (not `1.3.0`) |
| `">= 1.2.0"` | At least 1.2.0 | `1.2.0`, `1.3.0`, `2.0.0` |
| `">= 1.2, < 2.0"` | Range | `1.2.0` to `1.99.99` |
| `"*"` | Any version (discouraged) | Any |

**Default**: Caret (`^`) is implied if no operator specified.

**Examples**:
```toml
[dependencies]
exact-version = "=1.2.3"         # Exact
caret-default = "1.2.3"           # Same as ^1.2.3
caret-explicit = "^1.2.3"         # >=1.2.3, <2.0.0
tilde = "~1.2.3"                  # >=1.2.3, <1.3.0
range = ">= 1.2.0, < 1.5.0"      # Between 1.2.0 and 1.5.0
minimum = ">= 1.0"                # At least 1.0
wildcard-minor = "1.2.*"          # 1.2.0 to 1.2.∞
wildcard-major = "1.*"            # 1.0.0 to 1.∞.∞
```

---

### 3. `[dev-dependencies]` Section

Dependencies only needed for development (tests, benchmarks, examples).

```toml
[dev-dependencies]
nexuslang-test = "0.1"
nexuslang-benchmark = "0.2"
nexuslang-mock = "0.3"
```

Not included when package is used as a dependency by others.

---

### 4. `[build-dependencies]` Section

Dependencies for build scripts.

```toml
[build-dependencies]
nexuslang-build-utils = "0.1"
nexuslang-codegen = "0.2"
```

Available in `build.nlpl` script.

---

### 5. `[[bin]]` Section (Multiple Allowed)

Defines executable binary targets.

```toml
[[bin]]
name = "my-app"              # Binary name
path = "src/main.nxl"       # Entry point
required-features = ["cli"]   # Features needed to build
```

**Multiple Binaries**:
```toml
[[bin]]
name = "server"
path = "src/bin/server.nxl"

[[bin]]
name = "client"
path = "src/bin/client.nxl"

[[bin]]
name = "admin-tool"
path = "src/bin/admin.nxl"
```

**Auto-Discovery**: If `src/bin/*.nlpl` exists, automatically creates binary targets.

---

### 6. `[lib]` Section

Defines library target.

```toml
[lib]
name = "my_lib"                              # Library name
path = "src/lib.nxl"                         # Entry point
crate-type = ["lib", "staticlib", "dylib"]   # Output types
```

#### Crate Types

| Type | Description | Output |
|------|-------------|--------|
| `lib` | NexusLang library | `.nlpllib` (NLPL IR) |
| `staticlib` | Static C library | `.a` (Unix), `.lib` (Windows) |
| `dylib` | Dynamic C library | `.so` (Unix), `.dll` (Windows) |
| `cdylib` | C-compatible dynamic lib | `.so`/`.dll` with C ABI |

---

### 7. `[build]` Section

Global build configuration.

```toml
[build]
target-dir = "target"      # Build output directory
incremental = true          # Enable incremental compilation
parallel = true             # Parallel builds (use all cores)
jobs = 8                    # Max parallel jobs (default: CPU cores)
```

---

### 8. `[profile.dev]` and `[profile.release]` Sections

Build profiles for different scenarios.

#### Profile Fields

| Field | Type | Description | Default (dev) | Default (release) |
|-------|------|-------------|---------------|-------------------|
| `opt-level` | Int (0-3) | Optimization level | `0` | `3` |
| `debug` | Bool | Include debug symbols | `true` | `false` |
| `debug-assertions` | Bool | Enable assertions | `true` | `false` |
| `overflow-checks` | Bool | Check arithmetic overflow | `true` | `false` |
| `lto` | Bool/String | Link-time optimization | `false` | `true` or `"thin"` |
| `panic` | String | Panic strategy | `"unwind"` | `"abort"` |
| `incremental` | Bool | Incremental compilation | `true` | `false` |
| `codegen-units` | Int | Parallel codegen units | `256` | `16` |
| `strip` | Bool/String | Strip symbols | `false` | `true` or `"symbols"` |

**Custom Profiles**:
```toml
[profile.profiling]
inherits = "release"
debug = true
strip = false
```

Use with: `nexuslang build --profile profiling`

---

### 9. `[features]` Section

Conditional compilation features.

```toml
[features]
default = ["std"]           # Default features (can be disabled)
std = []                    # Feature with no dependencies
alloc = []
simd = []
experimental = ["dep:nexuslang-experimental"]  # Requires optional dep

# Feature dependencies
full = ["std", "alloc", "simd"]  # Meta-feature

# Feature that enables dependency features
graphics = ["nexuslang-graphics/opengl", "nexuslang-graphics/vulkan"]
```

**Usage**:
```bash
nlpl build --features simd,experimental
nlpl build --no-default-features --features alloc
nlpl build --all-features
```

---

### 10. `[target.'cfg(...)'.dependencies]` Section

Platform-specific dependencies.

```toml
# Unix-only dependencies
[target.'cfg(unix)'.dependencies]
nexuslang-unix-sockets = "0.2"

# Windows-only dependencies
[target.'cfg(windows)'.dependencies]
nexuslang-windows-api = "0.4"

# Architecture-specific
[target.'cfg(target_arch = "x86_64")'.dependencies]
nexuslang-simd-x86 = "0.1"

[target.'cfg(target_arch = "aarch64")'.dependencies]
nexuslang-simd-arm = "0.1"

# Operating system specific
[target.'cfg(target_os = "linux")'.dependencies]
nexuslang-linux-kernel = "0.3"

[target.'cfg(target_os = "macos")'.dependencies]
nexuslang-cocoa = "0.5"
```

#### Available `cfg` Options

| Option | Values | Example |
|--------|--------|---------|
| `target_os` | `linux`, `macos`, `windows`, `freebsd`, etc. | `cfg(target_os = "linux")` |
| `target_arch` | `x86`, `x86_64`, `arm`, `aarch64`, `wasm32` | `cfg(target_arch = "x86_64")` |
| `target_family` | `unix`, `windows` | `cfg(unix)` |
| `target_env` | `gnu`, `msvc`, `musl` | `cfg(target_env = "gnu")` |
| `target_pointer_width` | `32`, `64` | `cfg(target_pointer_width = "64")` |

---

### 11. `[workspace]` Section

For multi-package projects (monorepos).

```toml
[workspace]
members = [
    "crates/core",
    "crates/graphics",
    "crates/network",
    "tools/*"           # Glob patterns supported
]

exclude = [
    "examples",
    "benches",
    "deprecated/*"
]

# Shared dependencies across workspace
[workspace.dependencies]
nexuslang-common = "1.0"
```

**Workspace Members** inherit `[workspace.dependencies]`:
```toml
# In crates/core/nexuslang.toml
[dependencies]
nexuslang-common = { workspace = true }  # Uses workspace version
```

---

### 12. `[package.metadata.*]` Sections

Custom metadata for tools.

```toml
[package.metadata.nexuslang-build]
pre-build = "scripts/generate_code.nxl"
post-build = "scripts/package_assets.nxl"

[package.metadata.docs]
all-features = true
default-target = "x86_64-unknown-linux-gnu"

[package.metadata.cross]
image = "nlpl/cross:x86_64-unknown-linux-gnu"
```

---

## Complete Example

```toml
# nexuslang.toml - Example project manifest

[package]
name = "phoenix-engine"
version = "0.3.1"
authors = ["Phoenix Team <team@phoenix-engine.io>"]
license = "MIT OR Apache-2.0"
description = "Fast 3D game engine written in NLPL"
repository = "https://github.com/phoenix/phoenix-engine"
homepage = "https://phoenix-engine.io"
documentation = "https://docs.phoenix-engine.io"
readme = "README.md"
keywords = ["game", "graphics", "3d", "rendering", "physics"]
categories = ["game-engines", "graphics"]
edition = "2026"

[dependencies]
nexuslang-math = "1.2"
nexuslang-graphics = { version = "0.8", features = ["opengl", "vulkan"] }
nexuslang-physics = "0.5"
nexuslang-audio = "0.3"
nexuslang-input = "0.2"

[dev-dependencies]
nexuslang-test = "0.1"
nexuslang-benchmark = "0.2"

[[bin]]
name = "phoenix-editor"
path = "src/bin/editor.nxl"
required-features = ["editor"]

[[bin]]
name = "phoenix-runtime"
path = "src/bin/runtime.nxl"

[lib]
name = "phoenix_engine"
path = "src/lib.nxl"
crate-type = ["lib", "dylib"]

[features]
default = ["std", "opengl"]
std = []
opengl = ["nexuslang-graphics/opengl"]
vulkan = ["nexuslang-graphics/vulkan"]
editor = ["dep:nexuslang-gui"]
experimental = []

[profile.dev]
opt-level = 0
debug = true
incremental = true

[profile.release]
opt-level = 3
debug = false
lto = true
strip = true

[profile.profiling]
inherits = "release"
debug = true
strip = false

[target.'cfg(windows)'.dependencies]
nexuslang-windows-input = "0.1"

[target.'cfg(unix)'.dependencies]
nexuslang-unix-input = "0.1"

[package.metadata.phoenix]
asset-dir = "assets"
config-dir = "config"
```

---

## Validation Rules

1. **Required Fields**: `[package]` must have `name` and `version`
2. **Name Format**: Lowercase alphanumeric with hyphens/underscores
3. **Version Format**: Must be valid semver (x.y.z)
4. **Paths**: All file paths must be relative to project root
5. **Dependencies**: Must specify version or path or git
6. **Circular Dependencies**: Not allowed (build-time detection)
7. **Feature Names**: Must match `[a-zA-Z0-9_-]+`

---

## Error Messages

### Invalid Name
```
Error: Invalid package name `My Project`
  |
  | name = "My Project"
  |        ^^^^^^^^^^^^
  |
  = Package names must be lowercase with hyphens or underscores
  = Try: "my-project"
```

### Missing Version
```
Error: Missing required field `version` in [package] section
  |
1 | [package]
2 | name = "my-project"
  |
  = Add: version = "0.1.0"
```

### Invalid Version Constraint
```
Error: Invalid version constraint `>1.0`
  |
  | nexuslang-graphics = ">1.0"
  |                 ^^^^^^
  |
  = Valid operators: ^, ~, =, >=, <=, >, <
  = Try: "^1.0" or ">= 1.0"
```

---

## Migration from Other Systems

### From Cargo.toml

Most fields compatible. Changes:
- `crate-type = ["rlib"]` → `crate-type = ["lib"]`
- `[package.metadata.docs.rs]` → `[package.metadata.docs]`

### From package.json

```javascript
// package.json
{
  "name": "my-project",
  "version": "1.0.0",
  "dependencies": {
    "lodash": "^4.17.0"
  }
}
```

Becomes:
```toml
# nexuslang.toml
[package]
name = "my-project"
version = "1.0.0"

[dependencies]
nexuslang-lodash = "^4.17"
```

---

## Future Enhancements

**Version 1.1** (planned):
- Patch dependencies: `[patch.crates-io]`
- Replace dependencies: `[replace]`
- Dependency aliases: `tokio = { package = "tokio-async" }`
- Workspace inheritance: `{ workspace = true, optional = true }`

**Version 2.0** (future):
- Build profiles inheritance chain
- Conditional features: `[features.cfg(unix)]`
- Dynamic dependencies (runtime loading)

---

## References

- **Semantic Versioning**: https://semver.org/
- **TOML Specification**: https://toml.io/en/
- **SPDX License List**: https://spdx.org/licenses/
- **Cargo Manifest**: https://doc.rust-lang.org/cargo/reference/manifest.html (inspiration)

---

**Specification Version**: 1.0  
**Last Updated**: February 14, 2026  
**Status**: Draft - Ready for Implementation
