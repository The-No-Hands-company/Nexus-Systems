# Foreign Function Interface (FFI) in NexusLang

**Status:** ✅ Fully implemented with variadic support (February 2, 2026)  
**Complexity:** Advanced

---

## Overview

The Foreign Function Interface (FFI) allows NexusLang programs to call functions from C libraries and other native code. This enables:

- **Library integration:** Use existing C/C++ libraries
- **System programming:** Access OS APIs directly
- **Performance:** Call optimized native code
- **Hardware access:** Interface with device drivers
- **Interoperability:** Bridge between NexusLang and other languages

NLPL's FFI features:
- C function declarations with `extern` keyword
- Automatic type conversion (NLPL ↔ C types)
- Variadic function support (printf, etc.)
- Struct/union marshalling
- Pointer handling and memory management
- Library loading and symbol resolution

**New:** Variadic function support added February 2, 2026!

---

## Basic FFI Usage

### Declaring External Functions

```nexuslang
# Simple C function
extern function puts with message as String returns Integer

# Function with multiple parameters
extern function strcmp with str1 as String, str2 as String returns Integer

# Function returning pointer
extern function malloc with size as Integer returns Pointer

# Function taking pointer
extern function free with ptr as Pointer
```

**Syntax:**
- `extern` keyword marks function as external
- `function` followed by C function name
- Parameter types must match C function signature
- Return type must be specified (use `returns None` for void)

### Calling External Functions

```nexuslang
# Simple call
call puts with "Hello from NLPL"

# Store return value
set result to strcmp with "hello", "world"

# Memory allocation
set ptr to malloc with 1024
# ... use memory ...
call free with ptr
```

---

## Type Mapping

### NexusLang to C Type Conversion

| NexusLang Type | C Type | Size | Notes |
|-----------|--------|------|-------|
| `Integer` | `int` or `long` | 4 or 8 bytes | Platform-dependent |
| `Float` | `double` | 8 bytes | Double precision |
| `String` | `char*` | Pointer size | UTF-8 encoded |
| `Boolean` | `int` | 4 bytes | 0 = false, 1 = true |
| `Pointer` | `void*` | 8 bytes (x64) | Generic pointer |
| `Pointer to T` | `T*` | 8 bytes (x64) | Typed pointer |

**Important:** Ensure types match exactly! Mismatched types cause undefined behavior.

### Explicit Type Sizes

```nexuslang
# Use specific-sized types for predictable behavior
extern function read with fd as Integer, buf as Pointer, count as Integer returns Integer

# C signature: ssize_t read(int fd, void *buf, size_t count);
```

**Best practice:** Check C library documentation for exact type sizes.

---

## Common C Functions

### Standard Library Functions

#### String Functions

```nexuslang
# string.h functions
extern function strlen with str as String returns Integer
extern function strcpy with dest as String, src as String returns String
extern function strcat with dest as String, src as String returns String
extern function strcmp with str1 as String, str2 as String returns Integer
extern function strstr with haystack as String, needle as String returns String

# Usage
set len to strlen with "Hello"
# len = 5

set buffer to malloc with 100
call strcpy with buffer, "Hello"
call strcat with buffer, " World"
# buffer contains "Hello World"
```

#### Memory Functions

```nexuslang
# stdlib.h functions
extern function malloc with size as Integer returns Pointer
extern function calloc with nmemb as Integer, size as Integer returns Pointer
extern function realloc with ptr as Pointer, size as Integer returns Pointer
extern function free with ptr as Pointer

# string.h memory functions
extern function memcpy with dest as Pointer, src as Pointer, n as Integer returns Pointer
extern function memset with s as Pointer, c as Integer, n as Integer returns Pointer
extern function memcmp with s1 as Pointer, s2 as Pointer, n as Integer returns Integer

# Usage
set buffer to malloc with 1024
call memset with buffer, 0, 1024  # Zero out buffer
call free with buffer
```

#### Math Functions

```nexuslang
# math.h functions
extern function sin with x as Float returns Float
extern function cos with x as Float returns Float
extern function sqrt with x as Float returns Float
extern function pow with x as Float, y as Float returns Float
extern function log with x as Float returns Float
extern function exp with x as Float returns Float

# Usage
set result to sin with 1.57  # sin(π/2) ≈ 1.0
set root to sqrt with 16.0   # = 4.0
```

#### File I/O

```nexuslang
# stdio.h functions
extern function fopen with path as String, mode as String returns Pointer
extern function fclose with stream as Pointer returns Integer
extern function fread with ptr as Pointer, size as Integer, nmemb as Integer, stream as Pointer returns Integer
extern function fwrite with ptr as Pointer, size as Integer, nmemb as Integer, stream as Pointer returns Integer
extern function fprintf with stream as Pointer, format as String, ... returns Integer  # Variadic!

# Usage
set file to fopen with "data.txt", "r"
if file not equals 0
  # Read data
  call fclose with file
end
```

---

## Variadic Functions NEW!

**Added February 2, 2026!** NexusLang now supports calling variadic C functions.

### What are Variadic Functions?

Variadic functions accept a variable number of arguments:
- `printf(format, ...)` - Formatted output
- `fprintf(stream, format, ...)` - File output
- `sprintf(buffer, format, ...)` - String formatting
- `scanf(format, ...)` - Formatted input

### Declaring Variadic Functions

```nexuslang
# Use ... to indicate variadic parameters
extern function printf with format as String, ... returns Integer
extern function fprintf with stream as Pointer, format as String, ... returns Integer
extern function sprintf with buffer as String, format as String, ... returns Integer
```

### Calling Variadic Functions

```nexuslang
# printf examples
call printf with "Hello, world!\n"
call printf with "Number: %d\n", 42
call printf with "Float: %.2f\n", 3.14159
call printf with "String: %s\n", "Hello"
call printf with "Multiple: %d %f %s\n", 10, 2.5, "test"

# fprintf to file
set file to fopen with "output.txt", "w"
call fprintf with file, "Line %d: %s\n", 1, "First line"
call fprintf with file, "Line %d: %s\n", 2, "Second line"
call fclose with file

# sprintf for string formatting
set buffer to malloc with 256
call sprintf with buffer, "Result: %d + %d = %d", 5, 10, 15
print text buffer
call free with buffer
```

### Format Specifiers

Common printf/scanf format specifiers:

| Specifier | Type | Description |
|-----------|------|-------------|
| `%d`, `%i` | Integer | Signed decimal integer |
| `%u` | Integer | Unsigned decimal integer |
| `%f` | Float | Decimal floating point |
| `%e`, `%E` | Float | Scientific notation |
| `%g`, `%G` | Float | Shortest representation |
| `%s` | String | String of characters |
| `%c` | Integer | Single character |
| `%p` | Pointer | Pointer address |
| `%x`, `%X` | Integer | Hexadecimal |
| `%o` | Integer | Octal |
| `%%` | - | Literal % character |

**Width and precision:**
```nexuslang
call printf with "%10d\n", 42        # Right-aligned, width 10
call printf with "%-10d\n", 42       # Left-aligned, width 10
call printf with "%.2f\n", 3.14159   # 2 decimal places
call printf with "%10.2f\n", 3.14159 # Width 10, 2 decimals
```

### Type Safety Warning

**Critical:** NexusLang cannot verify variadic argument types at compile time!

```nexuslang
# Dangerous - wrong type!
call printf with "%d\n", "string"  # CRASH! Expected integer, got string

# Dangerous - wrong count!
call printf with "%d %d\n", 42  # CRASH! Missing second argument

# Dangerous - format mismatch!
call printf with "%s\n", 42  # CRASH! Expected string, got integer
```

**Best practices:**
1. Always match format specifiers to argument types
2. Count arguments carefully
3. Test with small inputs first
4. Use wrapper functions for safety

---

## Working with Structs

### Passing Structs to C Functions

```nexuslang
# Define struct matching C layout
struct Point
  x as Integer
  y as Integer
end

# C function: void print_point(struct Point *p);
extern function print_point with p as Pointer to Point

# Usage
set p to new Point
set p.x to 10
set p.y to 20
call print_point with (address of p)
```

### Receiving Structs from C

```nexuslang
# C function: struct Point create_point(int x, int y);
# Note: Struct return values require special handling

# Option 1: Return pointer
extern function create_point with x as Integer, y as Integer returns Pointer to Point

set p_ptr to create_point with 5, 10
set x to p_ptr.x
set y to p_ptr.y

# Option 2: Out parameter
extern function init_point with p as Pointer to Point, x as Integer, y as Integer

set p to new Point
call init_point with (address of p), 5, 10
```

### Packed Structs

```nexuslang
# C struct with __attribute__((packed))
packed struct NetworkHeader
  magic as Integer    # 4 bytes
  version as Integer  # 2 bytes  
  length as Integer   # 2 bytes
end

extern function parse_header with data as Pointer returns Pointer to NetworkHeader

set header_ptr to parse_header with data_buffer
set magic to header_ptr.magic
```

---

## Pointer Management

### Creating Pointers

```nexuslang
# Get address of variable
set x to 42
set x_ptr to address of x

# Allocate memory
set buffer to malloc with 1024

# Cast to typed pointer
set int_ptr to buffer as Pointer to Integer
```

### Dereferencing Pointers

```nexuslang
# Read through pointer
set value to dereference ptr

# Write through pointer
# (Note: Direct dereference assignment may require helper function)
extern function set_int_value with ptr as Pointer to Integer, value as Integer
call set_int_value with int_ptr, 42
```

### Pointer Arithmetic

```nexuslang
# C-style pointer arithmetic requires careful calculation
set ptr to malloc with 40  # 10 integers * 4 bytes

# Access element 0
set elem0_ptr to ptr as Pointer to Integer

# Access element 1 (ptr + sizeof(Integer))
set elem1_ptr to (ptr plus 4) as Pointer to Integer

# Better: Use helper function
extern function get_array_element with array as Pointer to Integer, index as Integer returns Integer
set value to get_array_element with int_ptr, 5
```

---

## Memory Safety

### Common Pitfalls

#### 1. Memory Leaks

```nexuslang
# Bad: Memory not freed
function bad_example
  set ptr to malloc with 1024
  # ptr goes out of scope without free - LEAK!
end

# Good: Always free
function good_example
  set ptr to malloc with 1024
  try
    # Use memory
  finally
    call free with ptr  # Always freed
  end
end
```

#### 2. Use After Free

```nexuslang
# Bad: Use after free
set ptr to malloc with 100
call free with ptr
set x to dereference ptr  # CRASH! Undefined behavior

# Good: Clear pointer after free
set ptr to malloc with 100
call free with ptr
set ptr to 0  # Mark as invalid
```

#### 3. Buffer Overflow

```nexuslang
# Bad: No bounds checking
set buffer to malloc with 10
call strcpy with buffer, "This string is too long!"  # OVERFLOW!

# Good: Use bounded functions
extern function strncpy with dest as String, src as String, n as Integer returns String
set buffer to malloc with 10
call strncpy with buffer, "Long string", 9
# buffer[9] = '\0'  # Null terminate
```

#### 4. Null Pointer Dereference

```nexuslang
# Bad: No null check
set ptr to malloc with 1024
set value to dereference ptr  # Might crash if malloc failed!

# Good: Check for null
set ptr to malloc with 1024
if ptr equals 0
  raise error "Memory allocation failed"
end
set value to dereference ptr  # Safe
```

### Best Practices

1. **Always check allocation results:**
```nexuslang
set ptr to malloc with size
if ptr equals 0
  raise error "Out of memory"
end
```

2. **Use RAII pattern:**
```nexuslang
function with_buffer with size as Integer, callback as Function
  set buffer to malloc with size
  try
    call callback with buffer
  finally
    call free with buffer
  end
end
```

3. **Prefer NexusLang types over raw C:**
```nexuslang
# Instead of C strings (char*)
set name to "Alice"  # NexusLang String (automatic memory management)

# Instead of malloc/free
set numbers to [1, 2, 3, 4, 5]  # NexusLang List (automatic)
```

4. **Wrap C functions in NexusLang:**
```nexuslang
function safe_read_file with path as String returns String
  extern function fopen with path as String, mode as String returns Pointer
  extern function fread with ptr as Pointer, size as Integer, nmemb as Integer, stream as Pointer returns Integer
  extern function fclose with stream as Pointer returns Integer
  
  set file to fopen with path, "r"
  if file equals 0
    return Err("Failed to open file")
  end
  
  set buffer to malloc with 4096
  try
    set bytes_read to fread with buffer, 1, 4096, file
    # Convert to NexusLang String
    return Ok(buffer as String)
  finally
    call fclose with file
    call free with buffer
  end
end
```

---

## Library Loading

### Dynamic Library Loading

```nexuslang
# Load shared library
extern function dlopen with filename as String, flag as Integer returns Pointer
extern function dlsym with handle as Pointer, symbol as String returns Pointer
extern function dlclose with handle as Pointer returns Integer

# RTLD flags
const RTLD_LAZY to 1
const RTLD_NOW to 2

# Load library
set lib_handle to dlopen with "libmylib.so", RTLD_NOW
if lib_handle equals 0
  raise error "Failed to load library"
end

# Get function pointer
set func_ptr to dlsym with lib_handle, "my_function"
if func_ptr equals 0
  raise error "Function not found"
end

# Call function (requires function pointer support)
# ... call through func_ptr ...

# Unload library
call dlclose with lib_handle
```

---

## Advanced Techniques

### Callbacks

```nexuslang
# C function: void process_array(int *arr, int len, void (*callback)(int));

# Define callback in NexusLang
function my_callback with value as Integer
  print text "Processing: " plus (value to_string)
end

extern function process_array with arr as Pointer to Integer, len as Integer, callback as Function

set numbers to [1, 2, 3, 4, 5]
call process_array with (address of numbers[0]), 5, my_callback
```

### Variable Argument Parsing

```nexuslang
# For implementing variadic functions in NexusLang (using va_list)
extern function va_start with ap as Pointer, last as Pointer
extern function va_arg with ap as Pointer, type as Integer returns Pointer
extern function va_end with ap as Pointer

# This is advanced and typically not needed
```

### Inline Assembly with FFI

```nexuslang
function call_c_function_optimized with arg as Integer returns Integer
  asm "
    ; Call C function directly
    mov rdi, [rbp-8]     ; Load argument
    call external_func   ; Call C function
    ; Return value in rax
  "
end
```

---

## Platform-Specific Considerations

### Linux

```nexuslang
# System calls
extern function open with path as String, flags as Integer, mode as Integer returns Integer
extern function read with fd as Integer, buf as Pointer, count as Integer returns Integer
extern function write with fd as Integer, buf as Pointer, count as Integer returns Integer
extern function close with fd as Integer returns Integer

# Flags
const O_RDONLY to 0
const O_WRONLY to 1
const O_RDWR to 2
```

### Windows

```nexuslang
# Windows API
extern function CreateFileA with filename as String, access as Integer, share as Integer, 
                               security as Pointer, disposition as Integer, 
                               flags as Integer, template as Pointer returns Pointer

extern function ReadFile with handle as Pointer, buffer as Pointer, bytes_to_read as Integer,
                             bytes_read as Pointer, overlapped as Pointer returns Integer

extern function CloseHandle with handle as Pointer returns Integer

# Constants
const GENERIC_READ to 0x80000000
const OPEN_EXISTING to 3
```

---

## Debugging FFI Code

### Common Errors

1. **Segmentation Fault:**
   - Wrong pointer passed to C function
   - Null pointer dereference
   - Buffer overflow
   - Use after free

2. **Unexpected Return Values:**
   - Type mismatch between NexusLang and C
   - Wrong parameter order
   - Endianness issues (rare)

3. **Memory Corruption:**
   - Writing past buffer boundaries
   - Double free
   - Stack smashing

### Debugging Tools

```nexuslang
# Add debug output
function debug_call_c_function with ptr as Pointer
  print text "Calling C function with pointer: " plus (ptr as String)
  extern function c_function with p as Pointer returns Integer
  set result to c_function with ptr
  print text "C function returned: " plus (result to_string)
  return result
end

# Use try/catch
try
  call dangerous_c_function with ptr
catch error
  print text "C function failed: " plus error
  # Cleanup
end
```

**External tools:**
- `valgrind` - Memory error detection
- `gdb` - Debugger for native code
- `strace` - System call tracing (Linux)
- AddressSanitizer - Buffer overflow detection

---

## Examples

### Example 1: File Operations

```nexuslang
# Read file contents using C functions
function read_file_contents with path as String returns String
  extern function fopen with path as String, mode as String returns Pointer
  extern function fclose with stream as Pointer returns Integer
  extern function fread with ptr as Pointer, size as Integer, nmemb as Integer, stream as Pointer returns Integer
  extern function fseek with stream as Pointer, offset as Integer, whence as Integer returns Integer
  extern function ftell with stream as Pointer returns Integer
  
  const SEEK_END to 2
  const SEEK_SET to 0
  
  set file to fopen with path, "r"
  if file equals 0
    raise error "Failed to open file"
  end
  
  # Get file size
  call fseek with file, 0, SEEK_END
  set size to ftell with file
  call fseek with file, 0, SEEK_SET
  
  # Allocate buffer
  set buffer to malloc with size plus 1
  
  # Read file
  set bytes_read to fread with buffer, 1, size, file
  call fclose with file
  
  # Convert to string (add null terminator)
  # buffer[size] = '\0'  # Requires pointer write support
  
  return buffer as String
end
```

### Example 2: SHA256 Hashing

```nexuslang
# Using OpenSSL for cryptography
extern function SHA256 with data as Pointer, len as Integer, md as Pointer returns Pointer

function calculate_sha256 with input as String returns String
  set digest_size to 32  # SHA256 produces 32 bytes
  set digest to malloc with digest_size
  
  call SHA256 with (input as Pointer), (length of input), digest
  
  # Convert to hex string
  extern function sprintf with buffer as String, format as String, ... returns Integer
  set hex_buffer to malloc with 65  # 32 bytes * 2 + null
  
  # Format as hex
  repeat 32 times with i
    set byte to dereference (digest plus i)
    call sprintf with (hex_buffer plus (i times 2)), "%02x", byte
  end
  
  set result to hex_buffer as String
  
  call free with digest
  call free with hex_buffer
  
  return result
end
```

### Example 3: HTTP Request

```nexuslang
# Using libcurl
extern function curl_easy_init returns Pointer
extern function curl_easy_setopt with handle as Pointer, option as Integer, parameter as String returns Integer
extern function curl_easy_perform with handle as Pointer returns Integer
extern function curl_easy_cleanup with handle as Pointer

const CURLOPT_URL to 10002

function http_get with url as String returns String
  set curl to curl_easy_init
  if curl equals 0
    raise error "Failed to initialize curl"
  end
  
  call curl_easy_setopt with curl, CURLOPT_URL, url
  # Set other options...
  
  set result_code to curl_easy_perform with curl
  
  call curl_easy_cleanup with curl
  
  if result_code equals 0
    return Ok("Success")
  else
    return Err("HTTP request failed")
  end
end
```

---

## Best Practices Summary

### ✅ DO

- Check all pointer results for null
- Free all allocated memory
- Match C types exactly
- Use bounded string functions (strncpy, not strcpy)
- Wrap C functions in safe NexusLang functions
- Test FFI code thoroughly
- Document C function signatures
- Use try/finally for cleanup

### ❌ DON'T

- Ignore allocation failures
- Use freed memory
- Overflow buffers
- Mix up parameter order
- Forget to close file handles
- Assume pointer validity
- Skip error checking
- Use variadic functions without format verification

---

## Further Reading

- **Struct/Union Guide:** [struct_union.md](struct_union.md)
- **Inline Assembly:** [inline_assembly.md](inline_assembly.md)
- **Memory Management:** [memory.md](memory.md)
- **Pointers:** [pointers.md](pointers.md)
- **C Standard Library:** https://en.cppreference.com/w/c
- **POSIX API:** https://pubs.opengroup.org/onlinepubs/9699919799/

---

## Summary

NLPL's FFI provides:

✅ **C library integration** - Call any C function  
✅ **Variadic functions** - printf, fprintf, scanf support  
✅ **Type conversion** - Automatic NexusLang ↔ C type mapping  
✅ **Struct marshalling** - Pass structs to/from C  
✅ **Pointer handling** - Full pointer operations  
✅ **Memory management** - malloc/free integration  

**Use FFI when:**
- You need to use existing C libraries
- You're interfacing with OS APIs
- You need maximum performance
- You're doing system programming

**Avoid FFI when:**
- NexusLang stdlib provides the functionality
- You don't need raw performance
- You want memory safety guarantees
- Cross-platform portability is critical

**Status:** Production-ready with full variadic function support!
