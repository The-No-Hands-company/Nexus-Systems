# NexusLang Debugger Integration - Quick Reference

## Compilation with Debug Info

```bash
# Compile with debug symbols
python nlplc_llvm.py program.nlpl -o program -g

# Check for debug symbols
file program
# Output: ELF 64-bit ... not stripped

# View DWARF sections
readelf -w program | head -50
```

## GDB Quick Reference

```bash
# Start debugger
gdb ./program

# Set breakpoint at line
(gdb) break program.nlpl:10

# Set breakpoint at function
(gdb) break function_name

# Run program
(gdb) run

# Step over (next line)
(gdb) next

# Step into (enter function)
(gdb) step

# Continue execution
(gdb) continue

# Print variable
(gdb) print variable_name

# Show all variables
(gdb) info locals

# Show call stack
(gdb) backtrace

# List source code
(gdb) list

# Quit
(gdb) quit
```

## LLDB Quick Reference

```bash
# Start debugger
lldb ./program

# Set breakpoint
(lldb) breakpoint set --file program.nlpl --line 10
(lldb) breakpoint set --name function_name

# Run program
(lldb) run

# Step over/into
(lldb) thread step-over
(lldb) thread step-in

# Continue
(lldb) continue

# Print variable
(lldb) frame variable variable_name

# Show all variables
(lldb) frame variable

# Show call stack
(lldb) bt

# Quit
(lldb) quit
```

## Debug Information Generated

The NexusLang compiler generates:
- Source file locations (file, line, column)
- Function debug info (name, parameters, return type)
- Variable debug info (name, type, scope)
- Type information (basic types, structs, classes)
- Call stack unwinding metadata

## Features

 **Supported:**
- Set breakpoints at source lines
- Step through code (next, step)
- Inspect variables
- View call stack
- Function-level debugging
- GDB and LLDB support

 **Future:**
- Watch expressions
- Conditional breakpoints
- Variable formatting
- Remote debugging

## Status

**Implementation:** Complete
**Tested with:** GDB, LLDB
**DWARF Version:** 4
**Overhead:** Zero runtime impact
