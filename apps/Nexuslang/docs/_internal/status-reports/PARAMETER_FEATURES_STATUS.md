# NexusLang Parameter Features - Implementation Status

**Date:** February 10, 2026  
**Status:** Named, Default, and Variadic Parameters COMPLETE ✅

---

## Completed Features

### ✅ Named/Keyword Parameters (February 2026)

**Status:** COMPLETE AND WORKING

**What Was Implemented:**

1. **Parser Support** (`src/nlpl/parser/parser.py`)
   - Recognizes `param: value` syntax
   - Parses mixed positional and named arguments
   - Enforces rule: positional args must precede named args
   - Handles contextual keywords (NAME, TEXT, etc.) as parameter names
   - Methods: `function_call()`, `_is_named_argument()`, `_parse_named_argument()`

2. **AST Support** (`src/nlpl/parser/ast.py`)
   - `FunctionCall` node has `named_arguments` dict
   - Stores mapping of parameter names to argument expressions

3. **Interpreter Support** (`src/nlpl/interpreter/interpreter.py`)
   - Evaluates named arguments
   - Resolves named args to correct parameter positions
   - Validates parameter names exist
   - Detects duplicate parameter assignments
   - Method: `_resolve_function_arguments()`
   - Passes `**kwargs` to Python stdlib functions

4. **Type Checker Support** (`src/nlpl/typesystem/typechecker.py`)
   - Counts total arguments (positional + named)
   - Type checks named argument expressions
   - Validates argument count matches function signature

**Syntax:**

```nlpl
# Named parameters only
greet with name: "Alice" and greeting: "Hello"

# Mixed positional and named
add_three with 10 and 20 and z: 30

# Reordered named parameters
configure with ssl: True and port: 443 and host: "api.com"

# All positional still works
greet with "Bob" and "Hi"
```

**Example File:** `examples/02_functions/06_named_parameters.nlpl`

**Benefits:**
- Self-documenting code
- Prevents argument order mistakes
- Enables parameter reordering
- Essential for functions with 3+ parameters

---

## Pending Features

### 2. Default Parameter Values ✅ COMPLETE

**Status**: Fully implemented and tested (February 10, 2026)

**Implementation Details**:
- ✅ AST `Parameter` class supports `default_value` field
- ✅ Parser recognizes "default to <expression>" syntax
- ✅ Interpreter evaluates defaults in `_resolve_function_arguments()`
- ✅ Type checker tracks `has_defaults` and `min_params` on `FunctionType`
- ✅ Validates argument counts between required and total parameters
- ✅ Works seamlessly with named arguments

**Syntax**:

---
**Syntax**:
```nlpl
function greet with name as String and greeting as String default to "Hello"
    print text greeting
    print text name
end

greet with "Alice"  # Uses default "Hello"
greet with "Bob" and "Hi"  # Overrides default
```

**Files Modified**:
- `src/nlpl/parser/ast.py`: Added `default_value` parameter to `Parameter.__init__`
- `src/nlpl/parser/parser.py`: Updated `parameter()` to parse "default to" syntax
- `src/nlpl/interpreter/interpreter.py`: Updated `_resolve_function_arguments()` to evaluate defaults
- `src/nlpl/typesystem/typechecker.py`: Added `has_defaults` and `min_params` tracking

**Test Coverage**:
- `test_programs/unit/basic/test_default_params.nlpl`: Comprehensive tests
- Covers: single defaults, multiple defaults, named args with defaults
- All tests passing ✅

---

### 3. Variadic Parameters (*args) ✅ COMPLETE

**Status**: Fully implemented and tested (February 10, 2026)

**Implementation Details**:
- ✅ AST `Parameter` class supports `is_variadic` flag
- ✅ Parser recognizes `*param_name as Type` syntax
- ✅ Interpreter collects remaining positional args into list
- ✅ Type checker wraps variadic parameter type in `ListType`
- ✅ Type checker tracks `has_variadic` and `variadic_index` on `FunctionType`
- ✅ Validates minimum required argument counts
- ✅ Works seamlessly with required and default parameters

**Syntax:**
```nlpl
# Simple variadic
function print_all with *messages as String
    for each msg in messages
        print text msg
    end
end

print_all with "Hello" and "World"  # Collects into list
print_all with "One"  # Works with single arg
print_all  # Empty list

# Combined with required parameters
function format_message with prefix as String and *parts as String
    print text prefix
    for each part in parts
        print text part
    end
end

format_message with "LOG:" and "Server" and "started"

# With numeric types
function sum_all with *numbers as Integer returns Integer
    set total to 0
    for each num in numbers
        set total to total plus num
    end
    return total
end

set result to sum_all with 1 and 2 and 3 and 4 and 5  # Returns 15
```

**Files Modified**:
- `src/nlpl/parser/ast.py`: Added `is_variadic` parameter to `Parameter.__init__`
- `src/nlpl/parser/parser.py`: Updated `parameter()` to parse `*param_name` syntax
- `src/nlpl/interpreter/interpreter.py`: Updated `_resolve_function_arguments()` to collect variadic args
- `src/nlpl/typesystem/typechecker.py`: Added `has_variadic`, `variadic_index` tracking, wrapped type in `ListType`

**Test Coverage**:
- `test_programs/unit/basic/test_variadic_params.nlpl`: Comprehensive tests
- `examples/02_functions/08_variadic_parameters.nlpl`: 5 documented examples
- Covers: simple variadic, required+variadic, numeric operations, all features combined, empty variadic
- All tests and examples passing ✅

**Benefits**:
- Variable-length argument lists (like print, log functions)
- Collection operations (sum, join, concat)
- Flexible APIs
- Natural expression of "all of these items"

---

## Pending Features

### 🎯 Trailing Block Syntax

**Status:** NOT IMPLEMENTED

**What's Needed:**
- Allow function calls to have trailing indented blocks
- Block becomes last parameter (function/lambda)
- Very natural for callbacks, event handlers, DSLs

**Proposed Syntax:**
```nlpl
# Callback style
button.on_click with
    print text "Button clicked!"
    update_ui
end

# Collection operations
set doubled to array.map with item
    return item times 2
end

# With parameters before block
process_data with filename: "data.txt" and
    for each line in lines
        parse_line with line
    end
end
```

**Benefits:**
- Extremely readable for DSLs
- Natural for event handlers
- Reduces visual clutter (no lambda syntax needed)
- Similar to Ruby blocks, Kotlin trailing lambdas

**Priority:** MEDIUM - Nice to have, great for certain patterns

**Estimated Effort:** 3-4 weeks
- Parser: Detect indented block after `with`
- AST: Represent block as anonymous function
- Interpreter: Create closure for trailing block
- Handle parameter capture/scope
- Test with various patterns (callbacks, loops, etc.)

---

### 📋 Keyword-Only Parameters

**Status:** NOT IMPLEMENTED

**What's Needed:**
- Force certain parameters to be named (not positional)
- Use `*` marker to separate positional from keyword-only
- Prevents confusion in functions with many similar parameters

**Proposed Syntax:**
```nlpl
# After *, all parameters must be named
function configure with host as String, *, timeout as Integer, retries as Integer, ssl as Boolean
    # ...

# Valid calls
configure with "localhost" and timeout: 5000 and retries: 3 and ssl: True

# INVALID: Cannot pass timeout positionally
configure with "localhost" and 5000  # ERROR
```

**Benefits:**
- Forces clarity for complex functions
- Prevents positional argument mistakes
- Documents "these parameters need names"

**Priority:** LOW - Nice for very complex APIs

**Estimated Effort:** 1-2 weeks
- Add `*` as separator in parameter list parsing
- Mark parameters after `*` as keyword-only
- Interpreter: Enforce keyword-only constraint
- Type checker: Validate usage

---

## Implementation Roadmap

**Phase 1 (Immediate - 1-2 weeks):**
- ✅ Named parameters (COMPLETE - Feb 9, 2026)
- ✅ Default parameter values (COMPLETE - Feb 10, 2026)

**Phase 2 (Short-term - 2-3 weeks):**
- ✅ Variadic parameters (COMPLETE - Feb 10, 2026)

**Phase 3 (Medium-term - 3-4 weeks):**
- 🎯 Trailing block syntax

**Phase 4 (Future - 1-2 weeks):**
- 📋 Keyword-only parameters

---

## Design Philosophy

NLPL's parameter system should prioritize:

1. **Natural Language Feel** - Read like English prose
2. **Clarity Over Brevity** - Self-documenting code
3. **Flexibility** - Support both terse and verbose styles
4. **Safety** - Catch mistakes at parse/type-check time
5. **Consistency** - All features work together harmoniously

**Named parameters achieve all five goals**, which is why they were prioritized first.

---

## Related Documentation

- Example file: `examples/02_functions/06_named_parameters.nlpl`
- Roadmap: `docs/project_status/MISSING_FEATURES_ROADMAP.md` (PART 0)
- GitHub Issue: TBD (create tracking issue)
- Test programs: TBD (create test_programs/unit/named_params/)

---

## Questions for Future Design

1. **Default + Named Interaction:**
   - `function f with x as Integer defaults to 5 and y as Integer`
   - Can you call `f with y: 10`? (skipping x, using default)
   - Answer: Yes, named params should allow this

2. **Variadic + Named Interaction:**
   - `function f with *args as Integer and named_param: value`
   - Is this confusing? Does `and` distinguish them clearly?
   - Consider: `function f with *args and **kwargs`?

3. **Trailing Block + Regular Params:**
   - `button.on_click with priority: 5 and <block>`
   - How to distinguish block from other parameters?
   - Maybe: Trailing block is always last, no ambiguity

4. **Type Inference for Defaults:**
   - `function f with x defaults to 5`  # Infer x: Integer?
   - Or require: `function f with x as Integer defaults to 5`?
   - Recommendation: Require type annotation for clarity
