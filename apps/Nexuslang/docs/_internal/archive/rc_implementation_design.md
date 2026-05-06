# Reference Counting (Rc<T>) Implementation Design

**Status**: 🔄 In Progress - Week 3 Day 1  
**Feature**: Smart pointers with automatic memory management  
**Target**: Enable complex data structures (trees, graphs, caches)

---

## Overview

Reference counting is NLPL's first automatic memory management feature. It enables shared ownership of heap-allocated data with automatic deallocation when the last reference is dropped.

**Key Benefits:**
- Automatic memory management (no manual free)
- Shared ownership (multiple references to same data)
- Predictable deallocation (when refcount reaches zero)
- Foundation for complex data structures

---

## Design Goals

### 1. Natural Syntax
**Principle**: Should read like natural language while being precise

**Good:**
```nlpl
set shared_data to Rc of Integer with 42
set another_ref to shared_data  # Automatic retain
# Automatic release when out of scope
```

**Avoid:**
```nlpl
set ptr to new_rc_integer(42)  # Too C-like
call retain(ptr)  # Manual management (defeats purpose)
```

### 2. Zero-Cost When Not Used
**Principle**: Programs not using Rc should have zero overhead

**Implementation:**
- Conditional IR generation (like coroutines)
- No runtime penalty for non-Rc programs
- Compile-time detection of Rc usage

### 3. Safety First
**Principle**: Prevent common memory errors

**Protected Against:**
- Use-after-free (refcount prevents premature deallocation)
- Double-free (automatic deallocation)
- Memory leaks (automatic cleanup)

**Not Protected Against (yet):**
- Circular references (need Weak<T> - Week 3 Day 3-4)
- Thread-safety (need Arc<T> - Week 5)

### 4. Debuggability
**Principle**: Easy to understand and debug

**Features:**
- Clear error messages for type mismatches
- Debug mode: track allocation/deallocation
- Leak detection mode (optional)

---

## NexusLang Syntax Design

### Type Declaration

**Syntax:**
```nlpl
Rc of Type
```

**Examples:**
```nlpl
set counter to Rc of Integer with 0
set name to Rc of String with "Alice"
set data to Rc of List of Integer with list 1, 2, 3
```

**Nested:**
```nlpl
set tree_node to Rc of Node
```

### Creation

**Syntax:**
```nlpl
set variable to Rc of Type with value
```

**Semantics:**
- Allocates memory on heap
- Initializes refcount to 1
- Returns Rc<T> smart pointer

**Examples:**
```nlpl
# Primitive types
set num to Rc of Integer with 42
set flag to Rc of Boolean with true

# Struct types
struct Point
    x as Integer
    y as Integer
end

set origin to Rc of Point with Point {x: 0, y: 0}

# Collections
set numbers to Rc of List of Integer with list 1, 2, 3
```

### Cloning (Incrementing Refcount)

**Syntax:**
```nlpl
set new_ref to existing_rc  # Automatic retain
```

**Semantics:**
- Increments refcount
- Both variables reference same data
- No data copying

**Example:**
```nlpl
set original to Rc of Integer with 42
set copy to original  # refcount: 1 -> 2
# Both point to same Integer(42)
```

### Dereferencing (Accessing Value)

**Syntax:**
```nlpl
# Automatic dereferencing (transparent)
set value to my_rc  # Gets the wrapped value
set my_rc to new_value  # Updates the wrapped value
```

**Example:**
```nlpl
set counter to Rc of Integer with 0
set counter to counter plus 1  # Automatic dereference and update
print counter  # Prints 1
```

### Dropping (Decrementing Refcount)

**Syntax:**
```nlpl
# Automatic when variable goes out of scope
```

**Semantics:**
- Decrements refcount
- If refcount reaches 0, deallocates memory
- Implicit at end of scope

**Example:**
```nlpl
function create_data returns Rc of Integer
    set data to Rc of Integer with 42
    return data  # refcount: 1
end

set my_data to call create_data  # refcount: 1
set copy to my_data  # refcount: 2
# my_data scope ends, refcount: 2 -> 1
# copy scope ends, refcount: 1 -> 0, memory freed
```

---

## Type System Integration

### RcType Class

**Location:** `src/nlpl/typesystem/types.py`

**Structure:**
```python
class RcType:
    def __init__(self, inner_type):
        self.inner_type = inner_type  # Type being wrapped
        self.name = f"Rc<{inner_type.name}>"
    
    def is_compatible_with(self, other):
        if not isinstance(other, RcType):
            return False
        return self.inner_type.is_compatible_with(other.inner_type)
    
    def can_dereference_to(self):
        return self.inner_type
```

**Properties:**
- **inner_type**: The wrapped type (Integer, String, custom struct, etc.)
- **name**: Display name for error messages
- **is_compatible_with()**: Type checking for assignments
- **can_dereference_to()**: Automatic dereferencing support

### Type Checking Rules

**Assignment:**
```nlpl
set rc1 to Rc of Integer with 42
set rc2 to rc1  # OK: Rc<Integer> = Rc<Integer>
set rc3 to Rc of String with "hi"
set rc1 to rc3  # ERROR: Rc<Integer> != Rc<String>
```

**Dereferencing:**
```nlpl
set rc to Rc of Integer with 42
set value to rc  # OK: Automatic dereference to Integer
set rc to 10  # OK: Wraps Integer into Rc<Integer>
```

**Function Parameters:**
```nlpl
function process with data as Rc of Integer
    print data  # Automatic dereference
end

set num to Rc of Integer with 42
call process with num  # OK
call process with 42  # ERROR: Integer != Rc<Integer>
```

---

## Runtime Implementation

### Memory Layout

**Rc<T> Structure:**
```c
typedef struct {
    size_t refcount;     // Reference count
    T data;              // Actual data (inline)
} RcBox;

typedef RcBox* Rc;  // Rc is a pointer to RcBox
```

**Memory Diagram:**
```
Stack:                    Heap:
┌─────────┐              ┌──────────────┐
│ rc1: ptr│─────────────>│ refcount: 2  │
└─────────┘              │ data: 42     │
┌─────────┐              └──────────────┘
│ rc2: ptr│─────────────>│              │
└─────────┘              └──────────────┘
                              ^
                              Same allocation
```

### Runtime Functions

**Location:** `src/nlpl/runtime/rc_runtime.c` (new file)

#### 1. rc_new()
**Signature:**
```c
void* rc_new(size_t data_size, void* initial_value);
```

**Implementation:**
```c
void* rc_new(size_t data_size, void* initial_value) {
    // Allocate: sizeof(size_t) for refcount + data_size
    size_t total_size = sizeof(size_t) + data_size;
    void* ptr = malloc(total_size);
    
    if (!ptr) {
        fprintf(stderr, "RC allocation failed\n");
        exit(1);
    }
    
    // Initialize refcount to 1
    *(size_t*)ptr = 1;
    
    // Copy initial value
    void* data_ptr = (char*)ptr + sizeof(size_t);
    if (initial_value) {
        memcpy(data_ptr, initial_value, data_size);
    }
    
    return ptr;  // Return pointer to refcount (box)
}
```

#### 2. rc_retain()
**Signature:**
```c
void* rc_retain(void* rc_ptr);
```

**Implementation:**
```c
void* rc_retain(void* rc_ptr) {
    if (!rc_ptr) return NULL;
    
    // Increment refcount
    size_t* refcount = (size_t*)rc_ptr;
    (*refcount)++;
    
    return rc_ptr;
}
```

#### 3. rc_release()
**Signature:**
```c
void rc_release(void* rc_ptr);
```

**Implementation:**
```c
void rc_release(void* rc_ptr) {
    if (!rc_ptr) return;
    
    // Decrement refcount
    size_t* refcount = (size_t*)rc_ptr;
    (*refcount)--;
    
    // If refcount reaches 0, free memory
    if (*refcount == 0) {
        free(rc_ptr);
    }
}
```

#### 4. rc_get_data()
**Signature:**
```c
void* rc_get_data(void* rc_ptr);
```

**Implementation:**
```c
void* rc_get_data(void* rc_ptr) {
    if (!rc_ptr) return NULL;
    
    // Data is stored after refcount
    return (char*)rc_ptr + sizeof(size_t);
}
```

#### 5. rc_refcount() (Debug)
**Signature:**
```c
size_t rc_refcount(void* rc_ptr);
```

**Implementation:**
```c
size_t rc_refcount(void* rc_ptr) {
    if (!rc_ptr) return 0;
    return *(size_t*)rc_ptr;
}
```

---

## LLVM IR Generation

### Conditional IR Generation

**Flag:** `has_rc_types` (similar to `has_async_functions`)

**Location:** `src/nlpl/compiler/backends/llvm_ir_generator.py`

```python
class LLVMIRGenerator:
    def __init__(self):
        # ...
        self.has_rc_types = False  # Track Rc usage
```

**Detection:**
```python
def visit_RcType(self, node):
    self.has_rc_types = True
    # ... generate IR
```

**Runtime Generation:**
```python
def generate_runtime(self):
    # ... existing runtime ...
    
    if self.has_rc_types:
        self.generate_rc_runtime()
```

### IR for Rc Operations

#### 1. Rc Creation
**NLPL:**
```nlpl
set num to Rc of Integer with 42
```

**Generated IR:**
```llvm
; Allocate and initialize
%temp_value = alloca i64
store i64 42, i64* %temp_value
%rc_ptr = call i8* @rc_new(i64 8, i8* %temp_value)
%num = bitcast i8* %rc_ptr to %RcBox*

; RcBox type definition
%RcBox = type { i64, i64 }  ; { refcount, data }
```

#### 2. Rc Clone (Retain)
**NLPL:**
```nlpl
set copy to original
```

**Generated IR:**
```llvm
%original_ptr = bitcast %RcBox* %original to i8*
%copy_ptr = call i8* @rc_retain(i8* %original_ptr)
%copy = bitcast i8* %copy_ptr to %RcBox*
```

#### 3. Rc Dereference
**NLPL:**
```nlpl
set value to my_rc
```

**Generated IR:**
```llvm
%rc_ptr = bitcast %RcBox* %my_rc to i8*
%data_ptr = call i8* @rc_get_data(i8* %rc_ptr)
%data_ptr_typed = bitcast i8* %data_ptr to i64*
%value = load i64, i64* %data_ptr_typed
```

#### 4. Rc Release (Automatic)
**NLPL:**
```nlpl
# End of scope
```

**Generated IR:**
```llvm
cleanup:
  %rc_ptr = bitcast %RcBox* %my_rc to i8*
  call void @rc_release(i8* %rc_ptr)
  br label %exit
```

### Scope-Based Cleanup

**Strategy:** Insert `rc_release` calls at scope exits

**Implementation:**
```python
def exit_scope(self):
    # Identify all Rc variables in current scope
    for var_name, var_type in self.current_scope.items():
        if isinstance(var_type, RcType):
            # Generate release call
            self.builder.call(self.rc_release_fn, [var_name])
    
    # Exit scope
    self.scopes.pop()
```

---

## Parser Integration

### Lexer Changes

**Location:** `src/nlpl/parser/lexer.py`

**New Token:**
```python
class TokenType(Enum):
    # ... existing tokens ...
    RC = auto()  # "Rc"
```

**Keyword Mapping:**
```python
keyword_map = {
    # ... existing keywords ...
    'rc': TokenType.RC,
}
```

### Parser Changes

**Location:** `src/nlpl/parser/parser.py`

**New Method:**
```python
def parse_rc_type(self):
    """
    Parse: Rc of Type
    """
    self.expect(TokenType.RC)
    self.expect(TokenType.OF)
    inner_type = self.parse_type()
    return RcTypeNode(inner_type)
```

**Update parse_type():**
```python
def parse_type(self):
    if self.current_token.type == TokenType.RC:
        return self.parse_rc_type()
    # ... existing type parsing ...
```

---

## Testing Strategy

### Unit Tests

**Location:** `test_programs/unit/rc/`

#### Test 1: Basic Creation and Access
```nlpl
# test_rc_basic.nlpl
set num to Rc of Integer with 42
print num  # Should print: 42
```

#### Test 2: Reference Sharing
```nlpl
# test_rc_sharing.nlpl
set original to Rc of Integer with 100
set copy to original
set original to 200
print copy  # Should print: 200 (shared reference)
```

#### Test 3: Scope Lifetime
```nlpl
# test_rc_scope.nlpl
function create_rc returns Rc of Integer
    set local to Rc of Integer with 99
    return local
end

set data to call create_rc
print data  # Should print: 99 (not freed)
```

#### Test 4: Nested Structures
```nlpl
# test_rc_nested.nlpl
struct Node
    value as Integer
    next as Rc of Node
end

set node1 to Rc of Node with Node {value: 1, next: null}
set node2 to Rc of Node with Node {value: 2, next: node1}
print node2.value  # Should print: 2
print node2.next.value  # Should print: 1
```

### Integration Tests

**Location:** `test_programs/integration/rc/`

#### Test 1: Linked List
```nlpl
# test_rc_linked_list.nlpl
struct ListNode
    data as Integer
    next as Rc of ListNode
end

function create_list with count as Integer returns Rc of ListNode
    if count is equal to 0
        return null
    end
    set node to ListNode {
        data: count,
        next: call create_list with count minus 1
    }
    return Rc of ListNode with node
end

set list to call create_list with 5
# Should create: 5 -> 4 -> 3 -> 2 -> 1 -> null
```

#### Test 2: Binary Tree
```nlpl
# test_rc_tree.nlpl
struct TreeNode
    value as Integer
    left as Rc of TreeNode
    right as Rc of TreeNode
end

function create_tree with depth as Integer returns Rc of TreeNode
    if depth is equal to 0
        return null
    end
    set node to TreeNode {
        value: depth,
        left: call create_tree with depth minus 1,
        right: call create_tree with depth minus 1
    }
    return Rc of TreeNode with node
end
```

---

## Performance Considerations

### Overhead Analysis

**Per-Allocation Overhead:**
- Refcount storage: 8 bytes (size_t)
- Negligible compared to typical allocations

**Per-Clone Overhead:**
- One integer increment: ~1 cycle
- Much faster than copying data

**Per-Release Overhead:**
- One integer decrement + conditional: ~2-3 cycles
- Amortized free cost only when refcount reaches 0

### Optimization Opportunities

**1. Inline Small Functions**
- `rc_retain()` can be inlined (single instruction)
- `rc_release()` can be inlined (2-3 instructions)

**2. Batch Cleanup**
- Release multiple Rc at scope exit in one pass
- Reduces branching overhead

**3. Compile-Time Refcount Elimination**
- If Rc never cloned, skip refcount (use unique_ptr-like ownership)
- Future optimization

---

## Limitations and Future Work

### Current Limitations

**1. Circular References** (Week 3 Day 3-4)
```nlpl
# This will leak memory:
struct Node
    next as Rc of Node
end

set a to Rc of Node
set b to Rc of Node
set a.next to b
set b.next to a  # Circular reference, never freed
```

**Solution:** Weak<T> (coming in Day 3-4)

**2. Thread Safety** (Week 5)
```nlpl
# Not safe for multi-threading
set shared to Rc of Integer with 42
# Multiple threads modifying refcount = data race
```

**Solution:** Arc<T> (Atomic Reference Counting)

**3. No Custom Destructors** (Week 3 Day 5)
```nlpl
# Can't run cleanup code when freed
struct File
    handle as Integer
end
# Need destructor to close file handle
```

**Solution:** Drop trait / destructor syntax

### Future Enhancements

**Week 4+:**
- Weak<T> for breaking cycles
- Arc<T> for thread-safe sharing
- Custom destructors (Drop trait)
- Rc::new_cyclic() for self-referential structures
- Rc::try_unwrap() for unique ownership check

---

## Implementation Checklist

### Day 1 (Today)
- [x] Design document complete
- [ ] Parser support for `Rc of Type`
- [ ] AST nodes for RcType
- [ ] Basic type system integration

### Day 2
- [ ] Runtime library (rc_runtime.c)
- [ ] LLVM IR generation for Rc operations
- [ ] Automatic scope-based cleanup
- [ ] First test program compiling

### Success Criteria
- ✅ `set x to Rc of Integer with 42` compiles
- ✅ Automatic refcount increment/decrement
- ✅ No memory leaks in test programs
- ✅ Linked list example works

---

**Status**: Design complete, ready to implement parser support  
**Next**: Add RC token to lexer, implement parse_rc_type()
