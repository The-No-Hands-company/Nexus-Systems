"""
Collections for NexusLang: Vec<T>, HashMap<K,V>, Set<T>

Production-ready implementations - NO SHORTCUTS.
"""

from typing import TypeVar, Generic, Optional, Iterator as PyIterator, Set
from ..option_result import Option, Some, NoneValue

T = TypeVar('T')
K = TypeVar('K')
V = TypeVar('V')


class Vec(Generic[T]):
    """Vec<T> - dynamic array/vector.
    
    Production-ready implementation with:
    - Dynamic resizing
    - Iterator integration
    - Bounds checking
    - Full error handling
    """
    
    def __init__(self, initial_capacity: int = 10):
        """Create a new Vec with optional initial capacity."""
        if initial_capacity < 0:
            raise ValueError(f"Capacity must be non-negative, got {initial_capacity}")
        
        self._data = []
        self._capacity = initial_capacity
    
    def push(self, value: T) -> None:
        """Add element to end of vector."""
        self._data.append(value)
    
    def pop(self) -> Option[T]:
        """Remove and return last element.
        
        Returns:
            Some(value) if vector not empty
            None if vector is empty
        """
        if len(self._data) == 0:
            return NoneValue()
        return Some(self._data.pop())
    
    def get(self, index: int) -> Option[T]:
        """Get element at index.
        
        Returns:
            Some(value) if index valid
            None if index out of bounds
        """
        if 0 <= index < len(self._data):
            return Some(self._data[index])
        return NoneValue()
    
    def set(self, index: int, value: T) -> bool:
        """Set element at index.
        
        Returns:
            True if successful
            False if index out of bounds
        """
        if 0 <= index < len(self._data):
            self._data[index] = value
            return True
        return False
    
    def len(self) -> int:
        """Return number of elements."""
        return len(self._data)
    
    def capacity(self) -> int:
        """Return current capacity."""
        return self._capacity
    
    def is_empty(self) -> bool:
        """Check if vector is empty."""
        return len(self._data) == 0
    
    def clear(self) -> None:
        """Remove all elements."""
        self._data.clear()
    
    def __iter__(self) -> PyIterator[T]:
        """Python iterator protocol."""
        return iter(self._data)
    
    def __len__(self) -> int:
        return len(self._data)
    
    def __repr__(self) -> str:
        return f"Vec({self._data})"


class HashMap(Generic[K, V]):
    """HashMap<K,V> - hash map/dictionary.
    
    Production-ready implementation with:
    - O(1) average insert/get/remove
    - Iterator integration
    - Full error handling
    """
    
    def __init__(self):
        """Create a new HashMap."""
        self._data = {}
    
    def insert(self, key: K, value: V) -> Option[V]:
        """Insert key-value pair.
        
        Returns:
            Some(old_value) if key existed
            None if key was new
        """
        old_value = self._data.get(key)
        self._data[key] = value
        
        if old_value is not None:
            return Some(old_value)
        return NoneValue()
    
    def get(self, key: K) -> Option[V]:
        """Get value for key.
        
        Returns:
            Some(value) if key exists
            None if key not found
        """
        value = self._data.get(key)
        if value is not None:
            return Some(value)
        return NoneValue()
    
    def remove(self, key: K) -> Option[V]:
        """Remove key-value pair.
        
        Returns:
            Some(value) if key existed
            None if key not found
        """
        value = self._data.pop(key, None)
        if value is not None:
            return Some(value)
        return NoneValue()
    
    def contains_key(self, key: K) -> bool:
        """Check if key exists."""
        return key in self._data
    
    def len(self) -> int:
        """Return number of key-value pairs."""
        return len(self._data)
    
    def is_empty(self) -> bool:
        """Check if map is empty."""
        return len(self._data) == 0
    
    def clear(self) -> None:
        """Remove all key-value pairs."""
        self._data.clear()
    
    def keys(self):
        """Return iterator over keys."""
        return iter(self._data.keys())
    
    def values(self):
        """Return iterator over values."""
        return iter(self._data.values())
    
    def items(self):
        """Return iterator over (key, value) pairs."""
        return iter(self._data.items())
    
    def __len__(self) -> int:
        return len(self._data)
    
    def __repr__(self) -> str:
        return f"HashMap({self._data})"


class Set(Generic[T]):
    """Set<T> - unordered set.
    
    Production-ready implementation with:
    - O(1) average add/contains/remove
    - Set operations (union, intersection, difference)
    - Iterator integration
    """
    
    def __init__(self):
        """Create a new Set."""
        self._data = set()
    
    def add(self, value: T) -> bool:
        """Add element to set.
        
        Returns:
            True if element was new
            False if element already existed
        """
        if value in self._data:
            return False
        self._data.add(value)
        return True
    
    def remove(self, value: T) -> bool:
        """Remove element from set.
        
        Returns:
            True if element was removed
            False if element not found
        """
        if value in self._data:
            self._data.remove(value)
            return True
        return False
    
    def contains(self, value: T) -> bool:
        """Check if element exists in set."""
        return value in self._data
    
    def len(self) -> int:
        """Return number of elements."""
        return len(self._data)
    
    def is_empty(self) -> bool:
        """Check if set is empty."""
        return len(self._data) == 0
    
    def clear(self) -> None:
        """Remove all elements."""
        self._data.clear()
    
    def union(self, other: 'Set[T]') -> 'Set[T]':
        """Return union of two sets."""
        result = Set()
        result._data = self._data.union(other._data)
        return result
    
    def intersection(self, other: 'Set[T]') -> 'Set[T]':
        """Return intersection of two sets."""
        result = Set()
        result._data = self._data.intersection(other._data)
        return result
    
    def difference(self, other: 'Set[T]') -> 'Set[T]':
        """Return difference of two sets."""
        result = Set()
        result._data = self._data.difference(other._data)
        return result
    
    def __iter__(self) -> PyIterator[T]:
        """Python iterator protocol."""
        return iter(self._data)
    
    def __len__(self) -> int:
        return len(self._data)
    
    def __repr__(self) -> str:
        return f"Set({self._data})"




def list_append(target, value):
    """Append a value to a list or collection.
    
    Args:
        target: List, Vec, or any collection with an append/push method
        value: Value to append
        
    Returns:
        None (modifies target in-place)
    """
    if isinstance(target, list):
        target.append(value)
    elif isinstance(target, Vec):
        target.push(value)
    elif hasattr(target, 'append'):
        target.append(value)
    elif hasattr(target, 'push'):
        target.push(value)
    else:
        raise TypeError(f"Cannot append to type {type(target).__name__}")
    return None


def dict_set(target, key, value):
    """Set a key-value pair in a dictionary or map.
    
    Args:
        target: dict, HashMap, or any mapping type
        key: Key to set
        value: Value to associate with key
        
    Returns:
        None (modifies target in-place)
    """
    if isinstance(target, dict):
        target[key] = value
    elif isinstance(target, HashMap):
        target.insert(key, value)
    elif hasattr(target, '__setitem__'):
        target[key] = value
    else:
        raise TypeError(f"Cannot set key in type {type(target).__name__}")
    return None


def dict_get(target, key, default=None):
    """Get a value from a dictionary or map.
    
    Args:
        target: dict, HashMap, or any mapping type
        key: Key to retrieve
        default: Default value if key not found
        
    Returns:
        Value associated with key, or default if not found
    """
    if isinstance(target, dict):
        return target.get(key, default)
    elif isinstance(target, HashMap):
        result = target.get(key)
        if hasattr(result, 'is_some') and result.is_some():
            return result.unwrap()
        return default
    elif hasattr(target, 'get'):
        return target.get(key, default)
    elif hasattr(target, '__getitem__'):
        try:
            return target[key]
        except (KeyError, IndexError):
            return default
    else:
        raise TypeError(f"Cannot get key from type {type(target).__name__}")


def keys(target):
    """Return a list of all keys in a dictionary or map.

    Args:
        target: dict, HashMap, or any mapping type

    Returns:
        List of keys
    """
    if isinstance(target, dict):
        return list(target.keys())
    elif isinstance(target, HashMap):
        return list(target._data.keys())
    elif hasattr(target, 'keys'):
        return list(target.keys())
    else:
        raise TypeError(f"Cannot get keys from type {type(target).__name__}")


def values(target):
    """Return a list of all values in a dictionary or map.

    Args:
        target: dict, HashMap, or any mapping type

    Returns:
        List of values
    """
    if isinstance(target, dict):
        return list(target.values())
    elif isinstance(target, HashMap):
        return list(target._data.values())
    elif hasattr(target, 'values'):
        return list(target.values())
    else:
        raise TypeError(f"Cannot get values from type {type(target).__name__}")


def items(target):
    """Return a list of [key, value] pairs from a dictionary or map.

    Args:
        target: dict, HashMap, or any mapping type

    Returns:
        List of [key, value] lists
    """
    if isinstance(target, dict):
        return [[k, v] for k, v in target.items()]
    elif isinstance(target, HashMap):
        return [[k, v] for k, v in target._data.items()]
    elif hasattr(target, 'items'):
        return [[k, v] for k, v in target.items()]
    else:
        raise TypeError(f"Cannot get items from type {type(target).__name__}")


# ---------------------------------------------------------------------------
# BTreeMap — ordered dictionary (sorted by key)
# ---------------------------------------------------------------------------

class BTreeMap(Generic[K, V]):
    """
    Ordered key-value map, sorted by key in ascending order.

    Backed by a sorted list of (key, value) pairs for O(log n) lookups
    via bisect, and O(n) inserts (acceptable for typical use cases).
    """

    def __init__(self) -> None:
        import bisect as _bisect
        self._bisect = _bisect
        self._keys: list = []
        self._vals: list = []

    def insert(self, key, value) -> None:
        """Insert or update a key."""
        idx = self._bisect.bisect_left(self._keys, key)
        if idx < len(self._keys) and self._keys[idx] == key:
            self._vals[idx] = value
        else:
            self._keys.insert(idx, key)
            self._vals.insert(idx, value)

    def get(self, key, default=None):
        """Return value for key, or default if not present."""
        idx = self._bisect.bisect_left(self._keys, key)
        if idx < len(self._keys) and self._keys[idx] == key:
            return self._vals[idx]
        return default

    def remove(self, key) -> bool:
        """Remove key, return True if it was present."""
        idx = self._bisect.bisect_left(self._keys, key)
        if idx < len(self._keys) and self._keys[idx] == key:
            self._keys.pop(idx)
            self._vals.pop(idx)
            return True
        return False

    def contains_key(self, key) -> bool:
        idx = self._bisect.bisect_left(self._keys, key)
        return idx < len(self._keys) and self._keys[idx] == key

    def keys(self) -> list:
        return list(self._keys)

    def values(self) -> list:
        return list(self._vals)

    def items(self) -> list:
        return list(zip(self._keys, self._vals))

    def len(self) -> int:
        return len(self._keys)

    def is_empty(self) -> bool:
        return len(self._keys) == 0

    def min_key(self):
        return self._keys[0] if self._keys else None

    def max_key(self):
        return self._keys[-1] if self._keys else None

    def __len__(self) -> int:
        return len(self._keys)

    def __repr__(self) -> str:
        pairs = ", ".join(f"{k}: {v}" for k, v in zip(self._keys, self._vals))
        return f"BTreeMap{{{pairs}}}"


# ---------------------------------------------------------------------------
# BTreeSet — ordered set
# ---------------------------------------------------------------------------

class BTreeSet(Generic[T]):
    """Ordered set, sorted in ascending order. O(log n) lookup via bisect."""

    def __init__(self) -> None:
        import bisect as _bisect
        self._bisect = _bisect
        self._data: list = []

    def insert(self, value) -> bool:
        """Insert value. Returns True if newly inserted."""
        idx = self._bisect.bisect_left(self._data, value)
        if idx < len(self._data) and self._data[idx] == value:
            return False
        self._data.insert(idx, value)
        return True

    def remove(self, value) -> bool:
        """Remove value. Returns True if it was present."""
        idx = self._bisect.bisect_left(self._data, value)
        if idx < len(self._data) and self._data[idx] == value:
            self._data.pop(idx)
            return True
        return False

    def contains(self, value) -> bool:
        idx = self._bisect.bisect_left(self._data, value)
        return idx < len(self._data) and self._data[idx] == value

    def to_list(self) -> list:
        return list(self._data)

    def len(self) -> int:
        return len(self._data)

    def is_empty(self) -> bool:
        return len(self._data) == 0

    def min(self):
        return self._data[0] if self._data else None

    def max(self):
        return self._data[-1] if self._data else None

    def union(self, other: "BTreeSet") -> "BTreeSet":
        result: BTreeSet = BTreeSet()
        result._data = sorted(set(self._data) | set(other._data))
        return result

    def intersection(self, other: "BTreeSet") -> "BTreeSet":
        result: BTreeSet = BTreeSet()
        result._data = sorted(set(self._data) & set(other._data))
        return result

    def difference(self, other: "BTreeSet") -> "BTreeSet":
        result: BTreeSet = BTreeSet()
        result._data = sorted(set(self._data) - set(other._data))
        return result

    def __iter__(self):
        return iter(self._data)

    def __len__(self) -> int:
        return len(self._data)

    def __repr__(self) -> str:
        return f"BTreeSet{{{', '.join(repr(v) for v in self._data)}}}"


# ---------------------------------------------------------------------------
# LinkedList — doubly-linked list
# ---------------------------------------------------------------------------

class _LLNode:
    __slots__ = ("value", "prev", "next")

    def __init__(self, value) -> None:
        self.value = value
        self.prev: "_LLNode | None" = None
        self.next: "_LLNode | None" = None


class LinkedList(Generic[T]):
    """
    Doubly-linked list with O(1) push_front, push_back, pop_front, pop_back.
    """

    def __init__(self) -> None:
        self._head: "_LLNode | None" = None
        self._tail: "_LLNode | None" = None
        self._size: int = 0

    def push_back(self, value) -> None:
        node = _LLNode(value)
        if self._tail is None:
            self._head = self._tail = node
        else:
            node.prev = self._tail
            self._tail.next = node
            self._tail = node
        self._size += 1

    def push_front(self, value) -> None:
        node = _LLNode(value)
        if self._head is None:
            self._head = self._tail = node
        else:
            node.next = self._head
            self._head.prev = node
            self._head = node
        self._size += 1

    def pop_front(self):
        if self._head is None:
            raise IndexError("pop_front on empty LinkedList")
        val = self._head.value
        self._head = self._head.next
        if self._head:
            self._head.prev = None
        else:
            self._tail = None
        self._size -= 1
        return val

    def pop_back(self):
        if self._tail is None:
            raise IndexError("pop_back on empty LinkedList")
        val = self._tail.value
        self._tail = self._tail.prev
        if self._tail:
            self._tail.next = None
        else:
            self._head = None
        self._size -= 1
        return val

    def front(self):
        if self._head is None:
            return None
        return self._head.value

    def back(self):
        if self._tail is None:
            return None
        return self._tail.value

    def is_empty(self) -> bool:
        return self._size == 0

    def len(self) -> int:
        return self._size

    def to_list(self) -> list:
        result = []
        node = self._head
        while node is not None:
            result.append(node.value)
            node = node.next
        return result

    def __iter__(self):
        node = self._head
        while node is not None:
            yield node.value
            node = node.next

    def __len__(self) -> int:
        return self._size

    def __repr__(self) -> str:
        return f"LinkedList([{', '.join(repr(v) for v in self)}])"


# ---------------------------------------------------------------------------
# VecDeque — double-ended queue (O(1) amortised push/pop both ends)
# ---------------------------------------------------------------------------

from collections import deque as _deque


class VecDeque(Generic[T]):
    """Double-ended queue backed by collections.deque."""

    def __init__(self) -> None:
        self._data: _deque = _deque()

    def push_back(self, value) -> None:
        self._data.append(value)

    def push_front(self, value) -> None:
        self._data.appendleft(value)

    def pop_back(self):
        if not self._data:
            raise IndexError("pop_back on empty VecDeque")
        return self._data.pop()

    def pop_front(self):
        if not self._data:
            raise IndexError("pop_front on empty VecDeque")
        return self._data.popleft()

    def front(self):
        return self._data[0] if self._data else None

    def back(self):
        return self._data[-1] if self._data else None

    def get(self, index: int):
        return self._data[index]

    def len(self) -> int:
        return len(self._data)

    def is_empty(self) -> bool:
        return len(self._data) == 0

    def to_list(self) -> list:
        return list(self._data)

    def __iter__(self):
        return iter(self._data)

    def __len__(self) -> int:
        return len(self._data)

    def __repr__(self) -> str:
        return f"VecDeque([{', '.join(repr(v) for v in self._data)}])"


# ---------------------------------------------------------------------------
# MinHeap / MaxHeap — priority queues
# ---------------------------------------------------------------------------

import heapq as _heapq


class MinHeap(Generic[T]):
    """Min-heap priority queue. Smallest element is always at the top."""

    def __init__(self) -> None:
        self._data: list = []

    def push(self, value) -> None:
        _heapq.heappush(self._data, value)

    def pop(self):
        if not self._data:
            raise IndexError("pop on empty MinHeap")
        return _heapq.heappop(self._data)

    def peek(self):
        if not self._data:
            return None
        return self._data[0]

    def len(self) -> int:
        return len(self._data)

    def is_empty(self) -> bool:
        return len(self._data) == 0

    def to_sorted_list(self) -> list:
        return sorted(self._data)

    def __len__(self) -> int:
        return len(self._data)

    def __repr__(self) -> str:
        return f"MinHeap(len={len(self._data)}, top={self.peek()!r})"


class MaxHeap(Generic[T]):
    """Max-heap priority queue. Largest element is always at the top."""

    def __init__(self) -> None:
        self._data: list = []  # Stores negated values for max semantics

    def push(self, value) -> None:
        _heapq.heappush(self._data, -value)

    def pop(self):
        if not self._data:
            raise IndexError("pop on empty MaxHeap")
        return -_heapq.heappop(self._data)

    def peek(self):
        if not self._data:
            return None
        return -self._data[0]

    def len(self) -> int:
        return len(self._data)

    def is_empty(self) -> bool:
        return len(self._data) == 0

    def to_sorted_list(self) -> list:
        return sorted((-x for x in self._data), reverse=True)

    def __len__(self) -> int:
        return len(self._data)

    def __repr__(self) -> str:
        return f"MaxHeap(len={len(self._data)}, top={self.peek()!r})"


# ---------------------------------------------------------------------------
# CustomHashMap — HashMap with a user-supplied hash function
# ---------------------------------------------------------------------------

class CustomHashMap:
    """HashMap with a user-supplied hash function.

    Allows custom key hashing strategies:
    - Case-insensitive string keys
    - Composite / structural keys
    - Domain-specific hash functions

    Internally uses a ``{hash_value: [(key, value), ...]}`` bucket dict so
    that hash collisions are resolved by linear scan using the optional
    equality function.

    Parameters
    ----------
    hash_fn : callable, optional
        ``hash_fn(key) -> hashable``.  Defaults to Python's built-in
        ``hash()``.
    eq_fn : callable, optional
        ``eq_fn(key_a, key_b) -> bool``.  Defaults to ``key_a == key_b``.
    """

    def __init__(self, hash_fn=None, eq_fn=None):
        self._hash_fn = hash_fn if hash_fn is not None else hash
        self._eq_fn = eq_fn if eq_fn is not None else (lambda a, b: a == b)
        self._buckets: dict = {}  # hash_value -> list of (key, value) pairs
        self._size: int = 0

    # ------------------------------------------------------------------
    # Mutating operations
    # ------------------------------------------------------------------

    def set(self, key, value) -> bool:
        """Insert or update a key-value pair.

        Returns True when a new key was inserted, False on update.
        """
        h = self._hash_fn(key)
        if h not in self._buckets:
            self._buckets[h] = []
        bucket = self._buckets[h]
        for i, (k, _v) in enumerate(bucket):
            if self._eq_fn(k, key):
                bucket[i] = (key, value)  # update in-place
                return False
        bucket.append((key, value))
        self._size += 1
        return True

    def remove(self, key) -> bool:
        """Remove a key.  Returns True on success, False if not found."""
        h = self._hash_fn(key)
        if h not in self._buckets:
            return False
        bucket = self._buckets[h]
        for i, (k, _v) in enumerate(bucket):
            if self._eq_fn(k, key):
                bucket.pop(i)
                self._size -= 1
                if not bucket:
                    del self._buckets[h]
                return True
        return False

    def clear(self) -> None:
        """Remove all entries."""
        self._buckets.clear()
        self._size = 0

    # ------------------------------------------------------------------
    # Query operations
    # ------------------------------------------------------------------

    def get(self, key, default=None):
        """Return the value for *key*, or *default* if not present."""
        h = self._hash_fn(key)
        if h not in self._buckets:
            return default
        for k, v in self._buckets[h]:
            if self._eq_fn(k, key):
                return v
        return default

    def has(self, key) -> bool:
        """Return True when *key* is present."""
        h = self._hash_fn(key)
        if h not in self._buckets:
            return False
        return any(self._eq_fn(k, key) for k, _v in self._buckets[h])

    def size(self) -> int:
        """Return the number of key-value pairs."""
        return self._size

    def keys(self) -> list:
        """Return a list of all keys."""
        return [k for bucket in self._buckets.values() for k, _v in bucket]

    def values(self) -> list:
        """Return a list of all values."""
        return [v for bucket in self._buckets.values() for _k, v in bucket]

    def items(self) -> list:
        """Return a list of (key, value) pairs."""
        return [(k, v) for bucket in self._buckets.values() for k, v in bucket]

    # ------------------------------------------------------------------
    # Python protocols
    # ------------------------------------------------------------------

    def __len__(self) -> int:
        return self._size

    def __repr__(self) -> str:
        pairs = ", ".join(f"{k!r}: {v!r}" for k, v in self.items())
        return f"CustomHashMap{{{pairs}}}"


# ---------------------------------------------------------------------------
# Standalone CustomHashMap helper functions (callable from NLPL)
# ---------------------------------------------------------------------------

def custom_hash_map_create(hash_fn=None, eq_fn=None) -> CustomHashMap:
    """Create and return a new CustomHashMap.

    Parameters
    ----------
    hash_fn : callable, optional
        User-supplied hash function ``hash_fn(key) -> hashable``.
    eq_fn : callable, optional
        User-supplied equality function ``eq_fn(key_a, key_b) -> bool``.
    """
    return CustomHashMap(hash_fn=hash_fn, eq_fn=eq_fn)


def custom_hash_map_set(m: CustomHashMap, key, value) -> bool:
    """Insert/update a key-value pair.  Returns True when key was new."""
    if not isinstance(m, CustomHashMap):
        return False
    return m.set(key, value)


def custom_hash_map_get(m: CustomHashMap, key, default=None):
    """Return the value for key, or default if not found."""
    if not isinstance(m, CustomHashMap):
        return default
    return m.get(key, default)


def custom_hash_map_has(m: CustomHashMap, key) -> bool:
    """Return True when key is present in the map."""
    if not isinstance(m, CustomHashMap):
        return False
    return m.has(key)


def custom_hash_map_remove(m: CustomHashMap, key) -> bool:
    """Remove key.  Returns True on success."""
    if not isinstance(m, CustomHashMap):
        return False
    return m.remove(key)


def custom_hash_map_keys(m: CustomHashMap) -> list:
    """Return a list of all keys."""
    if not isinstance(m, CustomHashMap):
        return []
    return m.keys()


def custom_hash_map_values(m: CustomHashMap) -> list:
    """Return a list of all values."""
    if not isinstance(m, CustomHashMap):
        return []
    return m.values()


def custom_hash_map_items(m: CustomHashMap) -> list:
    """Return a list of (key, value) tuples."""
    if not isinstance(m, CustomHashMap):
        return []
    return m.items()


def custom_hash_map_size(m: CustomHashMap) -> int:
    """Return the number of entries."""
    if not isinstance(m, CustomHashMap):
        return 0
    return m.size()


def custom_hash_map_clear(m: CustomHashMap) -> CustomHashMap:
    """Remove all entries and return the (empty) map."""
    if isinstance(m, CustomHashMap):
        m.clear()
    return m


def register_collections_functions(runtime):
    """Register collection constructors with runtime."""
    runtime.register_function("Vec", Vec)
    runtime.register_function("HashMap", HashMap)
    runtime.register_function("Set", Set)
    runtime.register_function("BTreeMap", BTreeMap)
    runtime.register_function("BTreeSet", BTreeSet)
    runtime.register_function("LinkedList", LinkedList)
    runtime.register_function("VecDeque", VecDeque)
    runtime.register_function("MinHeap", MinHeap)
    runtime.register_function("MaxHeap", MaxHeap)
    runtime.register_function("list_append", list_append)
    runtime.register_function("append", list_append)  # Alias
    runtime.register_function("dict_set", dict_set)
    runtime.register_function("dict_get", dict_get)
    runtime.register_function("keys", keys)
    runtime.register_function("dict_keys", keys)      # Alias
    runtime.register_function("values", values)
    runtime.register_function("dict_values", values)   # Alias
    runtime.register_function("items", items)
    runtime.register_function("dict_items", items)     # Alias

    # CustomHashMap — HashMap with user-supplied hash function
    runtime.register_function("CustomHashMap", CustomHashMap)
    runtime.register_function("custom_hash_map_create", custom_hash_map_create)
    runtime.register_function("custom_hash_map_set", custom_hash_map_set)
    runtime.register_function("custom_hash_map_get", custom_hash_map_get)
    runtime.register_function("custom_hash_map_has", custom_hash_map_has)
    runtime.register_function("custom_hash_map_remove", custom_hash_map_remove)
    runtime.register_function("custom_hash_map_keys", custom_hash_map_keys)
    runtime.register_function("custom_hash_map_values", custom_hash_map_values)
    runtime.register_function("custom_hash_map_items", custom_hash_map_items)
    runtime.register_function("custom_hash_map_size", custom_hash_map_size)
    runtime.register_function("custom_hash_map_clear", custom_hash_map_clear)
