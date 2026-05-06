# NexusLang Nested Struct Support

## Overview
NLPL now supports **nested struct member access and assignment**, allowing you to work with complex hierarchical data structures naturally.

## Feature Status
 **IMPLEMENTED** (December 2024)

## Syntax

### Nested Member Access (Read)
```nexuslang
struct Point
 x as Integer
 y as Integer

struct Rectangle 
 top_left as Point
 bottom_right as Point

set rect to new Rectangle
set rect.top_left to new Point
set rect.top_left.x to 10

# Access nested fields
print number rect.top_left.x # Prints: 10
```

### Nested Member Assignment (Write)
```nexuslang
# Single-level assignment
set point to new Point
set point.x to 42

# Multi-level assignment
set rect to new Rectangle
set rect.top_left.x to 100
set rect.top_left.y to 200
set rect.bottom_right.x to 500
set rect.bottom_right.y to 600
```

## Implementation Details

### Compiler Architecture

#### 1. Member Access Pointer Generation
The compiler uses `_generate_member_access_pointer()` to navigate nested struct hierarchies:

```python
# For rect.top_left.x:
# 1. Get pointer to rect.top_left (returns %Point*)
# 2. From that pointer, get pointer to x field
# 3. Load the value from that pointer
```

**Key Method**: `_generate_member_access_pointer(expr, indent) -> str`
- Recursively handles nested MemberAccess nodes
- Returns pointer to the field (not the loaded value)
- Supports both struct and class types

#### 2. Type Inference
`_infer_member_access_type(expr) -> str` determines the LLVM type at each level:

```python
# rect.top_left infers "%Point*"
# rect.top_left.x infers "i64" (Integer type)
```

#### 3. Enhanced Member Access
`_generate_member_access()` now detects nested access:

**Before Enhancement** (single-level only):
- `rect.x` Worked
- `rect.top_left.x` Failed (returned 0)

**After Enhancement** (multi-level):
- `rect.x` Works
- `rect.top_left.x` Works
- `rect.top_left.inner.value` Works (any depth)

#### 4. Enhanced Member Assignment
`_generate_member_assignment()` handles nested writes:

**Nested Assignment Flow**:
1. Detect that target.object_expr is a MemberAccess (not just Identifier)
2. Call `_generate_member_access_pointer()` to get parent struct pointer
3. Infer the type of the parent field
4. Generate GEP instruction to get final field pointer
5. Store the value

### LLVM IR Generation Pattern

For `rect.top_left.x = 10`:

```llvm
; 1. Get pointer to rect (local variable)
%rect = alloca %Rectangle, align 8

; 2. Get pointer to top_left field (field 0 in Rectangle)
%1 = getelementptr inbounds %Rectangle, %Rectangle* %rect, i32 0, i32 0

; 3. Get pointer to x field (field 0 in Point)
%2 = getelementptr inbounds %Point, %Point* %1, i32 0, i32 0

; 4. Store the value
store i64 10, i64* %2, align 8
```

For reading `rect.top_left.x`:

```llvm
; Steps 1-3 same as above to get field pointer

; 4. Load the value
%3 = load i64, i64* %2, align 8
```

## Supported Nesting Depth
**Unlimited** - The implementation is fully recursive and can handle arbitrarily deep nesting:

```nexuslang
struct A
 value as Integer

struct B
 a as A

struct C
 b as B

struct D
 c as C

set d to new D
set d.c.b.a.value to 42 # Works!
print number d.c.b.a.value # Prints: 42
```

## Testing

### Test File: `test_programs/compiler/test_struct_advanced.nlpl`

**Test Coverage**:
1. **Nested Structs** - Rectangle with Point fields
2. **Struct Pointers** - Address-of and dereferencing
3. **Mixed Types** - Integer + Float in same struct
4. **Multiple Instances** - Independent struct objects
5. **Struct Assignment** - Assigning entire struct instances
6. **Nested Assignment** - `rect.top_left.x = 10`
7. **Nested Reading** - `print number rect.top_left.y`

**Debug Test**: `test_programs/compiler/test_nested_struct_debug.nlpl`
- Minimal test case for isolating nested struct behavior
- Validates that `rect.top_left.x = 10` stores and retrieves correctly

### Test Results
All tests **PASSING** 

```
Test 1: Nested Structs
Top-left: 10, 20
Bottom-right: 100, 200

Test 2: Struct Pointers 
Via pointer: 50, 75

Test 3: Mixed Types
30, 5.900000, 165.500000
```

## Current Limitations

### 1. Function Parameters
 **Not Yet Supported**: Passing structs as function parameters
```nexuslang
function print_point with p as Point returns Integer
 # Currently causes parser error
```

**Workaround**: Pass individual fields or use global variables

### 2. Function Return Types 
 **Not Yet Supported**: Returning struct instances from functions
```nexuslang
function create_origin returns Point
 # Currently limited to primitive return types
```

**Workaround**: Use out parameters or global variables

### 3. Struct Arrays
 **Partial Support**: Can create struct instances but not array indexing combined with member access
```nexuslang
set points to list new Point, new Point # Syntax error
set arr_elem to points[0] # Works
set arr_elem.x to 10 # Works
set points[0].x to 10 # Not yet supported (requires chained indexing)
```

### 4. Struct Initialization
 **Not Yet Supported**: Constructor arguments or initializer lists
```nexuslang
set p to new Point(10, 20) # Not supported
```

**Current Approach**: Zero-initialization, then member assignment
```nexuslang
set p to new Point
set p.x to 10
set p.y to 20
```

## Implementation Files Modified

1. **`src/nexuslang/compiler/backends/llvm_ir_generator.py`**:
 - Added `_generate_member_access_pointer()` (line ~3838)
 - Added `_infer_member_access_type()` (line ~3910)
 - Enhanced `_generate_member_access()` (line ~3692)
 - Enhanced `_generate_member_assignment()` (line ~2444)

2. **Test Files**:
 - `test_programs/compiler/test_struct_advanced.nlpl` (6 tests)
 - `test_programs/compiler/test_nested_struct_debug.nlpl` (minimal case)

## Future Enhancements

### Planned Features
1. **Struct Function Parameters** - Pass structs to functions
2. **Struct Return Values** - Return struct instances
3. **Struct Constructors** - `new Point(x, y)`
4. **Struct Initializers** - `Point { x: 10, y: 20 }`
5. **Struct Arrays with Indexing** - `points[0].x = 10`
6. **Struct Methods** - C++-style member functions
7. **Struct Copying** - Deep copy operations

### Performance Optimizations
- **Pass by Reference**: Implement efficient parameter passing
- **Return Value Optimization**: Avoid unnecessary copies
- **Structure Padding**: Align fields for optimal memory access

## Examples

### Simple Nested Struct
```nexuslang
struct Inner
 value as Integer

struct Outer
 inner as Inner

set obj to new Outer
set obj.inner to new Inner
set obj.inner.value to 100

print number obj.inner.value # 100
```

### Complex Hierarchy
```nexuslang
struct Position
 x as Integer
 y as Integer

struct Velocity
 dx as Integer
 dy as Integer

struct Entity
 pos as Position
 vel as Velocity
 health as Integer

set player to new Entity
set player.pos to new Position
set player.vel to new Velocity

set player.pos.x to 100
set player.pos.y to 50
set player.vel.dx to 5
set player.vel.dy to -3
set player.health to 100

print number player.pos.x # 100
print number player.vel.dx # 5
```

### Struct Pointers
```nexuslang
struct Node
 value as Integer
 next as Node # Forward declaration would be needed

set node1 to new Node
set node1.value to 42

set ptr to address of node1
set node2 to value at ptr

print number node2.value # 42
```

## Technical Notes

### Memory Layout
Structs are allocated on the stack using LLVM's `alloca`:
```llvm
%obj = alloca %StructName, align 8
```

Fields are zero-initialized upon creation.

### Type Safety
The compiler enforces type checking:
- Field types must match assignment values
- Automatic type conversion for compatible types (i64 double)
- Pointer types are strictly checked

### Recursive Implementation
The nested access implementation is fully recursive, allowing:
- Unlimited nesting depth
- Mixed struct and class types
- Combination with pointers and other complex types

## Comparison with Other Languages

### C
```c
struct Point { int x, y; };
struct Rect { struct Point tl, br; };

struct Rect r;
r.tl.x = 10; // Similar syntax
```

### C++
```cpp
struct Point { int x, y; };
struct Rect { Point tl, br; };

Rect r;
r.tl.x = 10; // Identical syntax
```

### NexusLang
```nexuslang
struct Point
 x as Integer
 y as Integer

struct Rectangle
 tl as Point
 br as Point

set r to new Rectangle
set r.tl to new Point
set r.tl.x to 10 # Natural English-like syntax
```

**Key Difference**: NexusLang uses natural language keywords (`set`, `to`, `as`) while maintaining the familiar dot notation for member access.

## Version History
- **December 2024**: Initial implementation with full nested access support
- Future: Planned enhancements for function parameters, return values, and constructors
