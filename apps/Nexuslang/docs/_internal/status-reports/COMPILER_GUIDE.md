# NexusLang Compiler Quick Reference

## Building & Running Programs

### Basic Compilation
```bash
./nlplc program.nlpl # Compile to build/program
./nlplc program.nlpl --run # Compile and run
./nlplc program.nlpl -o myapp # Custom output name
```

### Optimization
```bash
./nlplc program.nlpl -O0 # No optimization (default)
./nlplc program.nlpl -O2 # Standard optimization
./nlplc program.nlpl -O3 # Aggressive optimization
```

### Advanced Options
```bash
./nlplc program.nlpl --ir-only # Generate LLVM IR only
./nlplc program.nlpl -v # Verbose output
./nlplc program.nlpl --compiler=gcc # Use GCC instead of Clang
```

### Using Makefile
```bash
make test # Run all tests
make test-verbose # Verbose test output
make clean # Clean build artifacts
```

## Language Features (68% Complete)

### Variables
```nlpl
set x to 42 # Local variable
set name to "Alice" # String
set pi to 3.14 # Float
```

### Global Variables
```nlpl
# At top level
set counter to 0

function increment that takes n as Integer returns Integer
 set counter to counter plus n
 return counter
```

### Functions
```nlpl
function add that takes a as Integer and b as Integer returns Integer
 return a plus b

set result to add with 5 and 3
```

### Control Flow
```nlpl
# If/else-if/else
if x > 10
 print text "Large"
else if x > 5
 print text "Medium"
else
 print text "Small"

# While loop
while counter < 10
 set counter to counter plus 1

# Range-based for
for i from 0 to 10
 print text i

# For-each
for each item in array
 print text item

# Break/continue
for i from 0 to 100
 if i == 50
 break
 if i % 2 == 0
 continue
 print text i
```

### Arrays
```nlpl
# 1D arrays
set numbers to [1, 2, 3, 4, 5]
set first to numbers[0]

# Multi-dimensional
set matrix to [[1, 2, 3], [4, 5, 6]]
set center to matrix[1][1]

# Jagged arrays
set jagged to [[1, 2], [3, 4, 5], [6]]
```

### Structs
```nlpl
struct Point
 x as Integer
 y as Integer
end

set p to new Point
set p.x to 10
set p.y to 20
print text p.x
```

### Operators

**Arithmetic:** `+` `-` `*` `/` `%` 
**Comparison:** `==` `!=` `<` `<=` `>` `>=` 
**Logical:** `and` `or` `not` 
**Bitwise:** `&` `|` `^` `~` `<<` `>>`

### Strings
```nlpl
set greeting to "Hello"
set name to "World"
set message to greeting plus " " plus name

if greeting == "Hello"
 print text "Match!"
```

## Type System

### Supported Types
- `Integer` (64-bit signed)
- `Float` (double precision)
- `String` (i8* pointer)
- `Boolean` (i1)
- Arrays (any type)
- Structs (user-defined)
- Pointers

### Type Inference
The compiler automatically infers types from expressions:
```nlpl
set x to 42 # Inferred as Integer
set y to 3.14 # Inferred as Float
set arr to [1, 2, 3] # Inferred as Integer array
```

## Error Handling

The compiler provides detailed error messages:
```
ERROR during compilation: Syntax Error: ...
 at line 10, column 5

 10 | if x ==
 ^
```

## Performance

### Binary Sizes (with -O2)
- Hello World: ~12KB
- Complex program: ~15-20KB

### Optimization Levels
- `-O0`: Fast compilation, no optimization
- `-O1`: Basic optimization
- `-O2`: Recommended for production
- `-O3`: Aggressive optimization
- `-Os`: Size optimization

## Limitations & TODO

### Not Yet Implemented
- Classes (only structs)
- Inheritance
- Interfaces/Traits
- Generics
- Exception handling
- Module system
- Standard library functions
- Memory management (malloc/free wrappers)
- File I/O
- String manipulation functions

### Known Issues
- String literals require manual memory management
- No bounds checking on arrays
- Limited type checking
- No garbage collection

## Next Development Priorities

1. **String Functions** - length, substring, split
2. **Array Functions** - push, pop, length, slice
3. **Memory Management** - proper malloc/free wrappers
4. **Standard Library** - math, I/O, collections
5. **Type System** - stronger type checking, generics
6. **Module System** - imports, namespaces
7. **Error Handling** - try/catch/finally

## Contributing

See `ROADMAP.md` for detailed development plan.
See `.github/copilot-instructions.md` for coding standards.
