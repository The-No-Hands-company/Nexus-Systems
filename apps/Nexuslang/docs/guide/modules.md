# NexusLang Module System Design

## Overview
The NexusLang module system provides a way to organize code into reusable components that can be imported and used across different files. This system enables better code organization, encapsulation, and reuse.

## Module Definition

A module in NexusLang is simply a file containing NexusLang code. Each file automatically becomes a module that can be imported by other files. The module name is derived from the file name (without the `.nlpl` extension).

## Import Syntax

### Basic Import
```
Import module_name.
```

This imports all public definitions from the specified module into the current namespace.

### Selective Import
```
Import function_name, another_function from module_name.
```

This imports only the specified definitions from the module.

### Aliased Import
```
Import module_name as alias_name.
```

This imports the module under a different name to avoid naming conflicts.

### Relative Imports
```
Import ./relative_module.  # Import from the same directory
Import ../parent_module.   # Import from the parent directory
```

## Export Syntax

By default, all top-level definitions (variables, functions, classes) in a module are public and can be imported by other modules. To make a definition private (only accessible within the module), use the `private` keyword:

```
Define private function internal_helper that takes nothing
    # This function cannot be imported by other modules
End function.
```

## Module Resolution

1. **Absolute imports**: The module name is resolved relative to the project root directory.
2. **Relative imports**: The module name is resolved relative to the current file's directory.
3. **Standard library imports**: Modules from the standard library are imported without a path.

## Module Initialization

A module is initialized the first time it is imported. Any top-level code in the module is executed during initialization. Subsequent imports of the same module will use the already initialized module.

## Circular Imports

The module system will detect and handle circular imports gracefully. If module A imports module B, and module B imports module A, both modules will be properly initialized without infinite recursion.

## Namespaces

When a module is imported, its definitions are placed in a namespace corresponding to the module name. This prevents naming conflicts between different modules.

```
Import math.

Create result and set it to math.square_root(16).
```

When using selective imports, the imported definitions are added directly to the current namespace:

```
Import square_root from math.

Create result and set it to square_root(16).
```

## Implementation Plan

1. **Update the lexer** to recognize module-related keywords (`import`, `from`, `as`, `private`).
2. **Update the parser** to parse import statements and private declarations.
3. **Implement a module loader** that can load and cache modules.
4. **Update the interpreter** to support module namespaces and resolution.
5. **Add support for the standard library** as a set of built-in modules.
6. **Implement circular import detection and handling**.
7. **Add support for relative imports**.

## Example Usage

### math_utils.nlpl
```
Define function add that takes a as Integer, b as Integer and returns Integer
    Return a + b.
End function.

Define function multiply that takes a as Integer, b as Integer and returns Integer
    Return a * b.
End function.

Define private function helper that takes nothing
    # Internal helper function
End function.
```

### main.nlpl
```
Import add, multiply from math_utils.

Create result1 and set it to add(5, 10).
Create result2 and set it to multiply(3, 4).

Print("Result 1: " + result1).
Print("Result 2: " + result2).
```

### alternative_import.nlpl
```
Import math_utils.

Create result1 and set it to math_utils.add(5, 10).
Create result2 and set it to math_utils.multiply(3, 4).

Print("Result 1: " + result1).
Print("Result 2: " + result2).
``` 