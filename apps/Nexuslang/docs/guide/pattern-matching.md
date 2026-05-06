# Pattern Matching in NexusLang

**Status:** ✅ Fully implemented (February 3, 2026)  
**Complexity:** Intermediate

---

## Overview

Pattern matching is a powerful feature that allows you to match values against patterns and execute code based on the match. NLPL's pattern matching system includes:

- Literal matching (numbers, strings, booleans)
- Variable binding (capture matched values)
- Wildcard patterns (`_` matches anything)
- Guard clauses (additional conditions)
- List destructuring
- Tuple destructuring
- Enum/union matching
- Nested patterns

Pattern matching makes code more readable and safer by:
- Eliminating complex if/else chains
- Ensuring all cases are handled
- Capturing values directly in the pattern
- Type-safe destructuring

---

## Basic Syntax

### Match Expression Structure

```nexuslang
match <expression> with
  case <pattern> then <expression>
  case <pattern> then <expression>
  else <expression>
end
```

The `match` expression:
1. Evaluates the input expression
2. Tests each pattern in order
3. Executes the first matching case
4. Returns the result of the matched expression
5. Falls back to `else` if no pattern matches

**Important:** Match expressions are **expressions**, not statements. They return values!

---

## Pattern Types

### 1. Literal Patterns

Match exact values:

```nexuslang
function describe_number with n as Integer returns String
  match n with
    case 0 then return "zero"
    case 1 then return "one"
    case 42 then return "the answer"
    else return "some other number"
  end
end

set result to describe_number with 1
# result = "one"
```

**Supported literal types:**
- Integers: `0`, `42`, `-5`
- Floats: `3.14`, `-0.5`
- Strings: `"hello"`, `""`
- Booleans: `true`, `false`

### 2. Variable Binding Patterns

Capture the matched value:

```nexuslang
function process_value with x as Integer returns String
  match x with
    case 0 then return "zero"
    case n then return "Got: " plus (n to_string)
  end
end

set result to process_value with 42
# result = "Got: 42"
```

**Key points:**
- Variable patterns always match
- The variable is bound to the matched value
- Use meaningful variable names for clarity

### 3. Wildcard Pattern

Match anything without binding:

```nexuslang
function is_weekend with day as String returns Boolean
  match day with
    case "Saturday" then return true
    case "Sunday" then return true
    case _ then return false
  end
end

set weekend to is_weekend with "Monday"
# weekend = false
```

**When to use `_`:**
- You need a catch-all but don't care about the value
- More explicit than `else` for remaining cases
- Common in list/tuple destructuring

### 4. List Patterns

Destructure lists:

```nexuslang
function sum_first_two with numbers as List of Integer returns Integer
  match numbers with
    case [] then return 0
    case [x] then return x
    case [x, y] then return x plus y
    case [x, y, rest...] then return x plus y
    else return 0
  end
end

set result to sum_first_two with [10, 20, 30]
# result = 30
```

**List pattern syntax:**
- `[]` - Empty list
- `[x]` - Single element (binds to `x`)
- `[x, y]` - Exactly two elements
- `[x, y, z...]` - Two or more elements (rest bound to `z`)
- `[head, tail...]` - Head-tail decomposition

### 5. Tuple Patterns

Destructure tuples:

```nexuslang
function describe_point with p as Tuple returns String
  match p with
    case (0, 0) then return "origin"
    case (x, 0) then return "on x-axis at " plus (x to_string)
    case (0, y) then return "on y-axis at " plus (y to_string)
    case (x, y) then return "at (" plus (x to_string) plus ", " plus (y to_string) plus ")"
  end
end

set point to (3, 4)
set desc to describe_point with point
# desc = "at (3, 4)"
```

**Tuple pattern features:**
- Fixed size matching
- Can mix literals and variables
- Nested tuples supported

### 6. Guard Clauses

Add conditions to patterns:

```nexuslang
function categorize_age with age as Integer returns String
  match age with
    case n if n is less than 0 then return "invalid"
    case n if n is less than 13 then return "child"
    case n if n is less than 20 then return "teenager"
    case n if n is less than 65 then return "adult"
    case n then return "senior"
  end
end

set category to categorize_age with 25
# category = "adult"
```

**Guard syntax:**
```nexuslang
case <pattern> if <condition> then <expression>
```

**Guard features:**
- Arbitrary boolean expressions
- Can reference pattern-bound variables
- Evaluated after pattern matches
- Multiple guards can match the same pattern

### 7. Nested Patterns

Combine patterns for complex matching:

```nexuslang
function analyze_data with data as List returns String
  match data with
    case [] then return "empty"
    case [x] if x is less than 0 then return "single negative"
    case [x] then return "single positive"
    case [x, y] if x equals y then return "pair of same values"
    case [x, y] then return "pair of different values"
    case [x, y, rest...] if x is greater than y then return "descending start"
    case _ then return "other"
  end
end
```

---

## Advanced Usage

### Option Type Matching

```nexuslang
function get_or_default with opt as Option of Integer, default as Integer returns Integer
  match opt with
    case Some(value) then return value
    case None then return default
  end
end

set maybe_value to Some(42)
set result to get_or_default with maybe_value, 0
# result = 42

set nothing to None
set result2 to get_or_default with nothing, 100
# result2 = 100
```

### Result Type Matching

```nexuslang
function handle_result with res as Result of Integer, String returns String
  match res with
    case Ok(value) then return "Success: " plus (value to_string)
    case Err(message) then return "Error: " plus message
  end
end

set result to divide with 10, 2
set msg to handle_result with result
# msg = "Success: 5"
```

### Enum Matching

```nexuslang
enum Status
  Pending
  InProgress
  Complete
  Failed
end

function status_message with s as Status returns String
  match s with
    case Status.Pending then return "Waiting to start"
    case Status.InProgress then return "Currently working"
    case Status.Complete then return "All done"
    case Status.Failed then return "Something went wrong"
  end
end
```

### Complex Destructuring

```nexuslang
function analyze_nested with data as List returns String
  match data with
    case [[x, y], [a, b]] if x equals a then
      return "First elements match"
    case [[x, y], [a, b]] then
      return "Nested pairs"
    case [first, rest...] then
      return "List starting with " plus (first to_string)
    case _ then
      return "Other structure"
  end
end
```

---

## Pattern Matching vs Switch

### Pattern Matching (Recommended)

```nexuslang
function classify with x as Integer returns String
  match x with
    case n if n is less than 0 then return "negative"
    case 0 then return "zero"
    case n if n is less than 10 then return "small"
    case n then return "large"
  end
end
```

**Advantages:**
- Can bind variables
- Supports guards
- Destructures complex data
- Type-safe
- Returns values

### Switch Statement (Legacy)

```nexuslang
function classify with x as Integer returns String
  switch x
    case 0
      return "zero"
    case 1
      return "one"
    default
      return "other"
  end
end
```

**When to use switch:**
- Simple literal matching only
- Compatibility with older code
- When you need fallthrough behavior

**Use pattern matching when:**
- You need variable binding
- You want to destructure data
- You need guards/conditions
- You want cleaner, more expressive code

---

## Best Practices

### 1. Order Patterns from Specific to General

```nexuslang
# Good: Specific patterns first
match x with
  case 0 then "zero"
  case 1 then "one"
  case n if n is less than 10 then "small"
  case n then "large"
end

# Bad: General pattern catches everything
match x with
  case n then "large"  # This matches everything!
  case 0 then "zero"   # Never reached
  case 1 then "one"    # Never reached
end
```

### 2. Use Meaningful Variable Names

```nexuslang
# Good: Clear intent
match user_input with
  case age if age is greater than 0 then process_age with age
  case _ then show_error
end

# Bad: Unclear
match user_input with
  case x if x is greater than 0 then process_age with x
  case _ then show_error
end
```

### 3. Avoid Wildcard When You Need the Value

```nexuslang
# Good: Bind to use the value
match calculate_result with
  case value then print text "Result: " plus (value to_string)
end

# Bad: Can't access the value
match calculate_result with
  case _ then print text "Result: ???"
end
```

### 4. Use Guards for Complex Conditions

```nexuslang
# Good: Guard makes intent clear
match score with
  case s if s is greater than or equal to 90 then "A"
  case s if s is greater than or equal to 80 then "B"
  case s if s is greater than or equal to 70 then "C"
  case _ then "F"
end

# Bad: Nested if statements
match score with
  case s then
    if s is greater than or equal to 90
      return "A"
    else if s is greater than or equal to 80
      return "B"
    # ...
end
```

### 5. Always Handle All Cases

```nexuslang
# Good: Catch-all case
match status with
  case "pending" then handle_pending
  case "complete" then handle_complete
  case _ then handle_unknown  # Safety net
end

# Risky: Missing cases
match status with
  case "pending" then handle_pending
  case "complete" then handle_complete
  # What if status is something else?
end
```

### 6. Use Destructuring for Clarity

```nexuslang
# Good: Intent is clear
match coordinates with
  case (x, y) if x equals 0 and y equals 0 then "origin"
  case (x, 0) then "on x-axis"
  case (0, y) then "on y-axis"
  case (x, y) then "general point"
end

# Bad: Manual extraction
match coordinates with
  case coords then
    set x to coords[0]
    set y to coords[1]
    if x equals 0 and y equals 0
      return "origin"
    # ...
end
```

---

## Common Patterns

### 1. List Processing

```nexuslang
function list_sum with numbers as List of Integer returns Integer
  match numbers with
    case [] then return 0
    case [head, tail...] then
      return head plus (list_sum with tail)
  end
end
```

### 2. Option Handling

```nexuslang
function safe_divide with a as Integer, b as Integer returns Option of Integer
  if b equals 0
    return None
  else
    return Some(a divided by b)
  end
end

match safe_divide with 10, 2 with
  case Some(result) then print text "Answer: " plus (result to_string)
  case None then print text "Cannot divide by zero"
end
```

### 3. State Machine

```nexuslang
enum State
  Idle
  Running
  Paused
  Stopped
end

function next_state with current as State, action as String returns State
  match (current, action) with
    case (State.Idle, "start") then return State.Running
    case (State.Running, "pause") then return State.Paused
    case (State.Paused, "resume") then return State.Running
    case (State.Running, "stop") then return State.Stopped
    case (State.Paused, "stop") then return State.Stopped
    case (state, _) then return state  # No transition
  end
end
```

### 4. Validation

```nexuslang
function validate_input with input as String returns Result of String, String
  match input with
    case "" then return Err("Input cannot be empty")
    case s if (length of s) is less than 3 then
      return Err("Input too short (minimum 3 characters)")
    case s if (length of s) is greater than 100 then
      return Err("Input too long (maximum 100 characters)")
    case s then return Ok(s)
  end
end
```

---

## Type Checking

Pattern matching is fully integrated with NLPL's type system:

```nexuslang
function process with value as Integer returns String
  match value with
    case n then n to_string  # Type checker knows n is Integer
  end
end

# Type error example:
function bad_example with value as Integer returns String
  match value with
    case n then return n  # ERROR: expected String, got Integer
  end
end
```

**Type checking features:**
- Pattern variables inherit type from matched expression
- Return type must match across all cases
- Guards must be boolean expressions
- Destructuring is type-safe

---

## Performance Considerations

### Pattern Matching Performance

Pattern matching is efficient:
- Patterns are tested in order (O(n) for n patterns)
- Guard evaluation is lazy (only checked if pattern matches)
- No overhead compared to equivalent if/else chains

### Optimization Tips

1. **Put common cases first:**
```nexuslang
# Good: Most common case first
match request_type with
  case "GET" then handle_get      # 80% of requests
  case "POST" then handle_post    # 15% of requests
  case "DELETE" then handle_delete # 5% of requests
end
```

2. **Use literals before guards:**
```nexuslang
# Good: Literal match is faster
match status with
  case 200 then "OK"
  case 404 then "Not Found"
  case code if code is greater than or equal to 500 then "Server Error"
end
```

3. **Avoid redundant patterns:**
```nexuslang
# Bad: Last case is redundant
match x with
  case n if n is less than 0 then "negative"
  case 0 then "zero"
  case n if n is greater than 0 then "positive"  # Redundant
  case n then "positive"  # This is sufficient
end
```

---

## Comparison with Other Languages

### Rust-style Matching

NLPL's pattern matching is inspired by Rust:

```rust
// Rust
match value {
    0 => "zero",
    n if n < 10 => "small",
    _ => "large"
}
```

```nexuslang
# NexusLang
match value with
  case 0 then "zero"
  case n if n is less than 10 then "small"
  case _ then "large"
end
```

**Differences:**
- NexusLang uses `with...case...then...end` instead of braces
- NexusLang uses natural language comparisons (`is less than` vs `<`)
- Both support guards, destructuring, and variable binding

### Python-style Matching (3.10+)

```python
# Python
match value:
    case 0:
        return "zero"
    case n if n < 10:
        return "small"
    case _:
        return "large"
```

NLPL's syntax is more explicit with `with`, `then`, and `end` keywords.

---

## Examples from Standard Library

Pattern matching is used throughout NLPL's standard library:

### Result Type Implementation

```nexuslang
class Result of T, E
  # ...
  
  function map with self, f as Function returns Result of U, E
    match self with
      case Ok(value) then return Ok(f with value)
      case Err(error) then return Err(error)
    end
  end
  
  function and_then with self, f as Function returns Result of U, E
    match self with
      case Ok(value) then return f with value
      case Err(error) then return Err(error)
    end
  end
end
```

### Option Type Implementation

```nexuslang
class Option of T
  # ...
  
  function map with self, f as Function returns Option of U
    match self with
      case Some(value) then return Some(f with value)
      case None then return None
    end
  end
  
  function filter with self, predicate as Function returns Option of T
    match self with
      case Some(value) if predicate with value then return Some(value)
      case _ then return None
    end
  end
end
```

---

## Migration Guide

### From Switch to Match

**Old code (switch):**
```nexuslang
switch status
  case "idle"
    handle_idle
  case "running"
    handle_running
  default
    handle_unknown
end
```

**New code (match):**
```nexuslang
match status with
  case "idle" then handle_idle
  case "running" then handle_running
  case _ then handle_unknown
end
```

### From If/Else to Match

**Old code (if/else):**
```nexuslang
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

**New code (match):**
```nexuslang
set grade to match score with
  case s if s is greater than or equal to 90 then "A"
  case s if s is greater than or equal to 80 then "B"
  case s if s is greater than or equal to 70 then "C"
  case _ then "F"
end
```

---

## Troubleshooting

### Pattern Never Matches

**Problem:**
```nexuslang
match x with
  case _ then "always matches"
  case 0 then "never reached"  # Dead code!
end
```

**Solution:** Order patterns from specific to general.

### Guard Always False

**Problem:**
```nexuslang
match x with
  case n if n is greater than 100 and n is less than 0 then "impossible"
end
```

**Solution:** Check guard logic for contradictions.

### Type Mismatch

**Problem:**
```nexuslang
function process with x as Integer returns String
  match x with
    case n then return n  # ERROR: Integer vs String
  end
end
```

**Solution:** Ensure all branches return the correct type.

### Missing Else/Catch-all

**Problem:**
```nexuslang
match x with
  case 0 then "zero"
  case 1 then "one"
  # What if x is 2?
end
```

**Solution:** Add `else` or catch-all pattern:
```nexuslang
match x with
  case 0 then "zero"
  case 1 then "one"
  case _ then "other"
end
```

---

## Further Reading

- **Type System Guide:** `docs/guide/type-system.md`
- **Option/Result Types:** `docs/reference/stdlib/index.md`
- **Enums:** `docs/guide/enum-types.md`
- **Control Flow:** `docs/tutorials/beginner/02-variables-functions-control-flow.md`
- **Examples:** `examples/15_pattern_matching.nlpl`

---

## Summary

Pattern matching in NexusLang provides:

✅ **Literal matching** - Exact value comparison  
✅ **Variable binding** - Capture matched values  
✅ **Destructuring** - Lists, tuples, structs  
✅ **Guards** - Conditional matching  
✅ **Type safety** - Full type checking integration  
✅ **Expressiveness** - Cleaner than if/else chains  

Pattern matching makes NexusLang code more readable, safer, and more maintainable. Use it whenever you need to match values against multiple patterns or destructure complex data structures.

**Status:** Fully implemented and ready for production use!
