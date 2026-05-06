# NexusLang Common Syntax Errors and Solutions

This document lists common syntax errors encountered when writing NexusLang code and how to fix them.

## Table of Contents
1. [String Concatenation with Parentheses](#string-concatenation-with-parentheses)
2. [Reserved Keywords as Identifiers](#reserved-keywords-as-identifiers)
3. [Raise Statement Syntax](#raise-statement-syntax)
4. [Try-Catch Syntax](#try-catch-syntax)
5. [Pattern Matching Syntax](#pattern-matching-syntax)

---

## String Concatenation with Parentheses

### Problem
Cannot use nested parentheses when calling `to_string` in string concatenation expressions.

### Incorrect
```nexuslang
set num to 42
print text "The number is: " plus (num to_string)  # SYNTAX ERROR
```

### Correct
```nexuslang
set num to 42
set num_str to num to_string
print text "The number is: " plus num_str  # Works!
```

### Explanation
NLPL's parser currently doesn't support nested parentheses in certain expression contexts. Always extract intermediate values to variables before using them in string concatenation.

### More Examples
```nexuslang
# Incorrect
print text "Result: " plus (calculate_value with x to_string)

# Correct
set result to calculate_value with x
set result_str to result to_string
print text "Result: " plus result_str
```

---

## Reserved Keywords as Identifiers

### Problem
Certain words are reserved tokens in NexusLang and cannot be used as variable names, parameter names, or function names.

### Known Reserved Keywords
- `callback` (TokenType.CALLBACK)
- `error` (TokenType.ERROR - can be used in catch blocks but not as general identifier)
- `message` (TokenType.MESSAGE - restricted in certain contexts)
- `with` (TokenType.WITH)
- `to` (TokenType.TO)
- `as` (TokenType.AS)
- `function` (TokenType.FUNCTION)
- `class` (TokenType.CLASS)
- `if`, `else`, `while`, `for`, `return`, etc.

### Incorrect
```nexuslang
function process with callback as Function
  # ...
end
```

### Correct
```nexuslang
function process with handler as Function
  # ...
end

# Or use alternative names:
# - handler, fn, func, processor
# - action, operation, task
# - on_complete, on_success, on_error
```

### Finding Reserved Keywords
If you encounter an error like "Expected X, got TokenType.IDENTIFIER", the identifier you used might be a reserved keyword. Try using a different name.

---

## Raise Statement Syntax

### Problem
The raise statement requires specific syntax with the `with message` clause.

### Incorrect
```nexuslang
raise error "Something went wrong"  # SYNTAX ERROR
raise "Something went wrong"        # SYNTAX ERROR
```

### Correct
```nexuslang
# Raise with message
raise error with message "Something went wrong"

# Raise with dynamic message
set msg to "Error: Invalid value"
raise error with message msg

# Raise custom exception type
raise ValueError with message "Value must be positive"

# Re-raise current exception (in catch block)
raise error
```

### Complete Example
```nexuslang
function validate_age with age as Integer returns Boolean
  if age is less than 0
    raise error with message "Age cannot be negative"
  end
  
  if age is greater than 150
    raise error with message "Age is unrealistic"
  end
  
  return true
end

try
  call validate_age with -5
catch error with message
  print text "Validation failed: " plus message
end
```

---

## Try-Catch Syntax

### Problem
Try-catch blocks have specific syntax requirements for exception handling.

### Basic Syntax
```nexuslang
try
  # Code that might fail
  set result to 10 divided by 0
catch error with message
  # Handle the error
  print text "Error occurred: " plus message
end
```

### Nested Try-Catch
⚠️ **Note:** Nested try-catch blocks may not be fully supported in the current version.

```nexuslang
# This may not work in all cases:
try
  try
    # Inner operation
  catch error with message
    # Inner handler
  end
catch error with message
  # Outer handler
end

# Workaround: Use separate functions
function inner_operation returns Boolean
  try
    # Risky operation
  catch error with message
    print text "Inner error: " plus message
    return false
  end
  return true
end

try
  set success to inner_operation
  if not success
    # Handle inner failure
  end
catch error with message
  print text "Outer error: " plus message
end
```

### Exception Properties
```nexuslang
# Catch with message variable
try
  raise error with message "Test error"
catch error with message
  print text message  # message variable is available
end

# Catch with error variable (stores error details)
try
  raise error with message "Test error"
catch error
  # error variable contains the exception
  print text error
end
```

---

## Pattern Matching Syntax

### Current Status
Pattern matching is partially implemented in NexusLang. The `match` statement has parser support but may have limitations.

### Basic Match (Parser Support)
```nexuslang
function describe_number with n as Integer returns String
  match n with
    case 0
      return "zero"
    case 1
      return "one"
    case 2
      return "two"
    default
      return "other"
  end
end
```

### Workaround: Use If-Else Chains
For reliable pattern matching, use if-else chains:

```nexuslang
function describe_number with n as Integer returns String
  if n is equal to 0
    return "zero"
  else if n is equal to 1
    return "one"
  else if n is equal to 2
    return "two"
  else
    return "other"
  end
end
```

---

## Additional Tips

### 1. Use `--debug` Flag
Run with `--debug` to see detailed tokenization and AST information:
```bash
python -m nexuslang.main myfile.nlpl --debug
```

### 2. Use `--no-type-check` Flag
If type checking is causing issues, disable it:
```bash
python -m nexuslang.main myfile.nlpl --no-type-check
```

### 3. Check Error Messages Carefully
NLPL error messages include:
- Line and column numbers
- Source code context with caret pointer (^)
- Expected vs actual token types
- Suggestions for common mistakes

### 4. Extract Complex Expressions
When in doubt, break complex expressions into multiple statements:

```nexuslang
# Instead of:
print text "Result: " plus (calculate with (get_value with x to_string) to_string)

# Do this:
set value to get_value with x
set value_str to value to_string
set result to calculate with value_str
set result_str to result to_string
print text "Result: " plus result_str
```

### 5. Consistent Naming
Follow consistent naming conventions to avoid confusion with reserved keywords:
- Use `snake_case` for variables and functions: `user_data`, `process_input`
- Use `PascalCase` for classes: `UserAccount`, `DataProcessor`
- Avoid single-letter names except for loop counters (`i`, `j`, `k`)
- Be descriptive: `handler` instead of `cb`, `message` instead of `msg` (when not conflicting)

---

## Getting Help

- Check the examples directory: `examples/` with organized subdirectories
- Read the documentation: `docs/`
- Look for similar examples: `grep -r "pattern" examples/`
- Use the error system's suggestions - they often point to the solution
- Review the BNF grammar: `src/nexuslang/parser/bnf_grammar.txt`

---

## Version Information

This document is current as of NexusLang v1.0 pre-release (February 2026).

Some features may be added or changed in future versions. Check the RELEASE_NOTES for updates.
