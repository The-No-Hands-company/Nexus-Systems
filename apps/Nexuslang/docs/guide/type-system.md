# NexusLang Type System Design

## Overview

The NexusLang type system provides static type checking to catch type errors at compile time rather than runtime. It supports both explicit type annotations and type inference.

## Type Annotation Syntax

### Variable Declarations

```
Set <variable_name> as <type> to <expression>.
```

Example:

```
Set count as Integer to 10.
Set name as String to "John".
Set is_valid as Boolean to true.
```

### Function Definitions

```
Define function <function_name>(<param1> as <type1>, <param2> as <type2>, ...) returns <return_type> as
    // Function body
End function.
```

Example:

```
Define function add(a as Integer, b as Integer) returns Integer as
    Return a + b.
End function.
```

### Optional Parameters with Default Values

```
Define function greet(name as String, greeting as String = "Hello") returns String as
    Return greeting + ", " + name + "!".
End function.
```

## Primitive Types

- `Integer`: Whole numbers (e.g., 1, 42, -10)
- `Float`: Floating-point numbers (e.g., 3.14, -0.5)
- `String`: Text strings (e.g., "Hello, world!")
- `Boolean`: True or false values
- `Null`: Represents the absence of a value

## Complex Types

### Lists

```
Set numbers as List<Integer> to [1, 2, 3, 4, 5].
Set names as List<String> to ["Alice", "Bob", "Charlie"].
```

### Dictionaries

```
Set person as Dictionary<String, String> to {"name": "John", "email": "john@example.com"}.
Set counts as Dictionary<String, Integer> to {"apples": 5, "oranges": 10}.
```

### Custom Types (Classes)

```
Define class Person as
    Property name as String.
    Property age as Integer.
    
    Define method greet() returns String as
        Return "Hello, my name is " + this.name + " and I am " + this.age + " years old.".
    End method.
End class.
```

## Type Inference

The type system supports type inference, allowing developers to omit explicit type annotations when the type can be inferred from the context.

Example:

```
Set count to 10.  // Inferred as Integer
Set name to "John".  // Inferred as String
Set is_valid to true.  // Inferred as Boolean
```

## Type Checking Rules

### Assignment Compatibility

A value can be assigned to a variable if the value's type is compatible with the variable's type.

### Function Call Compatibility

Arguments passed to a function must be compatible with the function's parameter types.

### Operator Compatibility

Operators require operands of compatible types.

## Type Conversion

Explicit type conversion functions:

- `to_integer(value)`: Convert to Integer
- `to_float(value)`: Convert to Float
- `to_string(value)`: Convert to String
- `to_boolean(value)`: Convert to Boolean

## Implementation Plan

1. Update the lexer to recognize type-related tokens
2. Update the parser to parse type annotations
3. Implement a type checker that validates type compatibility
4. Add type inference capabilities
5. Integrate type checking into the interpreter
