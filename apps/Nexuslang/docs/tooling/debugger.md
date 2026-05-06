# NexusLang Debugger Documentation

## Overview

The NexusLang Debugger provides interactive source-level debugging for NexusLang programs. Set breakpoints, step through code, inspect variables, and analyze call stacks.

## Features

### Core Capabilities

- **Breakpoint System**: Line-based and conditional breakpoints
- **Step Execution**: Step into, step over, step out
- **Variable Inspection**: View and modify variables at runtime
- **Call Stack**: Visualize function call hierarchy
- **Exception Handling**: Automatic pause on exceptions
- **Interactive REPL**: Debug commands at breakpoints

### Breakpoint Types

1. **Line Breakpoints**: Stop at specific line numbers
2. **Conditional Breakpoints**: Stop only when condition is true
3. **Temporary Breakpoints**: Auto-remove after first hit

## Getting Started

### Basic Usage

**Start debugger with file:**
```bash
python -m nexuslang.main program.nlpl --debugger
```

**Set breakpoints from command line:**
```bash
python -m nexuslang.main program.nlpl --debugger --break 10 --break 25
```

**Or use standalone debugger:**
```bash
python nxl_debug.py program.nlpl --break 10
```

### Interactive Debugging

When execution pauses at a breakpoint, you enter the debug REPL:

```
============================================================
Paused at test.nlpl:10 (breakpoint)
============================================================

Source:
 7 | function calculate_sum with numbers as List
 8 | set total to 0
 9 | for each num in numbers
 10 | set total to total plus num
 11 | end
 12 | return total
 13 | end

Current function: calculate_sum

(nlpl-dbg) 
```

## Debugger Commands

### Execution Control

| Command | Short | Description |
|---------|-------|-------------|
| `continue` | `c` | Continue execution until next breakpoint |
| `step` | `s` | Step into next statement (including function calls) |
| `next` | `n` | Step over next statement (don't enter functions) |
| `out` | `o` | Step out of current function |

### Breakpoint Management

| Command | Description | Example |
|---------|-------------|---------|
| `b <line>` | Set breakpoint at line | `b 15` |
| `d <line>` | Delete breakpoint at line | `d 15` |
| `list` / `l` | List all breakpoints | `list` |

### Variable Inspection

| Command | Description | Example |
|---------|-------------|---------|
| `p <var>` | Print variable value | `p total` |
| `vars` | Show all variables in scope | `vars` |

### Stack and Context

| Command | Short | Description |
|---------|-------|-------------|
| `stack` | `bt` | Show call stack (backtrace) |

### Control

| Command | Short | Description |
|---------|-------|-------------|
| `help` | `h` | Show command help |
| `quit` | `q` | Quit debugger (stop program) |

## Usage Examples

### Example 1: Basic Debugging

**Program** (`test_debug.nlpl`):
```nexuslang
function factorial with n as Integer returns Integer
 if n is less than or equal to 1
 return 1
 end
 
 set result to n times factorial with n minus 1
 return result
end

set value to factorial with 5
print text "Result: " plus value
```

**Debug session:**
```bash
$ python -m nexuslang.main test_debug.nlpl --debugger --break 2

Debugging: test_debug.nlpl
Breakpoints: 1

============================================================
Paused at test_debug.nlpl:2 (breakpoint)
============================================================

Source:
 1 | function factorial with n as Integer returns Integer
 2 | if n is less than or equal to 1
 3 | return 1
 4 | end

Current function: factorial

(nlpl-dbg) p n
n = 5

(nlpl-dbg) stack
Call Stack:
 #0: factorial at test_debug.nlpl:2

(nlpl-dbg) n
# Steps to next line...

(nlpl-dbg) c
# Continues execution...
```

### Example 2: Conditional Breakpoints

```python
# In Python code or debugger script:
debugger.add_breakpoint("program.nxl", 10, condition="x > 100")
```

This breakpoint only triggers when variable `x` is greater than 100.

### Example 3: Variable Modification

```
(nlpl-dbg) vars
Variables:
 x = 42
 total = 100

(nlpl-dbg) p x
x = 42

(nlpl-dbg) # Modify x in Python debugger API:
# debugger.set_variable("x", 999)

(nlpl-dbg) p x
x = 999

(nlpl-dbg) c
```

### Example 4: Call Stack Analysis

```
(nlpl-dbg) stack
Call Stack:
 #0: helper at program.nlpl:25
 #1: calculate at program.nlpl:15
 #2: main at program.nlpl:5

(nlpl-dbg) vars
Variables (in helper):
 z = 30
 result = 150
```

## Programmatic API

### Creating a Debugger

```python
from nexuslang.debugger.debugger import Debugger
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib

# Setup
runtime = Runtime()
register_stdlib(runtime)
interpreter = Interpreter(runtime)

# Create debugger
debugger = Debugger(interpreter, interactive=True)
interpreter.debugger = debugger
```

### Adding Breakpoints

```python
# Line breakpoint
bp1 = debugger.add_breakpoint("program.nxl", 10)

# Conditional breakpoint
bp2 = debugger.add_breakpoint("program.nxl", 25, condition="x > 100")

# Temporary breakpoint (auto-remove after hit)
bp3 = debugger.add_breakpoint("program.nxl", 50, temp=True)
```

### Managing Breakpoints

```python
# List all breakpoints
for bp in debugger.list_breakpoints():
 print(bp)

# Remove breakpoint
debugger.remove_breakpoint("program.nxl", 10)

# Toggle enabled/disabled
debugger.toggle_breakpoint("program.nxl", 25)

# Clear all breakpoints
debugger.clear_breakpoints()

# Clear breakpoints in specific file
debugger.clear_breakpoints("program.nxl")
```

### Inspecting Variables

```python
# Get variable value
value = debugger.inspect_variable("x")

# Get all variables
all_vars = debugger.inspect_all_variables()
for name, value in all_vars.items():
 print(f"{name} = {value}")

# Set variable
debugger.set_variable("x", 999)
```

### Call Stack

```python
# Get current frame
frame = debugger.current_frame()
print(frame.function_name)
print(frame.local_vars)

# Print full stack trace
debugger.print_stack_trace()

# Access call stack
for i, frame in enumerate(debugger.call_stack):
 print(f"#{i}: {frame.function_name} at {frame.file}:{frame.line}")
```

### Callbacks

```python
# Register callback for breakpoints
def on_breakpoint(bp, frame):
 print(f"Hit breakpoint at {bp.file}:{bp.line}")
 print(f"In function: {frame.function_name}")

debugger.on_breakpoint = on_breakpoint

# Register callback for steps
def on_step(file, line):
 print(f"Stepped to {file}:{line}")

debugger.on_step = on_step

# Register callback for exceptions
def on_exception(exception, frame):
 print(f"Exception: {exception}")
 if frame:
 print(f"In {frame.function_name}")

debugger.on_exception = on_exception
```

### Statistics

```python
# Print debugging statistics
debugger.print_statistics()

# Access stats directly
print(f"Total steps: {debugger.total_steps}")
print(f"Breakpoints hit: {debugger.breakpoints_hit}")
print(f"Active breakpoints: {len(debugger.list_breakpoints())}")
print(f"Call depth: {len(debugger.call_stack)}")
```

## Integration with REPL

The debugger can be integrated with the REPL for interactive debugging:

```python
# Future enhancement: REPL with debugger
from nexuslang.repl.repl import REPL

repl = REPL(debug=True)
repl.interpreter.debugger = debugger

# Now REPL commands can set breakpoints and debug code
```

## Architecture

### Components

1. **Debugger Class**: Main debugger controller
2. **DebuggerState**: Execution state (running, paused, stepping, etc.)
3. **Breakpoint**: Breakpoint representation
4. **CallFrame**: Call stack frame with local variables

### Integration Points

The debugger hooks into the interpreter at key points:

1. **`execute()`**: Called for each AST node execution
 - Checks breakpoints
 - Handles step modes
 - Tracks line numbers

2. **`execute_function_call()`**: Function entry/exit
 - Pushes call frames
 - Pops frames on return
 - Tracks call depth

3. **`interpret()`**: Exception handling
 - Catches exceptions
 - Provides inspection
 - Auto-pause on errors

### Hook Methods

```python
# Interpreter calls these methods:
debugger.trace_line(file, line) # Each line execution
debugger.trace_call(func, file, line, vars) # Function entry
debugger.trace_return(func, result) # Function exit
debugger.trace_exception(exception) # Exception occurred
```

## Best Practices

### 1. Set Strategic Breakpoints

Set breakpoints at:
- Function entry points
- Loop iterations
- Conditional branches
- Before/after critical operations

### 2. Use Conditional Breakpoints

Instead of breaking every iteration:
```python
# Bad: Break every loop iteration
debugger.add_breakpoint("prog.nxl", 15)

# Good: Break only when condition met
debugger.add_breakpoint("prog.nxl", 15, condition="i > 100")
```

### 3. Inspect Variables at Breakpoints

Use `vars` to see all variables, `p <var>` to inspect specific ones.

### 4. Use Step Over for High-Level Debugging

- `step (s)`: When you need to see inside function calls
- `next (n)`: When you trust the function and just want to see results
- `out (o)`: When you're done with current function

### 5. Monitor Call Stack

Use `stack` to understand:
- How you got to current location
- What functions are active
- Context of execution

## Troubleshooting

### Breakpoints Not Hitting

**Issue**: Breakpoint set but never triggers

**Solutions**:
1. Verify line number (1-indexed)
2. Check file path matches
3. Ensure breakpoint is enabled
4. Verify code actually executes that line

### Source Context Not Showing

**Issue**: Source code doesn't display at breakpoints

**Solutions**:
1. Verify file exists at specified path
2. Check file permissions
3. Use absolute paths for reliability

### Variables Not Visible

**Issue**: Variable not found during inspection

**Solutions**:
1. Check variable scope (may be in parent scope)
2. Verify variable name spelling
3. Use `vars` to see all available variables

### Debugger Exits Unexpectedly

**Issue**: Debugger terminates without completing

**Solutions**:
1. Check for unhandled exceptions
2. Verify program logic doesn't call `exit()`
3. Use `--debug` flag for detailed errors

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+C` | Interrupt (show command prompt) |
| `Ctrl+D` | Quit debugger |
| `Up/Down` | Command history (via readline) |

## Comparison with Other Debuggers

### Python pdb

| Feature | NexusLang Debugger | Python pdb |
|---------|---------------|------------|
| Line breakpoints | | |
| Conditional breakpoints | | |
| Step execution | | |
| Variable inspection | | |
| Call stack | | |
| Natural language | | |
| Interactive REPL | | Limited |

### GDB

| Feature | NexusLang Debugger | GDB |
|---------|---------------|-----|
| Source-level debugging | | |
| Breakpoints | | |
| Watchpoints | Future | |
| Memory inspection | Future | |
| Multi-threaded | Future | |
| Easy to use | | |

## Future Enhancements

Planned features for future versions:

- **Watchpoints**: Break when variable changes
- **Data breakpoints**: Break on memory access
- **Reverse debugging**: Step backwards through execution
- **Multi-threaded debugging**: Debug concurrent code
- **GUI integration**: Visual Studio Code integration
- **Remote debugging**: Debug programs on remote systems
- **Log points**: Log without stopping execution
- **Time-travel debugging**: Record and replay execution

## Performance Considerations

The debugger adds overhead to program execution:

- **No debugger**: Normal speed
- **Debugger attached, no breakpoints**: ~5-10% slower (line tracing)
- **With breakpoints**: Pause at breakpoints (no continuous overhead)
- **Step mode**: Interactive speed (user-controlled)

**Recommendation**: Only enable debugger when actively debugging.

## See Also

- **REPL Documentation**: `docs/tooling/repl.md`
- **Interpreter Architecture**: `docs/contributing/compiler-guide.md`
- **Examples**: `test_programs/integration/debugger/`
- **Test Suite**: `test_debugger.py`

## Contributing

To contribute to the debugger:

1. **Report bugs**: Include reproduction steps
2. **Request features**: Suggest debugger enhancements
3. **Submit PRs**: Follow style guide

## License

The NexusLang Debugger is part of the NexusLang project.

---

**Version**: 0.1.0 
**Last Updated**: January 6, 2026 
**Maintainer**: NexusLang Development Team
