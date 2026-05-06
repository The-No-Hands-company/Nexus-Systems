"""
Smart pointer types for NexusLang: Rc<T>, Arc<T>, Weak<T>, Box<T>, RefCell<T>,
Mutex<T>-wrapped values, and RwLock<T>-wrapped values.

Language-keyword types (Rc, Arc, Weak) are handled by the interpreter's
execute_rc_creation / execute_downgrade_expression / execute_upgrade_expression
methods.  This module registers the companion stdlib functions AND the Box /
RefCell / MutexValue / RwLockValue types, which are stdlib-only.

Registered NLPL-callable functions
------------------------------------
Rc / Arc helpers:
  rc_get            rc of T -> T
  rc_set            (rc of T, T) -> ()
  rc_strong_count   rc of T -> Integer
  rc_weak_count     rc of T -> Integer
  rc_clone          rc of T -> rc of T
  arc_clone         arc of T -> arc of T
  weak_get          weak of T -> T or None

Box<T>:
  box_new           T -> Box
  box_get           Box -> T
  box_set           (Box, T) -> ()
  box_into_inner    Box -> T  (consumes box; box becomes invalid)
  box_is_valid      Box -> Boolean

RefCell<T>:
  refcell_new           T -> RefCell
  refcell_get           RefCell -> T
  refcell_set           (RefCell, T) -> ()
  refcell_borrow        RefCell -> T  (records immutable borrow)
  refcell_borrow_mut    RefCell -> T  (mutable borrow; panics if already borrowed)
  refcell_release       RefCell -> ()  (release the most recent immutable borrow)
  refcell_release_mut   RefCell -> ()  (release the mutable borrow)
  refcell_borrow_count  RefCell -> Integer  (+n = n immut borrows, -1 = mut borrow)

Mutex<T>-owned value:
  mutex_value_new        T -> MutexValue
  mutex_value_lock       MutexValue -> T
  mutex_value_unlock     MutexValue -> ()
  mutex_value_set        (MutexValue, T) -> ()
  mutex_value_try_lock   MutexValue -> T or None

RwLock<T>-owned value:
  rwlock_value_new       T -> RwLockValue
  rwlock_value_read      RwLockValue -> T
  rwlock_value_write     (RwLockValue, T) -> ()
"""

import threading


# ---------------------------------------------------------------------------
# Runtime value classes
# ---------------------------------------------------------------------------

class _RcInner:
    """Shared heap cell for Rc / Arc / Weak."""

    __slots__ = ("value", "strong", "weak", "lock")

    def __init__(self, value, thread_safe: bool = False):
        self.value = value
        self.strong: int = 1
        self.weak: int = 0
        self.lock = threading.Lock() if thread_safe else None

    def inc_strong(self):
        if self.lock:
            with self.lock:
                self.strong += 1
        else:
            self.strong += 1

    def dec_strong(self) -> int:
        if self.lock:
            with self.lock:
                self.strong -= 1
                return self.strong
        else:
            self.strong -= 1
            return self.strong

    def inc_weak(self):
        if self.lock:
            with self.lock:
                self.weak += 1
        else:
            self.weak += 1

    def dec_weak(self) -> int:
        if self.lock:
            with self.lock:
                self.weak -= 1
                return self.weak
        else:
            self.weak -= 1
            return self.weak

    def is_alive(self) -> bool:
        if self.lock:
            with self.lock:
                return self.strong > 0
        return self.strong > 0


class RcValue:
    """Runtime representation of Rc<T> (single-threaded reference counted)."""

    def __init__(self, value, _inner: "_RcInner | None" = None):
        if _inner is None:
            self._inner = _RcInner(value, thread_safe=False)
        else:
            self._inner = _inner
            self._inner.inc_strong()

    @property
    def value(self):
        return self._inner.value

    @value.setter
    def value(self, v):
        self._inner.value = v

    def strong_count(self) -> int:
        return self._inner.strong

    def weak_count(self) -> int:
        return self._inner.weak

    def clone(self) -> "RcValue":
        """Increment strong count and return a new handle to the same inner."""
        rc = RcValue.__new__(RcValue)
        rc._inner = self._inner
        rc._inner.inc_strong()
        return rc

    def downgrade(self) -> "WeakValue":
        """Create a Weak reference from this Rc."""
        return WeakValue(self._inner)

    def drop(self):
        """Decrement the strong count; data is freed when strong reaches 0."""
        remaining = self._inner.dec_strong()
        return remaining

    def __repr__(self):
        return f"Rc<{self._inner.value!r}> (strong={self._inner.strong})"


class ArcValue:
    """Runtime representation of Arc<T> (atomic/thread-safe reference counted)."""

    def __init__(self, value, _inner: "_RcInner | None" = None):
        if _inner is None:
            self._inner = _RcInner(value, thread_safe=True)
        else:
            self._inner = _inner
            self._inner.inc_strong()

    @property
    def value(self):
        with self._inner.lock:
            return self._inner.value

    @value.setter
    def value(self, v):
        with self._inner.lock:
            self._inner.value = v

    def strong_count(self) -> int:
        with self._inner.lock:
            return self._inner.strong

    def weak_count(self) -> int:
        with self._inner.lock:
            return self._inner.weak

    def clone(self) -> "ArcValue":
        arc = ArcValue.__new__(ArcValue)
        arc._inner = self._inner
        arc._inner.inc_strong()
        return arc

    def downgrade(self) -> "WeakValue":
        return WeakValue(self._inner)

    def drop(self):
        return self._inner.dec_strong()

    def __repr__(self):
        return f"Arc<{self._inner.value!r}> (strong={self._inner.strong})"


class WeakValue:
    """Runtime representation of Weak<T> (non-owning reference)."""

    def __init__(self, inner: "_RcInner"):
        self._inner = inner
        inner.inc_weak()

    def upgrade(self) -> "RcValue | ArcValue | None":
        """Attempt to obtain a strong reference.  Returns None if dropped."""
        if not self._inner.is_alive():
            return None
        if self._inner.lock is not None:
            arc = ArcValue.__new__(ArcValue)
            arc._inner = self._inner
            arc._inner.inc_strong()
            return arc
        else:
            rc = RcValue.__new__(RcValue)
            rc._inner = self._inner
            rc._inner.inc_strong()
            return rc

    def is_alive(self) -> bool:
        return self._inner.is_alive()

    def drop(self):
        return self._inner.dec_weak()

    def __repr__(self):
        alive = "alive" if self._inner.is_alive() else "dropped"
        return f"Weak ({alive})"


class BoxValue:
    """Runtime representation of Box<T> (unique ownership heap allocation)."""

    def __init__(self, value):
        self._value = value
        self._valid = True

    @property
    def value(self):
        if not self._valid:
            raise RuntimeError("use of moved Box<T>")
        return self._value

    @value.setter
    def value(self, v):
        if not self._valid:
            raise RuntimeError("use of moved Box<T>")
        self._value = v

    def into_inner(self):
        """Consume the Box and return the contained value."""
        if not self._valid:
            raise RuntimeError("use of moved Box<T>")
        val = self._value
        self._value = None
        self._valid = False
        return val

    def is_valid(self) -> bool:
        return self._valid

    def __repr__(self):
        if self._valid:
            return f"Box<{self._value!r}>"
        return "Box<moved>"


class RefCellValue:
    """Runtime representation of RefCell<T> (interior mutability with runtime borrow checking)."""

    def __init__(self, value):
        self._value = value
        # _borrow_state: 0 = unborrowed, >0 = N immutable borrows, -1 = mutable borrow
        self._borrow_state: int = 0
        self._lock = threading.Lock()

    @property
    def value(self):
        return self._value

    @value.setter
    def value(self, v):
        self._value = v

    def borrow(self):
        """Acquire an immutable borrow.  Raises if mutably borrowed."""
        with self._lock:
            if self._borrow_state < 0:
                raise RuntimeError(
                    "RefCell: cannot borrow immutably — already mutably borrowed"
                )
            self._borrow_state += 1
            return self._value

    def borrow_mut(self):
        """Acquire the mutable borrow.  Raises if already borrowed."""
        with self._lock:
            if self._borrow_state != 0:
                raise RuntimeError(
                    "RefCell: cannot borrow mutably — already borrowed "
                    f"(borrow_state={self._borrow_state})"
                )
            self._borrow_state = -1
            return self._value

    def release(self):
        """Release ONE immutable borrow."""
        with self._lock:
            if self._borrow_state <= 0:
                raise RuntimeError(
                    "RefCell: release() called but no immutable borrow is active"
                )
            self._borrow_state -= 1

    def release_mut(self):
        """Release the mutable borrow."""
        with self._lock:
            if self._borrow_state != -1:
                raise RuntimeError(
                    "RefCell: release_mut() called but no mutable borrow is active"
                )
            self._borrow_state = 0

    def borrow_count(self) -> int:
        with self._lock:
            return self._borrow_state

    def __repr__(self):
        state = "mut-borrowed" if self._borrow_state == -1 else f"{self._borrow_state} borrows"
        return f"RefCell<{self._value!r}> ({state})"


class MutexValue:
    """A value guarded by a Mutex (Mutex<T>)."""

    def __init__(self, value):
        self._value = value
        self._lock = threading.Lock()
        self._locked_by_api = False

    def lock(self):
        """Acquire the mutex and return the contained value."""
        self._lock.acquire()
        self._locked_by_api = True
        return self._value

    def unlock(self):
        """Release the mutex."""
        if self._locked_by_api:
            self._locked_by_api = False
            self._lock.release()

    def set(self, value):
        """Acquire lock, set value, release lock atomically."""
        with self._lock:
            self._value = value

    def get(self):
        """Acquire lock, read value, release lock."""
        with self._lock:
            return self._value

    def try_lock(self):
        """Non-blocking lock attempt.  Returns value on success, None on failure."""
        if self._lock.acquire(blocking=False):
            self._locked_by_api = True
            return self._value
        return None

    def __repr__(self):
        locked = "locked" if self._locked_by_api else "unlocked"
        return f"Mutex<{self._value!r}> ({locked})"


class RwLockValue:
    """A value guarded by a read-write lock (RwLock<T>)."""

    def __init__(self, value):
        self._value = value
        self._write_lock = threading.Lock()
        self._read_count = 0
        self._read_lock = threading.Lock()

    def read(self):
        """Acquire shared (read) access and return the value."""
        with self._read_lock:
            self._read_count += 1
            if self._read_count == 1:
                self._write_lock.acquire()
        return self._value

    def release_read(self):
        """Release a read lock."""
        with self._read_lock:
            self._read_count -= 1
            if self._read_count == 0:
                self._write_lock.release()

    def write(self, value):
        """Acquire exclusive (write) access, set value, and release."""
        self._write_lock.acquire()
        try:
            self._value = value
        finally:
            self._write_lock.release()

    def __repr__(self):
        return f"RwLock<{self._value!r}>"


# ---------------------------------------------------------------------------
# Sentinel for moved / invalidated values
# ---------------------------------------------------------------------------

class MovedValue:
    """Sentinel placed in a variable slot after its value has been moved."""

    def __init__(self, name: str = "<unknown>"):
        self.name = name

    def __repr__(self):
        return f"<moved: {self.name}>"


_MOVED_SENTINEL = MovedValue()


# ---------------------------------------------------------------------------
# NLPL-callable functions
# ---------------------------------------------------------------------------

# --- Rc helpers ---

def _rc_get(runtime, rc):
    if isinstance(rc, (RcValue, ArcValue)):
        return rc.value
    raise RuntimeError(f"rc_get: expected Rc or Arc, got {type(rc).__name__}")


def _rc_set(runtime, rc, value):
    if isinstance(rc, (RcValue, ArcValue)):
        rc.value = value
        return None
    raise RuntimeError(f"rc_set: expected Rc or Arc, got {type(rc).__name__}")


def _rc_strong_count(runtime, rc):
    if isinstance(rc, RcValue):
        return rc.strong_count()
    if isinstance(rc, ArcValue):
        return rc.strong_count()
    raise RuntimeError(f"rc_strong_count: expected Rc or Arc, got {type(rc).__name__}")


def _rc_weak_count(runtime, rc):
    if isinstance(rc, RcValue):
        return rc.weak_count()
    if isinstance(rc, ArcValue):
        return rc.weak_count()
    raise RuntimeError(f"rc_weak_count: expected Rc or Arc, got {type(rc).__name__}")


def _rc_clone(runtime, rc):
    if isinstance(rc, RcValue):
        return rc.clone()
    raise RuntimeError(f"rc_clone: expected Rc, got {type(rc).__name__}")


def _arc_clone(runtime, arc):
    if isinstance(arc, ArcValue):
        return arc.clone()
    raise RuntimeError(f"arc_clone: expected Arc, got {type(arc).__name__}")


def _weak_get(runtime, weak):
    if isinstance(weak, WeakValue):
        upgraded = weak.upgrade()
        if upgraded is None:
            return None
        # Return the raw value rather than the Rc wrapper for simple access
        return upgraded.value
    raise RuntimeError(f"weak_get: expected Weak, got {type(weak).__name__}")


# --- Box helpers ---

def _box_new(runtime, value):
    return BoxValue(value)


def _box_get(runtime, b):
    if not isinstance(b, BoxValue):
        raise RuntimeError(f"box_get: expected Box, got {type(b).__name__}")
    return b.value


def _box_set(runtime, b, value):
    if not isinstance(b, BoxValue):
        raise RuntimeError(f"box_set: expected Box, got {type(b).__name__}")
    b.value = value
    return None


def _box_into_inner(runtime, b):
    if not isinstance(b, BoxValue):
        raise RuntimeError(f"box_into_inner: expected Box, got {type(b).__name__}")
    return b.into_inner()


def _box_is_valid(runtime, b):
    if not isinstance(b, BoxValue):
        return False
    return b.is_valid()


# --- RefCell helpers ---

def _refcell_new(runtime, value):
    return RefCellValue(value)


def _refcell_get(runtime, cell):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_get: expected RefCell, got {type(cell).__name__}")
    return cell.value


def _refcell_set(runtime, cell, value):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_set: expected RefCell, got {type(cell).__name__}")
    cell.value = value
    return None


def _refcell_borrow(runtime, cell):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_borrow: expected RefCell, got {type(cell).__name__}")
    return cell.borrow()


def _refcell_borrow_mut(runtime, cell):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_borrow_mut: expected RefCell, got {type(cell).__name__}")
    return cell.borrow_mut()


def _refcell_release(runtime, cell):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_release: expected RefCell, got {type(cell).__name__}")
    cell.release()
    return None


def _refcell_release_mut(runtime, cell):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_release_mut: expected RefCell, got {type(cell).__name__}")
    cell.release_mut()
    return None


def _refcell_borrow_count(runtime, cell):
    if not isinstance(cell, RefCellValue):
        raise RuntimeError(f"refcell_borrow_count: expected RefCell, got {type(cell).__name__}")
    return cell.borrow_count()


# --- Mutex<T> value helpers ---

def _mutex_value_new(runtime, value):
    return MutexValue(value)


def _mutex_value_lock(runtime, m):
    if not isinstance(m, MutexValue):
        raise RuntimeError(f"mutex_value_lock: expected MutexValue, got {type(m).__name__}")
    return m.lock()


def _mutex_value_unlock(runtime, m):
    if not isinstance(m, MutexValue):
        raise RuntimeError(f"mutex_value_unlock: expected MutexValue, got {type(m).__name__}")
    m.unlock()
    return None


def _mutex_value_set(runtime, m, value):
    if not isinstance(m, MutexValue):
        raise RuntimeError(f"mutex_value_set: expected MutexValue, got {type(m).__name__}")
    m.set(value)
    return None


def _mutex_value_try_lock(runtime, m):
    if not isinstance(m, MutexValue):
        raise RuntimeError(f"mutex_value_try_lock: expected MutexValue, got {type(m).__name__}")
    return m.try_lock()


# --- RwLock<T> value helpers ---

def _rwlock_value_new(runtime, value):
    return RwLockValue(value)


def _rwlock_value_read(runtime, rw):
    if not isinstance(rw, RwLockValue):
        raise RuntimeError(f"rwlock_value_read: expected RwLockValue, got {type(rw).__name__}")
    return rw.read()


def _rwlock_value_release_read(runtime, rw):
    if not isinstance(rw, RwLockValue):
        raise RuntimeError(f"rwlock_value_release_read: expected RwLockValue, got {type(rw).__name__}")
    rw.release_read()
    return None


def _rwlock_value_write(runtime, rw, value):
    if not isinstance(rw, RwLockValue):
        raise RuntimeError(f"rwlock_value_write: expected RwLockValue, got {type(rw).__name__}")
    rw.write(value)
    return None


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_stdlib(runtime) -> None:
    """Register all smart-pointer functions with the NexusLang runtime."""

    # Rc / Arc / Weak helpers
    runtime.register_function("rc_get",           _rc_get)
    runtime.register_function("rc_set",           _rc_set)
    runtime.register_function("rc_strong_count",  _rc_strong_count)
    runtime.register_function("rc_weak_count",    _rc_weak_count)
    runtime.register_function("rc_clone",         _rc_clone)
    runtime.register_function("arc_clone",        _arc_clone)
    runtime.register_function("weak_get",         _weak_get)

    # Box
    runtime.register_function("box_new",          _box_new)
    runtime.register_function("box_get",          _box_get)
    runtime.register_function("box_set",          _box_set)
    runtime.register_function("box_into_inner",   _box_into_inner)
    runtime.register_function("box_is_valid",     _box_is_valid)

    # RefCell
    runtime.register_function("refcell_new",          _refcell_new)
    runtime.register_function("refcell_get",          _refcell_get)
    runtime.register_function("refcell_set",          _refcell_set)
    runtime.register_function("refcell_borrow",       _refcell_borrow)
    runtime.register_function("refcell_borrow_mut",   _refcell_borrow_mut)
    runtime.register_function("refcell_release",      _refcell_release)
    runtime.register_function("refcell_release_mut",  _refcell_release_mut)
    runtime.register_function("refcell_borrow_count", _refcell_borrow_count)

    # Mutex<T> value
    runtime.register_function("mutex_value_new",       _mutex_value_new)
    runtime.register_function("mutex_value_lock",      _mutex_value_lock)
    runtime.register_function("mutex_value_unlock",    _mutex_value_unlock)
    runtime.register_function("mutex_value_set",       _mutex_value_set)
    runtime.register_function("mutex_value_try_lock",  _mutex_value_try_lock)

    # RwLock<T> value
    runtime.register_function("rwlock_value_new",          _rwlock_value_new)
    runtime.register_function("rwlock_value_read",         _rwlock_value_read)
    runtime.register_function("rwlock_value_release_read", _rwlock_value_release_read)
    runtime.register_function("rwlock_value_write",        _rwlock_value_write)
