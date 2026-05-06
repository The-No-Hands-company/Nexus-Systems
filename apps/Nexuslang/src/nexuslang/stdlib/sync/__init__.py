"""
NLPL Synchronization Primitives Module

Provides thread synchronization mechanisms:
- Mutexes (mutual exclusion locks)
- Condition variables (wait/notify)
- Read-Write locks (multiple readers, single writer)
- Semaphores (counting semaphores)
- Barriers (thread synchronization points)
- Once (one-time initialization)

These primitives enable safe concurrent access to shared resources.
"""

import threading
from typing import Optional, Any


# Registry for synchronization objects
_sync_objects = {}
_sync_id_counter = 0
_sync_lock = threading.Lock()


def _register_sync_object(obj: Any) -> int:
    """Register a synchronization object and return its ID"""
    global _sync_id_counter
    with _sync_lock:
        obj_id = _sync_id_counter
        _sync_id_counter += 1
        _sync_objects[obj_id] = obj
        return obj_id


def _get_sync_object(obj_id: int) -> Any:
    """Get a synchronization object by ID"""
    with _sync_lock:
        return _sync_objects.get(obj_id)


# ============================================================================
# MUTEX (Mutual Exclusion Lock)
# ============================================================================

def create_mutex(runtime) -> int:
    """
    Create a new mutex
    
    Returns:
        Mutex handle (integer ID)
        
    Example:
        set my_mutex to create_mutex()
    """
    mutex = threading.Lock()
    return _register_sync_object(mutex)


def lock_mutex(runtime, mutex_id: int):
    """
    Acquire a mutex lock (blocks until available)
    
    Args:
        mutex_id: Mutex handle
        
    Example:
        lock_mutex with my_mutex
    """
    mutex = _get_sync_object(mutex_id)
    if not mutex:
        raise RuntimeError(f"Invalid mutex ID: {mutex_id}")
    mutex.acquire()


def unlock_mutex(runtime, mutex_id: int):
    """
    Release a mutex lock
    
    Args:
        mutex_id: Mutex handle
        
    Example:
        unlock_mutex with my_mutex
    """
    mutex = _get_sync_object(mutex_id)
    if not mutex:
        raise RuntimeError(f"Invalid mutex ID: {mutex_id}")
    mutex.release()


def try_lock_mutex(runtime, mutex_id: int) -> bool:
    """
    Try to acquire mutex without blocking
    
    Args:
        mutex_id: Mutex handle
        
    Returns:
        True if lock acquired, False otherwise
        
    Example:
        set acquired to try_lock_mutex with my_mutex
        if acquired
            # Critical section
            unlock_mutex with my_mutex
        end
    """
    mutex = _get_sync_object(mutex_id)
    if not mutex:
        raise RuntimeError(f"Invalid mutex ID: {mutex_id}")
    return mutex.acquire(blocking=False)


# ============================================================================
# CONDITION VARIABLE
# ============================================================================

def create_condition_variable(runtime, mutex_id: Optional[int] = None) -> int:
    """
    Create a new condition variable
    
    Args:
        mutex_id: Optional mutex to associate with (creates new if None)
        
    Returns:
        Condition variable handle
        
    Example:
        set my_cond to create_condition_variable()
    """
    if mutex_id is not None:
        mutex = _get_sync_object(mutex_id)
        if not mutex:
            raise RuntimeError(f"Invalid mutex ID: {mutex_id}")
        cond = threading.Condition(mutex)
    else:
        cond = threading.Condition()
    
    return _register_sync_object(cond)


def wait_on_condition(runtime, cond_id: int, timeout: Optional[float] = None) -> bool:
    """
    Wait on a condition variable
    
    Args:
        cond_id: Condition variable handle
        timeout: Optional timeout in seconds
        
    Returns:
        True if notified, False if timeout
        
    Note: Must be called while holding the associated mutex
    
    Example:
        lock_mutex with my_mutex
        wait_on_condition with my_cond
        unlock_mutex with my_mutex
    """
    cond = _get_sync_object(cond_id)
    if not cond:
        raise RuntimeError(f"Invalid condition variable ID: {cond_id}")
    return cond.wait(timeout)


def notify_one(runtime, cond_id: int):
    """
    Wake up one thread waiting on condition variable
    
    Args:
        cond_id: Condition variable handle
        
    Example:
        notify_one with my_cond
    """
    cond = _get_sync_object(cond_id)
    if not cond:
        raise RuntimeError(f"Invalid condition variable ID: {cond_id}")
    cond.notify()


def notify_all(runtime, cond_id: int):
    """
    Wake up all threads waiting on condition variable
    
    Args:
        cond_id: Condition variable handle
        
    Example:
        notify_all with my_cond
    """
    cond = _get_sync_object(cond_id)
    if not cond:
        raise RuntimeError(f"Invalid condition variable ID: {cond_id}")
    cond.notify_all()


# ============================================================================
# READ-WRITE LOCK
# ============================================================================

class RWLock:
    """Read-Write lock: multiple readers OR single writer"""
    
    def __init__(self):
        self._readers = 0
        self._writers = 0
        self._read_ready = threading.Condition(threading.Lock())
        self._write_ready = threading.Condition(threading.Lock())
    
    def acquire_read(self):
        """Acquire read lock"""
        self._read_ready.acquire()
        while self._writers > 0:
            self._read_ready.wait()
        self._readers += 1
        self._read_ready.release()
    
    def release_read(self):
        """Release read lock"""
        self._read_ready.acquire()
        self._readers -= 1
        if self._readers == 0:
            self._read_ready.notify_all()
        self._read_ready.release()
    
    def acquire_write(self):
        """Acquire write lock"""
        self._write_ready.acquire()
        while self._writers > 0:
            self._write_ready.wait()
        self._writers += 1
        self._write_ready.release()
        
        self._read_ready.acquire()
        while self._readers > 0:
            self._read_ready.wait()
        self._read_ready.release()
    
    def release_write(self):
        """Release write lock"""
        self._write_ready.acquire()
        self._writers -= 1
        self._write_ready.notify_all()
        self._write_ready.release()
        
        self._read_ready.acquire()
        self._read_ready.notify_all()
        self._read_ready.release()


def create_rwlock(runtime) -> int:
    """
    Create a read-write lock
    
    Returns:
        RWLock handle
        
    Example:
        set my_rwlock to create_rwlock()
    """
    rwlock = RWLock()
    return _register_sync_object(rwlock)


def read_lock(runtime, rwlock_id: int):
    """
    Acquire read lock (allows multiple readers)
    
    Args:
        rwlock_id: RWLock handle
        
    Example:
        read_lock with my_rwlock
        # Read shared data
        read_unlock with my_rwlock
    """
    rwlock = _get_sync_object(rwlock_id)
    if not rwlock:
        raise RuntimeError(f"Invalid rwlock ID: {rwlock_id}")
    rwlock.acquire_read()


def read_unlock(runtime, rwlock_id: int):
    """
    Release read lock
    
    Args:
        rwlock_id: RWLock handle
        
    Example:
        read_unlock with my_rwlock
    """
    rwlock = _get_sync_object(rwlock_id)
    if not rwlock:
        raise RuntimeError(f"Invalid rwlock ID: {rwlock_id}")
    rwlock.release_read()


def write_lock(runtime, rwlock_id: int):
    """
    Acquire write lock (exclusive access)
    
    Args:
        rwlock_id: RWLock handle
        
    Example:
        write_lock with my_rwlock
        # Modify shared data
        write_unlock with my_rwlock
    """
    rwlock = _get_sync_object(rwlock_id)
    if not rwlock:
        raise RuntimeError(f"Invalid rwlock ID: {rwlock_id}")
    rwlock.acquire_write()


def write_unlock(runtime, rwlock_id: int):
    """
    Release write lock
    
    Args:
        rwlock_id: RWLock handle
        
    Example:
        write_unlock with my_rwlock
    """
    rwlock = _get_sync_object(rwlock_id)
    if not rwlock:
        raise RuntimeError(f"Invalid rwlock ID: {rwlock_id}")
    rwlock.release_write()


# ============================================================================
# SEMAPHORE (Counting Semaphore)
# ============================================================================

def create_semaphore(runtime, initial_value: int = 1) -> int:
    """
    Create a counting semaphore
    
    Args:
        initial_value: Initial semaphore count (default 1)
        
    Returns:
        Semaphore handle
        
    Example:
        set my_sem to create_semaphore with 3  # Allow 3 concurrent accesses
    """
    semaphore = threading.Semaphore(initial_value)
    return _register_sync_object(semaphore)


def wait_semaphore(runtime, sem_id: int, timeout: Optional[float] = None) -> bool:
    """
    Wait on semaphore (decrement count, block if zero)
    
    Args:
        sem_id: Semaphore handle
        timeout: Optional timeout in seconds
        
    Returns:
        True if acquired, False if timeout
        
    Example:
        wait_semaphore with my_sem
        # Access limited resource
        post_semaphore with my_sem
    """
    sem = _get_sync_object(sem_id)
    if not sem:
        raise RuntimeError(f"Invalid semaphore ID: {sem_id}")
    return sem.acquire(blocking=True, timeout=timeout)


def post_semaphore(runtime, sem_id: int):
    """
    Signal semaphore (increment count)
    
    Args:
        sem_id: Semaphore handle
        
    Example:
        post_semaphore with my_sem
    """
    sem = _get_sync_object(sem_id)
    if not sem:
        raise RuntimeError(f"Invalid semaphore ID: {sem_id}")
    sem.release()


def try_wait_semaphore(runtime, sem_id: int) -> bool:
    """
    Try to acquire semaphore without blocking
    
    Args:
        sem_id: Semaphore handle
        
    Returns:
        True if acquired, False otherwise
        
    Example:
        set acquired to try_wait_semaphore with my_sem
    """
    sem = _get_sync_object(sem_id)
    if not sem:
        raise RuntimeError(f"Invalid semaphore ID: {sem_id}")
    return sem.acquire(blocking=False)


# ============================================================================
# BARRIER (Thread Synchronization Point)
# ============================================================================

def create_barrier(runtime, num_threads: int) -> int:
    """
    Create a barrier for thread synchronization
    
    Args:
        num_threads: Number of threads that must reach barrier
        
    Returns:
        Barrier handle
        
    Example:
        set my_barrier to create_barrier with 4  # Wait for 4 threads
    """
    barrier = threading.Barrier(num_threads)
    return _register_sync_object(barrier)


def wait_barrier(runtime, barrier_id: int, timeout: Optional[float] = None) -> int:
    """
    Wait at barrier until all threads arrive
    
    Args:
        barrier_id: Barrier handle
        timeout: Optional timeout in seconds
        
    Returns:
        Thread index (0 to n-1)
        
    Example:
        set index to wait_barrier with my_barrier
        print text "All threads synchronized!"
    """
    barrier = _get_sync_object(barrier_id)
    if not barrier:
        raise RuntimeError(f"Invalid barrier ID: {barrier_id}")
    return barrier.wait(timeout)


def reset_barrier(runtime, barrier_id: int):
    """
    Reset barrier to initial state
    
    Args:
        barrier_id: Barrier handle
        
    Example:
        reset_barrier with my_barrier
    """
    barrier = _get_sync_object(barrier_id)
    if not barrier:
        raise RuntimeError(f"Invalid barrier ID: {barrier_id}")
    barrier.reset()


def abort_barrier(runtime, barrier_id: int):
    """
    Abort barrier, waking all waiting threads with error
    
    Args:
        barrier_id: Barrier handle
        
    Example:
        abort_barrier with my_barrier
    """
    barrier = _get_sync_object(barrier_id)
    if not barrier:
        raise RuntimeError(f"Invalid barrier ID: {barrier_id}")
    barrier.abort()


# ============================================================================
# ONCE (One-Time Initialization)
# ============================================================================

class Once:
    """One-time initialization primitive"""
    
    def __init__(self):
        self._lock = threading.Lock()
        self._done = False
    
    def call_once(self, func, *args):
        """Call function exactly once, even from multiple threads"""
        if self._done:
            return
        
        with self._lock:
            if not self._done:
                func(*args)
                self._done = True


def create_once(runtime) -> int:
    """
    Create a one-time initialization object
    
    Returns:
        Once handle
        
    Example:
        set my_once to create_once()
    """
    once = Once()
    return _register_sync_object(once)


def call_once(runtime, once_id: int, func, *args):
    """
    Execute function exactly once across all threads
    
    Args:
        once_id: Once handle
        func: Function to execute
        *args: Arguments to function
        
    Example:
        function initialize
            print text "Initialized!"
        
        call_once with my_once and initialize
    """
    once = _get_sync_object(once_id)
    if not once:
        raise RuntimeError(f"Invalid once ID: {once_id}")
    once.call_once(func, *args)


# ============================================================================
# REENTRANT LOCK (Recursive Mutex)
# ============================================================================

def create_reentrant_lock(runtime) -> int:
    """
    Create a reentrant lock (can be locked multiple times by same thread)
    
    Returns:
        Reentrant lock handle
        
    Example:
        set my_rlock to create_reentrant_lock()
    """
    rlock = threading.RLock()
    return _register_sync_object(rlock)


def lock_reentrant(runtime, rlock_id: int):
    """
    Acquire reentrant lock
    
    Args:
        rlock_id: Reentrant lock handle
        
    Example:
        lock_reentrant with my_rlock
    """
    rlock = _get_sync_object(rlock_id)
    if not rlock:
        raise RuntimeError(f"Invalid reentrant lock ID: {rlock_id}")
    rlock.acquire()


def unlock_reentrant(runtime, rlock_id: int):
    """
    Release reentrant lock
    
    Args:
        rlock_id: Reentrant lock handle
        
    Example:
        unlock_reentrant with my_rlock
    """
    rlock = _get_sync_object(rlock_id)
    if not rlock:
        raise RuntimeError(f"Invalid reentrant lock ID: {rlock_id}")
    rlock.release()


def register_stdlib(runtime):
    """Register synchronization primitives with NexusLang runtime"""
    from nexuslang.runtime.runtime import Runtime
    
    # Mutex operations
    runtime.register_function("create_mutex", create_mutex)
    runtime.register_function("lock_mutex", lock_mutex)
    runtime.register_function("unlock_mutex", unlock_mutex)
    runtime.register_function("try_lock_mutex", try_lock_mutex)
    
    # Condition variable operations
    runtime.register_function("create_condition_variable", create_condition_variable)
    runtime.register_function("wait_on_condition", wait_on_condition)
    runtime.register_function("notify_one", notify_one)
    runtime.register_function("notify_all", notify_all)
    
    # Read-write lock operations
    runtime.register_function("create_rwlock", create_rwlock)
    runtime.register_function("read_lock", read_lock)
    runtime.register_function("read_unlock", read_unlock)
    runtime.register_function("write_lock", write_lock)
    runtime.register_function("write_unlock", write_unlock)
    
    # Semaphore operations
    runtime.register_function("create_semaphore", create_semaphore)
    runtime.register_function("wait_semaphore", wait_semaphore)
    runtime.register_function("post_semaphore", post_semaphore)
    runtime.register_function("try_wait_semaphore", try_wait_semaphore)
    
    # Barrier operations
    runtime.register_function("create_barrier", create_barrier)
    runtime.register_function("wait_barrier", wait_barrier)
    runtime.register_function("reset_barrier", reset_barrier)
    runtime.register_function("abort_barrier", abort_barrier)
    
    # Once operations
    runtime.register_function("create_once", create_once)
    runtime.register_function("call_once", call_once)
    
    # Reentrant lock operations
    runtime.register_function("create_reentrant_lock", create_reentrant_lock)
    runtime.register_function("lock_reentrant", lock_reentrant)
    runtime.register_function("unlock_reentrant", unlock_reentrant)
