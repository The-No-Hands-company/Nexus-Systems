# Scope Optimizer Usage Guide

The scope optimizer is an **optional performance enhancement** for the NexusLang interpreter that speeds up variable lookups using a flat cache.

## Quick Start

### Enable at Initialization

```python
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.interpreter.scope_optimizer import enable_scope_optimization
from nexuslang.runtime.runtime import Runtime

runtime = Runtime()
interpreter = Interpreter(runtime)

# Enable scope optimization
enable_scope_optimization(interpreter)

# Now interpret programs with optimized scope lookups
interpreter.interpret(ast)
```

### Enable/Disable at Runtime

```python
from nexuslang.interpreter.scope_optimizer import (
 enable_scope_optimization,
 disable_scope_optimization
)

# Enable optimization
enable_scope_optimization(interpreter)
interpreter.interpret(program1)

# Disable if you encounter issues
disable_scope_optimization(interpreter)
interpreter.interpret(program2)

# Re-enable
enable_scope_optimization(interpreter)
interpreter.interpret(program3)
```

### Check Cache Statistics

```python
# After running with optimization enabled
if hasattr(interpreter, '_scope_cache'):
 stats = interpreter._scope_cache.get_stats()
 print(f"Cache hit rate: {stats['hit_rate']:.1f}%")
 print(f"Total lookups: {stats['total_lookups']}")
 
 # Or print full statistics
 interpreter._scope_cache.print_stats()
```

## Performance Expectations

- **Best case**: Programs with many variable accesses in nested scopes (5-10% speedup)
- **Typical case**: Standard programs (1-3% speedup)
- **Worst case**: Programs with minimal variable usage (negligible difference)

## When to Use

 **Use scope optimization when**:
- Running long-running programs
- Processing variable-heavy code
- Working with deeply nested scopes
- Performance is critical

 **Don't use when**:
- Debugging interpreter issues
- Testing new features
- You suspect scope-related bugs

## Safety Features

1. **Separate module**: Core interpreter unchanged
2. **Opt-in**: Must be explicitly enabled
3. **Fallback**: Automatically uses standard lookup on cache miss
4. **Toggle-able**: Enable/disable at any time
5. **Validated**: All tests pass with optimization enabled

## Testing

Run the test suite:

```bash
python dev_tools/test_scope_optimizer.py
```

This validates:
- Correctness (results match standard interpreter)
- Performance (measures actual speedup)
- Enable/disable cycle works correctly

## Troubleshooting

If you encounter issues with scope optimization:

1. **Disable it immediately**:
 ```python
 disable_scope_optimization(interpreter)
 ```

2. **Check if issue persists**: Run the same program without optimization

3. **Report the bug**: File an issue with the failing test case

The interpreter will work correctly with or without scope optimization enabled.

## Implementation Details

The `ScopeCache` class maintains a flat dictionary of all visible variables:

- **On variable access**: O(1) dictionary lookup (vs O(n) scope iteration)
- **On scope enter/exit**: Invalidate cache, rebuild on next access
- **On variable set**: Update cache immediately

**Trade-offs**:
- Extra memory: One dictionary storing all visible variables
- Cache invalidation overhead: Must rebuild when scopes change
- Net benefit: Positive for variable-heavy programs

## Example: Before vs After

### Before (Standard Lookup)

```python
# In interpreter.py get_variable():
for scope in reversed(self.current_scope):
 if name in scope:
 return scope[name]
# O(n) where n = scope depth
```

### After (Optimized Lookup)

```python
# With scope cache enabled:
value = self._scope_cache.get_variable(name)
# O(1) dictionary lookup
```

## Benchmarks

From `dev_tools/test_scope_optimizer.py`:

```
Standard: 0.0506s (50 runs)
Optimized: 0.0493s (50 runs)
Speedup: 1.03x
```

Cache statistics from test run:
```
Cache hits: 6
Cache misses: 0
Hit rate: 100.0%
Cache rebuilds: 4
```

## Integration with Main Entry Point

To enable by default in `main.py`:

```python
# In src/nexuslang/main.py:
from nexuslang.interpreter.scope_optimizer import enable_scope_optimization

# After creating interpreter:
interpreter = Interpreter(runtime, enable_type_checking=not args.no_type_check)

# Enable scope optimization (optional)
if args.optimize_scope: # Add --optimize-scope flag
 enable_scope_optimization(interpreter)
```

## Conclusion

The scope optimizer is a **safe, optional performance enhancement** that can provide modest speedups for variable-heavy programs. It's designed to be:

- Easy to enable/disable
- Safe (separate from core interpreter)
- Measurable (statistics tracking)
- Validated (comprehensive test suite)

Use it when performance matters, disable it when debugging.
