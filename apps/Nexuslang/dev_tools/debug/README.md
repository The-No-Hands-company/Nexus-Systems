# Debug Tools

Development and debugging utilities for NexusLang compiler development.

## Files

### debug_foreach.py
Execution tracing tool for debugging for-each loop code generation.

**Purpose**: Monkey-patches LLVM IR generator to trace method calls and identify code paths.

**Usage**:
```bash
python3 debug_foreach.py
```

**What it does**:
- Parses a minimal for-each loop test case
- Traces calls to `_generate_for_loop` and `_generate_foreach_loop`
- Displays AST structure and generated LLVM IR
- Helped identify Bug 8 (hasattr issue with None values)

### test_foreach_minimal.nlpl
Minimal for-each loop test case for isolated debugging.

**Content**:
```nlpl
set fruits to ["apple", "banana", "orange"]
for each fruit in fruits
    print text fruit
end
```

Used to test for-each loop compilation in isolation without other language features.

## Development Workflow

When debugging compiler issues:

1. Create minimal test case in this directory
2. Create debugging script to trace execution
3. Run script to identify issue
4. Fix bug in compiler
5. Verify with minimal test case
6. Test with full examples

## Historical Issues Debugged

**Bug 8 - For-Each Loop Dispatcher (Jan 24, 2026)**:
- Issue: For-each loops incorrectly dispatched to range-based handler
- Tool: debug_foreach.py with monkey-patching
- Discovery: `hasattr(node, 'start')` returns True even when `start=None`
- Fix: Changed to `node.start is not None`
