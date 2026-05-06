# Structs and Unions in NexusLang

**Status:** ✅ Fully implemented (verified February 3, 2026)  
**Complexity:** Intermediate

---

## Overview

Structs and unions provide C-style memory layout control for system programming, FFI interoperability, and performance-critical code.

**Structs** (structures):
- Group related data fields together
- Each field has its own memory location
- Size = sum of field sizes (plus padding/alignment)
- Similar to classes but focused on data layout

**Unions:**
- Multiple fields share the same memory location
- Size = size of largest field
- Only one field is active at a time
- Useful for type punning, memory optimization, variant types

NLPL's struct/union features:
- C-compatible memory layout
- Packed structs (no padding)
- Custom alignment control
- Nested structs and unions
- FFI integration for C library interop
- Type safety with optional type checking

---

## Struct Basics

### Defining a Struct

```nexuslang
struct Point
  x as Integer
  y as Integer
end
```

**Components:**
- `struct` keyword
- Struct name (PascalCase convention)
- Field declarations (`name as Type`)
- `end` keyword

### Creating Struct Instances

```nexuslang
struct Point
  x as Integer
  y as Integer
end

# Create instance
set p1 to new Point

# Set fields
set p1.x to 10
set p1.y to 20

# Read fields
print text "x = " plus (p1.x to_string)
print text "y = " plus (p1.y to_string)
```

### Struct with Initialization

```nexuslang
struct Rectangle
  x as Integer
  y as Integer
  width as Integer
  height as Integer
end

set rect to new Rectangle
set rect.x to 0
set rect.y to 0
set rect.width to 100
set rect.height to 50
```

---

## Struct Features

### Nested Structs

```nexuslang
struct Point
  x as Integer
  y as Integer
end

struct Rectangle
  top_left as Point
  bottom_right as Point
end

# Create nested structure
set rect to new Rectangle
set rect.top_left to new Point
set rect.top_left.x to 10
set rect.top_left.y to 20

set rect.bottom_right to new Point
set rect.bottom_right.x to 100
set rect.bottom_right.y to 80

# Access nested fields
print text "Top-left x: " plus (rect.top_left.x to_string)
```

### Struct with Methods

```nexuslang
struct Point
  x as Integer
  y as Integer
  
  function distance_from_origin with self returns Float
    set x_squared to self.x times self.x
    set y_squared to self.y times self.y
    return sqrt of (x_squared plus y_squared)
  end
  
  function move with self, dx as Integer, dy as Integer
    set self.x to self.x plus dx
    set self.y to self.y plus dy
  end
end

set p to new Point
set p.x to 3
set p.y to 4
set dist to p.distance_from_origin
# dist = 5.0

call p.move with 10, 10
# p.x = 13, p.y = 14
```

### Struct Memory Layout

```nexuslang
struct SimpleStruct
  a as Integer  # 8 bytes (64-bit)
  b as Integer  # 8 bytes
  c as Integer  # 8 bytes
end
# Total: 24 bytes (no padding needed)

struct MixedStruct
  a as Integer  # 8 bytes at offset 0
  b as Boolean  # 1 byte at offset 8
  # 7 bytes padding
  c as Integer  # 8 bytes at offset 16
end
# Total: 24 bytes (includes padding for alignment)
```

**Memory layout rules:**
- Fields laid out in declaration order
- Alignment requirements (e.g., 8-byte integers on 8-byte boundaries)
- Padding inserted to maintain alignment
- Total size rounded up to alignment of largest field

---

## Packed Structs

### Removing Padding

```nexuslang
packed struct TightlyPacked
  a as Integer   # 8 bytes at offset 0
  b as Boolean   # 1 byte at offset 8
  c as Integer   # 8 bytes at offset 9 (no padding!)
end
# Total: 17 bytes (vs 24 bytes unpacked)
```

**Packed struct behavior:**
- `packed` keyword before `struct`
- No padding between fields
- Fields can be misaligned
- Smaller memory footprint
- Slower access on some architectures

**When to use packed structs:**
- ✅ Network protocols (exact byte layout)
- ✅ File formats (binary data)
- ✅ Hardware registers (specific memory mapping)
- ✅ Memory-constrained systems
- ❌ Performance-critical code (misaligned access is slow)

### Custom Alignment

```nexuslang
aligned(16) struct AlignedStruct
  a as Integer
  b as Integer
end
# Total size: 16 bytes minimum (rounded up to 16-byte boundary)
```

**Alignment use cases:**
- SIMD operations (SSE/AVX require 16/32-byte alignment)
- Cache line optimization (64-byte alignment)
- Hardware requirements (DMA, memory-mapped I/O)

---

## Union Basics

### Defining a Union

```nexuslang
union Data
  int_value as Integer
  float_value as Float
  bool_value as Boolean
end
```

**Key properties:**
- All fields share the same memory location
- Size = size of largest field
- Writing to one field overwrites others
- Only one field is "active" at a time

### Using Unions

```nexuslang
union Data
  int_value as Integer
  float_value as Float
end

set data to new Data

# Write as integer
set data.int_value to 42
print text "As integer: " plus (data.int_value to_string)
# Output: As integer: 42

# Write as float (overwrites integer)
set data.float_value to 3.14
print text "As float: " plus (data.float_value to_string)
# Output: As float: 3.14

# Reading int_value now gives garbage (float bits interpreted as int)
```

### Type Punning with Unions

```nexuslang
union FloatBits
  float_value as Float
  int_bits as Integer
end

function float_to_bits with f as Float returns Integer
  set converter to new FloatBits
  set converter.float_value to f
  return converter.int_bits
end

function bits_to_float with bits as Integer returns Float
  set converter to new FloatBits
  set converter.int_bits to bits
  return converter.float_value
end

set bits to float_to_bits with 3.14
print text "Float 3.14 as bits: " plus (bits to_string)
```

**Warning:** Type punning is unsafe! Only use when you understand bit representations.

---

## Tagged Unions (Discriminated Unions)

### Manual Variant Type

```nexuslang
enum ValueType
  IntType
  FloatType
  StringType
end

struct Variant
  type as ValueType
  data as Data
end

union Data
  int_value as Integer
  float_value as Float
  string_value as String
end

function create_int_variant with value as Integer returns Variant
  set v to new Variant
  set v.type to ValueType.IntType
  set v.data to new Data
  set v.data.int_value to value
  return v
end

function get_int with v as Variant returns Integer
  if v.type equals ValueType.IntType
    return v.data.int_value
  else
    raise error "Not an integer variant"
  end
end
```

**Note:** NexusLang also has built-in Option and Result types for safer variant handling!

---

## Struct vs Class

### When to Use Structs

**Structs are well-suited to:**
- ✅ Plain data containers (POD types)
- ✅ C FFI interoperability
- ✅ Exact memory layout requirements
- ✅ Performance-critical code (stack allocation)
- ✅ Network/file format parsing
- ✅ Hardware interfacing

**Example:**
```nexuslang
struct PacketHeader
  magic as Integer      # 4 bytes
  version as Integer    # 4 bytes
  length as Integer     # 4 bytes
  checksum as Integer   # 4 bytes
end
# Exact 16-byte layout for network protocol
```

### When to Use Classes

**Classes are well-suited to:**
- ✅ Complex behavior and logic
- ✅ Inheritance hierarchies
- ✅ Encapsulation and data hiding
- ✅ Polymorphism
- ✅ Application logic

**Example:**
```nexuslang
class NetworkConnection
  private socket as Integer
  private buffer as String
  
  function send with self, data as String
    # Complex sending logic
  end
  
  function receive with self returns String
    # Complex receiving logic
  end
end
```

### Comparison Table

| Feature | Struct | Class |
|---------|--------|-------|
| Memory layout | Predictable, C-compatible | Implementation-defined |
| Inheritance | No | Yes |
| Methods | Yes (simple) | Yes (full OOP) |
| Access control | Public by default | Public/private/protected |
| Default allocation | Stack-friendly | Heap |
| Padding control | Yes (packed, aligned) | No |
| FFI use | Excellent | Poor |

---

## FFI Integration

### Calling C Functions with Structs

```nexuslang
# C library has:
# struct Point { int x; int y; };
# void move_point(struct Point* p, int dx, int dy);

struct Point
  x as Integer
  y as Integer
end

extern function move_point with p as Pointer to Point, dx as Integer, dy as Integer

set p to new Point
set p.x to 10
set p.y to 20

call move_point with (address of p), 5, 5
# p.x = 15, p.y = 25
```

### Struct Layout Compatibility

**NLPL struct:**
```nexuslang
struct CCompatible
  a as Integer  # int (4 bytes on most platforms)
  b as Integer
  c as Float    # float (4 bytes)
end
```

**Equivalent C struct:**
```c
struct CCompatible {
    int a;
    int b;
    float c;
};
```

**Important:** Ensure field types match C types exactly!
- NexusLang `Integer` = C `int` (32-bit) or `long` (64-bit) - check platform!
- NexusLang `Float` = C `float` (32-bit) or `double` (64-bit)
- NexusLang `Boolean` = C `bool` or `int`
- Use `packed` if C struct uses `__attribute__((packed))`

---

## Advanced Techniques

### Bit Fields

```nexuslang
struct Flags
  read_permission as Boolean    # 1 bit (stored as 1 byte without bit field support)
  write_permission as Boolean   # 1 bit
  execute_permission as Boolean # 1 bit
end

# Note: True bit fields with arbitrary bit widths are planned but not yet implemented
# Current implementation uses full bytes for each Boolean
```

**Workaround using bitwise operations:**
```nexuslang
struct PermissionFlags
  flags as Integer  # Pack multiple booleans into bits
end

function set_read_permission with perms as Pointer to PermissionFlags, value as Boolean
  if value
    set perms.flags to perms.flags or 0x01  # Set bit 0
  else
    set perms.flags to perms.flags and (not 0x01)  # Clear bit 0
  end
end

function get_read_permission with perms as PermissionFlags returns Boolean
  return (perms.flags and 0x01) not equals 0
end
```

### Flexible Array Members

```nexuslang
struct DynamicArray
  length as Integer
  capacity as Integer
  data as Pointer to Integer  # Points to dynamically allocated array
end

function create_array with capacity as Integer returns Pointer to DynamicArray
  set arr to allocate sizeof(DynamicArray)
  set arr.length to 0
  set arr.capacity to capacity
  set arr.data to allocate (capacity times sizeof(Integer))
  return arr
end

function destroy_array with arr as Pointer to DynamicArray
  free arr.data
  free arr
end
```

### Zero-Cost Abstractions

```nexuslang
struct Vector2D
  x as Float
  y as Float
  
  function add with self, other as Vector2D returns Vector2D
    set result to new Vector2D
    set result.x to self.x plus other.x
    set result.y to self.y plus other.y
    return result
  end
  
  function magnitude with self returns Float
    return sqrt of ((self.x times self.x) plus (self.y times self.y))
  end
end

# Struct methods have no overhead compared to manual field access
set v1 to new Vector2D
set v1.x to 3.0
set v1.y to 4.0

set v2 to new Vector2D
set v2.x to 1.0
set v2.y to 2.0

set v3 to v1.add with v2
set mag to v3.magnitude
```

---

## Memory Operations

### Copying Structs

```nexuslang
struct Point
  x as Integer
  y as Integer
end

set p1 to new Point
set p1.x to 10
set p1.y to 20

# Copy by assignment (creates new instance with same values)
set p2 to p1
set p2.x to 30
# p1.x is still 10 (independent copy)

# Manual memory copy
set p3 to allocate sizeof(Point)
# Copy bytes from p1 to p3 (would need memcpy-like function)
```

### Struct Pointers

```nexuslang
struct Node
  value as Integer
  next as Pointer to Node
end

# Create linked list
set head to allocate sizeof(Node)
set head.value to 1

set second to allocate sizeof(Node)
set second.value to 2
set second.next to 0  # NULL

set head.next to second

# Traverse
set current to head
while current not equals 0
  print text "Value: " plus (current.value to_string)
  set current to current.next
end
```

### sizeof Operator

```nexuslang
struct Example
  a as Integer
  b as Integer
  c as Float
end

set size to sizeof(Example)
print text "Struct size: " plus (size to_string)
# Output depends on platform and alignment

set size_int to sizeof(Integer)
set size_float to sizeof(Float)
set size_bool to sizeof(Boolean)
```

---

## Best Practices

### 1. Use Descriptive Field Names

```nexuslang
# Good: Clear intent
struct Employee
  employee_id as Integer
  first_name as String
  last_name as String
  salary as Float
end

# Bad: Unclear abbreviations
struct Employee
  id as Integer
  fn as String
  ln as String
  sal as Float
end
```

### 2. Group Related Fields

```nexuslang
# Good: Logical grouping
struct Person
  # Identity
  id as Integer
  name as String
  
  # Contact
  email as String
  phone as String
  
  # Address
  street as String
  city as String
  postal_code as String
end
```

### 3. Consider Alignment for Performance

```nexuslang
# Good: Largest fields first (minimizes padding)
struct Optimized
  large as Integer      # 8 bytes
  medium as Integer     # 4 bytes
  small as Boolean      # 1 byte
  # 3 bytes padding
end
# Total: 16 bytes

# Bad: Random order (more padding)
struct Unoptimized
  small as Boolean      # 1 byte
  # 7 bytes padding
  large as Integer      # 8 bytes
  medium as Integer     # 4 bytes
  # 4 bytes padding
end
# Total: 24 bytes
```

### 4. Document Memory Layout for FFI

```nexuslang
# C struct:
# struct NetworkPacket {
#     uint32_t magic;      // offset 0
#     uint16_t version;    // offset 4
#     uint16_t length;     // offset 6
#     uint8_t data[256];   // offset 8
# };

packed struct NetworkPacket
  magic as Integer       # 4 bytes at offset 0
  version as Integer     # 2 bytes at offset 4
  length as Integer      # 2 bytes at offset 6
  # data field would need array support
end
```

### 5. Initialize All Fields

```nexuslang
# Good: All fields initialized
function create_point returns Point
  set p to new Point
  set p.x to 0
  set p.y to 0
  return p
end

# Bad: Uninitialized fields contain garbage
function create_point_bad returns Point
  set p to new Point
  # x and y contain random values!
  return p
end
```

### 6. Use Unions Carefully

```nexuslang
# Good: Tagged union for type safety
enum DataType
  IntType
  FloatType
end

struct SafeUnion
  type as DataType
  value as ValueUnion
end

union ValueUnion
  int_val as Integer
  float_val as Float
end

function get_int_safely with su as SafeUnion returns Integer
  if su.type equals DataType.IntType
    return su.value.int_val
  else
    raise error "Type mismatch"
  end
end

# Bad: Raw union without type tag (unsafe!)
union UnsafeUnion
  int_val as Integer
  float_val as Float
end
# No way to know which field is valid!
```

---

## Common Patterns

### 1. Data Transfer Objects

```nexuslang
struct UserDTO
  id as Integer
  username as String
  email as String
  created_at as Integer  # Timestamp
end

# Used for API communication, database records, etc.
```

### 2. Configuration Structs

```nexuslang
struct ServerConfig
  host as String
  port as Integer
  max_connections as Integer
  timeout_seconds as Integer
  enable_ssl as Boolean
end

set config to new ServerConfig
set config.host to "localhost"
set config.port to 8080
set config.max_connections to 100
set config.timeout_seconds to 30
set config.enable_ssl to false
```

### 3. Hardware Registers

```nexuslang
packed struct GPIO_Register
  pin0 as Boolean
  pin1 as Boolean
  pin2 as Boolean
  pin3 as Boolean
  pin4 as Boolean
  pin5 as Boolean
  pin6 as Boolean
  pin7 as Boolean
end

# Map to hardware address
set gpio to (0x40020000 as Pointer to GPIO_Register)
set gpio.pin2 to true  # Set pin 2 high
```

### 4. Binary File Formats

```nexuslang
packed struct BitmapHeader
  signature as Integer    # "BM" (2 bytes)
  file_size as Integer    # 4 bytes
  reserved1 as Integer    # 2 bytes
  reserved2 as Integer    # 2 bytes
  data_offset as Integer  # 4 bytes
end
# Total: 14 bytes (exact BMP header format)
```

---

## Performance Considerations

### Stack vs Heap Allocation

```nexuslang
# Stack allocation (fast, automatic cleanup)
function use_stack returns Integer
  set p to new Point  # Allocated on stack
  set p.x to 10
  set p.y to 20
  return p.x plus p.y
end  # p automatically destroyed

# Heap allocation (slower, manual management)
function use_heap returns Pointer to Point
  set p to allocate sizeof(Point)  # Allocated on heap
  set p.x to 10
  set p.y to 20
  return p
end
# Caller must free the memory!
```

**Guidelines:**
- Use stack allocation for temporary, small structs
- Use heap allocation for large structs, dynamic lifetime
- Be careful with heap-allocated structs (memory leaks!)

### Cache-Friendly Layout

```nexuslang
# Good: Frequently accessed fields together
struct CacheFriendly
  # Hot fields (accessed frequently)
  count as Integer
  sum as Integer
  
  # Cold fields (accessed rarely)
  name as String
  description as String
end

# Better cache utilization in loops
```

### Struct Copying Cost

```nexuslang
struct LargeStruct
  data1 as Integer
  data2 as Integer
  # ... many more fields ...
  data100 as Integer
end

# Expensive: Copies entire struct
function expensive_copy with s as LargeStruct returns LargeStruct
  return s  # Full copy!
end

# Cheap: Pass by pointer
function cheap_reference with s as Pointer to LargeStruct returns Integer
  return s.data1  # No copy!
end
```

---

## Troubleshooting

### Issue: Field Not Found

**Problem:**
```nexuslang
struct Point
  x as Integer
  y as Integer
end

set p to new Point
set p.z to 30  # ERROR: Field 'z' not found
```

**Solution:** Check field name spelling and struct definition.

### Issue: Type Mismatch

**Problem:**
```nexuslang
struct Data
  value as Integer
end

set d to new Data
set d.value to "hello"  # ERROR: Expected Integer, got String
```

**Solution:** Use correct type for field.

### Issue: Accessing Union Field

**Problem:**
```nexuslang
union Data
  int_value as Integer
  float_value as Float
end

set d to new Data
set d.int_value to 42
print text d.float_value  # Garbage! float_value was never set
```

**Solution:** Only read the field you most recently wrote to.

### Issue: Struct Alignment Mismatch (FFI)

**Problem:**
```nexuslang
# NexusLang struct (unpacked, has padding)
struct Data
  a as Boolean  # 1 byte + 7 bytes padding
  b as Integer  # 8 bytes
end
# Total: 16 bytes

# C struct (packed)
# struct Data { char a; long b; } __attribute__((packed));
# Total: 9 bytes

# FFI call fails due to size mismatch!
```

**Solution:** Use `packed struct` to match C layout:
```nexuslang
packed struct Data
  a as Boolean
  b as Integer
end
# Now 9 bytes, matches C
```

---

## Further Reading

- **FFI Guide:** `docs/reference/stdlib/ffi.md`
- **Memory Management:** `docs/tutorials/advanced/02-memory-management.md`
- **Inline Assembly:** `docs/guide/inline-assembly.md`
- **Type System:** `docs/guide/type-system.md`
- **Examples:** `examples/24_struct_and_union.nlpl`

---

## Summary

Structs and unions in NexusLang provide:

✅ **C-compatible layout** - FFI and system programming  
✅ **Memory control** - Packed, aligned, predictable  
✅ **Type safety** - Full type checking integration  
✅ **Performance** - Zero-cost abstractions, stack allocation  
✅ **Flexibility** - Methods, nesting, pointers  

**Use structs when:**
- You need predictable memory layout
- Interfacing with C libraries
- Building network/file protocols
- System programming and hardware access

**Use unions when:**
- You need to save memory (only one field active)
- Type punning (bit-level manipulation)
- Building variant types (with tag field)

**Status:** Production-ready for system programming and FFI!
