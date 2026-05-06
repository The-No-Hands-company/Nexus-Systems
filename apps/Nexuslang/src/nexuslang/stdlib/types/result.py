"""
Result<T, E> type for NexusLang standard library.

Represents either success (Ok) or failure (Err).
Provides railway-oriented programming for error handling.
"""

from typing import Any, Callable, Union


class ResultValue:
    """Base class for Result variants."""
    
    def __init__(self, value: Any, is_ok: bool):
        self._value = value
        self._is_ok = is_ok
    
    def is_ok(self) -> bool:
        """Check if this is an Ok value."""
        return self._is_ok
    
    def is_err(self) -> bool:
        """Check if this is an Err value."""
        return not self._is_ok
    
    def unwrap(self) -> Any:
        """Get the Ok value, raises error if Err."""
        if not self._is_ok:
            raise ValueError(f"Called unwrap on Err value: {self._value}")
        return self._value
    
    def unwrap_or(self, default: Any) -> Any:
        """Get the Ok value or return default if Err."""
        return self._value if self._is_ok else default
    
    def unwrap_err(self) -> Any:
        """Get the Err value, raises error if Ok."""
        if self._is_ok:
            raise ValueError(f"Called unwrap_err on Ok value: {self._value}")
        return self._value
    
    def map(self, fn: Callable) -> 'ResultValue':
        """Transform the Ok value, leave Err unchanged."""
        if self._is_ok:
            try:
                new_value = fn(self._value)
                return Ok(new_value)
            except Exception as e:
                return Err(str(e))
        return self
    
    def map_err(self, fn: Callable) -> 'ResultValue':
        """Transform the Err value, leave Ok unchanged."""
        if not self._is_ok:
            try:
                new_err = fn(self._value)
                return Err(new_err)
            except Exception as e:
                return Err(str(e))
        return self
    
    def flat_map(self, fn: Callable) -> 'ResultValue':
        """Transform Ok value to another Result, flatten."""
        if self._is_ok:
            try:
                result = fn(self._value)
                if isinstance(result, ResultValue):
                    return result
                return Ok(result)
            except Exception as e:
                return Err(str(e))
        return self
    
    def and_then(self, fn: Callable) -> 'ResultValue':
        """Alias for flat_map."""
        return self.flat_map(fn)
    
    def or_else(self, fn: Callable) -> 'ResultValue':
        """If Err, call fn with error, otherwise return Ok."""
        if not self._is_ok:
            try:
                result = fn(self._value)
                if isinstance(result, ResultValue):
                    return result
                return Ok(result)
            except Exception as e:
                return Err(str(e))
        return self
    
    def match(self, ok_fn: Callable, err_fn: Callable) -> Any:
        """Pattern match on Result."""
        if self._is_ok:
            return ok_fn(self._value)
        else:
            return err_fn(self._value)
    
    def __repr__(self):
        if self._is_ok:
            return f"Ok({self._value!r})"
        return f"Err({self._value!r})"
    
    def __str__(self):
        return self.__repr__()
    
    def __eq__(self, other):
        if not isinstance(other, ResultValue):
            return False
        if self._is_ok != other._is_ok:
            return False
        return self._value == other._value
    
    def __bool__(self):
        """Result is truthy if it's Ok."""
        return self._is_ok


class Ok(ResultValue):
    """Represents a successful result."""
    
    def __init__(self, value: Any):
        super().__init__(value, is_ok=True)


class Err(ResultValue):
    """Represents an error result."""
    
    def __init__(self, error: Any):
        super().__init__(error, is_ok=False)


def Result(value: Any, is_ok: bool = True) -> ResultValue:
    """Create a Result from a value."""
    if is_ok:
        return Ok(value)
    return Err(value)


def try_catch(fn: Callable, *args, **kwargs) -> ResultValue:
    """Execute function and wrap result in Result."""
    try:
        result = fn(*args, **kwargs)
        return Ok(result)
    except Exception as e:
        return Err(str(e))


def register_result_functions(runtime):
    """Register Result type functions with the runtime."""
    
    def create_ok(value):
        """Create an Ok value: Ok(42)"""
        return Ok(value)
    
    def create_err(error):
        """Create an Err value: Err("error message")"""
        return Err(error)
    
    def is_ok(result):
        """Check if Result is Ok."""
        if isinstance(result, ResultValue):
            return result.is_ok()
        return False
    
    def is_err(result):
        """Check if Result is Err."""
        if isinstance(result, ResultValue):
            return result.is_err()
        return True
    
    def unwrap_result(result):
        """Unwrap Ok value, raises error if Err."""
        if isinstance(result, ResultValue):
            return result.unwrap()
        raise ValueError("Not a Result value")
    
    def unwrap_or(result, default):
        """Unwrap or return default."""
        if isinstance(result, ResultValue):
            return result.unwrap_or(default)
        return default
    
    def unwrap_err(result):
        """Unwrap Err value."""
        if isinstance(result, ResultValue):
            return result.unwrap_err()
        raise ValueError("Not a Result value")
    
    def map_result(result, fn):
        """Map over Ok value."""
        if isinstance(result, ResultValue):
            return result.map(fn)
        return Err("Not a Result value")
    
    def map_err_result(result, fn):
        """Map over Err value."""
        if isinstance(result, ResultValue):
            return result.map_err(fn)
        return result
    
    def flat_map_result(result, fn):
        """FlatMap over Result."""
        if isinstance(result, ResultValue):
            return result.flat_map(fn)
        return Err("Not a Result value")
    
    def match_result(result, ok_fn, err_fn):
        """Pattern match on Result."""
        if isinstance(result, ResultValue):
            return result.match(ok_fn, err_fn)
        return err_fn("Not a Result value")
    
    # Register functions
    runtime.register_function("Ok", create_ok)
    runtime.register_function("Err", create_err)
    runtime.register_function("Result", Result)
    runtime.register_function("try_catch", try_catch)
    runtime.register_function("is_ok", is_ok)
    runtime.register_function("is_err", is_err)
    runtime.register_function("result_unwrap", unwrap_result)
    runtime.register_function("result_unwrap_or", unwrap_or)
    runtime.register_function("result_unwrap_err", unwrap_err)
    runtime.register_function("result_map", map_result)
    runtime.register_function("result_map_err", map_err_result)
    runtime.register_function("result_flat_map", flat_map_result)
    runtime.register_function("result_match", match_result)
