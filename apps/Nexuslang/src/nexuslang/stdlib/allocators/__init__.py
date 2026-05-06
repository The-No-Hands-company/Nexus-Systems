"""
NLPL Allocators Module - Custom Memory Allocator Control

Provides allocator types and allocation control for fine-grained memory management:
- SystemAllocator: thin wrapper around the default system heap (malloc/free)
- ArenaAllocator:  bump/region allocator - O(1) alloc, whole-arena free
- PoolAllocator:   fixed-size block recycler - O(1) alloc and free
- SlabAllocator:   slab-cache allocator (kernel-style, one slab per object type)
- Global allocator override and per-type allocator assignment
- Statistics tracking (bytes allocated, peak, in-use count)

These primitives are essential for controlling memory layout and avoiding
per-allocation overhead in performance-critical or resource-constrained code.
"""

import ctypes
import threading
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any, Dict, Optional


# ---------------------------------------------------------------------------
# Allocation statistics
# ---------------------------------------------------------------------------

@dataclass
class AllocatorStats:
    """Live statistics for an allocator instance."""
    total_allocated: int = 0       # running sum of all bytes ever allocated
    total_deallocated: int = 0     # running sum of all bytes ever freed
    current_bytes: int = 0         # bytes currently in use
    peak_bytes: int = 0            # high-water mark
    allocation_count: int = 0      # successful alloc calls
    deallocation_count: int = 0    # free calls
    reallocation_count: int = 0    # realloc calls
    failed_allocations: int = 0    # OOM events

    def record_alloc(self, size: int) -> None:
        self.total_allocated += size
        self.current_bytes += size
        self.allocation_count += 1
        if self.current_bytes > self.peak_bytes:
            self.peak_bytes = self.current_bytes

    def record_dealloc(self, size: int) -> None:
        self.total_deallocated += size
        self.current_bytes = max(0, self.current_bytes - size)
        self.deallocation_count += 1

    def record_realloc(self, old_size: int, new_size: int) -> None:
        self.record_dealloc(old_size)
        self.record_alloc(new_size)
        self.reallocation_count += 1

    def reset(self) -> None:
        """Zero all counters (used when arena/pool is reset)."""
        self.total_allocated = 0
        self.total_deallocated = 0
        self.current_bytes = 0
        self.peak_bytes = 0
        self.allocation_count = 0
        self.deallocation_count = 0
        self.reallocation_count = 0
        self.failed_allocations = 0

    def as_dict(self) -> dict:
        return {
            "total_allocated": self.total_allocated,
            "total_deallocated": self.total_deallocated,
            "current_bytes": self.current_bytes,
            "peak_bytes": self.peak_bytes,
            "allocation_count": self.allocation_count,
            "deallocation_count": self.deallocation_count,
            "reallocation_count": self.reallocation_count,
            "failed_allocations": self.failed_allocations,
        }


# ---------------------------------------------------------------------------
# Internal allocation block (tracks every live allocation)
# ---------------------------------------------------------------------------

class _Block:
    """Represents a single allocated region."""

    __slots__ = ("data", "size", "type_name", "alignment")

    def __init__(self, size: int, type_name: str = "byte", alignment: int = 8):
        self.size = size
        self.type_name = type_name
        self.alignment = alignment
        # Backing store: bytearray for raw blocks, or a Python list for objects
        self.data: Any = bytearray(size) if size > 0 else None


# ---------------------------------------------------------------------------
# Allocator base class
# ---------------------------------------------------------------------------

class Allocator(ABC):
    """
    Abstract base for all NexusLang allocators.

    All concrete allocators must implement:
        allocate(size, type_name, alignment) -> _Block or None
        deallocate(block) -> None
        reallocate(block, new_size) -> _Block or None
        reset() -> None           (discard all allocations at once)
    """

    def __init__(self, name: str):
        self.name = name
        self.stats = AllocatorStats()
        self._lock = threading.Lock()

    @abstractmethod
    def allocate(self, size: int, type_name: str = "byte", alignment: int = 8) -> Optional[_Block]:
        """Allocate *size* bytes.  Returns None on OOM."""
        ...

    @abstractmethod
    def deallocate(self, block: _Block) -> None:
        """Free a previously allocated block."""
        ...

    @abstractmethod
    def reallocate(self, block: _Block, new_size: int) -> Optional[_Block]:
        """Resize a block.  Returns a new block (old one is invalidated)."""
        ...

    @abstractmethod
    def reset(self) -> None:
        """Release all allocations in O(1) (arena/pool semantics)."""
        ...

    def get_stats(self) -> AllocatorStats:
        return self.stats

    def __repr__(self) -> str:
        s = self.stats
        return (
            f"<{self.__class__.__name__} name={self.name!r} "
            f"current={s.current_bytes}B peak={s.peak_bytes}B "
            f"allocs={s.allocation_count}>"
        )


# ---------------------------------------------------------------------------
# 1. System Allocator
# ---------------------------------------------------------------------------

class SystemAllocator(Allocator):
    """
    Default system allocator - thin wrapper over Python memory management.

    Allocations delegate to Python's own heap (ultimately malloc/free).
    This is the fallback / reference allocator.
    """

    def __init__(self):
        super().__init__("system")
        self._live: Dict[int, _Block] = {}   # id(block) -> block

    def allocate(self, size: int, type_name: str = "byte", alignment: int = 8) -> Optional[_Block]:
        if size < 0:
            self.stats.failed_allocations += 1
            return None
        with self._lock:
            block = _Block(size, type_name, alignment)
            self._live[id(block)] = block
            self.stats.record_alloc(size)
            return block

    def deallocate(self, block: _Block) -> None:
        with self._lock:
            key = id(block)
            if key in self._live:
                self.stats.record_dealloc(block.size)
                del self._live[key]
                block.data = None

    def reallocate(self, block: _Block, new_size: int) -> Optional[_Block]:
        if new_size < 0:
            self.stats.failed_allocations += 1
            return None
        with self._lock:
            old_size = block.size
            # Keep existing data up to min(old_size, new_size)
            new_block = _Block(new_size, block.type_name, block.alignment)
            if block.data and new_size > 0:
                copy_len = min(old_size, new_size)
                new_block.data[:copy_len] = block.data[:copy_len]
            # Remove old, register new
            self._live.pop(id(block), None)
            self._live[id(new_block)] = new_block
            self.stats.record_realloc(old_size, new_size)
            block.data = None
            return new_block

    def reset(self) -> None:
        with self._lock:
            for block in list(self._live.values()):
                block.data = None
            self._live.clear()
            self.stats.reset()


# ---------------------------------------------------------------------------
# 2. Arena (Bump) Allocator
# ---------------------------------------------------------------------------

class ArenaAllocator(Allocator):
    """
    Bump / region allocator.

    All allocations come from a single contiguous buffer by advancing
    an offset cursor.  Individual deallocations are no-ops; the entire
    arena is freed in O(1) via reset().

    Ideal for phase-scoped work (parsing a frame, handling a request)
    where all temporaries can be discarded together.
    """

    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError(f"ArenaAllocator capacity must be > 0, got {capacity}")
        super().__init__("arena")
        self._capacity = capacity
        self._buffer = bytearray(capacity)
        self._offset = 0          # next free byte
        self._live: list = []     # track blocks so reset() can invalidate them

    @property
    def capacity(self) -> int:
        return self._capacity

    @property
    def remaining(self) -> int:
        return self._capacity - self._offset

    def allocate(self, size: int, type_name: str = "byte", alignment: int = 8) -> Optional[_Block]:
        if size < 0:
            self.stats.failed_allocations += 1
            return None
        with self._lock:
            # Align offset
            aligned_offset = (self._offset + alignment - 1) & ~(alignment - 1)
            if aligned_offset + size > self._capacity:
                self.stats.failed_allocations += 1
                return None
            block = _Block(size, type_name, alignment)
            # Share a memoryview slice of the arena buffer
            if size > 0:
                block.data = memoryview(self._buffer)[aligned_offset:aligned_offset + size]
            self._offset = aligned_offset + size
            self._live.append(block)
            self.stats.record_alloc(size)
            return block

    def deallocate(self, block: _Block) -> None:
        # Arena allocators do not support individual frees.
        # This is intentional - call reset() to free everything.
        pass

    def reallocate(self, block: _Block, new_size: int) -> Optional[_Block]:
        """Allocate a new block and copy data; the old slot is abandoned."""
        if new_size < 0:
            self.stats.failed_allocations += 1
            return None
        new_block = self.allocate(new_size, block.type_name, block.alignment)
        if new_block is None:
            return None
        if block.data and new_size > 0:
            copy_len = min(block.size, new_size)
            new_block.data[:copy_len] = block.data[:copy_len]
        self.stats.reallocation_count += 1
        return new_block

    def reset(self) -> None:
        """O(1) free: reset cursor and invalidate all outstanding blocks."""
        with self._lock:
            self._offset = 0
            for block in self._live:
                block.data = None
            self._live.clear()
            self.stats.reset()


# ---------------------------------------------------------------------------
# 3. Pool Allocator
# ---------------------------------------------------------------------------

class PoolAllocator(Allocator):
    """
    Fixed-size block recycler.

    Maintains a free-list of identically-sized slots.  Allocation and
    deallocation are both O(1).  Requesting a size different from
    block_size raises ValueError.

    Ideal for object caches (nodes, events, messages) where all objects
    have the same layout.
    """

    def __init__(self, block_size: int, capacity: int):
        if block_size <= 0:
            raise ValueError(f"PoolAllocator block_size must be > 0, got {block_size}")
        if capacity <= 0:
            raise ValueError(f"PoolAllocator capacity must be > 0, got {capacity}")
        super().__init__("pool")
        self._block_size = block_size
        self._capacity = capacity
        # Pre-allocate pool slots
        self._free_list: list = [_Block(block_size) for _ in range(capacity)]
        self._in_use: Dict[int, _Block] = {}

    @property
    def block_size(self) -> int:
        return self._block_size

    @property
    def capacity(self) -> int:
        return self._capacity

    @property
    def free_count(self) -> int:
        return len(self._free_list)

    def allocate(self, size: int, type_name: str = "byte", alignment: int = 8) -> Optional[_Block]:
        if size != self._block_size:
            raise ValueError(
                f"PoolAllocator only serves blocks of size {self._block_size}, requested {size}"
            )
        with self._lock:
            if not self._free_list:
                self.stats.failed_allocations += 1
                return None
            block = self._free_list.pop()
            block.type_name = type_name
            block.alignment = alignment
            # Reset data
            if block.data is not None:
                for i in range(len(block.data)):
                    block.data[i] = 0
            else:
                block.data = bytearray(self._block_size)
            self._in_use[id(block)] = block
            self.stats.record_alloc(size)
            return block

    def deallocate(self, block: _Block) -> None:
        with self._lock:
            key = id(block)
            if key in self._in_use:
                self.stats.record_dealloc(block.size)
                del self._in_use[key]
                self._free_list.append(block)

    def reallocate(self, block: _Block, new_size: int) -> Optional[_Block]:
        """Pool does not resize blocks - new_size must equal block_size."""
        if new_size != self._block_size:
            raise ValueError(
                f"PoolAllocator cannot resize from {self._block_size} to {new_size}"
            )
        self.stats.record_realloc(block.size, new_size)
        return block   # no-op

    def reset(self) -> None:
        """Return all in-use blocks to the free list."""
        with self._lock:
            for block in self._in_use.values():
                self._free_list.append(block)
            self._in_use.clear()
            self.stats.reset()


# ---------------------------------------------------------------------------
# 4. Slab Allocator
# ---------------------------------------------------------------------------

class _Slab:
    """One slab in a slab cache: a chunk of equal-sized object slots."""

    def __init__(self, obj_size: int, slab_capacity: int):
        self.obj_size = obj_size
        self.slab_capacity = slab_capacity
        self.slots: list = [_Block(obj_size) for _ in range(slab_capacity)]
        self.free_slots: list = list(self.slots)
        self.used_count = 0

    @property
    def is_full(self) -> bool:
        return len(self.free_slots) == 0

    @property
    def is_empty(self) -> bool:
        return self.used_count == 0


class SlabAllocator(Allocator):
    """
    Slab-cache allocator (kernel-style).

    Organises memory into slabs - fixed-size pages - each holding a
    fixed number of equal-sized objects.  New slabs are added as needed
    (no hard capacity limit) and fully-empty slabs can be reclaimed.

    Benefits over a plain pool:
    - Unlimited capacity (new slabs added on demand)
    - Slab reclamation avoids long-term fragmentation
    - Per-slab metadata makes debugging easier
    """

    SLAB_CAPACITY = 64   # objects per slab by default

    def __init__(self, obj_size: int, slab_capacity: int = SLAB_CAPACITY):
        if obj_size <= 0:
            raise ValueError(f"SlabAllocator obj_size must be > 0, got {obj_size}")
        if slab_capacity <= 0:
            raise ValueError(f"SlabAllocator slab_capacity must be > 0, got {slab_capacity}")
        super().__init__("slab")
        self._obj_size = obj_size
        self._slab_capacity = slab_capacity
        self._slabs: list = []           # all slabs
        self._partial: list = []         # slabs with free slots
        self._in_use: Dict[int, tuple] = {}  # id(block) -> (block, slab)

    @property
    def obj_size(self) -> int:
        return self._obj_size

    @property
    def slab_count(self) -> int:
        return len(self._slabs)

    def _new_slab(self) -> _Slab:
        slab = _Slab(self._obj_size, self._slab_capacity)
        self._slabs.append(slab)
        self._partial.append(slab)
        return slab

    def allocate(self, size: int, type_name: str = "byte", alignment: int = 8) -> Optional[_Block]:
        if size != self._obj_size:
            raise ValueError(
                f"SlabAllocator only serves objects of size {self._obj_size}, requested {size}"
            )
        with self._lock:
            if not self._partial:
                self._new_slab()
            slab = self._partial[0]
            block = slab.free_slots.pop()
            block.type_name = type_name
            block.alignment = alignment
            if block.data is not None:
                for i in range(len(block.data)):
                    block.data[i] = 0
            else:
                block.data = bytearray(self._obj_size)
            slab.used_count += 1
            if slab.is_full:
                self._partial.remove(slab)
            self._in_use[id(block)] = (block, slab)
            self.stats.record_alloc(size)
            return block

    def deallocate(self, block: _Block) -> None:
        with self._lock:
            key = id(block)
            if key not in self._in_use:
                return
            block_ref, slab = self._in_use.pop(key)
            self.stats.record_dealloc(block.size)
            was_full = slab.is_full
            slab.free_slots.append(block_ref)
            slab.used_count -= 1
            if was_full and not slab.is_full:
                self._partial.append(slab)
            # Optionally reclaim empty slabs
            if slab.is_empty and slab in self._partial:
                self._partial.remove(slab)
                self._slabs.remove(slab)

    def reallocate(self, block: _Block, new_size: int) -> Optional[_Block]:
        if new_size != self._obj_size:
            raise ValueError(
                f"SlabAllocator cannot resize objects from {self._obj_size} to {new_size}"
            )
        self.stats.record_realloc(block.size, new_size)
        return block   # no-op

    def reset(self) -> None:
        """Discard all slabs and reset stats."""
        with self._lock:
            self._slabs.clear()
            self._partial.clear()
            self._in_use.clear()
            self.stats.reset()


# ---------------------------------------------------------------------------
# Allocator-tracked collection wrappers
# ---------------------------------------------------------------------------

_BYTES_PER_ELEMENT = 8  # Conservative pointer-sized estimate per list/dict element


class AllocatorTrackedList(list):
    """
    A list whose mutations are tracked through an NexusLang allocator.

    Subclasses the built-in ``list`` so all existing code that checks
    ``isinstance(x, list)`` or calls ``len()``, iteration, and indexing
    continues to work without modification.

    Mutation methods (``append``, ``insert``, ``extend``, ``pop``,
    ``remove``, ``clear``) notify the backing allocator so that
    ``get_allocator_stats()`` reflects actual live collection memory.

    For arena/bump allocators that do not support per-element free the
    deallocation path updates only the stats counters rather than calling
    ``deallocate()`` (which would be a no-op anyway).
    """

    def __new__(cls, data=None, allocator=None):
        return super().__new__(cls, data or [])

    def __init__(self, data=None, allocator: Optional['Allocator'] = None):
        super().__init__(data or [])
        self._allocator: Optional['Allocator'] = allocator

    # ------------------------------------------------------------------
    # Mutation hooks
    # ------------------------------------------------------------------

    def _alloc_n(self, n: int) -> None:
        """Notify the allocator of *n* new element-slots (one call per element)."""
        if self._allocator is not None and hasattr(self._allocator, 'allocate'):
            for _ in range(n):
                self._allocator.allocate(_BYTES_PER_ELEMENT)

    def _dealloc_n(self, n: int) -> None:
        """Notify the allocator that *n* element-slots were freed."""
        if self._allocator is None:
            return
        # Use stats.record_dealloc to properly increment all counters.
        # Arena allocators do not support physical per-element frees, but we
        # still record the logical deallocation so stats remain accurate.
        freed_bytes = n * _BYTES_PER_ELEMENT
        stats = getattr(self._allocator, 'stats', None)
        if stats is not None:
            stats.record_dealloc(freed_bytes)

    def append(self, item) -> None:
        super().append(item)
        self._alloc_n(1)

    def insert(self, index: int, item) -> None:
        super().insert(index, item)
        self._alloc_n(1)

    def extend(self, items) -> None:
        items = list(items)
        super().extend(items)
        self._alloc_n(len(items))

    def pop(self, index: int = -1):
        item = super().pop(index)
        self._dealloc_n(1)
        return item

    def remove(self, item) -> None:
        super().remove(item)
        self._dealloc_n(1)

    def clear(self) -> None:
        n = len(self)
        super().clear()
        self._dealloc_n(n)

    def __setitem__(self, index, value) -> None:
        # Replacing an existing element does not change allocation count.
        super().__setitem__(index, value)

    def __delitem__(self, index) -> None:
        n = len(range(*index.indices(len(self)))) if isinstance(index, slice) else 1
        super().__delitem__(index)
        self._dealloc_n(n)

    def __iadd__(self, other):
        other = list(other)
        result = super().__iadd__(other)
        self._alloc_n(len(other))
        return result

    def __reduce__(self):
        # Pickling / copy support: drop the allocator reference.
        return (AllocatorTrackedList, (list(self), None))

    def __repr__(self) -> str:
        return f"AllocatorTrackedList({list.__repr__(self)})"


class AllocatorTrackedDict(dict):
    """
    A dict whose insertions and removals are tracked through an NexusLang allocator.

    Same design as :class:`AllocatorTrackedList`: subclasses ``dict`` for
    full compatibility with existing code.

    Each inserted key-value pair is modeled as two pointer-sized slots
    (key + value = 16 bytes).
    """

    _BYTES_PER_ENTRY = _BYTES_PER_ELEMENT * 2  # key + value pointers

    def __new__(cls, data=None, allocator=None):
        return super().__new__(cls, data or {})

    def __init__(self, data=None, allocator: Optional['Allocator'] = None):
        super().__init__(data or {})
        self._allocator: Optional['Allocator'] = allocator

    # ------------------------------------------------------------------
    # Mutation hooks
    # ------------------------------------------------------------------

    def _alloc_n(self, n: int) -> None:
        if self._allocator is not None and hasattr(self._allocator, 'allocate'):
            for _ in range(n):
                self._allocator.allocate(self._BYTES_PER_ENTRY)

    def _dealloc_n(self, n: int) -> None:
        if self._allocator is None:
            return
        freed_bytes = n * self._BYTES_PER_ENTRY
        stats = getattr(self._allocator, 'stats', None)
        if stats is not None:
            stats.record_dealloc(freed_bytes)

    def __setitem__(self, key, value) -> None:
        is_new = key not in self
        super().__setitem__(key, value)
        if is_new:
            self._alloc_n(1)

    def update(self, other=(), **kwargs) -> None:
        if hasattr(other, 'items'):
            other = dict(other)
        else:
            other = dict(other)
        other.update(kwargs)
        new_keys = sum(1 for k in other if k not in self)
        super().update(other)
        if new_keys:
            self._alloc_n(new_keys)

    def setdefault(self, key, default=None):
        if key not in self:
            self[key] = default
            # __setitem__ already calls _alloc_n
            return default
        return self[key]

    def pop(self, *args):
        existed = len(args) >= 1 and args[0] in self
        result = super().pop(*args)
        if existed:
            self._dealloc_n(1)
        return result

    def popitem(self):
        item = super().popitem()
        self._dealloc_n(1)
        return item

    def clear(self) -> None:
        n = len(self)
        super().clear()
        self._dealloc_n(n)

    def __delitem__(self, key) -> None:
        super().__delitem__(key)
        self._dealloc_n(1)

    def __reduce__(self):
        return (AllocatorTrackedDict, (dict(self), None))

    def __repr__(self) -> str:
        return f"AllocatorTrackedDict({dict.__repr__(self)})"


def wrap_collection_with_allocator(value, allocator: 'Allocator'):
    """
    Wrap *value* in an allocator-tracked collection type if it is a ``list``
    or ``dict``.  Already-wrapped values are re-wrapped with the new allocator.

    For other types (scalars, objects) the value is returned unchanged but the
    initial byte count is still reported to the allocator.
    """
    if isinstance(value, AllocatorTrackedList):
        # Re-wrap with the new allocator (rare but correct)
        wrapped = AllocatorTrackedList(list(value), allocator)
    elif isinstance(value, list):
        wrapped = AllocatorTrackedList(value, allocator)
    elif isinstance(value, AllocatorTrackedDict):
        wrapped = AllocatorTrackedDict(dict(value), allocator)
    elif isinstance(value, dict):
        wrapped = AllocatorTrackedDict(value, allocator)
    else:
        return value
    # Report the initial allocation for any pre-existing elements.
    initial_count = len(wrapped)
    if initial_count and hasattr(allocator, 'allocate'):
        for _ in range(initial_count):
            allocator.allocate(_BYTES_PER_ELEMENT)
    return wrapped


# ---------------------------------------------------------------------------
# Global allocator registry
# ---------------------------------------------------------------------------

class _GlobalAllocatorRegistry:
    """
    Singleton registry that holds:
    - The active global allocator (default: SystemAllocator)
    - Per-type allocator overrides  (type_name str -> Allocator)
    """

    def __init__(self):
        self._global: Allocator = SystemAllocator()
        self._per_type: Dict[str, Allocator] = {}
        self._lock = threading.Lock()

    def set_global(self, allocator: Allocator) -> None:
        with self._lock:
            self._global = allocator

    def get_global(self) -> Allocator:
        return self._global

    def set_for_type(self, type_name: str, allocator: Allocator) -> None:
        with self._lock:
            self._per_type[type_name] = allocator

    def get_for_type(self, type_name: str) -> Allocator:
        with self._lock:
            return self._per_type.get(type_name, self._global)

    def clear_per_type(self, type_name: str) -> None:
        with self._lock:
            self._per_type.pop(type_name, None)


_registry = _GlobalAllocatorRegistry()


# ---------------------------------------------------------------------------
# NexusLang runtime interface functions
# ---------------------------------------------------------------------------

def create_system_allocator(runtime, _=None) -> SystemAllocator:
    """
    Create a new system (default heap) allocator.

    Example:
        set alloc to create_system_allocator with 0
    """
    return SystemAllocator()


def create_arena_allocator(runtime, capacity: int) -> ArenaAllocator:
    """
    Create a new arena (bump) allocator with the given capacity in bytes.

    Args:
        capacity: Total arena size in bytes.

    Example:
        set arena to create_arena_allocator with 65536
    """
    return ArenaAllocator(int(capacity))


def create_pool_allocator(runtime, block_size: int, capacity: int) -> PoolAllocator:
    """
    Create a pool allocator for fixed-size blocks.

    Args:
        block_size: Size of each block in bytes.
        capacity:   Number of pre-allocated blocks in the pool.

    Example:
        set pool to create_pool_allocator with 64 and 1024
    """
    return PoolAllocator(int(block_size), int(capacity))


def create_slab_allocator(runtime, obj_size: int, slab_capacity: int = 64) -> SlabAllocator:
    """
    Create a slab-cache allocator for same-size objects.

    Args:
        obj_size:       Size of each object in bytes.
        slab_capacity:  Number of objects per slab (default 64).

    Example:
        set slab to create_slab_allocator with 128 and 64
    """
    return SlabAllocator(int(obj_size), int(slab_capacity))


def allocator_allocate(runtime, allocator: Allocator, size: int, type_name: str = "byte") -> Optional[_Block]:
    """
    Allocate *size* bytes from *allocator*.

    Returns the block on success, or None on OOM.

    Example:
        set blk to allocator_allocate with arena and 256 and "Matrix"
    """
    return allocator.allocate(int(size), type_name)


def allocator_deallocate(runtime, allocator: Allocator, block) -> None:
    """
    Deallocate a block previously allocated from *allocator*.

    Example:
        call allocator_deallocate with pool and blk
    """
    if isinstance(block, _Block):
        allocator.deallocate(block)


def allocator_reallocate(runtime, allocator: Allocator, block, new_size: int) -> Optional[_Block]:
    """
    Resize *block* to *new_size* bytes.

    Returns a new block (old block is invalidated).

    Example:
        set blk to allocator_reallocate with alloc and blk and 512
    """
    if not isinstance(block, _Block):
        return None
    return allocator.reallocate(block, int(new_size))


def allocator_reset(runtime, allocator: Allocator) -> None:
    """
    Free all memory managed by *allocator* in one operation (arena/pool semantics).

    For SystemAllocator this also clears all live blocks.

    Example:
        call allocator_reset with arena
    """
    allocator.reset()


def get_allocator_stats(runtime, allocator: Allocator) -> dict:
    """
    Return a dictionary with live statistics for *allocator*.

    Keys:
        total_allocated, total_deallocated, current_bytes,
        peak_bytes, allocation_count, deallocation_count,
        reallocation_count, failed_allocations

    Example:
        set stats to get_allocator_stats with arena
        print text stats["peak_bytes"]
    """
    return allocator.stats.as_dict()


def set_global_allocator(runtime, allocator: Allocator) -> None:
    """
    Replace the process-level global allocator.

    Affects all subsequent allocations that do not have a per-type override.

    Example:
        call set_global_allocator with arena
    """
    _registry.set_global(allocator)


def get_global_allocator(runtime, _=None) -> Allocator:
    """
    Return the currently active global allocator.

    Example:
        set alloc to get_global_allocator with 0
    """
    return _registry.get_global()


def set_type_allocator(runtime, type_name: str, allocator: Allocator) -> None:
    """
    Assign a custom allocator for all objects of type *type_name*.

    Example:
        call set_type_allocator with "Node" and pool
    """
    _registry.set_for_type(str(type_name), allocator)


def get_type_allocator(runtime, type_name: str) -> Allocator:
    """
    Return the allocator assigned to *type_name* (falls back to global).

    Example:
        set alloc to get_type_allocator with "Node"
    """
    return _registry.get_for_type(str(type_name))


def clear_type_allocator(runtime, type_name: str) -> None:
    """
    Remove the per-type allocator override for *type_name*.

    Example:
        call clear_type_allocator with "Node"
    """
    _registry.clear_per_type(str(type_name))


def allocator_block_size(runtime, block) -> int:
    """
    Return the size in bytes of an allocated block.

    Example:
        set sz to allocator_block_size with blk
    """
    if isinstance(block, _Block):
        return block.size
    return 0


def allocator_block_type(runtime, block) -> str:
    """
    Return the type name stored in an allocated block.

    Example:
        set tn to allocator_block_type with blk
    """
    if isinstance(block, _Block):
        return block.type_name
    return "unknown"


def allocator_write_byte(runtime, block, offset: int, value: int) -> None:
    """
    Write a single byte into *block* at *offset*.

    Example:
        call allocator_write_byte with blk and 0 and 255
    """
    if isinstance(block, _Block) and block.data is not None:
        block.data[int(offset)] = int(value) & 0xFF


def allocator_read_byte(runtime, block, offset: int) -> int:
    """
    Read a single byte from *block* at *offset*.

    Example:
        set byte to allocator_read_byte with blk and 0
    """
    if isinstance(block, _Block) and block.data is not None:
        return int(block.data[int(offset)])
    return 0


def register_stdlib(runtime) -> None:
    """Register all allocator functions with the NexusLang runtime."""

    # Allocator constructors
    runtime.register_function("create_system_allocator", create_system_allocator)
    runtime.register_function("create_arena_allocator",  create_arena_allocator)
    runtime.register_function("create_pool_allocator",   create_pool_allocator)
    runtime.register_function("create_slab_allocator",   create_slab_allocator)

    # Core allocator operations
    runtime.register_function("allocator_allocate",   allocator_allocate)
    runtime.register_function("allocator_deallocate", allocator_deallocate)
    runtime.register_function("allocator_reallocate", allocator_reallocate)
    runtime.register_function("allocator_reset",      allocator_reset)

    # Statistics
    runtime.register_function("get_allocator_stats",  get_allocator_stats)

    # Global allocator control
    runtime.register_function("set_global_allocator", set_global_allocator)
    runtime.register_function("get_global_allocator", get_global_allocator)

    # Per-type allocator assignment
    runtime.register_function("set_type_allocator",   set_type_allocator)
    runtime.register_function("get_type_allocator",   get_type_allocator)
    runtime.register_function("clear_type_allocator", clear_type_allocator)

    # Block introspection helpers
    runtime.register_function("allocator_block_size", allocator_block_size)
    runtime.register_function("allocator_block_type", allocator_block_type)
    runtime.register_function("allocator_write_byte", allocator_write_byte)
    runtime.register_function("allocator_read_byte",  allocator_read_byte)
