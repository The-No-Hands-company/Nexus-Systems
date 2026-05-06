# NexusLang Debugger - Quick Start Guide

**5-Minute Guide to Debugging NexusLang Programs**

---

## Prerequisites

- VS Code installed
- NexusLang workspace open in VS Code
- Extension compiled: `cd vscode-extension && npm run compile`

---

## Method 1: VS Code Debugger (Recommended)

### Step 1: Open Test Program

```bash
code examples/debug_test.nlpl
```

### Step 2: Set a Breakpoint

- Click in the **gutter** (left of line numbers) on line 22
- A red dot appears
- Or press **F9** on the line

### Step 3: Start Debugging

- Press **F5** (or click "Run and Debug" → "Debug NexusLang Program")
- Program executes and pauses at breakpoint

### Step 4: Use Debug Controls

**Toolbar buttons** (or keyboard):
- **Continue** (F5) - Resume until next breakpoint
- **Step Over** (F10) - Execute current line
- **Step Into** (F11) - Enter function call
- **Step Out** (Shift+F11) - Complete current function
- **Restart** (Ctrl+Shift+F5) - Restart debugging
- **Stop** (Shift+F5) - End session

### Step 5: Inspect Variables

**Variables Panel** (left sidebar):
- **Locals** - Variables in current function
- **Globals** - Module-level variables
- Expand objects to see properties

**Hover** over variables in code to see values

**Debug Console** (bottom panel):
- Type variable names: `x`, `numbers[0]`
- Evaluate expressions: `x + y`, `len(numbers)`

### Step 6: View Call Stack

**Call Stack Panel** (left sidebar):
- Shows function call hierarchy
- Click frames to switch context
- See where execution is paused

---

## Method 2: Command-Line Debugger

### Interactive Debugging

```bash
# Debug a program
python -m nexuslang.debugger.debugger examples/debug_test.nlpl

# With preset breakpoints
python -m nexuslang.debugger.debugger examples/debug_test.nlpl --break 22 --break 28
```

### Commands When Paused

```
c, continue    - Continue execution
s, step        - Step into
n, next        - Step over  
o, out         - Step out
p <var>        - Print variable value
vars           - Show all variables
stack          - Show call stack
b <line>       - Set breakpoint
d <line>       - Delete breakpoint
l              - List breakpoints
q              - Quit
```

### Example Session

```
$ python -m nexuslang.debugger.debugger examples/debug_test.nlpl --break 22

Debugging: examples/debug_test.nlpl
Breakpoints: 1

============================================================
Paused at examples/debug_test.nlpl:22 (breakpoint)
============================================================

Source:
    20 |     end
    21 | 
 →  22 |     set x to 42
    23 |     set y to 100

(nlpl-dbg) p x
x = 42

(nlpl-dbg) n    # Step over

(nlpl-dbg) p y  
y = 100

(nlpl-dbg) c    # Continue
Program finished
```

---

## Method 3: Test DAP Server Directly

```bash
# Start DAP server with debug logging
python3 -m nlpl.debugger --debug --log-file /tmp/nlpl-dap.log

# In another terminal, watch logs
tail -f /tmp/nlpl-dap.log

# Server waits for DAP initialize message from client
# (Normally VS Code sends these messages)
```

---

## Common Debugging Scenarios

### Debug a Specific Function

1. Open file with function
2. Set breakpoint on first line of function
3. Press F5
4. When breakpoint hits, inspect parameters

### Debug a Loop

1. Set breakpoint inside loop
2. F5 to start
3. Use F10 to step through each iteration
4. Watch variable values change in Variables panel

### Debug an Exception

1. Run without breakpoints (F5)
2. When exception occurs, debugger auto-pauses
3. Check Call Stack to see where error happened
4. Inspect variables to find cause

### Debug Complex Expression

1. Pause at line before expression
2. Open Debug Console
3. Type expression parts separately
4. See intermediate values

---

## Troubleshooting

### Breakpoint Shows Hollow Circle (Not Verified)

**Cause:** File path mismatch or breakpoint on non-executable line

**Fix:**
- Ensure file is saved
- Check breakpoint is on code line (not blank/comment)
- Try restarting debugger

### "Could not start debug adapter"

**Cause:** Python not found or DAP server missing

**Fix:**
```bash
# Check Python path
which python3

# Update VS Code setting
"nexuslang.debugger.pythonPath": "/usr/bin/python3"

# Verify DAP server exists
ls src/nexuslang/debugger/dap_server.py
```

### Variables Panel Empty

**Cause:** Not properly paused or scope issue

**Fix:**
- Verify execution is paused (yellow highlight on line)
- Try Debug Console instead: type variable name
- Check Call Stack shows current frame

### Debug Console Says "Variable Not Found"

**Cause:** Variable out of scope or misspelled

**Fix:**
- Check Variables panel for actual variable names
- Try inspecting from different stack frame
- Ensure execution passed variable declaration

---

## Next Steps

1. **Try advanced features:**
   - Conditional breakpoints (right-click breakpoint)
   - Expression evaluation in Debug Console
   - Call stack navigation

2. **Read full documentation:**
   - `docs/tooling/debugger.md`

3. **Report issues:**
   - Check `/tmp/nlpl-dap.log` for errors
   - Include log excerpt in bug reports

---

## Keyboard Shortcuts Summary

| Action | Windows/Linux | macOS |
|--------|---------------|-------|
| Start Debugging | F5 | F5 |
| Toggle Breakpoint | F9 | F9 |
| Step Over | F10 | F10 |
| Step Into | F11 | F11 |
| Step Out | Shift+F11 | Shift+F11 |
| Continue | F5 | F5 |
| Stop | Shift+F5 | Shift+F5 |
| Restart | Ctrl+Shift+F5 | Cmd+Shift+F5 |

---

**You're ready to debug! Press F5 and start exploring.**
