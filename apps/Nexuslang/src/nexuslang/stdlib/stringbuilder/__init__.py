"""
StringBuilder class for efficient string concatenation.

Provides a mutable buffer for building strings without creating
intermediate string objects on each concatenation.
"""

from ...runtime.runtime import Runtime


class StringBuilder:
    """Efficient string builder with dynamic buffer management."""
    
    def __init__(self, initial_capacity=16):
        """Initialize StringBuilder with optional initial capacity."""
        self._parts = []
        self._capacity = initial_capacity
    
    def append(self, text):
        """Append text to the builder."""
        self._parts.append(str(text))
        return self  # Allow chaining
    
    def append_line(self, text=""):
        """Append text followed by newline."""
        self._parts.append(str(text))
        self._parts.append('\n')
        return self
    
    def to_string(self):
        """Convert builder contents to string."""
        return ''.join(self._parts)
    
    def clear(self):
        """Clear the builder contents."""
        self._parts = []
        return self
    
    def length(self):
        """Get current length of built string."""
        return sum(len(str(part)) for part in self._parts)
    
    def is_empty(self):
        """Check if builder is empty."""
        return len(self._parts) == 0


def create_string_builder(initial_capacity=16):
    """Factory function to create a StringBuilder instance."""
    return StringBuilder(initial_capacity)


def register_stringbuilder_functions(runtime: Runtime) -> None:
    """Register StringBuilder functions with the runtime."""
    runtime.register_function("create_string_builder", create_string_builder)
    # Note: StringBuilder methods are called directly on the instance
