# NexusLang Security Guide
## Comprehensive Guide to Secure Programming in NexusLang

## Table of Contents
1. [Introduction](#introduction)
2. [Permission System](#permission-system)
3. [Safe File Operations](#safe-file-operations)
4. [Safe Network Operations](#safe-network-operations)
5. [Safe Subprocess Execution](#safe-subprocess-execution)
6. [Input Validation](#input-validation)
7. [Output Sanitization](#output-sanitization)
8. [Memory Safety](#memory-safety)
9. [Common Vulnerabilities](#common-vulnerabilities)
10. [Security Checklist](#security-checklist)

---

## Introduction

NLPL is designed with security as a core principle. Unlike many languages that allow unrestricted access to system resources by default, NexusLang follows a **deny-by-default** security model inspired by Deno.

### Core Security Principles

1. **Deny by Default**: All dangerous operations require explicit permission
2. **Least Privilege**: Grant only the minimum permissions needed
3. **Defense in Depth**: Multiple layers of security checks
4. **Safe by Default**: APIs designed to prevent common vulnerabilities
5. **Explicit Over Implicit**: Security decisions must be explicit

---

## Permission System

### Overview

NLPL's permission system controls access to:
- **Filesystem** (read/write)
- **Network** (TCP/UDP/HTTP)
- **Subprocesses** (executing external programs)
- **FFI** (Foreign Function Interface / C interop)
- **Inline Assembly** (direct hardware access)
- **Environment Variables** (system configuration)
- **Remote Imports** (loading code from URLs)

### Command-Line Flags

#### Grant All Permissions (Development Only - DANGEROUS)
```bash
nlpl --allow-all script.nlpl
```

#### Grant Specific Permissions
```bash
# Allow reading any file
nlpl --allow-read script.nlpl

# Allow reading specific directory
nlpl --allow-read=/home/user/data script.nlpl

# Allow writing to specific directory
nlpl --allow-write=/home/user/output script.nlpl

# Allow network access to any host
nlpl --allow-net script.nlpl

# Allow network access to specific hosts
nlpl --allow-net=api.example.com,cdn.example.com script.nlpl

# Allow running any subprocess
nlpl --allow-run script.nlpl

# Allow running specific programs
nlpl --allow-run=git,npm script.nlpl

# Allow Foreign Function Interface
nlpl --allow-ffi script.nlpl

# Allow inline assembly
nlpl --allow-asm script.nlpl

# Allow environment variable access
nlpl --allow-env script.nlpl

# Allow remote imports
nlpl --allow-import script.nlpl
```

#### Multiple Permissions
```bash
nlpl --allow-read --allow-write=/tmp --allow-net=api.example.com script.nlpl
```

#### Interactive Prompt Mode
```bash
nlpl --prompt script.nlpl
```
Prompts user for permission when needed:
```
⚠️  NexusLang requests READ permission
   Resource: /etc/passwd
   Allow? [y/N/A(always)]
```

### Runtime Permission API

Check permissions from within NexusLang code:

```nlpl
# Check if permission is granted
set has_network to check_permission "net"
if has_network is false
    print text "Error: Network access denied"
    print text "Run with: nlpl --allow-net script.nxl"
    exit with 1

# Continue with network operation
create_http_client
```

---

## Safe File Operations

### Path Traversal Prevention

**VULNERABLE CODE:**
```nlpl
# User input directly used in file path
set filename to read_input
set content to read_file from filename  # DANGEROUS!
# User could input: "../../../../etc/passwd"
```

**SECURE CODE:**
```nlpl
# Validate path before use
set user_filename to read_input
set safe_path to validate_path with user_filename
if safe_path is null
    print text "Error: Invalid file path"
    exit with 1

set content to read_file from safe_path  # SAFE
```

### Safe Path Validation

```nlpl
function validate_user_path with user_input returns String or Null
    # Check for path traversal attempts
    if user_input contains "../"
        return null
    if user_input contains "..\\"
        return null
    
    # Normalize path
    set normalized to normalize_path with user_input
    
    # Ensure path is within allowed directory
    set allowed_dir to "/home/user/data"
    if normalized does not start with allowed_dir
        return null
    
    return normalized
```

### Whitelist Allowed Directories

```nlpl
# Define allowed directories at program start
set allowed_read_dirs to [
    "/home/user/data",
    "/home/user/documents"
]

set allowed_write_dirs to [
    "/home/user/output"
]

function can_read_file with path returns Boolean
    for each allowed_dir in allowed_read_dirs
        if path starts with allowed_dir
            return true
    return false
```

---

## Safe Network Operations

### URL Validation

**VULNERABLE CODE:**
```nlpl
# User input used directly in URL
set url to "https://api.example.com/" concatenate user_input
fetch_from url  # DANGEROUS - could be manipulated
```

**SECURE CODE:**
```nlpl
# Validate and sanitize URL
function build_safe_url with endpoint returns String or Null
    # Whitelist allowed characters in endpoint
    if not validate_url_path with endpoint
        return null
    
    # Build URL with proper encoding
    set base_url to "https://api.example.com"
    set safe_url to base_url concatenate "/" concatenate url_encode with endpoint
    return safe_url
```

### HTTPS Only

```nlpl
function ensure_https with url returns Boolean
    if url does not start with "https://"
        print text "Error: Only HTTPS URLs allowed"
        return false
    return true
```

### Domain Whitelist

```nlpl
set allowed_domains to [
    "api.example.com",
    "cdn.example.com"
]

function is_allowed_domain with url returns Boolean
    set domain to extract_domain from url
    for each allowed in allowed_domains
        if domain is equal to allowed
            return true
    return false
```

---

## Safe Subprocess Execution

### Command Injection Prevention

**VULNERABLE CODE:**
```nlpl
# Never concatenate user input into shell commands!
set user_input to read_input
set command to "ls " concatenate user_input
execute_shell command  # DANGEROUS!
# User could input: "; rm -rf /"
```

**SECURE CODE:**
```nlpl
# Use safe_execute with argument list (no shell expansion)
set user_input to read_input
set result to safe_execute with "ls" and [user_input]
# Shell metacharacters are treated as literal strings
```

### Never Use Shell Expansion

```nlpl
# WRONG: Using shell for command execution
execute_shell "cat /tmp/*.txt"  # Shell interprets *, $, |, etc.

# RIGHT: Use safe_execute with explicit arguments
set files to list_files in "/tmp" matching "*.txt"
for each file in files
    set content to read_file from file
```

### Whitelist Allowed Programs

```nlpl
set allowed_programs to ["git", "npm", "python3"]

function safe_run with program and args returns Result
    if program is not in allowed_programs
        print text "Error: Program not allowed"
        return null
    
    return safe_execute with program and args
```

---

## Input Validation

### Email Validation

```nlpl
function is_valid_email with email returns Boolean
    # Use built-in validator
    return validate_email with email
```

### URL Validation

```nlpl
function is_safe_url with url returns Boolean
    # Validate format
    if not validate_url with url
        return false
    
    # Only allow HTTPS
    if not url starts with "https://"
        return false
    
    # Check domain whitelist
    return is_allowed_domain with url
```

### Integer Validation with Bounds

```nlpl
function get_safe_integer with input and min and max returns Integer or Null
    if not validate_integer with input
        return null
    
    set value to parse_integer from input
    if value is less than min
        return null
    if value is greater than max
        return null
    
    return value
```

### String Length Limits

```nlpl
function validate_user_input with text and max_length returns Boolean
    if length of text is greater than max_length
        print text "Error: Input too long"
        return false
    
    # Check for null bytes
    if text contains null_byte
        print text "Error: Invalid characters"
        return false
    
    return true
```

---

## Output Sanitization

### HTML Escaping (XSS Prevention)

**VULNERABLE CODE:**
```nlpl
# User input directly in HTML
set username to read_input
set html to "<div>Hello, " concatenate username concatenate "</div>"
send_html html  # DANGEROUS if username contains <script>
```

**SECURE CODE:**
```nlpl
# Always escape user input in HTML
set username to read_input
set safe_username to escape_html with username
set html to "<div>Hello, " concatenate safe_username concatenate "</div>"
send_html html  # SAFE
```

### SQL Injection Prevention

**VULNERABLE CODE:**
```nlpl
# Never concatenate user input into SQL!
set user_id to read_input
set query to "SELECT * FROM users WHERE id = " concatenate user_id
execute_sql query  # DANGEROUS!
# User could input: "1 OR 1=1--"
```

**SECURE CODE:**
```nlpl
# Use parameterized queries
set user_id to read_input
set query to "SELECT * FROM users WHERE id = ?"
set result to execute_sql with query and [user_id]  # SAFE
```

### Log Injection Prevention

```nlpl
function safe_log with message returns Nothing
    # Remove newlines to prevent log injection
    set safe_message to replace_newlines in message with " "
    write_log safe_message
```

---

## Memory Safety

### Bounds Checking

NLPL automatically checks array bounds at runtime:

```nlpl
set arr to [1, 2, 3]
set val to arr[10]  # ERROR: Index out of bounds
# Program terminates safely instead of corrupting memory
```

### Null Pointer Protection

```nlpl
set ptr to null

# WRONG: Dereferencing without checking
set val to dereference ptr  # ERROR: Null pointer dereference

# RIGHT: Check before dereferencing
if ptr is not null
    set val to dereference ptr  # SAFE
```

### Use-After-Free Detection

```nlpl
set ptr to allocate 100 bytes
free ptr
set val to dereference ptr  # ERROR: Use after free detected
# NexusLang tracks allocations and prevents UAF
```

### Buffer Overflow Protection

```nlpl
set buffer to allocate 10 bytes
write_to_buffer buffer at offset 15 with data  # ERROR: Buffer overflow
# NexusLang checks buffer bounds at runtime
```

---

## Common Vulnerabilities

### 1. Path Traversal

**Attack:** `../../../../etc/passwd`
**Prevention:** Validate paths, use `validate_path` function

### 2. Command Injection

**Attack:** `; rm -rf /`
**Prevention:** Use `safe_execute`, never use shell

### 3. SQL Injection

**Attack:** `' OR '1'='1`
**Prevention:** Use parameterized queries

### 4. Cross-Site Scripting (XSS)

**Attack:** `<script>alert('XSS')</script>`
**Prevention:** Use `escape_html` for all user input

### 5. Integer Overflow

**Attack:** Very large numbers causing wrap-around
**Prevention:** Validate integer ranges

### 6. Race Conditions

**Attack:** TOCTOU (Time-of-Check-Time-of-Use)
**Prevention:** Minimize time between check and use

### 7. Denial of Service

**Attack:** Resource exhaustion (CPU, memory, disk)
**Prevention:** Rate limiting, resource limits, timeouts

---

## Security Checklist

Before deploying NexusLang code, verify:

### Input Validation
- [ ] All user input is validated
- [ ] String lengths are limited
- [ ] Integers are within expected ranges
- [ ] Email addresses are validated
- [ ] URLs are validated and whitelisted

### File Operations
- [ ] File paths are validated (no `../`)
- [ ] Paths are within allowed directories
- [ ] File size limits are enforced
- [ ] Permissions are checked before read/write

### Network Operations
- [ ] Only HTTPS is used (no HTTP)
- [ ] Domains are whitelisted
- [ ] Timeouts are configured
- [ ] Rate limiting is implemented
- [ ] Certificate validation is enabled

### Subprocess Execution
- [ ] `safe_execute` is used (not shell)
- [ ] Programs are whitelisted
- [ ] Arguments are in list form (no concatenation)
- [ ] Timeouts are configured

### Database Operations
- [ ] Parameterized queries are used
- [ ] SQL identifiers are validated
- [ ] Connection strings are not logged
- [ ] Least privilege database accounts

### Output
- [ ] HTML is escaped (use `escape_html`)
- [ ] Logs don't contain sensitive data
- [ ] Error messages don't leak internal info
- [ ] Stack traces are not shown to users

### Permissions
- [ ] Minimum necessary permissions granted
- [ ] No `--allow-all` in production
- [ ] FFI and ASM require explicit approval
- [ ] Environment variables are restricted

### Memory Safety
- [ ] Bounds checking is enabled
- [ ] Null pointers are checked
- [ ] Allocations are freed properly
- [ ] No manual pointer arithmetic

---

## Best Practices

### 1. Principle of Least Privilege

Grant only the permissions actually needed:

```bash
# Instead of:
nlpl --allow-all script.nlpl

# Do:
nlpl --allow-read=/home/user/data --allow-write=/home/user/output script.nlpl
```

### 2. Defense in Depth

Multiple layers of security:

```nlpl
# Layer 1: Permission check
if not has_permission "read"
    error "No permission"

# Layer 2: Path validation
set safe_path to validate_path with user_path
if safe_path is null
    error "Invalid path"

# Layer 3: Directory whitelist
if not path_in_whitelist with safe_path
    error "Path not allowed"

# Layer 4: Actual operation
set content to read_file from safe_path
```

### 3. Fail Securely

When security checks fail, deny access:

```nlpl
function process_request with request returns Response
    # Default to deny
    set allowed to false
    
    # Only allow if all checks pass
    if validate_auth with request
        if validate_input with request
            if check_rate_limit with request
                set allowed to true
    
    if not allowed
        return error_response
    
    # Process request
    return success_response
```

### 4. Validate Early, Validate Often

```nlpl
function handle_user_data with data returns Nothing
    # Validate immediately
    if not validate with data
        return
    
    # Validate before each use
    if not validate_for_database with data
        return
    save_to_database with data
    
    if not validate_for_api with data
        return
    send_to_api with data
```

### 5. Log Security Events

```nlpl
function log_security_event with event_type and details returns Nothing
    set timestamp to current_time
    set log_entry to create_dict with [
        "timestamp", timestamp,
        "type", event_type,
        "details", details
    ]
    append_to_security_log with log_entry
```

---

## Examples

See `examples/security/` directory for complete examples:
- `safe_file_operations.nlpl`
- `safe_subprocess.nlpl`
- `input_validation.nlpl`
- `secure_web_server.nlpl`

---

## Resources

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CWE Top 25](https://cwe.mitre.org/top25/)
- [Deno Security Model](https://deno.land/manual/getting_started/permissions)
- NexusLang Security API Documentation

---

## Reporting Security Issues

If you discover a security vulnerability in NexusLang:

1. **DO NOT** create a public GitHub issue
2. Email security@nlpl-lang.org
3. Include detailed description and proof-of-concept
4. Allow time for patching before public disclosure

---

*Last Updated: February 1, 2026*
