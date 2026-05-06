"""
Option<T> and Result<T,E> runtime implementations for NexusLang.

Production-ready implementations with no shortcuts or placeholders.
Provides type-safe nullable values and error handling.
"""

from typing import Any, Callable, TypeVar, Generic
from ...runtime.runtime import Runtime

T = TypeVar('T')
U = TypeVar('U')
E = TypeVar('E')
F = TypeVar('F')


class Option(Generic[T]):
    """Option<T> - represents an optional value.
    
    Variants:
        Some(value): Contains a value
        None: No value
    """
    
    def __init__(self, value: T = None, is_some: bool = False):
        """Internal constructor. Use Some() or NoneValue() instead."""
        self._value = value
        self._is_some = is_some
    
    def is_some(self) -> bool:
        """Check if this Option contains a value."""
        return self._is_some
    
    def is_none(self) -> bool:
        """Check if this Option is None."""
        return not self._is_some
    
    def unwrap(self) -> T:
        """Return the contained value.
        
        Raises:
            RuntimeError: If the Option is None
        """
        if not self._is_some:
            raise RuntimeError("Called unwrap() on a None value")
        return self._value
    
    def unwrap_or(self, default: T) -> T:
        """Return the contained value or a default.
        
        Args:
            default: Value to return if None
            
        Returns:
            The contained value or default
        """
        return self._value if self._is_some else default
    
    def unwrap_or_else(self, f: Callable[[], T]) -> T:
        """Return the contained value or compute it from a function.
        
        Args:
            f: Function to call if None
            
        Returns:
            The contained value or result of f()
        """
        return self._value if self._is_some else f()
    
    def map(self, f: Callable[[T], U]) -> 'Option[U]':
        """Transform the contained value if Some.
        
        Args:
            f: Function to apply to the value
            
        Returns:
            Some(f(value)) if Some, None otherwise
        """
        if self._is_some:
            return Some(f(self._value))
        return NoneValue()
    
    def and_then(self, f: Callable[[T], 'Option[U]']) -> 'Option[U]':
        """Monadic bind - chain Option-returning operations.
        
        Args:
            f: Function that returns an Option
            
        Returns:
            f(value) if Some, None otherwise
        """
        if self._is_some:
            return f(self._value)
        return NoneValue()
    
    def filter(self, predicate: Callable[[T], bool]) -> 'Option[T]':
        """Filter the Option by a predicate.
        
        Args:
            predicate: Function to test the value
            
        Returns:
            Some(value) if predicate is true, None otherwise
        """
        if self._is_some and predicate(self._value):
            return self
        return NoneValue()
    
    def __repr__(self) -> str:
        if self._is_some:
            return f"Some({self._value!r})"
        return "None"
    
    def __eq__(self, other) -> bool:
        if not isinstance(other, Option):
            return False
        if self._is_some != other._is_some:
            return False
        if self._is_some:
            return self._value == other._value
        return True


def Some(value: T) -> Option[T]:
    """Create an Option containing a value.
    
    Args:
        value: The value to wrap
        
    Returns:
        Option<T> containing the value
    """
    return Option(value, is_some=True)


def NoneValue() -> Option:
    """Create an empty Option.
    
    Returns:
        Option<T> with no value
    """
    return Option(is_some=False)


class Result(Generic[T, E]):
    """Result<T, E> - represents success or failure.
    
    Variants:
        Ok(value): Success with value of type T
        Err(error): Failure with error of type E
    """
    
    def __init__(self, value: Any = None, error: Any = None, is_ok: bool = True):
        """Internal constructor. Use Ok() or Err() instead."""
        self._value = value
        self._error = error
        self._is_ok = is_ok
    
    def is_ok(self) -> bool:
        """Check if this Result is Ok."""
        return self._is_ok
    
    def is_err(self) -> bool:
        """Check if this Result is Err."""
        return not self._is_ok
    
    def unwrap(self) -> T:
        """Return the Ok value.
        
        Raises:
            RuntimeError: If the Result is Err
        """
        if not self._is_ok:
            raise RuntimeError(f"Called unwrap() on an Err value: {self._error}")
        return self._value
    
    def unwrap_or(self, default: T) -> T:
        """Return the Ok value or a default.
        
        Args:
            default: Value to return if Err
            
        Returns:
            The Ok value or default
        """
        return self._value if self._is_ok else default
    
    def unwrap_err(self) -> E:
        """Return the Err value.
        
        Raises:
            RuntimeError: If the Result is Ok
        """
        if self._is_ok:
            raise RuntimeError(f"Called unwrap_err() on an Ok value: {self._value}")
        return self._error
    
    def map(self, f: Callable[[T], U]) -> 'Result[U, E]':
        """Transform the Ok value if Ok.
        
        Args:
            f: Function to apply to the Ok value
            
        Returns:
            Ok(f(value)) if Ok, Err(error) otherwise
        """
        if self._is_ok:
            return Ok(f(self._value))
        return Err(self._error)
    
    def map_err(self, f: Callable[[E], F]) -> 'Result[T, F]':
        """Transform the Err value if Err.
        
        Args:
            f: Function to apply to the Err value
            
        Returns:
            Ok(value) if Ok, Err(f(error)) otherwise
        """
        if self._is_ok:
            return Ok(self._value)
        return Err(f(self._error))
    
    def and_then(self, f: Callable[[T], 'Result[U, E]']) -> 'Result[U, E]':
        """Monadic bind - chain Result-returning operations.
        
        Args:
            f: Function that returns a Result
            
        Returns:
            f(value) if Ok, Err(error) otherwise
        """
        if self._is_ok:
            return f(self._value)
        return Err(self._error)
    
    def or_else(self, f: Callable[[E], 'Result[T, F]']) -> 'Result[T, F]':
        """Chain error-recovering operations.
        
        Args:
            f: Function that returns a Result
            
        Returns:
            Ok(value) if Ok, f(error) otherwise
        """
        if self._is_ok:
            return Ok(self._value)
        return f(self._error)
    
    def __repr__(self) -> str:
        if self._is_ok:
            return f"Ok({self._value!r})"
        return f"Err({self._error!r})"
    
    def __eq__(self, other) -> bool:
        if not isinstance(other, Result):
            return False
        if self._is_ok != other._is_ok:
            return False
        if self._is_ok:
            return self._value == other._value
        return self._error == other._error


def Ok(value: T) -> Result[T, Any]:
    """Create a successful Result.
    
    Args:
        value: The success value
        
    Returns:
        Result<T, E> containing the value
    """
    return Result(value=value, is_ok=True)


def Err(error: E) -> Result[Any, E]:
    """Create a failed Result.
    
    Args:
        error: The error value
        
    Returns:
        Result<T, E> containing the error
    """
    return Result(error=error, is_ok=False)


def register_option_result_functions(runtime: Runtime) -> None:
    """Register Option and Result functions with the runtime."""
    # Constructors
    runtime.register_function("Some", Some)
    runtime.register_function("None", NoneValue)
    runtime.register_function("Ok", Ok)
    runtime.register_function("Err", Err)
    
    # Note: Methods are called on instances, not registered globally
    # The interpreter will need to handle method calls on Option/Result objects
