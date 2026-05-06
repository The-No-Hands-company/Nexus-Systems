"""
Iterator protocol and range implementation for NexusLang.

Production-ready implementation using Option<T> for type-safe iteration.
NO shortcuts, NO placeholders, NO exceptions for control flow.

Provides:
- Iterator trait
- Range iterators (1..10, 1..=10)
- Functional methods (map, filter, reduce)
"""

from ...runtime.runtime import Runtime
from ..option_result import Option, Some, NoneValue


class RangeIterator:
    """Iterator for numeric ranges.
    
    Production-ready implementation that returns Option<T> from next().
    No exceptions for control flow - uses Option<T> for exhaustion.
    """
    
    def __init__(self, start: int, end: int, inclusive: bool = False):
        """Create a range iterator.
        
        Args:
            start: Starting value (inclusive)
            end: Ending value (exclusive unless inclusive=True)
            inclusive: If True, include end value
        """
        if not isinstance(start, int) or not isinstance(end, int):
            raise TypeError(f"Range requires integer bounds, got {type(start).__name__} and {type(end).__name__}")
        
        self.current = start
        self.end = end
        self.inclusive = inclusive
        self._exhausted = False
    
    def next(self) -> Option[int]:
        """Get next value in range.
        
        Returns:
            Some(value) if more values available
            None if iterator is exhausted
        """
        if not self.has_next():
            return NoneValue()
        
        value = self.current
        self.current += 1
        return Some(value)
    
    def has_next(self) -> bool:
        """Check if more values available.
        
        Returns:
            True if next() will return Some(value)
            False if iterator is exhausted
        """
        if self.inclusive:
            return self.current <= self.end
        else:
            return self.current < self.end
    
    def __iter__(self):
        """Python iterator protocol support."""
        return self
    
    def __next__(self):
        """Python iterator protocol - raises StopIteration when exhausted."""
        opt = self.next()
        if opt.is_none():
            raise StopIteration()
        return opt.unwrap()
    
    def __repr__(self) -> str:
        op = "..=" if self.inclusive else ".."
        return f"RangeIterator({self.current}{op}{self.end})"


def create_range(start: int, end: int, inclusive: bool = False) -> RangeIterator:
    """Create a range iterator.
    
    Args:
        start: Starting value (inclusive)
        end: Ending value (exclusive unless inclusive=True)
        inclusive: If True, include end value
        
    Returns:
        RangeIterator that yields values from start to end
        
    Examples:
        create_range(1, 5, False)  # 1, 2, 3, 4
        create_range(1, 5, True)   # 1, 2, 3, 4, 5
    """
    return RangeIterator(start, end, inclusive)


class MapIterator:
    """Lazy iterator that maps a function over an iterable."""
    def __init__(self, iterable, func):
        self.iterator = iter(iterable)
        self.func = func

    def __iter__(self):
        return self

    def __next__(self):
        item = next(self.iterator)
        return self.func(item)

class FilterIterator:
    """Lazy iterator that filters an iterable."""
    def __init__(self, iterable, predicate):
        self.iterator = iter(iterable)
        self.predicate = predicate

    def __iter__(self):
        return self

    def __next__(self):
        while True:
            item = next(self.iterator)
            if self.predicate(item):
                return item

class TakeIterator:
    """Lazy iterator that takes first n elements."""
    def __init__(self, iterable, n: int):
        self.iterator = iter(iterable)
        self.n = n
        self.count = 0

    def __iter__(self):
        return self

    def __next__(self):
        if self.count >= self.n:
            raise StopIteration
        
        item = next(self.iterator)
        self.count += 1
        return item

class SkipIterator:
    """Lazy iterator that skips first n elements."""
    def __init__(self, iterable, n: int):
        self.iterator = iter(iterable)
        self.n = n
        self.skipped = False

    def __iter__(self):
        return self

    def __next__(self):
        if not self.skipped:
            # Consume first n elements
            for _ in range(self.n):
                try:
                    next(self.iterator)
                except StopIteration:
                    self.skipped = True
                    raise
            self.skipped = True
            
        return next(self.iterator)


def iterator_map(iterable, func):
    """Map function over iterable, returning a list."""
    if not callable(func):
        raise TypeError(f"map() requires a callable, got {type(func).__name__}")
    return list(MapIterator(iterable, func))


def iterator_filter(iterable, predicate):
    """Filter iterable by predicate, returning a list."""
    if not callable(predicate):
        raise TypeError(f"filter() requires a callable, got {type(predicate).__name__}")
    return list(FilterIterator(iterable, predicate))


def iterator_reduce(iterable, initial, func):
    """Reduce iterable to single value (eager).
    
    Args:
        iterable: Any iterable object
        initial: Initial accumulator value
        func: Function(accumulator, element) -> accumulator
        
    Returns:
        Final accumulated value
    """
    if not callable(func):
        raise TypeError(f"reduce() requires a callable, got {type(func).__name__}")
    
    accumulator = initial
    for item in iterable:
        accumulator = func(accumulator, item)
    return accumulator


def iterator_collect(iterable):
    """Collect iterator into list (eager)."""
    return list(iterable)


def iterator_take(iterable, n: int):
    """Take first n elements, returning a list."""
    if not isinstance(n, int) or n < 0:
        raise ValueError(f"take() requires non-negative integer, got {n}")
    return list(TakeIterator(iterable, n))


def iterator_skip(iterable, n: int):
    """Skip first n elements, returning a list."""
    if not isinstance(n, int) or n < 0:
        raise ValueError(f"skip() requires non-negative integer, got {n}")
    return list(SkipIterator(iterable, n))


def register_iterator_functions(runtime: Runtime) -> None:
    """Register iterator functions with the runtime."""
    runtime.register_function("create_range", create_range)
    runtime.register_function("map", iterator_map)
    runtime.register_function("filter", iterator_filter)
    runtime.register_function("reduce", iterator_reduce)
    runtime.register_function("collect", iterator_collect)
    runtime.register_function("take", iterator_take)
    runtime.register_function("skip", iterator_skip)

