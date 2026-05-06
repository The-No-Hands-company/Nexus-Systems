"""
Optional<T> type for NexusLang standard library.

Represents a value that may or may not be present.
Provides type-safe null handling with Some/None variants.
"""

from typing import Any, Callable, Optional as PyOptional


class OptionalValue:
    """Base class for Optional variants."""
    
    def __init__(self, value: PyOptional[Any] = None, is_some: bool = False):
        self._value = value
        self._is_some = is_some
    
    def is_some(self) -> bool:
        """Check if this is a Some value."""
        return self._is_some
    
    def is_none(self) -> bool:
        """Check if this is None."""
        return not self._is_some
    
    def get(self) -> Any:
        """Get the value, raises error if None."""
        if not self._is_some:
            raise ValueError("Cannot get value from None")
        return self._value
    
    def get_or_else(self, default: Any) -> Any:
        """Get the value or return default if None."""
        return self._value if self._is_some else default
    
    def map(self, fn: Callable) -> 'OptionalValue':
        """Transform the value if Some, otherwise return None."""
        if self._is_some:
            try:
                new_value = fn(self._value)
                return Some(new_value)
            except Exception:
                return NoneValue()
        return self
    
    def flat_map(self, fn: Callable) -> 'OptionalValue':
        """Transform the value to another Optional, flatten the result."""
        if self._is_some:
            try:
                result = fn(self._value)
                if isinstance(result, OptionalValue):
                    return result
                return Some(result)
            except Exception:
                return NoneValue()
        return self
    
    def filter(self, predicate: Callable) -> 'OptionalValue':
        """Keep the value if predicate returns true, otherwise None."""
        if self._is_some:
            try:
                if predicate(self._value):
                    return self
            except Exception:
                pass
        return NoneValue()
    
    def or_else(self, alternative: Callable) -> 'OptionalValue':
        """Return this if Some, otherwise call alternative."""
        if self._is_some:
            return self
        try:
            result = alternative()
            if isinstance(result, OptionalValue):
                return result
            return Some(result)
        except Exception:
            return NoneValue()
    
    def __repr__(self):
        if self._is_some:
            return f"Some({self._value!r})"
        return "None"
    
    def __str__(self):
        return self.__repr__()
    
    def __eq__(self, other):
        if not isinstance(other, OptionalValue):
            return False
        if self._is_some != other._is_some:
            return False
        if not self._is_some:  # Both are None
            return True
        return self._value == other._value
    
    def __bool__(self):
        """Optional is truthy if it's Some."""
        return self._is_some


class Some(OptionalValue):
    """Represents a present value."""
    
    def __init__(self, value: Any):
        super().__init__(value, is_some=True)


class NoneValue(OptionalValue):
    """Represents an absent value."""
    
    def __init__(self):
        super().__init__(None, is_some=False)


# Singleton None instance
_NONE_INSTANCE = None


def None_():
    """Get the singleton None instance."""
    global _NONE_INSTANCE
    if _NONE_INSTANCE is None:
        _NONE_INSTANCE = NoneValue()
    return _NONE_INSTANCE


def Optional(value: Any) -> OptionalValue:
    """Create an Optional from a value (None becomes NoneValue)."""
    if value is None:
        return None_()
    return Some(value)


def register_optional_functions(runtime):
    """Register Optional type functions with the runtime."""
    
    def create_some(value):
        """Create a Some value: Some(42)"""
        return Some(value)
    
    def create_none():
        """Create a None value: None()"""
        return None_()
    
    def is_some(optional):
        """Check if Optional is Some."""
        if isinstance(optional, OptionalValue):
            return optional.is_some()
        return False
    
    def is_none(optional):
        """Check if Optional is None."""
        if isinstance(optional, OptionalValue):
            return optional.is_none()
        return True
    
    def get_value(optional):
        """Get value from Optional, raises error if None."""
        if isinstance(optional, OptionalValue):
            return optional.get()
        raise ValueError("Not an Optional value")
    
    def get_or_else(optional, default):
        """Get value or return default."""
        if isinstance(optional, OptionalValue):
            return optional.get_or_else(default)
        return default
    
    def map_optional(optional, fn):
        """Map over Optional value."""
        if isinstance(optional, OptionalValue):
            return optional.map(fn)
        return None_()
    
    def flat_map_optional(optional, fn):
        """FlatMap over Optional value."""
        if isinstance(optional, OptionalValue):
            return optional.flat_map(fn)
        return None_()
    
    def filter_optional(optional, predicate):
        """Filter Optional value."""
        if isinstance(optional, OptionalValue):
            return optional.filter(predicate)
        return None_()
    
    # Register functions
    runtime.register_function("Some", create_some)
    runtime.register_function("None", create_none)
    runtime.register_function("Optional", Optional)
    runtime.register_function("is_some", is_some)
    runtime.register_function("is_none", is_none)
    runtime.register_function("optional_get", get_value)
    runtime.register_function("optional_get_or_else", get_or_else)
    runtime.register_function("optional_map", map_optional)
    runtime.register_function("optional_flat_map", flat_map_optional)
    runtime.register_function("optional_filter", filter_optional)
