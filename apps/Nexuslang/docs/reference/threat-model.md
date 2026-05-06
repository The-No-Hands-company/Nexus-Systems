# NexusLang Threat Model

This document describes the security threats that the NexusLang runtime is designed to
resist, the trust boundaries that bound the system, and the mapping from threats to
mitigations.

For practical security guidance see [security.md](security.md).
For the vulnerability reporting process see [cve-process.md](cve-process.md).

---

## Contents

1. [System Description](#1-system-description)
2. [Trust Boundaries](#2-trust-boundaries)
3. [Attack Surfaces](#3-attack-surfaces)
4. [Threat Actors](#4-threat-actors)
5. [Threat Catalog](#5-threat-catalog)
6. [Mitigation Matrix](#6-mitigation-matrix)
7. [Residual Risks and Caveats](#7-residual-risks-and-caveats)
8. [Threat Model Assumptions](#8-threat-model-assumptions)

---

## 1. System Description

The NexusLang runtime is a tree-walking interpreter implemented in CPython.  It
executes `.nlpl` source programs on behalf of a user who invoked it on the
command line or embedded it in a host application.

Key components for this analysis:

- **Lexer / Parser** — converts source text to an AST; controlled entirely by
  the source file.
- **Interpreter** — walks the AST and executes program semantics.
- **Runtime** — manages memory allocation, object creation, and global state.
- **Standard Library** — pure-Python NexusLang library functions (math, io, net,
  collections, system, etc.).
- **Permission System** — deny-by-default capability gate enforced before any
  dangerous operation.
- **FFI Bridge** — ctypes-based interface that lets NexusLang programs call C
  libraries.
- **Security Module** — taint analysis, CFI, sandbox, runtime protections.

---

## 2. Trust Boundaries

```
+-------------------------------------------------------------+
|  (Untrusted) NexusLang Source Program                            |
|    - provided by developer, end user, or network download   |
+-------------------------------------------------------------+
      |
      | Parse + permission check
      v
+-------------------------------------------------------------+
|  (Trusted) NexusLang Interpreter Process                         |
|    - runs as the OS user who invoked `nlpl`                  |
|    - grants only explicitly requested capabilities           |
+-------------------------------------------------------------+
      |                        |
      | FFI bridge             | Subprocess spawn
      v                        v
+-------------------+    +-------------------+
| (Untrusted) C     |    | (Untrusted) Child  |
| Libraries         |    | Processes          |
+-------------------+    +-------------------+
      |
      | OS system calls
      v
+-------------------------------------------------------------+
|  Kernel / Hardware                                          |
+-------------------------------------------------------------+
```

**Boundary 1 — Source program to interpreter:**
The interpreter treats the source program as untrusted input.  The parser
validates syntax; the permission system validates capability requests;
taint analysis tracks values originating from external sources.

**Boundary 2 — Interpreter to C libraries (FFI):**
C code called via FFI executes in the same process as the interpreter but at
native privilege.  It can call arbitrary syscalls, overwrite memory, and bypass
the permission system.  FFI access is denied by default and must be explicitly
granted.

**Boundary 3 — Interpreter to child processes:**
Subprocess spawning is denied by default.  Spawned processes are separate OS
processes with independent address spaces.

**Boundary 4 — Interpreter to OS/kernel:**
The interpreter runs at the privilege of the invoking OS user.  Seccomp-BPF
can narrow the allowed syscall surface when running untrusted programs.

---

## 3. Attack Surfaces

| Surface | Access Needed | Notes |
|---------|--------------|-------|
| NexusLang source file (argv) | Write to disk | Code execution with interpreter's privileges |
| Stdin / interactive prompts | Remote if networked | Data source — tainted by default |
| Environment variables | Process environment | May leak secrets; denied by default |
| File system (via `--allow-read/write`) | Interpreter process | Scoped to granted paths |
| Network (via `--allow-net`) | Interpreter process | Scoped to granted hosts |
| FFI-loaded C libraries | Interpreter process + `--allow-ffi` | Full native code execution |
| Inline assembly (via `--allow-asm`) | Interpreter process | Arbitrary machine code |
| Dynamic imports (via `--allow-import`) | Interpreter process | Arbitrary code execution through module loading |
| CPython internals | Interpreted language (no restriction) | Python objects accessible to NexusLang programs via FFI bridge |
| Subprocess output | Subprocess writes to pipe | Tainted input to parent program |

---

## 4. Threat Actors

### T-A1: Malicious NexusLang Script

A `.nlpl` file constructed to:
- Exfiltrate files or credentials
- Spawn persistent processes
- Load unauthorized C libraries
- Exploit parser or interpreter bugs for privilege escalation

**Typical context:** CI/CD pipelines executing untrusted community programs, IDE
plugin systems, server-side eval of user-supplied scripts.

### T-A2: Compromised Dependency

A legitimate NexusLang module or a C library loaded via FFI that has been supply-chain
attacked.  The code initially appears safe but performs malicious actions at runtime
(e.g., exfiltrates data before raising an exception).

### T-A3: User-Controlled Input at Runtime

An attacker who provides crafted user input intended to:
- Cause path traversal in file operations
- Inject shell commands into subprocess calls
- Corrupt SQL queries
- Trigger integer overflow to bypass buffer size checks

This actor does not control the source program itself.

### T-A4: Local Privilege Escalation

An attacker with local shell access who exploits the NexusLang interpreter process
(e.g., through a bug in the interpreter or an FFI library) to gain elevated
OS privileges.

---

## 5. Threat Catalog

### T1 — Unauthorized File System Access

An NexusLang program reads or writes files outside its intended scope.

**Vectors:** Using `READ`/`WRITE` permissions without path restrictions; path
traversal via `../` sequences in user-controlled file paths.

**Affected actors:** T-A1, T-A3

**Severity:** High

---

### T2 — Unauthorized Network Access

An NexusLang program opens outbound connections to arbitrary hosts, exfiltrating
data or contacting command-and-control infrastructure.

**Vectors:** `--allow-net` without host restrictions; SSRF through URL
parameters from user input.

**Affected actors:** T-A1, T-A3

**Severity:** High

---

### T3 — Command Injection

User-controlled input reaches a subprocess call without sanitization, causing
execution of attacker-specified shell commands.

**Vectors:** String interpolation of user data into subprocess arguments;
shell metacharacter injection.

**Affected actors:** T-A1, T-A3

**Severity:** Critical

---

### T4 — FFI / Library Exploit

A C library called via FFI performs unintended memory operations, calls
additional syscalls, or exfiltrates data.

**Vectors:** Compromised library, use-after-free in C code, type confusion
in the FFI bridge.

**Affected actors:** T-A1, T-A2

**Severity:** Critical

---

### T5 — Resource Exhaustion (DoS)

An NexusLang program or an input triggers unbounded CPU, memory, or file descriptor
consumption, causing the host process or system to become unresponsive.

**Vectors:** Infinite loops, exponential regex matching, large allocations,
fork bombs.

**Affected actors:** T-A1, T-A3

**Severity:** Medium

---

### T6 — Integer Overflow Leading to Memory Corruption

Arithmetic on attacker-controlled values (e.g., an array size from user input)
wraps to a small number, causing an under-allocated buffer and a subsequent
out-of-bounds write via FFI.

**Vectors:** Multiplying user-supplied width/height/count values without
bounds checking.

**Affected actors:** T-A1, T-A3

**Severity:** High

---

### T7 — Tainted Data at Dangerous Sink

Unvalidated external data (user input, network, env vars, FFI return values)
flows into a dangerous operation such as a SQL query, format string, or eval.

**Vectors:** Missing input validation, missing sanitization, missing taint checks.

**Affected actors:** T-A1, T-A3

**Severity:** High

---

### T8 — Control Flow Hijacking

An attacker manipulates which function is called at a function-pointer call
site (e.g., through a type-confused FFI return, a tampered callback list, or
a heap spray targeting function objects).

**Vectors:** FFI memory corruption, untrusted callback registration.

**Affected actors:** T-A1, T-A2

**Severity:** High

---

### T9 — Stack Frame Corruption via FFI

A C function invoked through the FFI bridge stomps on stack canaries or return
addresses in the CPython call stack, redirecting execution.

**Vectors:** Stack buffer overflow in a C extension, use-after-free targeting
interpreter stack frames.

**Affected actors:** T-A2

**Severity:** High

---

### T10 — Information Disclosure via Error Messages

Verbose error messages or stack traces expose internal paths, variable values,
or secrets to an attacker-controlled logging channel.

**Vectors:** Unhandled exceptions propagating to network responses; debug
logging left enabled in production.

**Affected actors:** T-A3

**Severity:** Medium

---

## 6. Mitigation Matrix

| Threat | Primary Mitigation | Secondary / Defence-in-Depth |
|--------|-------------------|------------------------------|
| T1 — FS access | Permission system (`READ`/`WRITE` deny-by-default); `validate_path` | `STRICT_POLICY` sandbox; seccomp blocks `open()`/`openat()` without explicit grant |
| T2 — Network access | Permission system (`NET` deny-by-default) | `STRICT_POLICY` sandbox; seccomp blocks `connect()`/`sendto()` |
| T3 — Command injection | Permission system (`RUN` deny-by-default); `safe_execute`; `escape_shell_arg` | Taint analysis (`SHELL_EXEC` sink check); seccomp blocks `execve()` |
| T4 — FFI exploit | Permission system (`FFI` deny-by-default); `check_ffi()` in `RestrictedMode` | Memory safety validator; CFI checker; seccomp narrows syscalls |
| T5 — DoS / resource exhaustion | `ResourceLimits` (RLIMIT_CPU, RLIMIT_AS, RLIMIT_NOFILE, RLIMIT_NPROC) | Seccomp blocks fork bombs |
| T6 — Integer overflow | `IntegerOverflowChecker` on arithmetic results used as sizes | Bounds checker validates indices |
| T7 — Tainted sink | `TaintTracker.check_sink()` at all dangerous operations | Input validation utilities |
| T8 — CFI hijack | `CFIChecker.check_call()` at indirect call sites | `CallGraph` registration |
| T9 — Stack corruption | `StackCanary` on FFI-adjacent interpreter frames | ASLR (kernel feature); stack canaries reduce exploitability |
| T10 — Info disclosure | Error messages stripped of internal paths in production | Log sanitization in `errors.py` |

---

## 7. Residual Risks and Caveats

### 7.1 Interpreter Interpreted in CPython

The NexusLang interpreter runs inside CPython, which means:
- CPython memory management bugs can undermine memory safety guarantees.
- CPython's GIL does not protect against C extension race conditions.
- Python's dynamic import system can be abused if `IMPORT` permission is
  not denied.

### 7.2 Seccomp Is Irreversible

Once installed, a seccomp-BPF filter cannot be removed from a process.
Filters should only be installed in forked child processes that will be
discarded after the untrusted program completes.

### 7.3 POSIX Resource Limits on Hard Limits

`setrlimit` can lower the soft limit but cannot raise the hard limit above its
current value.  If the system administrator has set very low hard limits,
`ResourceLimits.exit()` may not fully restore the previous state.

### 7.4 Taint Analysis Coverage

Taint analysis is opt-in at the interpreter/embedding level.  Programs that
do not call `TaintTracker.taint()` at input sources receive no taint protection.
The standard library modules mark common sources (stdin, network reads), but
FFI return values must be manually marked.

### 7.5 CFI Scope

CFI only prevents calls to functions not registered in the `CallGraph`.  A
compromised C library executing entirely in C code (not going through the FFI
bridge dispatcher) is outside the CFI scope.  Seccomp is the defence for
that scenario.

### 7.6 ASLR Requires OS Support

Stack canaries and ASLR reduce exploit reliability but do not provide
absolute protection against memory-safety bugs in C extensions.  Full
mitigation requires:
- Running on a kernel with ASLR enabled (`randomize_va_space=2`).
- Using compiler-hardened builds of C extensions (PIE, stack protector, RELRO).

---

## 8. Threat Model Assumptions

1. **The NexusLang interpreter binary is trusted.** Attackers cannot modify the
   interpreter executable or the Python installation.

2. **The host OS is not compromised.** Kernel vulnerabilities are out of scope.

3. **The user who invokes `nlpl` sets permissions deliberately.** If a user
   passes `--allow-all`, the responsibility for the resulting capabilities is
   theirs.

4. **Cryptographic primitives are correct.** Vulnerabilities in the underlying
   Python or OS cryptographic libraries are out of scope.

5. **Denial-by-default for unknown capabilities.** Any capability not
   explicitly listed in `PermissionType` is not granted regardless of flags.

6. **Resource limit granularity is POSIX-coarse.** RLIMIT limits are per-process;
   they constrain the entire interpreter process, including the trusted runtime code.
