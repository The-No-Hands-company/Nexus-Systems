# Tutorial 9: FFI and C Libraries

**Time:** ~60 minutes  
**Prerequisites:** [Error Handling](../beginner/04-error-handling.md), [Objects and Classes](../beginner/03-objects-and-classes.md)

---

## Part 1 — What Is FFI?

The **Foreign Function Interface** (FFI) lets NexusLang call functions compiled in
C or any C-compatible language.  This gives NexusLang access to the vast world of
existing C libraries: compression, cryptography, image processing, databases,
hardware drivers, and more.

**Safety note:** FFI is a powerful but unsafe feature.  Bugs in C code can
crash the interpreter or corrupt memory.  Always validate arguments before
passing them across the FFI boundary.

Enable FFI access with:

```bash
PYTHONPATH=src python -m nexuslang.main program.nlpl --allow-ffi
```

---

## Part 2 — Declaring an External Function

```nexuslang
# extern function NAME with PARAMS returns RETURN_TYPE from library "LIB"
extern function strlen with str as Pointer returns Integer from library "c"
```

`library "c"` resolves to `libc.so.6` on Linux, `libc.dylib` on macOS, and
`msvcrt.dll` on Windows.

---

## Part 3 — Calling the Declared Function

```nexuslang
extern function strlen with str as Pointer returns Integer from library "c"

set message to "Hello, World!"
set length to call strlen with message
print text "Length: " plus convert length to string    # Length: 13
```

---

## Part 4 — Common C Standard Library Calls

### `abs` (absolute value)

```nexuslang
extern function abs with n as Integer returns Integer from library "c"

print text convert (call abs with -42) to string    # 42
```

### `strcmp` (string comparison)

```nexuslang
extern function strcmp with s1 as Pointer and s2 as Pointer returns Integer from library "c"

set a to "apple"
set b to "banana"
set cmp to call strcmp with a and b
if cmp is less than 0
    print text a plus " comes before " plus b
else if cmp is greater than 0
    print text b plus " comes before " plus a
else
    print text "Strings are equal"
```

---

## Part 5 — Using Structs with FFI

Define an NexusLang struct that matches a C struct layout:

```nexuslang
# C definition:
# typedef struct { int x; int y; } Point2D;

struct Point2D
    x as Integer
    y as Integer
end

extern function render_point with p as Pointer returns Integer from library "myrenderer"

set p to create Point2D
set p.x to 100
set p.y to 200
set ptr to address of p
set result to call render_point with ptr
```

---

## Part 6 — Loading a Third-Party Library

Suppose you want to use **zlib** for compression:

```nexuslang
extern function compress with dest as Pointer and dest_len as Pointer and src as Pointer and src_len as Integer returns Integer from library "z"

extern function uncompress with dest as Pointer and dest_len as Pointer and src as Pointer and src_len as Integer returns Integer from library "z"
```

For a full Zlib binding example, see [examples/07_low_level/03_ffi_c_interop.nlpl](../../examples/07_low_level/03_ffi_c_interop.nxl).

---

## Part 7 — Error Handling at the FFI Boundary

C functions communicate errors through return codes, `errno`, or output
parameters — not exceptions.  Check return values explicitly:

```nexuslang
extern function fopen with path as Pointer and mode as Pointer returns Pointer from library "c"
extern function fclose with fp as Pointer returns Integer from library "c"

set fp to call fopen with "data.bin" and "rb"
if fp equals null
    raise error with "Failed to open file"

# ... use file ...

call fclose with fp
```

---

## Part 8 — Resource Safety Pattern

Always free C-allocated resources even when errors occur:

```nexuslang
set fp to null
try
    set fp to call fopen with "input.dat" and "rb"
    if fp equals null
        raise error with "Cannot open input.dat"
    # ... read data ...
catch error with message
    print text "FFI error: " plus message
always
    if fp is not null
        call fclose with fp
```

---

## Part 9 — Sandbox FFI Calls

Run FFI-heavy code in a sandbox to limit blast radius:

```nexuslang
from nexuslang.security.sandbox import Sandbox, SandboxPolicy

set policy to create SandboxPolicy with allow_ffi: true and max_memory_mb: 256
set sandbox to create Sandbox with policy
sandbox.enter()
try
    call_heavy_c_library()
catch error with message
    print text "Error: " plus message
always
    sandbox.exit()
```

---

## Summary

| Concept | Syntax |
|---------|--------|
| Declare extern | `extern function F with P as T returns R from library "lib"` |
| Call extern | `call F with args` |
| Null check | `if result equals null` |
| Struct + pointer | `set ptr to address of struct_var` |

**Next:** [Building Projects](05-building-projects.md)
