# NexusLang REPL Documentation

## Overview

The NexusLang REPL (Read-Eval-Print Loop) is an interactive shell for NexusLang. It provides an immediate feedback environment for testing code, learning the language, and rapid prototyping.

## Features

### Core Capabilities

- **Interactive Execution**: Evaluate NexusLang code immediately and see results
- **Multi-line Input**: Automatically detects incomplete statements (functions, classes, control flow)
- **Command History**: Navigate previous commands with arrow keys, persistent across sessions
- **Auto-completion**: Tab completion for keywords, variables, and functions
- **Error Recovery**: Catch exceptions and continue running (no need to restart)
- **Pretty-print Results**: Formatted output for complex data structures

### Special Commands

The REPL provides special commands prefixed with `:` for controlling the environment:

| Command | Description |
|---------|-------------|
| `:help` | Show help message with all commands |
| `:quit`, `:exit`, `:q` | Exit the REPL |
| `:clear` | Clear the screen |
| `:vars` | Show all variables in current scope |
| `:funcs` | Show all defined functions with signatures |
| `:reset` | Reset interpreter state (clear all variables/functions) |
| `:history` | Show command history (last 20 entries) |
| `:debug` | Toggle debug mode (show tokens and AST) |
| `:type-check` | Toggle type checking on/off |

## Getting Started

### Starting the REPL

**Method 1: Via main module (no file argument)**
```bash
python -m nexuslang.main
```

**Method 2: Explicit REPL flag**
```bash
python -m nexuslang.main --repl
```

**Method 3: Convenience script**
```bash
python nxl_repl.py
```

**Method 4: After running a file**
```bash
python -m nexuslang.main examples/01_basic_concepts.nlpl --repl
```

### Command-line Options

```bash
python -m nexuslang.main [--debug] [--no-type-check] [--repl]
```

- `--debug`: Enable debug mode (show tokens and AST for each command)
- `--no-type-check`: Disable type checking
- `--repl`: Start REPL (automatic if no file specified)

## Usage Examples

### Example 1: Basic Variable Assignment

```
>>> set x to 42
=> 42
>>> set name to "NexusLang"
=> NexusLang
>>> print text name
NLPL
=> NexusLang
>>> :vars

Variables:
 Scope 1:
 x = 42
 name = NexusLang
```

### Example 2: Multi-line Function Definition

The REPL automatically detects incomplete statements and switches to multi-line mode:

```
>>> function greet with name as String returns String
... return "Hello, " plus name
... end
=> greet
>>> greet with "World"
=> Hello, World
```

**Note**: The prompt changes from `>>>` to `...` for continuation lines.

### Example 3: Complex Control Flow

```
>>> function factorial with n as Integer returns Integer
... if n is less than or equal to 1
... return 1
... end
... return n times factorial with n minus 1
... end
=> factorial
>>> factorial with 5
=> 120
```

### Example 4: Working with Collections

```
>>> set numbers to [1, 2, 3, 4, 5]
=> [1, 2, 3, 4, 5]
>>> set total to 0
=> 0
>>> for each num in numbers
... set total to total plus num
... end
=> null
>>> print text total
15
=> 15
```

### Example 5: Struct Definitions

```
>>> struct Point
... x as Integer
... y as Integer
... end
=> Point
>>> set p to new Point
=> <struct StructureInstance>
>>> set p.x to 10
=> 10
>>> set p.y to 20
=> 20
>>> :vars

Variables:
 Scope 1:
 p = <struct StructureInstance>
```

### Example 6: Error Recovery

```
>>> set x to "invalid" plus 42
Error: Type error: Cannot add string and integer
>>> set y to 100
=> 100
>>> print text y
100
=> 100
```

The REPL catches errors and continues execution without restarting.

## Advanced Features

### Tab Completion

Press `Tab` to auto-complete:

- **Keywords**: `func<TAB>` `function`
- **Variables**: `my_var<TAB>` `my_variable`
- **Functions**: `calc<TAB>` `calculate_average`
- **Commands**: `:h<TAB>` `:help`

### Command History

Navigate history with arrow keys:

- **Up Arrow**: Previous command
- **Down Arrow**: Next command

History is persistent across sessions, stored in `~/.nxl_history`.

### Multi-line Input Modes

The REPL detects incomplete input and enters multi-line mode when:

1. **Block keywords**: `function`, `class`, `struct`, `if`, `while`, `for`, `try`
2. **Unmatched brackets**: `(`, `[`, `{`
3. **Explicit continuation**: Line ends with `\`

To complete multi-line input, press Enter on an empty line or type `end`.

### Debug Mode

Enable debug mode to see the internal compilation process:

```
>>> :debug
Debug mode: enabled
>>> set x to 42

--- Tokens ---
 Token(TokenType.SET, 'set')
 Token(TokenType.IDENTIFIER, 'x')
 Token(TokenType.TO, 'to')
 Token(TokenType.INTEGER, '42')

--- AST ---
 VariableDeclaration(name='x', value=42)

=> 42
```

### Type Checking

Toggle type checking on/off:

```
>>> :type-check
Type checking: disabled
>>> set x to "string"
=> string
>>> set y to x plus 42 # No type error
=> string42
>>> :type-check
Type checking: enabled
```

## Tips and Tricks

### 1. Use :vars to Inspect State

After running code, use `:vars` to see what variables are defined:

```
>>> :vars

Variables:
 Scope 1:
 x = 42
 name = NexusLang
 numbers = [1, 2, 3, 4, 5]
```

### 2. Use :funcs to See Available Functions

```
>>> :funcs

Functions:
 greet(name as String) returns String
 factorial(n as Integer) returns Integer
 calculate_average(numbers as List) returns Float
```

### 3. Reset When Things Get Messy

```
>>> :reset
Resetting interpreter...
Interpreter reset complete
```

This clears all variables and functions, giving you a clean slate.

### 4. Copy-Paste Multi-line Code

You can paste multi-line code directly into the REPL:

```
>>> function fibonacci with n as Integer returns Integer
... if n is less than or equal to 1
... return n
... end
... return fibonacci with n minus 1 plus fibonacci with n minus 2
... end
```

### 5. Use History for Repetitive Tasks

Press Up Arrow to recall previous commands instead of retyping:

```
>>> set x to 10
>>> print text x
<UP> <UP> # Recalls "set x to 10"
>>> set x to 20
```

### 6. Combine with Debug for Learning

Enable debug mode to understand how NexusLang parses your code:

```
>>> :debug
Debug mode: enabled
>>> set x to 42
[Shows tokens and AST]
```

This is great for learning NexusLang syntax and troubleshooting issues.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Tab` | Auto-complete |
| `Up Arrow` | Previous command |
| `Down Arrow` | Next command |
| `Ctrl+C` | Interrupt current command (doesn't exit REPL) |
| `Ctrl+D` | Exit REPL (EOF) |
| `Ctrl+L` | Clear screen (alternative to `:clear`) |

## Persistent History

The REPL saves your command history to `~/.nxl_history`. This file persists across sessions, so you can access previous commands even after restarting the REPL.

To view history:
```
>>> :history

History:
 1: set x to 42
 2: function greet with name as String
 3: print text "Hello"
 ...
```

## Integration with File Execution

You can run a file and then drop into the REPL to continue working:

```bash
python -m nexuslang.main examples/01_basic_concepts.nlpl --repl
```

This executes the file first, then starts the REPL with the file's context loaded (variables, functions, etc.).

## Troubleshooting

### Multi-line Input Stuck

If the REPL is stuck in multi-line mode (showing `...` prompt):

1. Type `end` to close the block
2. Press Enter on an empty line
3. Press `Ctrl+C` to interrupt and start fresh

### Auto-completion Not Working

Make sure you have `readline` installed (should be built-in on Linux/macOS):

```bash
pip install readline # If needed on some systems
```

On Windows, consider using `pyreadline3`:

```bash
pip install pyreadline3
```

### History Not Saving

Check permissions on `~/.nxl_history`:

```bash
ls -la ~/.nxl_history
chmod 644 ~/.nxl_history # If needed
```

### Errors Not Showing Details

Enable debug mode for detailed error information:

```
>>> :debug
Debug mode: enabled
```

## Comparison with Other REPLs

### Python REPL

| Feature | NexusLang REPL | Python REPL |
|---------|-----------|-------------|
| Natural syntax | (English-like) | (Technical) |
| Multi-line detection | Automatic | Manual `...` |
| Special commands | `:help`, `:vars`, etc. | Limited |
| Auto-completion | Context-aware | Basic |
| Type checking toggle | Runtime toggle | Static only |
| Reset capability | `:reset` | Must restart |

### Node.js REPL

| Feature | NexusLang REPL | Node.js REPL |
|---------|-----------|-------------|
| Variable inspection | `:vars` | Manual |
| Function listing | `:funcs` | Manual |
| History persistence | Automatic | Automatic |
| Debug mode | Token/AST view | Limited |
| Error recovery | Automatic | Automatic |

## Best Practices

### 1. Start Simple

Begin with basic commands to get comfortable:

```
>>> set x to 42
>>> print text x
```

### 2. Use :vars Frequently

After defining variables, check `:vars` to confirm:

```
>>> set user_name to "Alice"
>>> :vars # Verify it's set correctly
```

### 3. Test Functions Interactively

Define functions in the REPL before adding them to files:

```
>>> function calculate_tax with amount as Float returns Float
... return amount times 0.08
... end
>>> calculate_tax with 100.0 # Test it
=> 8.0
```

### 4. Use Debug for Complex Code

When debugging complex logic, enable debug mode:

```
>>> :debug
>>> # Your complex code here
```

### 5. Save Successful Code

Once you've tested code in the REPL, save it to a file:

1. Use `:history` to see your commands
2. Copy working code
3. Save to a `.nlpl` file
4. Run with `python -m nexuslang.main yourfile.nlpl`

## Future Enhancements

Planned features for future versions:

- **Syntax highlighting**: Colorized output for better readability
- **Code formatting**: Auto-format pasted code
- **Breakpoint integration**: Set breakpoints for debugging
- **Variable watching**: Monitor variable changes in real-time
- **Export session**: Save REPL session to a file
- **Import modules**: Load NexusLang modules interactively
- **Inline help**: `:help <topic>` for specific help

## See Also

- **NexusLang Language Specification**: `docs/reference/language-spec.md`
- **Examples**: `examples/01_basic_concepts.nlpl` through `examples/22_feature_showcase.nlpl`
- **Standard Library**: `docs/reference/stdlib/index.md`
- **Debugger Documentation**: `docs/tooling/debugger.md`
- **LSP Integration**: `docs/tooling/lsp-features.md`

## Contributing

To contribute to the REPL:

1. **Report bugs**: Open an issue with reproduction steps
2. **Request features**: Suggest enhancements via GitHub issues
3. **Submit PRs**: Follow the style guide in `docs/contributing/style-guide.md`

## License

The NexusLang REPL is part of the NexusLang project and follows the same license.

---

**Version**: 0.1.0 
**Last Updated**: 2024 
**Maintainer**: NexusLang Development Team
