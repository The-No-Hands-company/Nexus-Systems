# NexusLang FFI Quick Reference Guide

## Basic FFI Usage

### Declaring External Functions

```nlpl
# Basic external function
extern function printf with format as Pointer returns Integer from library "c"

# Multiple parameters
extern function strcmp with str1 as Pointer with str2 as Pointer returns Integer from library "c"

# Different calling conventions
foreign function WinAPI_Call with handle as Integer returns Integer from library "kernel32" calling convention stdcall

# No return value
extern function exit with code as Integer returns nothing from library "c"
```

### Declaring External Variables

```nlpl
# External global variable
extern variable errno as Integer from library "c"
extern variable stdin as Pointer from library "c"
```

### Calling External Functions

```nlpl
# Simple call
printf("Hello, World!\n")

# With variables
set msg to "Value: %d\n"
set value to 42
printf(msg, value)

# Storing result
set length to strlen("Hello")
set comparison to strcmp("apple", "banana")
```

---

## Common C Library Functions

### stdio.h (Input/Output)

```nlpl
extern function printf with format as Pointer returns Integer from library "c"
extern function scanf with format as Pointer returns Integer from library "c"
extern function puts with str as Pointer returns Integer from library "c"
extern function putchar with c as Integer returns Integer from library "c"
extern function getchar returns Integer from library "c"
extern function fprintf with stream as Pointer with format as Pointer returns Integer from library "c"
extern function sprintf with buffer as Pointer with format as Pointer returns Integer from library "c"
```

**Usage**:
```nlpl
# Print text
printf("Hello, %s!\n", name)

# Read input
scanf("%d", address of number)

# Simple output
puts("Simple message")
```

### stdlib.h (Memory & Utilities)

```nlpl
extern function malloc with size as Integer returns Pointer from library "c"
extern function calloc with count as Integer with size as Integer returns Pointer from library "c"
extern function realloc with ptr as Pointer with size as Integer returns Pointer from library "c"
extern function free with ptr as Pointer from library "c"
extern function exit with code as Integer from library "c"
extern function atoi with str as Pointer returns Integer from library "c"
extern function atof with str as Pointer returns Float from library "c"
```

**Usage**:
```nlpl
# Allocate memory
set buffer to malloc(1024)

# Use memory
# ... do work ...

# Free memory
free(buffer)

# Convert string to number
set text to "42"
set number to atoi(text)
```

### string.h (String Operations)

```nlpl
extern function strlen with str as Pointer returns Integer from library "c"
extern function strcmp with str1 as Pointer with str2 as Pointer returns Integer from library "c"
extern function strncmp with str1 as Pointer with str2 as Pointer with n as Integer returns Integer from library "c"
extern function strcpy with dest as Pointer with src as Pointer returns Pointer from library "c"
extern function strncpy with dest as Pointer with src as Pointer with n as Integer returns Pointer from library "c"
extern function strcat with dest as Pointer with src as Pointer returns Pointer from library "c"
extern function memcpy with dest as Pointer with src as Pointer with n as Integer returns Pointer from library "c"
extern function memset with ptr as Pointer with value as Integer with num as Integer returns Pointer from library "c"
```

**Usage**:
```nlpl
# Get string length
set text to "Hello"
set len to strlen(text)

# Compare strings
set result to strcmp("apple", "banana") # result < 0

# Copy strings
set dest to malloc(100)
strcpy(dest, "Hello")
```

### math.h (Mathematics)

```nlpl
extern function sin with x as Float returns Float from library "m"
extern function cos with x as Float returns Float from library "m"
extern function tan with x as Float returns Float from library "m"
extern function sqrt with x as Float returns Float from library "m"
extern function pow with base as Float with exponent as Float returns Float from library "m"
extern function exp with x as Float returns Float from library "m"
extern function log with x as Float returns Float from library "m"
extern function floor with x as Float returns Float from library "m"
extern function ceil with x as Float returns Float from library "m"
extern function abs with x as Integer returns Integer from library "m"
extern function fabs with x as Float returns Float from library "m"
```

**Usage**:
```nlpl
# Calculate square root
set num to 16.0
set result to sqrt(num) # 4.0

# Power function
set squared to pow(5.0, 2.0) # 25.0

# Trigonometry
set angle to 1.5708 # pi/2
set sine to sin(angle) # ~1.0
```

---

## Type Mappings

### NexusLang C Type Correspondence

| NexusLang Type | C Type | LLVM Type | Notes |
|--------------|------------------|-----------|--------------------------|
| `Integer` | `long` (64-bit) | `i64` | Default integer type |
| `Int8` | `char` | `i8` | 8-bit signed |
| `Int16` | `short` | `i16` | 16-bit signed |
| `Int32` | `int` | `i32` | 32-bit signed |
| `Int64` | `long` | `i64` | 64-bit signed |
| `UInt8` | `unsigned char` | `i8` | 8-bit unsigned |
| `UInt16` | `unsigned short` | `i16` | 16-bit unsigned |
| `UInt32` | `unsigned int` | `i32` | 32-bit unsigned |
| `UInt64` | `unsigned long` | `i64` | 64-bit unsigned |
| `Float` | `double` | `double` | Double precision |
| `Float32` | `float` | `float` | Single precision |
| `Float64` | `double` | `double` | Double precision |
| `Boolean` | `bool` / `int` | `i1` | True/False |
| `Char` | `char` | `i8` | Single character |
| `Pointer` | `void*` | `i8*` | Generic pointer |
| `String` | `char*` | `i8*` | Null-terminated string |
| `nothing` | `void` | `void` | No return value |

---

## Common Patterns

### Pattern 1: Memory Allocation

```nlpl
extern function malloc with size as Integer returns Pointer from library "c"
extern function free with ptr as Pointer from library "c"

function allocate_buffer with size as Integer returns Pointer
 set buffer to malloc(size)
 
 if buffer is null
 print text "Memory allocation failed!"
 return null
 end
 
 return buffer
end

function cleanup with buffer as Pointer
 if buffer is not null
 free(buffer)
 end
end
```

### Pattern 2: String Operations

```nlpl
extern function strlen with str as Pointer returns Integer from library "c"
extern function strcpy with dest as Pointer with src as Pointer returns Pointer from library "c"
extern function malloc with size as Integer returns Pointer from library "c"

function duplicate_string with original as String returns String
 set len to strlen(original)
 set copy to malloc(len plus 1) # +1 for null terminator
 
 if copy is not null
 strcpy(copy, original)
 end
 
 return copy
end
```

### Pattern 3: Math Operations

```nlpl
extern function sqrt with x as Float returns Float from library "m"
extern function pow with base as Float with exponent as Float returns Float from library "m"

function distance with x1 as Float with y1 as Float with x2 as Float with y2 as Float returns Float
 set dx to x2 minus x1
 set dy to y2 minus y1
 set dx_squared to pow(dx, 2.0)
 set dy_squared to pow(dy, 2.0)
 set sum to dx_squared plus dy_squared
 return sqrt(sum)
end
```

### Pattern 4: File Operations

```nlpl
extern function fopen with filename as Pointer with mode as Pointer returns Pointer from library "c"
extern function fclose with stream as Pointer returns Integer from library "c"
extern function fprintf with stream as Pointer with format as Pointer returns Integer from library "c"

function write_to_file with filename as String with content as String
 set file to fopen(filename, "w")
 
 if file is not null
 fprintf(file, "%s", content)
 fclose(file)
 return true
 end
 
 return false
end
```

---

## Platform-Specific Libraries

### Linux/Unix Libraries

```nlpl
# POSIX threads
extern function pthread_create with thread as Pointer with attr as Pointer with start_routine as Pointer with arg as Pointer returns Integer from library "pthread"

# Dynamic linking
extern function dlopen with filename as Pointer with flag as Integer returns Pointer from library "dl"
extern function dlsym with handle as Pointer with symbol as Pointer returns Pointer from library "dl"
extern function dlclose with handle as Pointer returns Integer from library "dl"

# POSIX file operations
extern function open with pathname as Pointer with flags as Integer returns Integer from library "c"
extern function read with fd as Integer with buf as Pointer with count as Integer returns Integer from library "c"
extern function write with fd as Integer with buf as Pointer with count as Integer returns Integer from library "c"
extern function close with fd as Integer returns Integer from library "c"
```

### Windows Libraries

```nlpl
# Windows API (note: use stdcall for Windows API)
extern function MessageBoxA with hwnd as Integer with text as Pointer with caption as Pointer with type as Integer returns Integer from library "user32" calling convention stdcall

extern function GetLastError returns Integer from library "kernel32" calling convention stdcall

extern function CreateFileA with filename as Pointer with access as Integer with share as Integer with security as Pointer with creation as Integer with flags as Integer with template as Pointer returns Pointer from library "kernel32" calling convention stdcall
```

---

## Error Handling

### Checking Return Values

```nlpl
extern function malloc with size as Integer returns Pointer from library "c"
extern function printf with format as Pointer returns Integer from library "c"

function safe_malloc with size as Integer returns Pointer
 set buffer to malloc(size)
 
 if buffer is null
 printf("ERROR: malloc failed for %d bytes\n", size)
 # Handle error (exit, throw, return error code, etc.)
 end
 
 return buffer
end
```

### Using errno

```nlpl
extern variable errno as Integer from library "c"
extern function strerror with errnum as Integer returns Pointer from library "c"
extern function printf with format as Pointer returns Integer from library "c"

function print_error
 set error_code to errno
 set error_msg to strerror(error_code)
 printf("Error %d: %s\n", error_code, error_msg)
end
```

---

## Best Practices

### 1. Always Check Pointers
```nlpl
set buffer to malloc(1024)
if buffer is null
 # Handle error
 return error
end
# ... use buffer ...
free(buffer)
```

### 2. Match Allocation with Deallocation
```nlpl
# Good: Free what you allocate
set data to malloc(100)
# ... use data ...
free(data)

# Bad: Memory leak
set data to malloc(100)
# ... forgot to free!
```

### 3. Use Correct Types
```nlpl
# Good: Correct type mapping
extern function strlen with str as Pointer returns Integer from library "c"

# Bad: Wrong return type
extern function strlen with str as Pointer returns Float from library "c" # WRONG!
```

### 4. Specify Libraries Correctly
```nlpl
# Good: Explicit library specification
extern function sqrt with x as Float returns Float from library "m"

# Unclear: Missing library (may not link correctly)
extern function sqrt with x as Float returns Float
```

---

## Debugging Tips

### 1. Print Debugging
```nlpl
extern function printf with format as Pointer returns Integer from library "c"

printf("DEBUG: Variable x = %d\n", x)
printf("DEBUG: Pointer = %p\n", ptr)
```

### 2. Check NULL Pointers
```nlpl
if my_pointer is null
 printf("ERROR: Pointer is NULL!\n")
end
```

### 3. Validate Return Codes
```nlpl
set result to some_c_function()
printf("Function returned: %d\n", result)

if result is less than 0
 printf("ERROR: Function failed!\n")
end
```

---

## Advanced Features (Future)

### Callbacks (Planned)
```nlpl
# Pass NexusLang function to C library
function my_comparator with a as Integer with b as Integer returns Integer
 if a is less than b
 return -1
 else if a is greater than b
 return 1
 else
 return 0
 end
end

extern function qsort with base as Pointer with count as Integer with size as Integer with comparator as Pointer from library "c"

# Call with callback
qsort(array, 10, 8, address of my_comparator)
```

### Variadic Functions (Planned)
```nlpl
extern variadic function printf with format as Pointer returns Integer from library "c"

# Variable arguments
printf("Name: %s, Age: %d, Score: %f\n", name, age, score)
```

---

## Reference

### Documentation
- FFI Implementation Status: `FFI_IMPLEMENTATION_STATUS.md`
- Session Summary: `FFI_SESSION_SUMMARY.md`

### Test Programs
- `test_programs/ffi/test_printf.nlpl`
- `test_programs/ffi/test_malloc.nlpl`
- `test_programs/ffi/test_math.nlpl`
- `test_programs/ffi/test_strings.nlpl`

### Source Files
- `src/nexuslang/compiler/ffi.py` - FFI code generation
- `src/nexuslang/parser/parser.py` - extern_declaration()
- `src/nexuslang/parser/ast.py` - FFI AST nodes
- `src/nexuslang/stdlib/ffi/` - Runtime FFI support

---

**Last Updated**: 2025-11-25 
**Version**: 1.0 (Phase 1 Complete)
