# NexusLang Memory Management

## Overview

NLPL provides low-level memory management capabilities similar to C, allowing manual control over memory allocation, deallocation, and reallocation. This is essential for systems programming, performance-critical applications, and implementing custom data structures.

## Memory Management Functions

### `alloc` - Allocate Memory
```nlpl
set ptr to alloc with size_in_bytes
```

Allocates a block of memory of the specified size in bytes. Returns a pointer to the allocated memory (`i8*` in LLVM IR).

**Example:**
```nlpl
# Allocate 40 bytes (enough for 5 64-bit integers)
set data_ptr to alloc with 40
```

### `dealloc` - Free Memory
```nlpl
call dealloc with pointer
```

Frees previously allocated memory. The pointer must have been returned by `alloc` or `realloc`. Returns `void` (no value).

**Example:**
```nlpl
call dealloc with data_ptr
```

### `realloc` - Reallocate Memory
```nlpl
set ptr to realloc with old_ptr and new_size
```

Changes the size of a previously allocated memory block. If the new size is larger, the additional memory is uninitialized. If smaller, excess memory is freed. Returns a pointer to the reallocated memory (may be the same or different address).

**Example:**
```nlpl
# Expand allocation from 40 to 80 bytes
set data_ptr to realloc with data_ptr and 80
```

## Pointer Operations

### Address-Of Operator (Future)
```nlpl
set ptr to address of variable
```

**Note:** Currently planned but not yet implemented.

### Dereference - Read from Pointer
```nlpl
set value to value at pointer
```

Reads an integer (64-bit) value from the memory location pointed to by the pointer.

**Example:**
```nlpl
set data_ptr to alloc with 8
set (value at data_ptr) to 100
set retrieved_value to value at data_ptr  # retrieved_value = 100
```

### Dereference - Write to Pointer
```nlpl
set (value at pointer) to value
```

Writes an integer (64-bit) value to the memory location pointed to by the pointer.

**Example:**
```nlpl
set data_ptr to alloc with 8
set (value at data_ptr) to 42
```

### Pointer Arithmetic
```nlpl
set new_ptr to pointer plus offset_in_bytes
set new_ptr to pointer minus offset_in_bytes
```

Adds or subtracts a byte offset from a pointer, allowing navigation through memory.

**Example:**
```nlpl
# Allocate space for 2 integers (16 bytes)
set array_ptr to alloc with 16

# Write to first integer
set (value at array_ptr) to 10

# Move to second integer (8 bytes later)
set second_ptr to array_ptr plus 8
set (value at second_ptr) to 20

# Read both values
set first to value at array_ptr      # first = 10
set second to value at second_ptr    # second = 20
```

## Implementation Details

### LLVM IR Representation

- **`alloc`**: Wraps `malloc()` from C standard library, returns `i8*`
- **`dealloc`**: Wraps `free()`, takes `i8*`, returns `void`
- **`realloc`**: Wraps `realloc()`, takes `i8*` and `i64` size, returns `i8*`
- **Dereference**: Pointers are cast from `i8*` to `i64*` for integer read/write operations
- **Pointer arithmetic**: Uses LLVM `getelementptr` instruction for safe pointer offsetting

### Type System

- All pointers from `alloc`/`realloc` are typed as `i8*` (generic byte pointer)
- When dereferencing, pointers are automatically cast to `i64*` for integer operations
- Future support planned for typed pointers (e.g., `Integer*`, `MyStruct*`)

### Memory Safety

**Current Status:** No automatic safety checks. Memory management is manual and requires discipline.

**Planned Safety Features:**
- Null pointer checks before dereference
- Bounds checking for array-like access
- Double-free detection
- Memory leak warnings
- Use-after-free detection (in debug mode)

## Complete Example

```nlpl
# NexusLang Memory Management Demo
print text "=== NexusLang Memory Management Demo ==="

# Allocate memory for 5 integers (8 bytes each = 40 bytes)
set data_ptr to alloc with 40
print text "Allocated 40 bytes"

# Write values using pointer dereference
set (value at data_ptr) to 100
print text "Stored 100 at data_ptr"

# Read it back
set first_value to value at data_ptr
print text "Read value: "
print number first_value

# Pointer arithmetic to access second element (8 bytes later)
set second_ptr to data_ptr plus 8
set (value at second_ptr) to 200
print text "Stored 200 at offset 8"

set second_value to value at second_ptr
print text "Read value at offset 8: "
print number second_value

# Reallocate to bigger size (80 bytes for 10 integers)
set data_ptr to realloc with data_ptr and 80
print text "Reallocated to 80 bytes"

# Verify first value still there
set check_value to value at data_ptr
print text "First value still: "
print number check_value

# Clean up
call dealloc with data_ptr
print text "Memory freed successfully"
```

**Output:**
```
=== NexusLang Memory Management Demo ===
Allocated 40 bytes
Stored 100 at data_ptr
Read value: 
100
Stored 200 at offset 8
Read value at offset 8: 
200
Reallocated to 80 bytes
First value still: 
100
Memory freed successfully
```

## Common Patterns

### Dynamic Array
```nlpl
# Allocate array for N integers
set count to 10
set bytes to count times 8
set array to alloc with bytes

# Initialize elements
set index to 0
while index is less than count
    set offset to index times 8
    set element_ptr to array plus offset
    set (value at element_ptr) to index times 100
    set index to index plus 1
end

# Access elements
set third_ptr to array plus 16  # offset = 2 * 8
set third_value to value at third_ptr

# Clean up
call dealloc with array
```

### Resizable Buffer
```nlpl
# Start with small buffer
set buffer to alloc with 64
set current_size to 64

# ... use buffer ...

# Need more space? Reallocate
set new_size to current_size times 2
set buffer to realloc with buffer and new_size
set current_size to new_size

# Clean up
call dealloc with buffer
```

## Best Practices

1. **Always free allocated memory**: Call `dealloc` for every `alloc`/`realloc`
2. **Don't use freed memory**: After `dealloc`, don't dereference the pointer
3. **Don't free twice**: Only call `dealloc` once per allocation
4. **Check allocation success**: Future versions will support null checks
5. **Use realloc carefully**: Pointer may change, update all references
6. **Mind alignment**: 64-bit integers are 8 bytes, pointers should be 8-byte aligned

## Limitations (Current Version)

- No automatic garbage collection
- No ownership tracking
- No bounds checking
- No null pointer checks
- Dereference only supports 64-bit integers
- No support for storing pointers in memory (pointer-to-pointer)
- No custom struct member access via pointers (planned)

## Future Enhancements

- **Typed pointers**: `Integer*`, `Float*`, `MyStruct*` with automatic size calculation
- **Struct member access**: `value at (ptr.field)`
- **Pointer-to-pointer**: `set ptr_ptr to address of ptr`, `set val to value at (value at ptr_ptr)`
- **Memory pools**: Custom allocators for performance
- **Smart pointers**: RAII-style automatic deallocation
- **Array bounds**: Optionally store size metadata with allocations

## Testing

Comprehensive tests available in:
- `test_programs/compiler/test_memory_complete.nlpl` - Full feature test suite
- `test_programs/compiler/test_memory_demo.nlpl` - Simple demonstration
- `test_programs/compiler/test_alloc_toplevel.nlpl` - Basic alloc/dealloc

Run tests:
```bash
./nlplc test_programs/compiler/test_memory_complete.nlpl --run
```

## Technical Notes

### Parser Syntax
- **Allocation**: `alloc with size` (function call syntax)
- **Deallocation**: `dealloc with ptr` (statement call syntax)
- **Reallocation**: `realloc with ptr and size` (function call syntax)
- **Dereference read**: `value at ptr` (unary expression)
- **Dereference write**: `set (value at ptr) to value` (assignment with parenthesized dereference)

### AST Nodes
- `FunctionCall(name='alloc', arguments=[size_expr])` for allocation
- `FunctionCall(name='dealloc', arguments=[ptr_expr])` for deallocation
- `FunctionCall(name='realloc', arguments=[ptr_expr, size_expr])` for reallocation
- `DereferenceExpression(pointer=ptr_expr)` for reading
- `DereferenceAssignment(target=DereferenceExpression(ptr_expr), value=value_expr)` for writing

### LLVM IR Generation
- Memory functions wrapped via `_generate_memory_function_call()`
- Dereference expressions via `_generate_dereference_expression()`
- Dereference assignments via `_generate_dereference_assignment()`
- Pointer arithmetic via modified `_generate_binary_operation()` using `getelementptr`

## See Also

- [Pointer Operations Example](../examples/23_pointer_operations.nxl) - Comprehensive pointer usage
- [Struct and Union Example](../examples/24_struct_and_union.nxl) - Using memory with custom types
- [ROADMAP.md](../ROADMAP.md) - Future memory management features
