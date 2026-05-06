# Repeat While Loop Feature

## Overview
NLPL now supports `repeat while` loops, providing a natural language alternative to traditional `while` loops.

## Syntax

### Basic Syntax
```nexuslang
repeat while <condition>
    # loop body
```

### With Break and Continue
```nexuslang
repeat while <condition>
    if <break_condition>
        break
    if <skip_condition>
        continue
    # rest of loop body
```

### Nested Loops
```nexuslang
repeat while <outer_condition>
    repeat while <inner_condition>
        # inner loop body
    # outer loop body
```

## Examples

### Basic Counter Loop
```nexuslang
set counter to 0
repeat while counter is less than 5
    print text "Counter:"
    print value counter
    set counter to counter plus 1
```

Output:
```
Counter:
0
Counter:
1
Counter:
2
Counter:
3
Counter:
4
```

### Loop with Break
```nexuslang
set i to 0
repeat while i is less than 100
    print value i
    set i to i plus 1
    if i is equal to 5
        break
```

Output: 0, 1, 2, 3, 4

### Loop with Continue
```nexuslang
set i to 0
repeat while i is less than 10
    set i to i plus 1
    if i modulo 2 is equal to 0
        continue
    print text "Odd:"
    print value i
```

Output: Odd: 1, 3, 5, 7, 9

### Complex Conditions
```nexuslang
set x to 1
set y to 20
repeat while x is less than y and x is less than 15
    print value x
    set x to x times 2
    set y to y minus 3
```

## Comparison with `while` Loop

Both `repeat while` and `while` are functionally equivalent:

```nexuslang
# Traditional while loop
while x is greater than 0
    set x to x minus 1

# Natural language repeat while loop
repeat while x is greater than 0
    set x to x minus 1
```

The `repeat while` syntax is more natural for users coming from plain English, while `while` is familiar to programmers from other languages.

## Implementation Details

### Parser
- Added `RepeatWhileLoop` AST node class
- Parser checks for `repeat while` keyword sequence
- Supports both indentation-based and `end`/`end repeat` keyword-based syntax

### Interpreter
- Added `execute_repeat_while_loop()` method
- Uses direct condition evaluation (Python's truthiness)
- Supports `break` and `continue` statements
- Maintains scope correctly (no new scope created for loop body)

### Type Checker
- Added `check_repeat_while_loop()` method
- Validates condition type (any type accepted, follows Python truthiness)
- Type checks loop body statements

## Test Files
- `test_programs/unit/basic/test_repeat_while.nlpl` - Basic tests
- `test_programs/unit/basic/test_repeat_while_advanced.nlpl` - Advanced tests with break/continue

## Future Enhancements
- Optional `else` clause (execute if loop completes without break)
- Loop labels for breaking/continuing specific nested loops
- Performance optimizations for compiled mode
