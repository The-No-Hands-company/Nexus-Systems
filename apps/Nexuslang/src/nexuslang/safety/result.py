"""
Result<T, E> Type Implementation
=================================

Rust-inspired Result type for error handling without exceptions.

Usage:
    result = parse_int("123")
    match result:
        case Ok(value): print(value)
        case Err(error): print(error)
"""

from typing import TypeVar, Generic, Union, Callable, Optional
from dataclasses import dataclass

T = TypeVar('T')
E = TypeVar('E')
U = TypeVar('U')


@dataclass
class Ok(Generic[T]):
    """Successful result containing a value."""
    value: T
    
    def is_ok(self) -> bool:
        return True
    
    def is_err(self) -> bool:
        return False
    
    def unwrap(self) -> T:
        """Get the value (safe for Ok)."""
        return self.value
    
    def unwrap_or(self, default: T) -> T:
        """Get the value or default."""
        return self.value
    
    def unwrap_or_else(self, f: Callable[[], T]) -> T:
        """Get the value or compute from function."""
        return self.value
    
    def map(self, f: Callable[[T], U]) -> 'Result[U, E]':
        """Transform the Ok value."""
        return Ok(f(self.value))
    
    def map_err(self, f: Callable[[E], U]) -> 'Result[T, U]':
        """Transform the Err value (no-op for Ok)."""
        return self
    
    def and_then(self, f: Callable[[T], 'Result[U, E]']) -> 'Result[U, E]':
        """Chain operations (flatMap)."""
        return f(self.value)
    
    def or_else(self, f: Callable[[E], 'Result[T, E]']) -> 'Result[T, E]':
        """Provide alternative (no-op for Ok)."""
        return self
    
    def __repr__(self) -> str:
        return f"Ok({self.value!r})"


@dataclass
class Err(Generic[E]):
    """Error result containing an error value."""
    error: E
    
    def is_ok(self) -> bool:
        return False
    
    def is_err(self) -> bool:
        return True
    
    def unwrap(self) -> T:
        """Get the value (panics for Err)."""
        raise ValueError(f"Called unwrap() on Err: {self.error}")
    
    def unwrap_or(self, default: T) -> T:
        """Get the value or default."""
        return default
    
    def unwrap_or_else(self, f: Callable[[], T]) -> T:
        """Get the value or compute from function."""
        return f()
    
    def map(self, f: Callable[[T], U]) -> 'Result[U, E]':
        """Transform the Ok value (no-op for Err)."""
        return self
    
    def map_err(self, f: Callable[[E], U]) -> 'Result[T, U]':
        """Transform the Err value."""
        return Err(f(self.error))
    
    def and_then(self, f: Callable[[T], 'Result[U, E]']) -> 'Result[U, E]':
        """Chain operations (no-op for Err)."""
        return self
    
    def or_else(self, f: Callable[[E], 'Result[T, E]']) -> 'Result[T, E]':
        """Provide alternative."""
        return f(self.error)
    
    def __repr__(self) -> str:
        return f"Err({self.error!r})"


# Type alias for Result
Result = Union[Ok[T], Err[E]]


# Helper functions
def wrap_result(func: Callable[..., T]) -> Callable[..., Result[T, Exception]]:
    """
    Decorator to convert exceptions into Result types.
    
    Usage:
        @wrap_result
        def parse_int(s: str) -> int:
            return int(s)
        
        result = parse_int("123")  # Returns Ok(123)
        result = parse_int("abc")  # Returns Err(ValueError(...))
    """
    def wrapper(*args, **kwargs) -> Result[T, Exception]:
        try:
            return Ok(func(*args, **kwargs))
        except Exception as e:
            return Err(e)
    return wrapper


def collect_results(results: list[Result[T, E]]) -> Result[list[T], E]:
    """
    Collect a list of Results into a Result of a list.
    If any result is Err, returns the first error.
    Otherwise returns Ok with list of all values.
    """
    values = []
    for result in results:
        if isinstance(result, Err):
            return result
        values.append(result.value)
    return Ok(values)


# Export main types
__all__ = ['Result', 'Ok', 'Err', 'wrap_result', 'collect_results']
