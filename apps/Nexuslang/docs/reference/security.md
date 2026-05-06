# NexusLang Security Guide

This document describes the security architecture of the NexusLang runtime, the hardening
features available in version 2.0+ of the security module (`nlpl.security`), and
practical guidance for writing secure NexusLang programs.

For the threat model see [threat-model.md](threat-model.md).
For the vulnerability reporting process see [cve-process.md](cve-process.md).

---

## Contents

1. [Security Model Overview](#1-security-model-overview)
2. [Permission System](#2-permission-system)
3. [Static Analysis Tools](#3-static-analysis-tools)
   - [Taint Analysis](#31-taint-analysis)
   - [Control Flow Integrity](#32-control-flow-integrity)
   - [Memory Safety Validation](#33-memory-safety-validation)
4. [Sandboxing](#4-sandboxing)
   - [Restricted Mode](#41-restricted-mode)
   - [Resource Limits](#42-resource-limits)
   - [Seccomp Filter (Linux)](#43-seccomp-filter-linux)
   - [Sandbox Facade](#44-sandbox-facade)
5. [Runtime Protections](#5-runtime-protections)
   - [Stack Canaries](#51-stack-canaries)
   - [Bounds Checking](#52-bounds-checking)
   - [Integer Overflow Detection](#53-integer-overflow-detection)
   - [ASLR Awareness](#54-aslr-awareness)
6. [Path and Input Validation Utilities](#6-path-and-input-validation-utilities)
7. [Security Checklist](#7-security-checklist)
8. [Configuration Reference](#8-configuration-reference)

---

## 1. Security Model Overview

NLPL uses a **deny-by-default** security model inspired by Deno.  Every
potentially dangerous capability is off unless the program explicitly requests
it at startup via command-line flags or the runtime API.

Dangerous capabilities include:

| Category | What it controls |
|----------|-----------------|
| `READ`   | File-system read access |
| `WRITE`  | File-system write access |
| `NET`    | Network socket access |
| `RUN`    | Subprocess spawning |
| `FFI`    | Calls to C/foreign libraries |
| `ASM`    | Inline assembly execution |
| `ENV`    | `os.environ` / environment variable access |
| `IMPORT` | Dynamic module imports |

Enabling all capabilities (`--allow-all`) is intended for development only.
Production deployments should grant only the minimum set needed.

### Layers of Protection

```
NLPL source program
     |
     | (1) Permission checks (deny-by-default)
     v
  Interpreter
     |
     | (2) Taint analysis (track untrusted data)
     | (3) CFI checks (validate call targets)
     | (4) Memory safety (bounds + use-after-free)
     v
  Runtime execution
     |
     | (5) Sandbox (RestrictedMode + ResourceLimits + Seccomp)
     | (6) Stack canaries (detect frame corruption)
     | (7) Bounds checker (enforce index safety)
     v
  OS / Hardware
```

Each layer is independent and can be enabled or disabled individually.

---

## 2. Permission System

### Command-line Flags

```bash
PYTHONPATH=src python -m nexuslang.main program.nlpl --allow-read=/data --allow-net=api.example.com
```

| Flag | Effect |
|------|--------|
| `--allow-read[=<paths>]` | Grant file read; optionally restrict to `<paths>` |
| `--allow-write[=<paths>]` | Grant file write; optionally restrict to `<paths>` |
| `--allow-net[=<hosts>]`  | Grant network; optionally restrict to `<hosts>` |
| `--allow-run[=<progs>]`  | Grant subprocess; optionally restrict to `<progs>` |
| `--allow-ffi[=<libs>]`   | Grant FFI; optionally restrict to `<libs>` |
| `--allow-asm`            | Grant inline assembly |
| `--allow-env[=<vars>]`   | Grant env access; optionally restrict to `<vars>` |
| `--allow-import[=<urls>]`| Grant dynamic import |
| `--allow-all`            | Grant everything (development only) |
| `--prompt`               | Interactive prompt on each new capability request |

### Python API

```python
from nexuslang.security import PermissionManager, PermissionType, set_permission_manager

mgr = PermissionManager()
mgr.grant(PermissionType.READ, scope=["/data/input"])
mgr.grant(PermissionType.NET, scope=["api.example.com"])
set_permission_manager(mgr)
```

### Checking Permissions

```python
from nexuslang.security import get_permission_manager, PermissionType, PermissionDeniedError

mgr = get_permission_manager()

# Raises PermissionDeniedError if denied
mgr.check(PermissionType.WRITE, resource="/tmp/output.txt")

# Non-raising version
if mgr.has_permission(PermissionType.FFI, resource="libcustom.so"):
    load_library("libcustom.so")
```

---

## 3. Static Analysis Tools

### 3.1 Taint Analysis

Taint analysis tracks values flowing from untrusted sources to dangerous
sinks.  When a tainted value reaches a sink, a `TaintViolation` is raised
(or logged, depending on policy).

**Sources** (values that become tainted):

| Source | Label |
|--------|-------|
| User input (stdin, argv, prompts) | `TaintLabel.USER_INPUT` |
| Network received data | `TaintLabel.NETWORK` |
| FFI function return values | `TaintLabel.FFI_RETURN` |
| Environment variable reads | `TaintLabel.ENV_VAR` |
| File reads | `TaintLabel.FILE_READ` |

**Sinks** (dangerous destinations):

| Sink | `TaintSink` constant |
|------|---------------------|
| Shell command execution | `SHELL_EXEC` |
| File write | `FILE_WRITE` |
| Format strings | `FORMAT_STRING` |
| SQL queries | `SQL_QUERY` |
| Dynamic imports | `DYNAMIC_IMPORT` |
| FFI call arguments | `FFI_CALL_ARG` |
| eval / exec | `EVAL` |
| Network send | `NETWORK_SEND` |

**Usage:**

```python
from nexuslang.security.analysis import TaintTracker, TaintLabel, TaintSink

tracker = TaintTracker()

# Mark user input as tainted
raw = input("Enter value: ")
user_val = tracker.taint(raw, TaintLabel.USER_INPUT, source="stdin")

# Propagate taint through computations
derived = tracker.propagate(user_val.value + "_suffix", user_val)

# Check before reaching a dangerous sink
tracker.check_sink(derived, TaintSink.SHELL_EXEC, location="line 42")
```

**Violation policies:**

```python
from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy, set_analysis_policy

set_analysis_policy(AnalysisPolicy(
    taint_policy=ViolationPolicy.RAISE,   # raise TaintViolation
    cfi_policy=ViolationPolicy.WARN,      # print warning
    memory_policy=ViolationPolicy.LOG,    # record silently
))
```

### 3.2 Control Flow Integrity

CFI ensures that function calls only target registered, expected callables.

```python
from nexuslang.security.analysis import CFIChecker

checker = CFIChecker()

# Register valid callables at definition time
def my_handler(): ...
checker.call_graph.register_callable(my_handler, "my_handler")

# Restrict a call site to specific targets
checker.call_graph.register_call_site(
    "on_event",
    valid_targets=[my_handler],
    location="events.nlpl:10"
)

# Verify at dispatch time
checker.check_call(callee, site_name="on_event", location="main.nlpl:55")

# Frame-level return-address validation
frame_id = checker.enter_frame("process_request")
# ... execute function body ...
checker.exit_frame(frame_id, "process_request")
```

### 3.3 Memory Safety Validation

```python
from nexuslang.security.analysis import MemorySafetyValidator

v = MemorySafetyValidator()

# Bounds checking
v.check_bounds(index=3, size=len(my_list), location="loop.nlpl:7")

# Use-after-free detection
v.record_free(address=ptr.address)
# ... later ...
v.check_not_freed(address=ptr.address, location="deref.nlpl:22")

# Re-allocation clears the freed record
v.record_alloc(address=ptr.address)
```

### Combined Facade

```python
from nexuslang.security.analysis import SecurityAnalyser, TaintLabel, TaintSink

sa = SecurityAnalyser()

# Taint tracking
tv = sa.taint.taint(user_data, TaintLabel.USER_INPUT, "form_field")
sa.taint.check_sink(tv, TaintSink.SQL_QUERY)

# CFI
sa.cfi.call_graph.register_callable(handler, "handler")
sa.cfi.check_call(handler)

# Memory
sa.memory.check_bounds(idx, size)

# Report all violations
sa.report()
```

---

## 4. Sandboxing

### 4.1 Restricted Mode

`RestrictedMode` patches the global `PermissionManager` to enforce a
`SandboxPolicy`.  It works on all platforms.

```python
from nexuslang.security.sandbox import RestrictedMode, SandboxPolicy

policy = SandboxPolicy(
    allow_ffi=False,
    allow_asm=False,
    allow_network=False,
    allow_file_write=False,
)

with RestrictedMode(policy):
    run_untrusted_program()
```

**FFIManager integration** — call these in `load_library()` and
`call_function()`:

```python
rm = RestrictedMode(policy)
rm.enter()

rm.check_ffi("libcustom.so")  # raises PermissionDeniedError if denied
rm.check_asm()                  # raises PermissionDeniedError if denied
```

### 4.2 Resource Limits

`ResourceLimits` wraps POSIX `resource.setrlimit()`.  No-op on Windows.

```python
from nexuslang.security.sandbox import ResourceLimits, SandboxPolicy

policy = SandboxPolicy(
    max_memory_mb=256,
    max_cpu_seconds=10.0,
    max_open_files=64,
    max_processes=4,
)

with ResourceLimits(policy):
    run_resource_limited_task()
```

Limits are restored on context exit (best-effort — lowering the hard limit
is irreversible).

### 4.3 Seccomp Filter (Linux)

`SeccompFilter` installs a BPF program via `prctl(PR_SET_SECCOMP,
SECCOMP_MODE_FILTER)`.

**Requirements:**
- Linux >= 3.5
- x86-64 architecture (default safe syscall list is x86-64)
- `PR_SET_NO_NEW_PRIVS` is set automatically before the filter

**Warnings:**
- Seccomp filters are permanent and inherited by child processes.
- Use only in disposable forked processes.
- The default safe list is tuned for CPython 3.x; extend `allowed_syscalls`
  if custom C extensions need additional syscalls.

```python
from nexuslang.security.sandbox import SeccompFilter, SandboxPolicy

policy = SandboxPolicy(
    enable_seccomp=True,
    allowed_syscalls=frozenset({"io_uring_enter"}),  # optional extras
)

sf = SeccompFilter(policy)
if sf.available:
    sf.install()
```

### 4.4 Sandbox Facade

`Sandbox` combines all three layers:

```python
from nexuslang.security.sandbox import Sandbox, SandboxPolicy, STRICT_POLICY

# Use the built-in strict policy
with Sandbox(STRICT_POLICY):
    run_untrusted_nxl()

# Or a custom policy
policy = SandboxPolicy(
    allow_ffi=False,
    max_memory_mb=128,
    max_cpu_seconds=5.0,
    enable_seccomp=False,
)
with Sandbox(policy) as sb:
    assert sb.active
    run_program()
```

**Predefined policies:**

| Policy constant | Description |
|----------------|-------------|
| `STRICT_POLICY` | No FFI/ASM/NET/WRITE, 256 MiB, 10 s CPU, 64 FDs |
| `DEVELOPMENT_POLICY` | All capabilities allowed; no resource limits |

---

## 5. Runtime Protections

### 5.1 Stack Canaries

Stack canaries detect in-process frame corruption caused by misbehaving
C extensions accessed via FFI.

```python
from nexuslang.security.runtime_protections import StackCanary, StackSmashingDetected

sc = StackCanary(enabled=True)

# At function entry
frame_id = sc.enter_frame("process_data")

# At function exit — compare stored canary against current value
canary_val = sc.get_canary(frame_id)
sc.exit_frame(frame_id, "process_data", current_canary=canary_val)
```

Integration in the interpreter:

```python
# Interpreter._call_function():
frame_id = self.rt_protector.canary.enter_frame(func.name)
try:
    result = self._execute_body(func.body)
finally:
    canary = self.rt_protector.canary.get_canary(frame_id)
    self.rt_protector.canary.exit_frame(frame_id, func.name, canary)
return result
```

### 5.2 Bounds Checking

```python
from nexuslang.security.runtime_protections import BoundsChecker, BoundsCheckError

bc = BoundsChecker(enabled=True)

# Before any subscript access:
bc.check(index=i, size=len(collection), location="file.nlpl:30")

# Slice validation:
bc.check_slice(start=2, stop=7, size=len(data))
```

### 5.3 Integer Overflow Detection

```python
from nexuslang.security.runtime_protections import IntegerOverflowChecker

checker = IntegerOverflowChecker(enabled=True, threshold=(1 << 63) - 1)

# After any arithmetic used as a buffer size or offset:
result = width * height * channels
checker.check("width * height * channels", result)

# Validate bit-shift amounts:
checker.check_shift(shift_amount)
```

### 5.4 ASLR Awareness

```python
from nexuslang.security.runtime_protections import check_and_warn_aslr

# Call at interpreter startup to log a warning if ASLR is disabled:
check_and_warn_aslr()
```

### RuntimeProtector Facade

```python
from nexuslang.security.runtime_protections import RuntimeProtector, RuntimeProtectorConfig

config = RuntimeProtectorConfig(
    enable_canaries=True,
    enable_bounds=True,
    enable_overflow=True,
    overflow_threshold=(1 << 32),
)
rp = RuntimeProtector(config)
rp.startup_checks()  # warns if ASLR disabled
```

---

## 6. Path and Input Validation Utilities

```python
from nexuslang.security import (
    validate_path, PathTraversalError,
    safe_execute, CommandInjectionError,
    validate_email, validate_url,
    sanitize_sql_identifier, ValidationError,
    escape_html,
    check_rate_limit,
)

# Path traversal prevention
safe = validate_path(user_path, allowed_dirs=["/data/uploads"])

# Safe subprocess (shell=False always)
result = safe_execute("convert", ["input.jpg", "output.png"],
                      allowed_programs=["convert"])

# Input validation
assert validate_email("user@example.com")
assert validate_url("https://api.example.com", allowed_schemes=["https"])
identifier = sanitize_sql_identifier(column_name)

# Output sanitization
html_safe = escape_html(user_content)

# Rate limiting
if not check_rate_limit(client_ip, max_calls=100, window_seconds=60):
    raise TooManyRequestsError
```

---

## 7. Security Checklist

Use this checklist before deploying a NexusLang program in production:

### Permissions
- [ ] Only grant permissions actually needed (`--allow-read`, not `--allow-all`)
- [ ] Scope permissions to specific paths/hosts where possible
- [ ] Never use `--allow-all` in production

### Input Handling
- [ ] Validate all external input before use (`validate_path`, `validate_email`, etc.)
- [ ] Sanitize output inserted into HTML (`escape_html`)
- [ ] Sanitize SQL identifiers (`sanitize_sql_identifier`)
- [ ] Reject or escape shell arguments; prefer `safe_execute` over subprocess

### Taint Analysis
- [ ] Mark user input, network data, FFI returns, and env var reads as tainted
- [ ] Check tainted values before they reach sinks (shell, file write, SQL)
- [ ] Run with `ViolationPolicy.RAISE` in CI/testing, `WARN` or `LOG` in production

### Sandboxing
- [ ] Run untrusted code in a `Sandbox` with `STRICT_POLICY` or tighter
- [ ] Enable `ResourceLimits` to cap CPU and memory for untrusted workloads
- [ ] Enable `SeccompFilter` in forked child processes for maximum isolation (Linux)

### Runtime Protections
- [ ] Enable `StackCanary` when loading C extensions via FFI
- [ ] Enable `BoundsChecker` during testing; optionally in production for safety
- [ ] Confirm ASLR is enabled on production hosts (`randomize_va_space=2`)

### Filesystem
- [ ] Validate all file paths with `validate_path` before opening
- [ ] Sanitize user-supplied filenames with `get_safe_filename`
- [ ] Constrain file operations to an allowed directory list

### Dependencies
- [ ] Audit all FFI libraries for known CVEs
- [ ] Pin FFI library versions
- [ ] Do not expose raw FFI handles to untrusted NexusLang code

---

## 8. Configuration Reference

### `AnalysisPolicy`

```python
from nexuslang.security.analysis import AnalysisPolicy, ViolationPolicy

AnalysisPolicy(
    taint_policy=ViolationPolicy.RAISE,   # RAISE | WARN | LOG | IGNORE
    cfi_policy=ViolationPolicy.RAISE,
    memory_policy=ViolationPolicy.RAISE,
    always_log=False,                     # True: always populate violation log
)
```

### `SandboxPolicy`

```python
from nexuslang.security.sandbox import SandboxPolicy

SandboxPolicy(
    allow_ffi=False,
    allow_asm=False,
    allow_network=False,
    allow_file_write=False,
    allow_subprocess=False,
    max_memory_mb=None,          # MiB cap on virtual memory (POSIX)
    max_cpu_seconds=None,        # CPU seconds cap (POSIX)
    max_open_files=None,         # Max file descriptors (POSIX)
    max_processes=None,          # Max child processes (POSIX)
    enable_seccomp=False,        # Install BPF filter (Linux x86-64)
    allowed_syscalls=frozenset() # Additional syscalls to whitelist
)
```

### `RuntimeProtectorConfig`

```python
from nexuslang.security.runtime_protections import RuntimeProtectorConfig

RuntimeProtectorConfig(
    enable_canaries=False,                # Stack canary protection
    enable_bounds=False,                  # Array bounds checking
    enable_overflow=False,                # Integer overflow detection
    overflow_threshold=(1 << 63) - 1,    # Max safe integer magnitude
)
```
