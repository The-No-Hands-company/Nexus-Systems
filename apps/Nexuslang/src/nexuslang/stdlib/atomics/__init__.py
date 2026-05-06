"""
NLPL Atomics Module - Atomic Operations and Memory Ordering

Provides atomic types and operations for lock-free concurrent programming:
- Atomic types (AtomicInteger, AtomicBoolean, AtomicPointer)
- Atomic operations (load, store, add, sub, CAS, fetch-and-add)
- Memory ordering (relaxed, acquire, release, seq_cst)
- Memory fences/barriers

These primitives are essential for writing correct concurrent code without
data races. They provide hardware-level atomicity guarantees.
"""

import threading
from enum import Enum
from typing import Any, Optional


class MemoryOrder(Enum):
    """Memory ordering constraints for atomic operations"""
    RELAXED = "relaxed"  # No synchronization, just atomicity
    ACQUIRE = "acquire"  # Acquire semantics (load barrier)
    RELEASE = "release"  # Release semantics (store barrier)
    ACQ_REL = "acq_rel"  # Both acquire and release
    SEQ_CST = "seq_cst"  # Sequential consistency (strongest)


class AtomicInteger:
    """
    Atomic integer with lock-free operations
    
    Provides atomic read-modify-write operations on integers.
    """
    
    def __init__(self, initial_value: int = 0):
        self._value = initial_value
        self._lock = threading.Lock()  # Fallback for Python (C extension would use hardware atomics)
    
    def load(self, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically load the value"""
        with self._lock:
            return self._value
    
    def store(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST):
        """Atomically store a value"""
        with self._lock:
            self._value = value
    
    def exchange(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically exchange value and return old value"""
        with self._lock:
            old = self._value
            self._value = value
            return old
    
    def compare_exchange_strong(
        self,
        expected: int,
        desired: int,
        order: MemoryOrder = MemoryOrder.SEQ_CST
    ) -> tuple[bool, int]:
        """
        Compare-and-swap operation
        Returns (success, actual_value)
        """
        with self._lock:
            actual = self._value
            if actual == expected:
                self._value = desired
                return (True, actual)
            return (False, actual)
    
    def fetch_add(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically add value and return old value"""
        with self._lock:
            old = self._value
            self._value += value
            return old
    
    def fetch_sub(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically subtract value and return old value"""
        with self._lock:
            old = self._value
            self._value -= value
            return old
    
    def fetch_and(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically AND value and return old value"""
        with self._lock:
            old = self._value
            self._value &= value
            return old
    
    def fetch_or(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically OR value and return old value"""
        with self._lock:
            old = self._value
            self._value |= value
            return old
    
    def fetch_xor(self, value: int, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically XOR value and return old value"""
        with self._lock:
            old = self._value
            self._value ^= value
            return old
    
    def increment(self, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically increment and return new value"""
        with self._lock:
            self._value += 1
            return self._value
    
    def decrement(self, order: MemoryOrder = MemoryOrder.SEQ_CST) -> int:
        """Atomically decrement and return new value"""
        with self._lock:
            self._value -= 1
            return self._value


class AtomicBoolean:
    """
    Atomic boolean with lock-free operations
    """
    
    def __init__(self, initial_value: bool = False):
        self._value = initial_value
        self._lock = threading.Lock()
    
    def load(self, order: MemoryOrder = MemoryOrder.SEQ_CST) -> bool:
        """Atomically load the value"""
        with self._lock:
            return self._value
    
    def store(self, value: bool, order: MemoryOrder = MemoryOrder.SEQ_CST):
        """Atomically store a value"""
        with self._lock:
            self._value = value
    
    def exchange(self, value: bool, order: MemoryOrder = MemoryOrder.SEQ_CST) -> bool:
        """Atomically exchange value and return old value"""
        with self._lock:
            old = self._value
            self._value = value
            return old
    
    def compare_exchange_strong(
        self,
        expected: bool,
        desired: bool,
        order: MemoryOrder = MemoryOrder.SEQ_CST
    ) -> tuple[bool, bool]:
        """
        Compare-and-swap operation
        Returns (success, actual_value)
        """
        with self._lock:
            actual = self._value
            if actual == expected:
                self._value = desired
                return (True, actual)
            return (False, actual)


class AtomicPointer:
    """
    Atomic pointer with lock-free operations
    
    Stores any Python object reference atomically.
    """
    
    def __init__(self, initial_value: Any = None):
        self._value = initial_value
        self._lock = threading.Lock()
    
    def load(self, order: MemoryOrder = MemoryOrder.SEQ_CST) -> Any:
        """Atomically load the pointer"""
        with self._lock:
            return self._value
    
    def store(self, value: Any, order: MemoryOrder = MemoryOrder.SEQ_CST):
        """Atomically store a pointer"""
        with self._lock:
            self._value = value
    
    def exchange(self, value: Any, order: MemoryOrder = MemoryOrder.SEQ_CST) -> Any:
        """Atomically exchange pointer and return old value"""
        with self._lock:
            old = self._value
            self._value = value
            return old
    
    def compare_exchange_strong(
        self,
        expected: Any,
        desired: Any,
        order: MemoryOrder = MemoryOrder.SEQ_CST
    ) -> tuple[bool, Any]:
        """
        Compare-and-swap operation
        Returns (success, actual_value)
        """
        with self._lock:
            actual = self._value
            if actual is expected:  # Identity comparison for pointers
                self._value = desired
                return (True, actual)
            return (False, actual)


# NexusLang runtime interface functions
def create_atomic_integer(runtime, initial_value: int = 0) -> AtomicInteger:
    """
    Create an atomic integer
    
    Example:
        set counter to create_atomic_integer with initial_value 0
    """
    return AtomicInteger(initial_value)


def create_atomic_boolean(runtime, initial_value: bool = False) -> AtomicBoolean:
    """
    Create an atomic boolean
    
    Example:
        set flag to create_atomic_boolean with initial_value false
    """
    return AtomicBoolean(initial_value)


def create_atomic_pointer(runtime, initial_value: Any = None) -> AtomicPointer:
    """
    Create an atomic pointer
    
    Example:
        set ptr to create_atomic_pointer with initial_value null
    """
    return AtomicPointer(initial_value)


def atomic_load(runtime, atomic, order: str = "seq_cst"):
    """
    Load value from atomic variable
    
    Args:
        atomic: Atomic variable (AtomicInteger, AtomicBoolean, AtomicPointer)
        order: Memory ordering (relaxed, acquire, seq_cst)
    
    Returns:
        Current value
        
    Example:
        set value to atomic_load with atomic counter and order "seq_cst"
    """
    mem_order = MemoryOrder(order)
    return atomic.load(mem_order)


def atomic_store(runtime, atomic, value, order: str = "seq_cst"):
    """
    Store value to atomic variable
    
    Args:
        atomic: Atomic variable
        value: Value to store
        order: Memory ordering (relaxed, release, seq_cst)
        
    Example:
        atomic_store with atomic counter and value 42 and order "seq_cst"
    """
    mem_order = MemoryOrder(order)
    atomic.store(value, mem_order)


def atomic_exchange(runtime, atomic, value, order: str = "seq_cst"):
    """
    Atomically exchange value and return old value
    
    Example:
        set old_value to atomic_exchange with atomic counter and value 10
    """
    mem_order = MemoryOrder(order)
    return atomic.exchange(value, mem_order)


def atomic_compare_exchange(runtime, atomic, expected, desired, order: str = "seq_cst"):
    """
    Compare-and-swap operation
    
    Returns tuple: (success as Boolean, actual_value)
    
    Example:
        set result to atomic_compare_exchange with atomic counter and expected 0 and desired 1
        set success to result[0]
        set actual to result[1]
    """
    mem_order = MemoryOrder(order)
    return atomic.compare_exchange_strong(expected, desired, mem_order)


def atomic_fetch_add(runtime, atomic: AtomicInteger, value: int, order: str = "seq_cst") -> int:
    """
    Atomically add value and return old value
    
    Example:
        set old_value to atomic_fetch_add with atomic counter and value 5
    """
    mem_order = MemoryOrder(order)
    return atomic.fetch_add(value, mem_order)


def atomic_fetch_sub(runtime, atomic: AtomicInteger, value: int, order: str = "seq_cst") -> int:
    """
    Atomically subtract value and return old value
    
    Example:
        set old_value to atomic_fetch_sub with atomic counter and value 3
    """
    mem_order = MemoryOrder(order)
    return atomic.fetch_sub(value, mem_order)


def atomic_fetch_and(runtime, atomic: AtomicInteger, value: int, order: str = "seq_cst") -> int:
    """
    Atomically AND value and return old value
    
    Example:
        set old_value to atomic_fetch_and with atomic flags and value 0xFF
    """
    mem_order = MemoryOrder(order)
    return atomic.fetch_and(value, mem_order)


def atomic_fetch_or(runtime, atomic: AtomicInteger, value: int, order: str = "seq_cst") -> int:
    """
    Atomically OR value and return old value
    
    Example:
        set old_value to atomic_fetch_or with atomic flags and value 0x01
    """
    mem_order = MemoryOrder(order)
    return atomic.fetch_or(value, mem_order)


def atomic_fetch_xor(runtime, atomic: AtomicInteger, value: int, order: str = "seq_cst") -> int:
    """
    Atomically XOR value and return old value
    
    Example:
        set old_value to atomic_fetch_xor with atomic flags and value 0xFF
    """
    mem_order = MemoryOrder(order)
    return atomic.fetch_xor(value, mem_order)


def atomic_increment(runtime, atomic: AtomicInteger, order: str = "seq_cst") -> int:
    """
    Atomically increment and return new value
    
    Example:
        set new_value to atomic_increment with atomic counter
    """
    mem_order = MemoryOrder(order)
    return atomic.increment(mem_order)


def atomic_decrement(runtime, atomic: AtomicInteger, order: str = "seq_cst") -> int:
    """
    Atomically decrement and return new value
    
    Example:
        set new_value to atomic_decrement with atomic counter
    """
    mem_order = MemoryOrder(order)
    return atomic.decrement(mem_order)


def atomic_fence(runtime, order: str = "seq_cst"):
    """
    Memory fence/barrier
    
    Ensures all memory operations before the fence are visible to other threads
    before any operations after the fence.
    
    Args:
        order: Memory ordering (acquire, release, acq_rel, seq_cst)
        
    Example:
        atomic_fence with order "seq_cst"
    """
    # In Python, this is a no-op since we use locks
    # In compiled code, this would emit CPU fence instructions
    pass


def register_stdlib(runtime):
    """Register atomics module functions with NexusLang runtime"""
    from nexuslang.runtime.runtime import Runtime
    
    # Atomic type constructors
    runtime.register_function("create_atomic_integer", create_atomic_integer)
    runtime.register_function("create_atomic_boolean", create_atomic_boolean)
    runtime.register_function("create_atomic_pointer", create_atomic_pointer)
    
    # Atomic operations
    runtime.register_function("atomic_load", atomic_load)
    runtime.register_function("atomic_store", atomic_store)
    runtime.register_function("atomic_exchange", atomic_exchange)
    runtime.register_function("atomic_compare_exchange", atomic_compare_exchange)
    
    # Atomic integer operations
    runtime.register_function("atomic_fetch_add", atomic_fetch_add)
    runtime.register_function("atomic_fetch_sub", atomic_fetch_sub)
    runtime.register_function("atomic_fetch_and", atomic_fetch_and)
    runtime.register_function("atomic_fetch_or", atomic_fetch_or)
    runtime.register_function("atomic_fetch_xor", atomic_fetch_xor)
    runtime.register_function("atomic_increment", atomic_increment)
    runtime.register_function("atomic_decrement", atomic_decrement)
    
    # Memory fences
    runtime.register_function("atomic_fence", atomic_fence)
    
    # Memory order constants
    runtime.register_constant("MEMORY_ORDER_RELAXED", "relaxed")
    runtime.register_constant("MEMORY_ORDER_ACQUIRE", "acquire")
    runtime.register_constant("MEMORY_ORDER_RELEASE", "release")
    runtime.register_constant("MEMORY_ORDER_ACQ_REL", "acq_rel")
    runtime.register_constant("MEMORY_ORDER_SEQ_CST", "seq_cst")

