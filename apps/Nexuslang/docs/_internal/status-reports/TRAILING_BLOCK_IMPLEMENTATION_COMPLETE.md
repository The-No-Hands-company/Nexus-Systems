# Trailing Block Syntax - Implementation Complete

**Date**: February 11, 2026  
**Feature**: Trailing Block Syntax for Callbacks and DSLs  
**Status**: ✅ **COMPLETE**

## Overview

Trailing block syntax allows passing code blocks as the last argument to functions, enabling clean DSL-style syntax for callbacks, iterators, and functional programming patterns.

## Syntax

```nlpl
# Zero-argument function with trailing block
function_name do
    # block body
end

# Function with arguments and trailing block
function_name with arg1 and arg2 do
    # block body
end

# Blocks with parameters
function_name do param1 and param2
    # block can use param1 and param2
end

# Mixed: regular args + block with parameters
function_name with data do item
    # process item
end
```

## Implementation Details

### Parser Changes (`src/nlpl/parser/parser.py`)

1. **Added context flag** (`_in_argument_context`) to prevent ambiguous parsing
   - Prevents `variable do` from being interpreted as function call when parsing arguments
   - Set to True when parsing function arguments, False elsewhere

2. **Modified `primary()` method** (lines 4468-4497)
   - Checks for `do` token after identifier (but only if NOT in argument context)
   - Creates FunctionCall with trailing block for zero-arg functions

3. **Modified `function_call()` method** (lines 4753-4760, 4768-4775)
   - Sets `_in_argument_context = True` when parsing arguments
   - Resets to False after each argument is parsed
   - Checks for `do` after all arguments are parsed

4. **Implemented `parse_trailing_block()` method** (lines 4833-4925)
   - Parses `do [param1 and param2 ...] ... end` syntax
   - Parameters separated by `and` keyword
   - Optional type annotations: `do x as Integer`
   - **Bug fix**: Changed condition from `_can_be_identifier(token)` to `token.type == TokenType.IDENTIFIER or _can_be_identifier(token)`
   - Creates `LambdaExpression` AST node

### AST Changes (`src/nlpl/parser/ast.py`)

- **FunctionCall** class now has `trailing_block` parameter
- Blocks represented as `LambdaExpression` nodes with parameters and body

### Interpreter Changes (`src/nlpl/interpreter/interpreter.py`)

1. **Added `_create_closure()` method** (lines 2004-2047)
   - Creates Python callable from LambdaExpression
   - Captures current scope in closure
   - Binds block parameters to arguments when called
   - Handles explicit `return` statements in blocks

2. **Modified `execute_function_call()` (lines 2192-2195)
   - Converts trailing block to closure and appends to positional_args
   - Block passed as last argument to the function

3. **Added closure invocation support** (lines 2199-2219)
   - Checks if function_name is actually a variable holding a callable
   - If so, calls it directly with arguments
   - Enables `block()` syntax to invoke stored closures

## Key Bugs Fixed

### Bug 1: Parser Ambiguity with Variable Arguments
**Problem**: `func with variable do block` was parsed as nested function call  
**Symptom**: `Runtime Error: Function 'variable' is not defined`  
**Root Cause**: `primary()` eagerly consumed `variable do` during argument parsing  
**Solution**: Added `_in_argument_context` flag to disable trailing block parsing in argument context

### Bug 2: FunctionCall Constructor Misalignment
**Problem**: `FunctionCall(name, arguments, type_arguments, line_num)` - positional args out of order  
**Symptom**: `Runtime Error: 'int' object has no attribute 'items'`  
**Solution**: Use named parameters: `FunctionCall(..., named_arguments=None, trailing_block=None, line_number=line_num)`

### Bug 3: Block Parameters Not Parsed
**Problem**: `do x and y` resulted in 0 parameters being parsed  
**Symptom**: `Runtime Error: Name Error: Name 'x' is not defined`  
**Root Cause**: Condition used `_can_be_identifier(token)` which only checks contextual keywords, not IDENTIFIER tokens  
**Solution**: Changed to `token.type == TokenType.IDENTIFIER or _can_be_identifier(token)`

## Features Validated

✅ **Zero-arg functions with blocks**: `func do ... end`  
✅ **Functions with literal args + blocks**: `func with 42 do ... end`  
✅ **Functions with variable args + blocks**: `func with my_var do ... end`  
✅ **Blocks with parameters**: `do x and y ... end`  
✅ **Closure invocation**: `block()` or `block(args)`  
✅ **Scope capture**: Blocks access outer scope variables  
✅ **Return values**: Blocks can use `return` statement  
✅ **Nested blocks**: Blocks within blocks  
✅ **Reusable closures**: Store blocks in variables and call multiple times

## Test Coverage

All test files in `test_programs/unit/basic/`:

1. **test_trailing_block_simple.nlpl** - Zero-arg function with block
2. **test_var_and_block.nlpl** - Function with variable arg and block
3. **test_arg_and_block.nlpl** - Function with literal arg and block
4. **test_closure_call.nlpl** - Calling closure with `block()` syntax
5. **test_block_with_params.nlpl** - Blocks with parameters
6. **test_block_params_debug.nlpl** - Parameter binding validation
7. **test_trailing_blocks_complete.nlpl** - Comprehensive feature test (6 scenarios)

All tests passing ✅

## Example Usage

```nlpl
# Iterator with callback
function each with list and block
    for each item in list
        set result to block(item)
    end
end

set numbers to [1, 2, 3, 4, 5]
each with numbers do value
    print text value times 2
end

# Reusable transformer
function make_transformer with operation
    return operation
end

set double to make_transformer do x
    return x times 2
end

print text double(5)   # Output: 10
print text double(10)  # Output: 20
```

## Performance Characteristics

- **Block creation**: O(1) - captures scope dictionary
- **Block invocation**: O(1) + O(n) for scope restoration (n = captured variables)
- **Memory overhead**: Scope dictionary per closure
- **No optimization yet**: Future work to optimize scope capture (only capture referenced variables)

## Remaining Work

### High Priority
- ✅ Parser implementation - COMPLETE
- ✅ Closure creation - COMPLETE
- ✅ Closure invocation - COMPLETE
- ✅ Basic testing - COMPLETE
- ⏳ **Type checking** - Validate block signatures match function expectations
- ⏳ **Comprehensive testing** - Edge cases, error conditions
- ⏳ **Documentation** - Tutorial and API guide

### Medium Priority
- Standard library integration:
  - `map(list, block)` - Transform elements
  - `filter(list, block)` - Select elements
  - `reduce(list, initial, block)` - Accumulate
  - `each(list, block)` - Iterate with side effects
  - `sort(list, block)` - Custom comparator
  - `find(list, block)` - Search with predicate

### Low Priority
- Performance optimization: Selective scope capture
- Syntax sugar: `&block` parameter syntax
- Block inspection: Arity checking, parameter names

## API Reference

### Parser Methods
- `parse_trailing_block()` - Parse `do...end` block syntax
- Returns `LambdaExpression` with parameters and body

### Interpreter Methods
- `_create_closure(lambda_node)` - Create Python callable from AST node
- Returns closure function that captures scope and binds parameters

### AST Nodes
- `FunctionCall(name, arguments, type_arguments, named_arguments, trailing_block, line_number)`
- `LambdaExpression(parameters, body, line_number)`

## Design Rationale

### Why `do...end` syntax?
- Natural English phrasing: "do this, then do that"
- Clear block boundaries (no ambiguity with indentation)
- Familiar to Ruby developers (similar syntax)
- Distinguishable from function definitions

### Why `and` for parameters?
- Consistent with NLPL's natural language philosophy
- Reads naturally: "do x and y" = "using x and y"
- Alternative considered: commas (rejected as less natural)

### Why closures as Python callables?
- Enables seamless interop with Python runtime
- Simple implementation (no need for custom callable class)
- Natural invocation syntax: `block(args)`
- Easy to pass to Python stdlib functions

### Why capture entire scope?
- Simplicity: No need to analyze variable references
- Correctness: Guaranteed to capture all needed variables
- Future optimization: Can add selective capture later
- Trade-off: Memory overhead acceptable for initial implementation

## Integration with Existing Features

### Works With
- ✅ Named parameters: `func with x: 10 do block`
- ✅ Default parameters: Function can have defaults, block is additional
- ✅ Variadic parameters: Block passed after *args
- ✅ Type annotations: `do x as Integer`
- ✅ Return statements: Blocks can use `return`
- ✅ Error handling: Try/catch works in blocks

### Future Integration
- ⏳ Generics: Type-safe blocks `do<T>(item: T)`
- ⏳ Async blocks: `async do ... end`
- ⏳ Coroutines: `yield` in blocks
- ⏳ Pattern matching: `match value do case ...`

## Conclusion

Trailing block syntax is **fully functional** and ready for use. The core implementation is complete, with all major features working correctly. Remaining work is enhancement (type checking, standard library, optimizations) rather than core functionality.

This feature enables NexusLang to support clean DSL-style syntax for:
- Callbacks and event handlers
- Iterators and functional programming
- Custom control structures
- Configuration and builders
- Test frameworks (e.g., `describe ... do`, `it ... do`)

The implementation follows NLPL's philosophy of natural, readable code while maintaining low-level power and flexibility.
