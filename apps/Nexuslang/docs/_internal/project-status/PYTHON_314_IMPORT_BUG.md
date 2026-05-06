# CRITICAL: Python 3.14 Import Bug

## Issue Summary

**Status**: BLOCKING - All NexusLang tools hang on startup 
**Affected**: Python 3.14.0 
**Cause**: Import system regression in Python 3.14 
**Impact**: Cannot use nlplc_llvm.py, tests, or any NexusLang tooling 

## Symptoms

- Any Python script that imports `nlpl.parser.lexer` hangs indefinitely
- The module code is valid (can be loaded with `exec()`)
- Only affects `import` statement, not `exec()`
- Clears all `__pycache__` directories doesn't help

## Reproduction

```bash
$ python3 --version
Python 3.14.0

$ timeout 3 python3 -c "from nexuslang.parser.lexer import Lexer"
# Hangs... timeout kills it
```

## Root Cause

Python 3.14 has a regression in the import system that causes circular dependency 
detection or module loading to enter an infinite loop when:
- Large enum definitions (TokenType has 201 entries)
- Module with complex `__init__` methods
- Certain import graph patterns

The lexer.py module itself is valid Python code and works when loaded via `exec()`.

## Workarounds

### Option 1: Use Python 3.13 or Earlier (RECOMMENDED)

```bash
# Install Python 3.13
sudo pacman -S python313 # Arch/Manjaro
sudo apt install python3.13 # Ubuntu/Debian

# Use it for NexusLang
python3.13 nlplc_llvm.py program.nlpl -o output
```

### Option 2: Use exec() Workaround

See `dev_tools/import_workaround.py` for a module loader that uses `exec()` 
instead of `import`. This works but is not recommended for production.

### Option 3: Monkey-patch sys.path

**NOT TESTED** - might work:

```python
import sys
import importlib
sys.path.insert(0, '/path/to/nlpl/src')

# Disable import caching
importlib.invalidate_caches()
```

## Temporary Solution

Until Python 3.14 is fixed or we can use 3.13, NexusLang development is BLOCKED on 
systems with only Python 3.14.

## Action Items

- [ ] Test on Python 3.13 / 3.12 / 3.11
- [ ] File bug report with Python 
- [ ] Add Python version check to nlplc_llvm.py
- [ ] Document Python version requirement in README

## Testing Status

- Lexer code is valid (exec test passes)
- Tokenization works (via exec workaround) 
- Cannot import normally
- Cannot run compiler
- Cannot run tests
- FFI tests blocked

## Files Affected

- `nlplc_llvm.py` - Compiler entry point
- `src/nlpl/__init__.py` - Package initialization 
- All test files in `tests/`
- All dev tools importing nlpl modules

---

**Date**: 2025-11-26 
**Reporter**: AI Development Session 
**Python Version**: 3.14.0 
**OS**: Linux (Manjaro/Arch)
