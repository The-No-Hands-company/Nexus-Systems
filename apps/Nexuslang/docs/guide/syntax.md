# NexusLang Syntax Reference


---

## Overview

NLPL (NexusLang) uses English-like syntax that reads like prose while maintaining programming precision. This guide covers all major syntax features.

**Design Principles:**
- Natural language keywords (`set`, `to`, `with`, `called`, `function`)
- Explicit statement boundaries (`end` keyword)
- Type annotations optional but encouraged
- Whitespace-insensitive (except newlines)
- Case-sensitive identifiers

---

## Table of Contents

1. [Variables](#variables)
2. [Data Types](#data-types)
3. [Functions](#functions)
4. [Control Flow](#control-flow)
5. [Pattern Matching](#pattern-matching)
6. [Object-Oriented Programming](#object-oriented-programming)
7. [Structs and Unions](#structs-and-unions)
8. [Inline Assembly](#inline-assembly)
9. [Memory Operations](#memory-operations)
10. [Modules and Imports](#modules-and-imports)
11. [Error Handling](#error-handling)
12. [Operators](#operators)

---

## Variables

### Declaration and Assignment

```nexuslang
# Basic variable declaration
set name to "Alice"
set age to 25
set height to 5.9
set is_student to true

# With type annotations
set count to 0 as Integer
set price to 19.99 as Float
set message to "Hello" as String

# Multiple assignments
set x to 10
set y to 20
set z to x plus y
```

### Variable Naming Rules

- Start with letter or underscore
- Contain letters, digits, underscores
- Case-sensitive (`myVar` ≠ `MyVar`)
- Cannot be keywords (`set`, `function`, `class`, etc.)

**Conventions:**
- Variables: `snake_case` (e.g., `user_name`, `total_count`)
- Constants: `UPPER_CASE` (e.g., `MAX_SIZE`, `PI`)
- Classes: `PascalCase` (e.g., `UserAccount`, `FileSystem`)
- Functions: `snake_case` (e.g., `calculate_total`, `get_user`)

---

## Data Types

### Primitive Types

```nexuslang
# Integer
set count to 42
set negative to -100

# Float
set pi to 3.14159
set temperature to -5.5

# String
set name to "Alice"
set message to 'Hello, world!'
set multiline to "Line 1
Line 2
Line 3"

# Boolean
set is_valid to true
set has_error to false
```

### Collection Types

```nexuslang
# List
set numbers to [1, 2, 3, 4, 5]
set names to ["Alice", "Bob", "Charlie"]
set mixed to [1, "two", 3.0, true]

# Dictionary
set user to {"name": "Alice", "age": 25}
set config to {"host": "localhost", "port": 8080}

# Tuple
set point to (10, 20)
set rgb to (255, 128, 0)

# Set
set unique_numbers to {1, 2, 3, 4, 5}
```

### Special Types

```nexuslang
# Option type (Rust-style)
set maybe_value to Some(42)
set nothing to None

# Result type (for error handling)
set success to Ok(100)
set failure to Err("Something went wrong")

# Enum
enum Status
  Pending
  InProgress
  Complete
  Failed
end

set current_status to Status.Pending
```

---

## Functions

### Function Definition

```nexuslang
# Basic function
function greet
  print text "Hello, world"
end

# Function with parameters
function greet_person with name as String
  print text "Hello, " plus name
end

# Function with return value
function add with a as Integer, b as Integer returns Integer
  return a plus b
end

# Multiple parameters
function calculate_area with width as Float, height as Float returns Float
  return width times height
end
```

### Calling Functions

```nexuslang
# No parameters
call greet

# With parameters
call greet_person with "Alice"

# With return value
set sum to add with 5, 10
set area to calculate_area with 10.0, 20.0
```

### Lambda Expressions

```nexuslang
# Simple lambda
set double to lambda x returns x times 2

# Lambda with multiple parameters
set add to lambda a, b returns a plus b

# Using lambdas
set result to double with 5  # result = 10
set sum to add with 3, 7     # sum = 10
```

---

## Control Flow

### If Statements

```nexuslang
# Simple if
if age is greater than or equal to 18
  print text "Adult"
end

# If-else
if score is greater than or equal to 60
  print text "Pass"
else
  print text "Fail"
end

# If-else if-else
if score is greater than or equal to 90
  set grade to "A"
else if score is greater than or equal to 80
  set grade to "B"
else if score is greater than or equal to 70
  set grade to "C"
else
  set grade to "F"
end
```

### Comparison Operators

```nexuslang
# Natural language comparisons
if x equals y              # x == y
if x is equal to y         # x == y
if x is not equal to y     # x != y
if x is greater than y     # x > y
if x is less than y        # x < y
if x is greater than or equal to y  # x >= y
if x is less than or equal to y     # x <= y
```

### Loops

```nexuslang
# While loop
while count is less than 10
  print text count
  set count to count plus 1
end

# For each loop
for each item in items
  print text item
end

# Repeat loop
repeat 5 times
  print text "Hello"
end

# Repeat with condition
repeat while counter is less than 100
  set counter to counter plus 10
end

# Break and continue
while true
  if condition1
    break      # Exit loop
  end
  if condition2
    continue   # Skip to next iteration
  end
end
```

### Switch Statement

```nexuslang
switch day
  case "Monday"
    print text "Start of week"
  case "Friday"
    print text "End of week"
  case "Saturday", "Sunday"
    print text "Weekend"
  default
    print text "Midweek"
end
```

---

## Pattern Matching

 See [pattern_matching.md](../3_core_concepts/pattern_matching.md) for full guide.

### Basic Pattern Matching

```nexuslang
match value with
  case 0 then "zero"
  case 1 then "one"
  case n then "other: " plus (n to_string)
end
```

### With Guards

```nexuslang
match age with
  case n if n is less than 0 then "invalid"
  case n if n is less than 13 then "child"
  case n if n is less than 20 then "teenager"
  case n if n is less than 65 then "adult"
  case n then "senior"
end
```

### List Destructuring

```nexuslang
match numbers with
  case [] then "empty"
  case [x] then "single element"
  case [x, y] then "pair"
  case [head, tail...] then "list starting with " plus (head to_string)
end
```

### Option/Result Matching

```nexuslang
match result with
  case Ok(value) then print text "Success: " plus (value to_string)
  case Err(msg) then print text "Error: " plus msg
end
```

---

## Object-Oriented Programming

### Class Definition

```nexuslang
class Person
  # Properties
  name as String
  age as Integer
  
  # Constructor
  function init with self, name as String, age as Integer
    set self.name to name
    set self.age to age
  end
  
  # Methods
  function greet with self
    print text "Hello, I'm " plus self.name
  end
  
  function is_adult with self returns Boolean
    return self.age is greater than or equal to 18
  end
end
```

### Creating Objects

```nexuslang
set person to new Person with "Alice", 25
call person.greet
set adult to person.is_adult
```

### Inheritance

```nexuslang
class Student inherits Person
  student_id as String
  
  function init with self, name as String, age as Integer, id as String
    call super.init with name, age
    set self.student_id to id
  end
  
  function study with self, subject as String
    print text self.name plus " is studying " plus subject
  end
end
```

### Interfaces

```nexuslang
interface Drawable
  function draw with self
  function get_bounds with self returns Rectangle
end

class Circle implements Drawable
  radius as Float
  
  function draw with self
    # Drawing code
  end
  
  function get_bounds with self returns Rectangle
    # Calculate bounding box
  end
end
```

### Properties

```nexuslang
class BankAccount
  private balance as Float
  
  # Getter
  property balance with self returns Float
    return self.balance
  end
  
  # Setter
  property balance with self, value as Float
    if value is less than 0
      raise error "Balance cannot be negative"
    end
    set self.balance to value
  end
end
```

---

## Structs and Unions

 See [struct_union.md](../3_core_concepts/struct_union.md) for full guide.

### Struct Definition

```nexuslang
struct Point
  x as Integer
  y as Integer
end

# Create instance
set p to new Point
set p.x to 10
set p.y to 20
```

### Packed Struct

```nexuslang
packed struct NetworkPacket
  magic as Integer
  version as Integer
  length as Integer
end
```

### Union Definition

```nexuslang
union Data
  int_value as Integer
  float_value as Float
  bool_value as Boolean
end

set data to new Data
set data.int_value to 42
```

---

## Inline Assembly

 See [inline_assembly.md](../3_core_concepts/inline_assembly.md) for full guide.

### Basic Assembly

```nexuslang
function get_value returns Integer
  asm "
    mov rax, 42
    ret
  "
end
```

### Assembly with Parameters

```nexuslang
function add_asm with a as Integer, b as Integer returns Integer
  asm "
    mov rax, rdi     ; First argument
    add rax, rsi     ; Add second argument
    ret
  "
end
```

---

## Memory Operations

### Pointers

```nexuslang
# Get address
set ptr to address of my_variable

# Dereference
set value to dereference ptr

# Sizeof
set size to sizeof Integer
set struct_size to sizeof MyStruct
```

### Memory Allocation

```nexuslang
# Allocate memory
set ptr to allocate 1024  # 1024 bytes

# Free memory
free ptr

# Allocate for type
set point_ptr to allocate sizeof(Point)
```

### Bitwise Operations

```nexuslang
# Bitwise AND
set result to x bitwise_and y

# Bitwise OR
set result to x bitwise_or y

# Bitwise XOR
set result to x bitwise_xor y

# Bitwise NOT
set result to bitwise_not x

# Shift operations
set result to x shift_left 2
set result to x shift_right 3
```

---

## Modules and Imports

### Import Statements

```nexuslang
# Basic import
import math

# Import with alias
import collections as col

# Selective import
from string import split, join, upper

# Relative import
from ..utils import helper_function
```

### Module Definition

```nexuslang
# my_module.nlpl
module MyModule

function helper_function with x as Integer returns Integer
  return x times 2
end

# Private function (not exported)
private function internal_helper
  # Only accessible within this module
end

export helper_function
```

---

## Error Handling

### Try-Catch-Finally

```nexuslang
try
  set result to divide with 10, 0
catch error
  print text "Error: " plus error
finally
  print text "Cleanup"
end
```

### Raising Errors

```nexuslang
function validate_age with age as Integer
  if age is less than 0
    raise error "Age cannot be negative"
  end
  if age is greater than 150
    raise error "Age is unrealistic"
  end
end
```

### Custom Exception Types

```nexuslang
class ValidationError inherits Exception
  message as String
  
  function init with self, msg as String
    set self.message to msg
  end
end

try
  raise ValidationError with "Invalid input"
catch e as ValidationError
  print text "Validation failed: " plus e.message
catch e
  print text "Other error: " plus e
end
```

---

## Operators

### Arithmetic Operators

```nexuslang
set sum to a plus b           # Addition
set diff to a minus b         # Subtraction
set product to a times b      # Multiplication
set quotient to a divided by b # Division
set remainder to a modulo b   # Modulus
set power to a to the power of b # Exponentiation
```

### Comparison Operators

```nexuslang
a equals b                    # Equal
a is equal to b               # Equal
a is not equal to b           # Not equal
a is greater than b           # Greater than
a is less than b              # Less than
a is greater than or equal to b # Greater or equal
a is less than or equal to b    # Less or equal
```

### Logical Operators

```nexuslang
condition1 and condition2     # Logical AND
condition1 or condition2      # Logical OR
not condition                 # Logical NOT
```

### String Operations

```nexuslang
set greeting to "Hello" plus " " plus "World"  # Concatenation
set length to length of "Hello"                # Length
set upper to upper of "hello"                  # Uppercase
set lower to lower of "HELLO"                  # Lowercase
set substring to substring of "Hello" from 1 to 4 # Substring
```

### List Operations

```nexuslang
# Access element
set first to items[0]

# Modify element
set items[1] to "new value"

# Length
set count to length of items

# Append
append "item" to items

# Remove
remove "item" from items
```

### Dictionary Operations

```nexuslang
# Access value
set value to dict["key"]

# Set value
set dict["key"] to "value"

# Check key existence
if "key" in dict
  print text "Key exists"
end

# Remove key
remove "key" from dict
```

---

## Comments

```nexuslang
# Single-line comment

# Multi-line comment
# across multiple lines
# using # at the start of each line

set x to 10  # Inline comment
```

---

## Type Annotations

### Variable Type Annotations

```nexuslang
set name to "Alice" as String
set age to 25 as Integer
set scores to [90, 85, 92] as List of Integer
set user to {"name": "Alice"} as Dictionary of String to String
```

### Function Type Annotations

```nexuslang
function process with items as List of Integer, threshold as Float returns Boolean
  # Function body
  return true
end
```

### Generic Type Annotations

```nexuslang
function find with haystack as List of T, needle as T returns Option of Integer
  # Generic function
end
```

---

## Best Practices

### 1. Use Descriptive Names

```nexuslang
# Good
function calculate_total_price with items as List of Product returns Float

# Bad
function calc with x as List returns Float
```

### 2. Add Type Annotations

```nexuslang
# Good - clear types
function process_user with user as User, action as String returns Result of User, String

# Acceptable - types inferred
function add with a, b
  return a plus b
end
```

### 3. Use Pattern Matching Over If/Else

```nexuslang
# Good - clear and exhaustive
match status with
  case "pending" then handle_pending
  case "active" then handle_active
  case "complete" then handle_complete
  case _ then handle_unknown
end

# Acceptable but less clear
if status equals "pending"
  handle_pending
else if status equals "active"
  handle_active
# ...
```

### 4. Handle Errors Explicitly

```nexuslang
# Good
try
  set result to risky_operation
  return Ok(result)
catch error
  log_error with error
  return Err(error)
end

# Bad - ignores errors
set result to risky_operation
```

### 5. Use Const for Immutable Values

```nexuslang
const PI to 3.14159
const MAX_USERS to 1000
const DEFAULT_TIMEOUT to 30
```

---

## Quick Reference Card

| Category | Syntax | Example |
|----------|--------|---------|
| Variable | `set name to value` | `set x to 10` |
| Function | `function name with params returns Type` | `function add with a, b returns Integer` |
| If | `if condition ... end` | `if x is greater than 0 ... end` |
| Loop | `while condition ... end` | `while count is less than 10 ... end` |
| For | `for each item in list ... end` | `for each name in names ... end` |
| Match | `match expr with case pattern then result end` | `match x with case 0 then "zero" end` |
| Class | `class Name ... end` | `class Person ... end` |
| Struct | `struct Name ... end` | `struct Point ... end` |
| Import | `import module` | `import math` |
| Try | `try ... catch ... end` | `try ... catch error ... end` |

---

## Further Reading

- **Pattern Matching:** [pattern_matching.md](../3_core_concepts/pattern_matching.md)
- **Inline Assembly:** [inline_assembly.md](../3_core_concepts/inline_assembly.md)
- **Structs/Unions:** [struct_union.md](../3_core_concepts/struct_union.md)
- **Type System:** [../5_type_system/](../5_type_system/)
- **Standard Library:** [../stdlib/](../stdlib/)
- **Examples:** [../../examples/](../../examples/)

---

## Summary

NLPL provides:

✅ **Natural syntax** - English-like keywords and phrases  
✅ **Strong typing** - Optional type annotations with inference  
✅ **Modern features** - Pattern matching, generics, lambdas  
✅ **Low-level access** - Inline assembly, pointers, memory ops  
✅ **OOP support** - Classes, inheritance, interfaces  
✅ **Safety** - Try/catch, Option/Result types  

This syntax reference covers the core language features. For detailed guides on specific topics, see the linked documentation files.