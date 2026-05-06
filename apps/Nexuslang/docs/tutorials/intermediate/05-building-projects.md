# Tutorial 10: Building Projects with `nlpl`

**Time:** ~30 minutes  
**Prerequisites:** [Modules and Imports](../beginner/05-modules-and-imports.md)

---

## Part 1 — Project Structure

A NexusLang project is a directory containing a manifest file (`nexuslang.toml`) and
one or more `.nxl` source files.

```
my-project/
  nexuslang.toml   -- project manifest
  src/
    main.nxl       -- entry point
    utils.nxl      -- helper module
  tests/
    test_utils.nxl
```

---

## Part 2 — The `nexuslang.toml` Manifest

```toml
[package]
name    = "my-project"
version = "0.1.0"
authors = ["Your Name <you@example.com>"]
license = "MIT"
edition = "2026"

[build]
source_dir = "src"
output_dir = "build"
target = "c"

[dependencies]
nexuslang-math = "1.0"
```

| Key | Meaning |
|-----|---------|
| `[package]` | Package metadata |
| `[dependencies]` | Runtime library dependencies |
| `[dev-dependencies]` | Test / benchmark dependencies |
| `[build]` | Build output and compiler settings |

---

## Part 3 — Common Build Commands

```bash
# Build the project
nlpl build

# Run the project
nlpl run

# Run with arguments
nlpl run -- input.csv --verbose

# Run tests
nlpl test

# Run tests matching a name fragment
nlpl test utils

# Check for errors without running
nlpl check

# Lint
nlpl lint
```

---

## Part 4 — Build Profiles

Profiles control optimisation and debug output:

```toml
[profile.dev]
opt-level  = 0      # No optimisation; fast rebuilds
debug      = true

[profile.release]
opt-level  = 3      # Full optimisation
lto        = true   # Link-time optimisation
strip      = true   # Strip debug symbols
```

```bash
# Run under dev profile (default)
nlpl run

# Run under release profile
nlpl run --release
```

---

## Part 5 — Dependencies

Add a library to `[dependencies]` and import it in your code:

```toml
[dependencies]
nexuslang-csv = "2.0"
nexuslang-http = "1.3"
```

```nexuslang
import nxl_csv
import nxl_http

set rows to nxl_csv.parse_file with "data.csv"
set response to nxl_http.get with "https://api.example.com"
```

Lock dependencies and refresh the lock file:

```bash
nlpl lock
```

---

## Part 6 — Multiple Binaries

A single project can expose several entry-point source files, but the current
CLI build flow is centered on the project’s configured default target.

```toml
[build]
source_dir = "src"
output_dir = "build"
```

```bash
nlpl build
nlpl run
```

---

## Part 7 — Writing Tests

Test files live in `tests/` and commonly start with `test_`:

```nexuslang
# tests/test_math.nxl
import math

function test_sqrt
    set result to math.sqrt with 9.0
    assert result equals 3.0 with message "sqrt(9) should be 3"
end

function test_abs
    assert math.abs with -5 equals 5 with message "abs(-5) should be 5"
end
```

Run them:

```bash
nlpl test
```

Output:

```
Running tests/test_math.nxl
  test_sqrt ... ok
  test_abs  ... ok
2 tests passed
```

---

## Summary

| Command | Effect |
|---------|--------|
| `nlpl build` | Build the current project |
| `nlpl run` | Build and run the current project |
| `nlpl test` | Run discovered tests |
| `nlpl check` | Type-check without producing output |
| `nlpl lint` | Run the static analyzer |

**You have completed the Intermediate Track!**

**Next steps:**
- [Advanced Track: Inline Assembly](../advanced/01-inline-assembly.md)
- [Cookbook: Common Tasks](../../cookbook/common-tasks.md)
