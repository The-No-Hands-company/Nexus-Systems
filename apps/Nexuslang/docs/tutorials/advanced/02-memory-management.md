# Tutorial 12: Memory Management Deep Dive

**Time:** ~90 minutes  
**Prerequisites:** [FFI and C Libraries](../intermediate/04-ffi-and-c-libraries.md), [Objects and Classes](../beginner/03-objects-and-classes.md)

---

## Part 1 — NexusLang Memory Model

NLPL uses automatic memory management for most values — you do not need to
free ordinary variables, lists, or objects.  Manual memory management is
available for:

- Passing raw buffers to C functions via FFI
- Implementing custom allocators or data structures
- Precise control over allocation placement (e.g., DMA buffers)

---

## Part 2 — Automatic Memory

For everyday programming, NexusLang automatically manages object lifetimes:

```nexuslang
function make_greeting with name as String returns String
    set msg to "Hello, " plus name plus "!"   # allocated automatically
    return msg                                 # caller now owns msg
end
# msg is freed when no longer referenced
```

---

## Part 3 — Pointers

### Taking an Address

```nexuslang
set x to 42
set ptr to address of x      # ptr holds the memory address of x
```

### Dereferencing

```nexuslang
set val to dereference ptr   # read the value at the address
# or:
set val to value at ptr
```

### Writing Through a Pointer

```nexuslang
set (value at ptr) to 99    # x is now 99
```

---

## Part 4 — `sizeof`

`sizeof` returns the byte size of a type or value:

```nexuslang
print text convert (sizeof Integer) to string   # 8 (64-bit)
print text convert (sizeof Float)   to string   # 8
print text convert (sizeof Boolean) to string   # 1

struct Color
    r as Integer
    g as Integer
    b as Integer
end

print text convert (sizeof Color) to string     # 24
```

---

## Part 5 — Manual Allocation

### Allocating a Raw Buffer

```nexuslang
allocate buffer of size 1024 bytes
set ptr to address of buffer

# Write a byte
set (value at ptr) to 255

# Offset access (byte index 4)
set (value at (ptr plus 4)) to 42

# Always free manually allocated memory
free buffer
```

### Always Free in `always`

```nexuslang
allocate buffer of size 4096 bytes
try
    # ... use buffer ...
    call c_library_function with (address of buffer)
catch error with message
    print text "Error: " plus message
always
    free buffer    # runs even if an error occurred
```

---

## Part 6 — Pointer to Pointer

Two levels of indirection (useful for output parameters in C APIs):

```nexuslang
set value to 100
set ptr1 to address of value
set ptr2 to address of ptr1

# Double dereference
set inner_ptr to value at ptr2
set inner_val to value at inner_ptr   # 100
```

---

## Part 7 — Typed Pointers

Declare a pointer with an explicit pointee type for clarity:

```nexuslang
set int_ptr   to 0 as Pointer to Integer
set float_ptr to 0 as Pointer to Float

struct Node
    data as Integer
    next as Pointer to Node
end

set n to create Node
set n.data to 7
set n.next to 0 as Pointer to Node   # null pointer for leaf
```

---

## Part 8 — Array Pointer Arithmetic

```nexuslang
set arr to [10, 20, 30, 40, 50]
set base to address of arr[0]

# Read element at offset (assuming 8-byte integers)
set element_size to sizeof Integer
set ptr_to_3rd to base plus (2 times element_size)
set third to value at ptr_to_3rd     # 30
```

---

## Part 9 — Function Pointers

Store a reference to a function and call it later:

```nexuslang
function add with a as Integer and b as Integer returns Integer
    return a plus b
end

function multiply with a as Integer and b as Integer returns Integer
    return a times b
end

# Store function address
set op to address of add
set r1 to call (value at op) with 4 and 5    # 9

set op to address of multiply
set r2 to call (value at op) with 4 and 5    # 20
```

This is the foundation for vtables, callbacks, and plugin systems.

---

## Part 10 — Avoiding Use-After-Free

Once a buffer is freed, any pointer to it is dangling.  Use the
`MemorySafetyValidator` from the security module to catch this in
development:

```nexuslang
from nexuslang.security.analysis import MemorySafetyValidator

set mv to create MemorySafetyValidator

allocate buf of size 64 bytes
set ptr to address of buf

mv.record_alloc with ptr    # tell the validator this address is live
# ... use ptr ...
free buf
mv.record_free with ptr     # mark as freed

# Later (illegal access):
try
    mv.check_not_freed with ptr and location: "main.nlpl:45"
    set val to value at ptr   # would be undefined behaviour
catch error with message
    print text "Use-after-free detected: " plus message
```

---

## Part 11 — Custom Allocator Sketch

```nexuslang
class ArenaAllocator
    private set memory as Pointer
    private set capacity as Integer
    private set offset as Integer

    public function initialize with size_bytes as Integer
        allocate raw of size size_bytes bytes
        set this.memory to address of raw
        set this.capacity to size_bytes
        set this.offset to 0

    public function alloc with size as Integer returns Pointer
        if this.offset plus size is greater than this.capacity
            raise error with "Arena out of memory"
        set result to this.memory plus this.offset
        set this.offset to this.offset plus size
        return result

    public function reset
        set this.offset to 0

    public function destroy
        free (value at this.memory)
```

---

## Summary

| Concept | Syntax |
|---------|--------|
| Take address | `set ptr to address of var` |
| Dereference read | `value at ptr` |
| Dereference write | `set (value at ptr) to val` |
| Type size | `sizeof Type` |
| Manual alloc | `allocate name of size N bytes` |
| Manual free | `free name` |
| Typed pointer | `0 as Pointer to Integer` |

**Next:** [Performance Optimization](03-performance-optimization.md)
