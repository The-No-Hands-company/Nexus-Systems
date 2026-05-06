# Function Pointers in NexusLang

## Overview

Function pointers in NexusLang allow you to store references to functions and pass them around as values. This enables callbacks, higher-order programming patterns, and dynamic function selection.

## Current Implementation Status

** Implemented:**
- `address of function_name` - Get function address as pointer
- `call (value at func_ptr) with args` - Indirect function calls through pointers
- Storing function pointers in variables
- Function pointer reassignment
- Type inference for function pointers (i8* generic pointer type)
- Proper LLVM IR generation with function type casting
- Support for functions with 0, 1, or multiple parameters

** Planned:**
- Function pointers as function parameters
- Arrays of function pointers
- Function pointer type declarations
- Typed function pointers with compile-time signature checking

## Syntax

### Getting a Function Address

```nexuslang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
```

## Examples

### Basic Function Pointer

```nexuslang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
print text "Function pointer obtained"
```

**Output:** `Function pointer obtained`

### Indirect Function Call

```nexuslang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
set result to call (value at func_ptr) with 5 and 3
print text "5 + 3 = "
print number result
```

**Output:**
```
5 + 3 = 
8
```

### Multiple Function Pointers

```nexuslang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

function multiply_numbers that takes x as Integer and y as Integer returns Integer
 return x times y

function subtract_numbers that takes a as Integer and b as Integer returns Integer
 return a minus b

set add_ptr to address of add_numbers
set mult_ptr to address of multiply_numbers
set sub_ptr to address of subtract_numbers

print text "Multiple function pointers obtained"
```

### Single Parameter Function

```nexuslang
function square_number that takes n as Integer returns Integer
 return n times n

set square_ptr to address of square_number
print text "Single parameter function pointer obtained"
```

### No Parameter Function

```nexuslang
function get_constant returns Integer
 return 42

set const_ptr to address of get_constant
print text "No parameter function pointer obtained"
```

### Function Pointer Reassignment

```nexuslang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

function multiply_numbers that takes x as Integer and y as Integer returns Integer
 return x times y

set operation_ptr to address of add_numbers
print text "Initial pointer set to add_numbers"

set operation_ptr to address of multiply_numbers
print text "Pointer reassigned to multiply_numbers"
```

**Output:**
```
Initial pointer set to add_numbers
Pointer reassigned to multiply_numbers
```

## Implementation Details

### Compilation

Function pointers are compiled to generic `i8*` (byte pointers) in LLVM IR:

**NLPL Code:**
```nexuslang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
```

**Generated LLVM IR:**
```llvm
define i64 @add_numbers(i64 %a, i64 %b) {
 ; ... function body
}

@func_ptr = private unnamed_addr global i8* null, align 8

define i32 @main(i32 %argc, i8** %argv) {
entry:
 %0 = bitcast i64 (i64, i64)* @add_numbers to i8*
 store i8* %0, i8** @func_ptr, align 8
 ret i32 0
}
```

### Type System

- **Function Pointer Type**: `i8*` (generic pointer)
- **Type Inference**: `AddressOfExpression` returns `i8*`
- **Storage**: Function pointers are stored as global or local variables of type `i8*`

### Name Mangling

Function names are mangled according to the same rules as function definitions:
- Global functions: `@function_name`
- Module functions: `@module_name_function_name`
- Main function: `@nxl_main`

### LLVM Bitcast

When taking the address of a function, the compiler generates a `bitcast` instruction to convert the function's native type to a generic `i8*` pointer:

```llvm
%ptr = bitcast return_type (param_types...)* @function_name to i8*
```

This allows all function pointers to be stored uniformly as `i8*` regardless of their signature.

## Use Cases

### Callback Pattern (Future)

```nexuslang
# Planned syntax for callbacks
function process_data with data as Integer and callback as FunctionPointer
 set result to data times 2
 # Call callback with result
 # callback with result
```

### Strategy Pattern (Future)

```nexuslang
# Planned: Select function based on runtime conditions
set operation to address of add_numbers

if user_choice is equal to 1
 set operation to address of multiply_numbers
else if user_choice is equal to 2
 set operation to address of subtract_numbers

# Call selected operation
# set result to call operation with 10 and 5
```

### Function Table (Future)

```nexuslang
# Planned: Array of function pointers for dispatch
set operations to [
 address of add_numbers,
 address of subtract_numbers,
 address of multiply_numbers
]

# Call based on index
# set result to call operations[choice] with x and y
```

## Comparison with Other Languages

### C Style

```c
// C
int add(int a, int b) { return a + b; }

int (*func_ptr)(int, int) = &add;
int result = func_ptr(5, 3);
```

```nexuslang
# NexusLang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
# Indirect calls coming soon
```

### Python Style

```python
# Python
def add(a, b):
 return a + b

func_ptr = add
result = func_ptr(5, 3)
```

```nexuslang
# NexusLang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
# Indirect calls coming soon
```

### Rust Style

```rust
// Rust
fn add(a: i64, b: i64) -> i64 { a + b }

let func_ptr: fn(i64, i64) -> i64 = add;
let result = func_ptr(5, 3);
```

```nexuslang
# NexusLang
function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set func_ptr to address of add_numbers
# Indirect calls coming soon
```

## Current Limitations

1. **No Function Pointer Types**: Cannot declare explicit function pointer types
 - All function pointers are generic `i8*`
 - Type-safe function pointers planned for future
 - Return types assumed to be i64 for indirect calls

2. **No Function Pointers as Parameters**: Cannot pass function pointers to functions yet
 - Requires function parameter type annotations to support pointer types

3. **No Arrays of Function Pointers**: Cannot create arrays holding function pointers
 - Requires proper array type inference for pointer types

4. **Variable Scope Limitations**: Variables declared inside if/else blocks have scope issues
 - Variables should be hoisted to entry block for proper SSA form
 - Current workaround: declare variables before conditional blocks

## Future Enhancements

### Function Pointer Type Declarations

```nexuslang
# Planned syntax
type BinaryOperation as Function that takes Integer and Integer returns Integer

function add_numbers that takes a as Integer and b as Integer returns Integer
 return a plus b

set operation as BinaryOperation to address of add_numbers
set result to call (value at operation) with 10 and 5
```

### Function Pointers as Parameters

```nexuslang
# Planned syntax
function apply_operation with x as Integer and y as Integer and op as FunctionPointer returns Integer
 return call (value at op) with x and y

set result to apply_operation with 10 and 5 and address of add_numbers
```

### Method Pointers

```nexuslang
# Future: Pointers to class methods
class Calculator
 public function add that takes a as Integer and b as Integer returns Integer
 return a plus b

set calc to new Calculator
set method_ptr to address of calc.add # Method pointer
```

## Testing

Comprehensive test suite in `test_programs/compiler/test_function_pointers.nlpl`:
- Basic function pointer address-of
- Multiple function pointers
- Single parameter functions
- No parameter functions
- Function pointer reassignment

Indirect call test suite in `test_programs/compiler/test_indirect_calls.nlpl`:
- Basic indirect call (add: 5 + 3 = 8)
- Multiplication through pointer (7 * 6 = 42)
- Single parameter indirect call (9^2 = 81)
- No parameter indirect call (constant = 42)
- Multiple different calls (20 + 5 = 25, 20 - 5 = 15)
- Pointer reassignment and reuse (add/multiply with same pointer variable)

Run tests:
```bash
./nlplc test_programs/compiler/test_function_pointers.nlpl --run
./nlplc test_programs/compiler/test_indirect_calls.nlpl --run
```

All 11 tests passing 

## Implementation Notes

### Compiler Pipeline

1. **Parser**: `address of function_name` `AddressOfExpression(Identifier("function_name"))`
2. **Type Inference**: `AddressOfExpression` `i8*`
3. **Code Generation**: 
 - Look up function in `self.functions`
 - Apply name mangling
 - Generate `bitcast FunctionType* @function_name to i8*`
 - Return temporary register holding the pointer

### Key Code Locations

- **AST Node**: `src/nexuslang/parser/ast.py` - `AddressOfExpression` class (line 662)
- **Parser**: `src/nexuslang/parser/parser.py` - `unary()` method handles `address of` (line 2904)
- **Type Inference**: `src/nexuslang/compiler/backends/llvm_ir_generator.py` - `_infer_expression_type()` (line 4077)
- **Code Generation**: `src/nexuslang/compiler/backends/llvm_ir_generator.py` - `_generate_address_of_expression()` (line 2586)

### Design Decisions

1. **Generic Pointers**: Using `i8*` instead of typed function pointers simplifies the initial implementation
2. **Bitcast Approach**: LLVM's `bitcast` instruction provides type safety at compile time while allowing generic storage
3. **Name Mangling Consistency**: Function pointer references use the same mangling as function definitions

## See Also

- Memory Management Documentation (for general pointer operations)
- Type System Documentation (for future typed function pointers)
- Standard Library (for potential callback-based APIs)
