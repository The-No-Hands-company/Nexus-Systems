"""
Optimized scope lookup for the NexusLang interpreter.

This module provides an optional performance optimization layer for variable
scope lookups. It maintains a flat cache of variables across all scopes to
avoid O(n) iteration through the scope stack on every variable access.

SAFETY: This is a separate module that can be enabled/disabled via a flag.
If bugs are discovered, the interpreter can fall back to the standard
scope lookup without any changes to core interpreter code.
"""

from typing import Any, Dict, List, Optional

class ScopeCache:
 """
 Optimized scope lookup cache.
 
 Maintains a flattened view of variables across all scopes for O(1) lookups.
 Automatically invalidates and rebuilds when scopes change.
 
 Performance Impact:
 - get_variable(): O(n) O(1) for cache hits
 - set_variable(): O(1) O(1) (cache update)
 - enter_scope(): O(1) O(1)
 - exit_scope(): O(n) O(n) (must rebuild cache for invalidated scope)
 
 Trade-offs:
 - Memory: Extra dictionary storing all visible variables
 - Complexity: Additional bookkeeping on scope changes
 - Benefit: Significant speedup for variable-heavy programs
 """
 
 def __init__(self, scope_stack: List[Dict[str, Any]]):
 """
 Initialize scope cache.
 
 Args:
 scope_stack: Reference to the interpreter's scope stack
 """
 self.scope_stack = scope_stack
 self._cache: Dict[str, Any] = {}
 self._cache_valid = False
 self._cache_scope_depth = 0
 
 # Statistics for monitoring cache effectiveness
 self.stats = {
 'hits': 0,
 'misses': 0,
 'invalidations': 0,
 'rebuilds': 0
 }
 
 def _rebuild_cache(self) -> None:
 """
 Rebuild the flat cache from the scope stack.
 
 Variables in inner scopes shadow those in outer scopes.
 We iterate from outermost to innermost, so later assignments overwrite.
 """
 self._cache.clear()
 
 # Build flat dictionary with proper shadowing
 for scope in self.scope_stack:
 self._cache.update(scope)
 
 self._cache_valid = True
 self._cache_scope_depth = len(self.scope_stack)
 self.stats['rebuilds'] += 1
 
 def get_variable(self, name: str) -> Optional[Any]:
 """
 Get a variable from the cache.
 
 Returns:
 Variable value if found, None if not found
 
 Note: Returns None to indicate "not found" rather than raising an exception.
 The caller (interpreter) should handle the NameError with proper error messages.
 """
 # Rebuild cache if invalidated
 if not self._cache_valid or self._cache_scope_depth != len(self.scope_stack):
 self._rebuild_cache()
 
 # Try cache lookup
 if name in self._cache:
 self.stats['hits'] += 1
 return self._cache[name]
 
 self.stats['misses'] += 1
 return None
 
 def has_variable(self, name: str) -> bool:
 """Check if a variable exists in any scope."""
 if not self._cache_valid or self._cache_scope_depth != len(self.scope_stack):
 self._rebuild_cache()
 return name in self._cache
 
 def set_variable(self, name: str, value: Any, scope_index: int = -1) -> None:
 """
 Update cache when a variable is set.
 
 Args:
 name: Variable name
 value: Variable value
 scope_index: Which scope to set in (-1 = innermost)
 """
 # Update the actual scope stack (caller should do this too)
 # We just update our cache to stay in sync
 if self._cache_valid:
 self._cache[name] = value
 # If cache is invalid, it will rebuild on next access
 
 def enter_scope(self) -> None:
 """
 Called when entering a new scope.
 
 Invalidates cache since scope structure changed.
 """
 self._cache_valid = False
 self.stats['invalidations'] += 1
 
 def exit_scope(self) -> None:
 """
 Called when exiting a scope.
 
 Invalidates cache and removes variables from the exited scope.
 """
 if not self.scope_stack:
 return
 
 # Mark cache as invalid - will rebuild on next access
 self._cache_valid = False
 self.stats['invalidations'] += 1
 
 def invalidate(self) -> None:
 """Manually invalidate the cache."""
 self._cache_valid = False
 self.stats['invalidations'] += 1
 
 def get_stats(self) -> Dict[str, int]:
 """Get cache statistics for performance monitoring."""
 total = self.stats['hits'] + self.stats['misses']
 hit_rate = (self.stats['hits'] / total * 100) if total > 0 else 0
 
 return {
 **self.stats,
 'total_lookups': total,
 'hit_rate': hit_rate
 }
 
 def print_stats(self) -> None:
 """Print cache statistics (for debugging/profiling)."""
 stats = self.get_stats()
 print("=== Scope Cache Statistics ===")
 print(f" Total lookups: {stats['total_lookups']}")
 print(f" Cache hits: {stats['hits']}")
 print(f" Cache misses: {stats['misses']}")
 print(f" Hit rate: {stats['hit_rate']:.1f}%")
 print(f" Cache rebuilds: {stats['rebuilds']}")
 print(f" Invalidations: {stats['invalidations']}")

class OptimizedScopeLookup:
 """
 Mixin class to add optimized scope lookup to the interpreter.
 
 Usage:
 interpreter = Interpreter(runtime, enable_scope_cache=True)
 
 This adds the following optimized methods:
 - _cached_get_variable()
 - _cached_set_variable()
 - _cached_enter_scope()
 - _cached_exit_scope()
 
 The interpreter can call these instead of the standard methods when
 the scope cache is enabled.
 """
 
 def __init__(self, *args, enable_scope_cache=False, **kwargs):
 """Initialize with optional scope cache."""
 super().__init__(*args, **kwargs)
 
 self.enable_scope_cache = enable_scope_cache
 self._scope_cache = None
 
 if enable_scope_cache:
 self._scope_cache = ScopeCache(self.current_scope)
 
 def _cached_get_variable(self, name: str) -> Any:
 """
 Get variable using scope cache if enabled, otherwise use standard lookup.
 
 This method can replace get_variable() calls in performance-critical paths.
 """
 if self.enable_scope_cache and self._scope_cache:
 value = self._scope_cache.get_variable(name)
 if value is not None or self._scope_cache.has_variable(name):
 return value
 
 # Fall back to standard scope lookup (from interpreter.py)
 return self._original_get_variable(name)
 
 def _cached_set_variable(self, name: str, value: Any) -> Any:
 """Set variable and update cache if enabled."""
 # Update scope stack (standard behavior)
 self.current_scope[-1][name] = value
 
 # Update cache
 if self.enable_scope_cache and self._scope_cache:
 self._scope_cache.set_variable(name, value)
 
 return value
 
 def _cached_enter_scope(self) -> None:
 """Enter scope and invalidate cache if enabled."""
 self.current_scope.append({})
 
 if self.enable_scope_cache and self._scope_cache:
 self._scope_cache.enter_scope()
 
 def _cached_exit_scope(self) -> None:
 """Exit scope and invalidate cache if enabled."""
 if len(self.current_scope) > 1:
 self.current_scope.pop()
 
 if self.enable_scope_cache and self._scope_cache:
 self._scope_cache.exit_scope()
 
 def get_scope_cache_stats(self) -> Optional[Dict[str, int]]:
 """Get scope cache statistics if enabled."""
 if self.enable_scope_cache and self._scope_cache:
 return self._scope_cache.get_stats()
 return None
 
 def print_scope_cache_stats(self) -> None:
 """Print scope cache statistics if enabled."""
 if self.enable_scope_cache and self._scope_cache:
 self._scope_cache.print_stats()
 else:
 print("Scope cache is not enabled")

def enable_scope_optimization(interpreter_instance) -> None:
 """
 Enable scope optimization on an existing interpreter instance.
 
 This patches the interpreter's methods to use cached lookups.
 Can be called at runtime to enable optimization.
 
 Args:
 interpreter_instance: An Interpreter instance
 
 Example:
 interpreter = Interpreter(runtime)
 enable_scope_optimization(interpreter) # Enable optimization
 """
 if hasattr(interpreter_instance, '_scope_cache'):
 return # Already enabled
 
 # Create cache
 interpreter_instance._scope_cache = ScopeCache(interpreter_instance.current_scope)
 
 # Save original methods
 interpreter_instance._original_get_variable = interpreter_instance.get_variable
 interpreter_instance._original_set_variable = interpreter_instance.set_variable
 interpreter_instance._original_enter_scope = interpreter_instance.enter_scope
 interpreter_instance._original_exit_scope = interpreter_instance.exit_scope
 
 # Replace with cached versions
 def cached_get_variable(name: str):
 value = interpreter_instance._scope_cache.get_variable(name)
 if value is not None or interpreter_instance._scope_cache.has_variable(name):
 return value
 return interpreter_instance._original_get_variable(name)
 
 def cached_set_variable(name: str, value):
 result = interpreter_instance._original_set_variable(name, value)
 interpreter_instance._scope_cache.set_variable(name, value)
 return result
 
 def cached_enter_scope():
 interpreter_instance._original_enter_scope()
 interpreter_instance._scope_cache.enter_scope()
 
 def cached_exit_scope():
 interpreter_instance._original_exit_scope()
 interpreter_instance._scope_cache.exit_scope()
 
 interpreter_instance.get_variable = cached_get_variable
 interpreter_instance.set_variable = cached_set_variable
 interpreter_instance.enter_scope = cached_enter_scope
 interpreter_instance.exit_scope = cached_exit_scope

def disable_scope_optimization(interpreter_instance) -> None:
 """
 Disable scope optimization on an interpreter instance.
 
 Restores original methods. Useful for debugging or if bugs are found.
 
 Args:
 interpreter_instance: An Interpreter instance with scope optimization enabled
 """
 if not hasattr(interpreter_instance, '_scope_cache'):
 return # Not enabled
 
 # Restore original methods
 interpreter_instance.get_variable = interpreter_instance._original_get_variable
 interpreter_instance.set_variable = interpreter_instance._original_set_variable
 interpreter_instance.enter_scope = interpreter_instance._original_enter_scope
 interpreter_instance.exit_scope = interpreter_instance._original_exit_scope
 
 # Clean up
 del interpreter_instance._scope_cache
 del interpreter_instance._original_get_variable
 del interpreter_instance._original_set_variable
 del interpreter_instance._original_enter_scope
 del interpreter_instance._original_exit_scope
