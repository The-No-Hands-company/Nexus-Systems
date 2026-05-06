# Switch Statement in NexusLang

## Overview

NLPL provides a `switch` statement for multi-way branching based on the value of an expression. This is more elegant and efficient than long chains of `if-else` statements when testing a single value against multiple possibilities.

## Syntax

```nexuslang
switch <expression>
    case <value1>
        <statements>
    case <value2>
        <statements>
    default
        <statements>
```

## Components

- **`switch <expression>`**: Evaluates the expression once and compares it against each case
- **`case <value>`**: Matches when the switch expression equals the case value
- **`default`**: Optional catch-all that executes if no cases match

## Basic Example

```nexuslang
set day to 3
switch day
    case 1
        print text "Monday"
    case 2
        print text "Tuesday"
    case 3
        print text "Wednesday"
    case 4
        print text "Thursday"
    case 5
        print text "Friday"
    default
        print text "Weekend"
```

**Output:** `Wednesday`

## Features

### Integer Switch

The compiler generates optimized LLVM `switch` instructions for integer case values:

```nexuslang
set status_code to 404
switch status_code
    case 200
        print text "OK"
    case 404
        print text "Not Found"
    case 500
        print text "Server Error"
    default
        print text "Unknown Status"
```

### Negative Values

Switch statements support negative integer values:

```nexuslang
set temperature to -5
switch temperature
    case -10
        print text "Very cold"
    case -5
        print text "Cold"
    case 0
        print text "Freezing"
    case 5
        print text "Cool"
    default
        print text "Other temperature"
```

**Output:** `Cold`

### Zero as Case Value

Zero is a valid case value:

```nexuslang
set value to 0
switch value
    case 0
        print text "Zero detected"
    case 1
        print text "One"
    default
        print text "Other"
```

**Output:** `Zero detected`

### Multiple Statements per Case

Each case can contain multiple statements:

```nexuslang
set operation to 1
set result to 0
switch operation
    case 1
        set result to 10 plus 5
        print text "Addition: "
        print number result
    case 2
        set result to 10 minus 5
        print text "Subtraction: "
        print number result
    default
        print text "Unknown operation"
```

**Output:**
```
Addition: 
15
```

### Default Case (Optional)

The `default` case is optional. If omitted and no cases match, the switch does nothing:

```nexuslang
switch value
    case 1
        print text "One"
    case 2
        print text "Two"
# If value is 3, nothing happens
```

### Nested Switch Statements

Switch statements can be nested:

```nexuslang
set category to 1
set subcategory to 2
switch category
    case 1
        print text "Category 1"
        switch subcategory
            case 1
                print text "  Subcategory 1-1"
            case 2
                print text "  Subcategory 1-2"
            default
                print text "  Unknown subcategory"
    case 2
        print text "Category 2"
    default
        print text "Unknown category"
```

**Output:**
```
Category 1
  Subcategory 1-2
```

## Implementation Details

### LLVM Switch Instruction

For integer constants, the compiler generates efficient LLVM `switch` instructions:

```llvm
switch i64 %value, label %default [
    i64 1, label %case1
    i64 2, label %case2
    i64 3, label %case3
]
```

This is typically more efficient than a chain of conditional branches, especially for many cases.

### If-Else Fallback

For non-constant case values or non-integer types, the compiler automatically falls back to generating an if-else chain:

```nexuslang
set target to get_value()
switch target
    case compute_value(1)  # Non-constant - uses if-else
        print text "Match 1"
    case compute_value(2)
        print text "Match 2"
```

### Constant Expression Evaluation

The compiler can evaluate simple constant expressions at compile time:

```nexuslang
switch value
    case 1 plus 1     # Evaluates to 2
        print text "Two"
    case 3 times 2    # Evaluates to 6
        print text "Six"
```

**Supported constant operations:**
- Unary: `-x`, `+x`
- Binary: `+`, `-`, `*`, `/`, `%`
- Literals: integers, booleans

## Control Flow

### No Fall-Through

Unlike C/C++, NexusLang switch cases do NOT fall through. Each case automatically breaks after execution:

```nexuslang
switch value
    case 1
        print text "One"
        # Automatically exits switch - no fall-through
    case 2
        print text "Two"
```

### Early Exit with Return

Cases can return from functions:

```nexuslang
function get_day_name with day_number as Integer returns String
    switch day_number
        case 1
            return "Monday"
        case 2
            return "Tuesday"
        case 3
            return "Wednesday"
        default
            return "Unknown"
```

## Best Practices

1. **Use switch for multiple comparisons**: When testing one value against 3+ possibilities, prefer switch over if-else chains

2. **Provide default case**: Always include a `default` case for robust error handling:
   ```nexuslang
   switch user_input
       case 1
           # Handle option 1
       case 2
           # Handle option 2
       default
           print text "Invalid option"
   ```

3. **Keep cases simple**: Put complex logic in functions, keep case bodies concise:
   ```nexuslang
   switch command
       case 1
           call process_option_1()
       case 2
           call process_option_2()
   ```

4. **Order cases logically**: Put most common cases first for readability (though compiler optimization handles this)

## Comparison with Match Expression

NLPL also has a `match` expression for pattern matching (more advanced):

**Switch (value comparison):**
```nexuslang
switch value
    case 1
        # Simple value matching
```

**Match (pattern matching):**
```nexuslang
match result with
    case Ok value
        # Destructuring patterns
    case Error message
        # Variant matching
```

Use **switch** for simple value comparisons, **match** for complex pattern matching with destructuring.

## Common Patterns

### Enum-like Values

```nexuslang
# Define constants
set RED to 0
set GREEN to 1
set BLUE to 2

set color to GREEN
switch color
    case 0
        print text "Red"
    case 1
        print text "Green"
    case 2
        print text "Blue"
    default
        print text "Unknown color"
```

### Status Code Handling

```nexuslang
function handle_response with status as Integer
    switch status
        case 200
            print text "Success"
        case 201
            print text "Created"
        case 400
            print text "Bad Request"
        case 401
            print text "Unauthorized"
        case 404
            print text "Not Found"
        case 500
            print text "Server Error"
        default
            print text "Unknown status code"
```

### Menu Systems

```nexuslang
function show_menu
    print text "1. New Game"
    print text "2. Load Game"
    print text "3. Settings"
    print text "4. Exit"
    
function handle_menu_choice with choice as Integer
    switch choice
        case 1
            call start_new_game()
        case 2
            call load_game()
        case 3
            call show_settings()
        case 4
            call exit_game()
        default
            print text "Invalid choice"
```

## Limitations

1. **No ranges**: Switch doesn't support range matching. Use if-else for ranges:
   ```nexuslang
   # Can't do: case 1 to 10
   if score is greater than or equal to 90
       print text "A"
   else if score is greater than or equal to 80
       print text "B"
   ```

2. **No complex patterns**: For complex matching, use `match` expression instead

3. **Case values must be comparable**: The switch expression and case values must be of compatible types

## Performance

- **Integer switches**: Compiled to LLVM `switch` instruction - O(1) for most cases via jump table
- **String switches**: Falls back to strcmp comparisons - O(n) where n is number of cases
- **Non-constant cases**: Falls back to if-else chain - O(n) linear search

## Testing

Comprehensive test suite in `test_programs/compiler/test_switch_statement.nlpl`:
- Integer switch with multiple cases
- Switch with default only
- Switch with operations in cases
- Nested switch statements
- Zero value switching
- Negative value switching

Run tests:
```bash
./nlplc test_programs/compiler/test_switch_statement.nlpl --run
```

## See Also

- Match Expression (for pattern matching)
- If Statement (for complex conditions)
- Control Flow Documentation
- LLVM Switch Instruction Reference
