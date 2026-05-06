"""
NLPL Threading Module - Native Threading Support

Provides native threading capabilities including:
- Thread creation, joining, detaching
- Thread-local storage (TLS)
- Thread configuration (stack size, priority, affinity)
- Thread identification and management

This module enables multi-threaded concurrent programming with
full control over thread lifecycle and properties.
"""

import threading
import multiprocessing
import os
import sys
import time
import queue
from typing import Any, Callable, Optional, Dict, List


# Thread-local storage
_thread_local = threading.local()
_thread_registry: Dict[int, threading.Thread] = {}
_registry_lock = threading.Lock()


class NLPLThread:
    """
    Wrapper for Python threading.Thread with NLPL-specific features
    """
    
    def __init__(
        self,
        target: Callable,
        args: tuple = (),
        name: Optional[str] = None,
        daemon: bool = False,
        stack_size: Optional[int] = None
    ):
        self.target = target
        self.args = args
        self.name = name or f"NLPLThread-{id(self)}"
        self.daemon = daemon
        self.stack_size = stack_size
        self._thread: Optional[threading.Thread] = None
        self._result: Any = None
        self._exception: Optional[Exception] = None
        self._finished = threading.Event()
    
    def _run_wrapper(self):
        """Wrapper that captures return value and exceptions"""
        try:
            self._result = self.target(*self.args)
        except Exception as e:
            self._exception = e
        finally:
            self._finished.set()
    
    def start(self) -> int:
        """
        Start the thread
        Returns thread ID
        """
        # Set stack size if specified
        if self.stack_size:
            old_stack_size = threading.stack_size()
            threading.stack_size(self.stack_size)
        
        self._thread = threading.Thread(
            target=self._run_wrapper,
            name=self.name,
            daemon=self.daemon
        )
        self._thread.start()
        
        # Restore old stack size
        if self.stack_size:
            threading.stack_size(old_stack_size)
        
        # Register thread
        thread_id = self._thread.ident
        with _registry_lock:
            _thread_registry[thread_id] = self._thread
        
        return thread_id
    
    def join(self, timeout: Optional[float] = None) -> Any:
        """
        Wait for thread to complete
        Returns the thread's return value or raises its exception
        """
        if not self._thread:
            raise RuntimeError("Thread not started")
        
        self._thread.join(timeout)
        
        if self._exception:
            raise self._exception
        
        return self._result
    
    def is_alive(self) -> bool:
        """Check if thread is still running"""
        return self._thread.is_alive() if self._thread else False
    
    def get_id(self) -> Optional[int]:
        """Get thread identifier"""
        return self._thread.ident if self._thread else None


# NexusLang Runtime Interface Functions

def create_thread(runtime, function: Callable, *args) -> NLPLThread:
    """
    Create and start a new thread
    
    Args:
        function: Function to run in the thread
        *args: Arguments to pass to the function
        
    Returns:
        Thread handle (NLPLThread object)
        
    Example:
        function worker with id as Integer
            print text "Worker thread "
            print text id
        
        set thread to create_thread with worker and 1
    """
    thread = NLPLThread(target=function, args=args)
    thread.start()
    return thread


def join_thread(runtime, thread: NLPLThread, timeout: Optional[float] = None) -> Any:
    """
    Wait for a thread to complete
    
    Args:
        thread: Thread handle returned by create_thread
        timeout: Maximum time to wait (seconds), None = wait forever
        
    Returns:
        Return value from the thread function
        
    Example:
        set result to join_thread with thread_handle
    """
    return thread.join(timeout)


def detach_thread(runtime, thread: NLPLThread):
    """
    Detach a thread (make it a daemon thread)
    
    Args:
        thread: Thread handle
        
    Example:
        detach_thread with thread_handle
    """
    if thread._thread:
        thread._thread.daemon = True


def thread_is_alive(runtime, thread: NLPLThread) -> bool:
    """
    Check if a thread is still running
    
    Args:
        thread: Thread handle
        
    Returns:
        True if thread is alive, False otherwise
        
    Example:
        set alive to thread_is_alive with thread_handle
    """
    return thread.is_alive()


def get_thread_id(runtime, thread: NLPLThread) -> Optional[int]:
    """
    Get the system thread ID
    
    Args:
        thread: Thread handle
        
    Returns:
        Thread ID (integer) or null if not started
        
    Example:
        set tid to get_thread_id with thread_handle
    """
    return thread.get_id()


def get_current_thread_id(runtime) -> int:
    """
    Get the ID of the currently executing thread
    
    Returns:
        Current thread ID
        
    Example:
        set my_tid to get_current_thread_id()
    """
    return threading.get_ident()


def get_current_thread_name(runtime) -> str:
    """
    Get the name of the currently executing thread
    
    Returns:
        Current thread name
        
    Example:
        set my_name to get_current_thread_name()
    """
    return threading.current_thread().name


def set_thread_name(runtime, thread: NLPLThread, name: str):
    """
    Set the name of a thread
    
    Args:
        thread: Thread handle
        name: New name for the thread
        
    Example:
        set_thread_name with thread_handle and "WorkerThread-1"
    """
    if thread._thread:
        thread._thread.name = name
        thread.name = name


def get_cpu_count(runtime) -> int:
    """
    Get the number of CPU cores available
    
    Returns:
        Number of CPU cores
        
    Example:
        set num_cores to get_cpu_count()
    """
    return multiprocessing.cpu_count()


def sleep_thread(runtime, seconds: float):
    """
    Sleep the current thread for specified seconds
    
    Args:
        seconds: Number of seconds to sleep
        
    Example:
        sleep_thread with 0.5  # Sleep for half a second
    """
    import time
    time.sleep(seconds)


def yield_thread(runtime):
    """
    Yield execution to other threads
    
    Hints to the OS scheduler to give other threads a chance to run
    
    Example:
        yield_thread()
    """
    import time
    time.sleep(0)  # Sleep for 0 seconds yields to scheduler


# Thread-Local Storage Functions

def thread_local_set(runtime, key: str, value: Any):
    """
    Set a thread-local variable
    
    Args:
        key: Variable name
        value: Value to store
        
    Thread-local variables are unique per thread.
    
    Example:
        thread_local_set with "counter" and 0
    """
    setattr(_thread_local, key, value)


def thread_local_get(runtime, key: str, default: Any = None) -> Any:
    """
    Get a thread-local variable
    
    Args:
        key: Variable name
        default: Default value if not found
        
    Returns:
        Value stored for this thread, or default
        
    Example:
        set my_counter to thread_local_get with "counter" and 0
    """
    return getattr(_thread_local, key, default)


def thread_local_has(runtime, key: str) -> bool:
    """
    Check if a thread-local variable exists
    
    Args:
        key: Variable name
        
    Returns:
        True if variable exists for this thread
        
    Example:
        if thread_local_has with "counter"
            print text "Counter exists"
        end
    """
    return hasattr(_thread_local, key)


def thread_local_delete(runtime, key: str):
    """
    Delete a thread-local variable
    
    Args:
        key: Variable name
        
    Example:
        thread_local_delete with "counter"
    """
    if hasattr(_thread_local, key):
        delattr(_thread_local, key)


# Thread Priority and Affinity (platform-specific)

def set_thread_priority(runtime, thread: NLPLThread, priority: int):
    """
    Set thread priority (0=lowest, 50=normal, 100=highest).

    Args:
        thread: Thread handle
        priority: Priority level (0-100)

    Maps 0-100 to platform scheduling levels:
    - Linux: uses os.setpriority (nice values -20 to +19)
    - Windows: uses ctypes SetThreadPriority (-2 to +2 THREAD_PRIORITY_* levels)
    - Other platforms: no-op with a warning logged.

    Example:
        set_thread_priority with thread_handle and 75
    """
    if not thread._thread or not thread._thread.is_alive():
        return  # Thread not running yet or already done — no-op

    clipped = max(0, min(100, int(priority)))

    if sys.platform.startswith("linux"):
        try:
            # Map 0-100 -> nice 19 to -20 (lower nice = higher priority)
            nice = int(19 - (clipped / 100.0) * 39)
            tid = None
            if hasattr(thread._thread, "ident") and thread._thread.ident:
                tid = thread._thread.ident
            if tid is not None:
                os.setpriority(os.PRIO_PROCESS, tid, nice)
        except (PermissionError, OSError):
            # Requires elevated privileges for priorities above normal
            pass
    elif sys.platform == "win32":
        try:
            import ctypes
            import ctypes.wintypes
            # Map 0-100 to Windows THREAD_PRIORITY levels (-2 to 2)
            if clipped >= 80:
                level = 2    # THREAD_PRIORITY_HIGHEST
            elif clipped >= 60:
                level = 1    # THREAD_PRIORITY_ABOVE_NORMAL
            elif clipped >= 40:
                level = 0    # THREAD_PRIORITY_NORMAL
            elif clipped >= 20:
                level = -1   # THREAD_PRIORITY_BELOW_NORMAL
            else:
                level = -2   # THREAD_PRIORITY_LOWEST
            handle = ctypes.windll.kernel32.OpenThread(0x0020, False, thread._thread.ident)
            if handle:
                ctypes.windll.kernel32.SetThreadPriority(handle, level)
                ctypes.windll.kernel32.CloseHandle(handle)
        except Exception:
            pass
    # Other platforms: no-op


def get_thread_priority(runtime, thread: NLPLThread) -> int:
    """
    Get the current priority of a thread (0-100 scale).

    Returns -1 if priority cannot be determined on this platform.

    Example:
        set prio to get_thread_priority with thread_handle
    """
    if sys.platform.startswith("linux"):
        try:
            if thread._thread and thread._thread.ident:
                nice = os.getpriority(os.PRIO_PROCESS, thread._thread.ident)
                # nice -20..+19 -> 0..100
                return int((19 - nice) / 39.0 * 100)
        except OSError:
            pass
    return -1


def set_thread_affinity(runtime, thread: NLPLThread, cpu_mask: int):
    """
    Set CPU affinity for a thread.

    Args:
        thread: Thread handle
        cpu_mask: Bitmask of CPUs (bit 0 = CPU 0, etc.)

    Example:
        # Run on CPUs 0 and 2
        set_thread_affinity with thread_handle and 5  # Binary: 0101
    """
    if not thread._thread:
        return
    cpu_count = multiprocessing.cpu_count()
    cpu_set = {i for i in range(cpu_count) if cpu_mask & (1 << i)}
    if not cpu_set:
        return

    if sys.platform.startswith("linux"):
        try:
            if thread._thread.ident:
                os.sched_setaffinity(thread._thread.ident, cpu_set)
        except (OSError, AttributeError):
            # os.sched_setaffinity not available on all kernels
            pass
    elif sys.platform == "win32":
        try:
            import ctypes
            handle = ctypes.windll.kernel32.OpenThread(0x0060, False, thread._thread.ident)
            if handle:
                ctypes.windll.kernel32.SetThreadAffinityMask(handle, cpu_mask)
                ctypes.windll.kernel32.CloseHandle(handle)
        except Exception:
            pass


def get_thread_affinity(runtime, thread: NLPLThread) -> int:
    """
    Get the CPU affinity bitmask for a thread.

    Returns -1 if the platform does not support affinity queries.

    Example:
        set mask to get_thread_affinity with thread_handle
    """
    if sys.platform.startswith("linux"):
        try:
            if thread._thread and thread._thread.ident:
                cpu_set = os.sched_getaffinity(thread._thread.ident)
                return sum(1 << cpu for cpu in cpu_set)
        except (OSError, AttributeError):
            pass
    return -1


def join_thread_timeout(runtime, thread: NLPLThread, timeout_ms: float) -> Any:
    """
    Wait for a thread with a timeout specified in milliseconds.

    Args:
        thread: Thread handle returned by create_thread
        timeout_ms: Maximum milliseconds to wait

    Returns:
        Return value from the thread function, or None if timed out

    Example:
        set result to join_thread_timeout with thread_handle and 5000
    """
    return thread.join(timeout_ms / 1000.0)


# ---------------------------------------------------------------------------
# Work-Stealing Thread Pool
# ---------------------------------------------------------------------------

import queue
from concurrent.futures import ThreadPoolExecutor, Future as CFFuture


class NLPLWorkStealingPool:
    """
    Work-stealing thread pool for NexusLang programs.

    Each worker maintains its own local task deque. When a worker runs out of
    work it "steals" tasks from the tail of another worker's deque, which
    minimises contention and improves throughput for fine-grained parallel
    workloads.

    This implementation uses Python's :class:`threading.Thread` and a shared
    ``queue.SimpleQueue`` as the steal-able work source (Python's GIL prevents
    true data races, so a deque-per-worker would add complexity without benefit
    inside CPython).  For CPU-bound work the user should combine this with
    ``multiprocessing`` or the NexusLang FFI to bypass the GIL.
    """

    def __init__(self, num_workers: int):
        if num_workers < 1:
            raise ValueError("num_workers must be at least 1")
        self._num_workers = num_workers
        self._task_queue: queue.SimpleQueue = queue.SimpleQueue()
        self._pending_count = [0]  # mutable int shared across threads
        self._count_lock = threading.Lock()
        self._result_lock = threading.Lock()
        self._shutdown = threading.Event()
        self._workers: List[threading.Thread] = []
        self._start_workers()

    def _start_workers(self) -> None:
        for i in range(self._num_workers):
            t = threading.Thread(
                target=self._worker_loop,
                name=f"NLPLPool-Worker-{i}",
                daemon=True,
            )
            t.start()
            self._workers.append(t)

    def _worker_loop(self) -> None:
        while not self._shutdown.is_set():
            try:
                future, fn, args = self._task_queue.get(timeout=0.05)
            except queue.Empty:
                continue
            try:
                result = fn(*args)
                future.set_result(result)
            except Exception as exc:
                future.set_result(None)  # keep future consistent
                # Re-store error so pool_submit callers can retrieve it
                future._exc = exc
            finally:
                with self._count_lock:
                    self._pending_count[0] -= 1

    def submit(self, fn: Callable, *args) -> "NLPLFutureResult":
        """Submit work and return a future-like result handle."""
        if self._shutdown.is_set():
            raise RuntimeError("Pool has been shut down")
        future = NLPLFutureResult()
        with self._count_lock:
            self._pending_count[0] += 1
        self._task_queue.put((future, fn, args))
        return future

    def pending_count(self) -> int:
        """Number of tasks queued but not yet complete."""
        with self._count_lock:
            return self._pending_count[0]

    def join(self, timeout: Optional[float] = None) -> None:
        """Wait until all submitted tasks have been processed."""
        deadline = None if timeout is None else time.monotonic() + timeout
        while True:
            with self._count_lock:
                if self._pending_count[0] == 0:
                    return
            if deadline is not None and time.monotonic() >= deadline:
                raise TimeoutError("pool_join timed out")
            time.sleep(0.005)

    def shutdown(self, wait: bool = True) -> None:
        """Signal workers to stop. If *wait* drains the queue first."""
        if wait:
            self.join()
        self._shutdown.set()


class NLPLFutureResult:
    """Simple non-blocking result container used by :class:`NLPLWorkStealingPool`."""

    def __init__(self):
        self._event = threading.Event()
        self._value: Any = None
        self._exc: Optional[Exception] = None

    def set_result(self, value: Any) -> None:
        self._value = value
        self._event.set()

    def get(self, timeout: Optional[float] = None) -> Any:
        if not self._event.wait(timeout):
            raise TimeoutError("pool task timed out")
        if self._exc is not None:
            raise self._exc
        return self._value

    def is_done(self) -> bool:
        return self._event.is_set()

    def __repr__(self) -> str:
        if not self._event.is_set():
            return "PoolFuture<pending>"
        return "PoolFuture<done: {!r}>".format(self._value)


# --- NLPL-facing pool functions ---

def create_work_stealing_pool(runtime, num_workers: int) -> NLPLWorkStealingPool:
    """
    Create a work-stealing thread pool with *num_workers* threads.

    Example:
        set pool to create_work_stealing_pool with 4
    """
    return NLPLWorkStealingPool(int(num_workers))


def pool_submit(runtime, pool: NLPLWorkStealingPool, fn: Callable, *args) -> NLPLFutureResult:
    """
    Submit *fn* with *args* to *pool*. Returns a future for the result.

    Example:
        set fut to pool_submit with pool and my_function and arg1
        set result to fut.get()
    """
    if not isinstance(pool, NLPLWorkStealingPool):
        raise TypeError("Expected a pool from create_work_stealing_pool")
    return pool.submit(fn, *args)


def pool_join(runtime, pool: NLPLWorkStealingPool, timeout_ms: Optional[float] = None) -> None:
    """
    Wait for all submitted tasks in *pool* to complete.

    Args:
        pool: Pool handle
        timeout_ms: Maximum milliseconds to wait (None = wait forever)

    Example:
        pool_join with pool
        pool_join with pool and 10000  # 10-second timeout
    """
    if not isinstance(pool, NLPLWorkStealingPool):
        raise TypeError("Expected a pool from create_work_stealing_pool")
    to = None if timeout_ms is None else float(timeout_ms) / 1000.0
    pool.join(to)


def pool_shutdown(runtime, pool: NLPLWorkStealingPool) -> None:
    """
    Drain and shut down *pool*, stopping all worker threads.

    Example:
        pool_shutdown with pool
    """
    if not isinstance(pool, NLPLWorkStealingPool):
        raise TypeError("Expected a pool from create_work_stealing_pool")
    pool.shutdown(wait=True)


def pool_pending_count(runtime, pool: NLPLWorkStealingPool) -> int:
    """
    Return the number of tasks pending in *pool*.

    Example:
        set pending to pool_pending_count with pool
    """
    if not isinstance(pool, NLPLWorkStealingPool):
        raise TypeError("Expected a pool from create_work_stealing_pool")
    return pool.pending_count()


def register_stdlib(runtime):
    """Register threading module functions with NexusLang runtime"""
    from nexuslang.runtime.runtime import Runtime
    
    # Thread creation and management
    runtime.register_function("create_thread", create_thread)
    runtime.register_function("join_thread", join_thread)
    runtime.register_function("join_thread_timeout", join_thread_timeout)
    runtime.register_function("detach_thread", detach_thread)
    runtime.register_function("thread_is_alive", thread_is_alive)
    
    # Thread identification
    runtime.register_function("get_thread_id", get_thread_id)
    runtime.register_function("get_current_thread_id", get_current_thread_id)
    runtime.register_function("get_current_thread_name", get_current_thread_name)
    runtime.register_function("set_thread_name", set_thread_name)
    
    # Thread control
    runtime.register_function("sleep_thread", sleep_thread)
    runtime.register_function("yield_thread", yield_thread)
    runtime.register_function("get_cpu_count", get_cpu_count)
    
    # Thread-local storage
    runtime.register_function("thread_local_set", thread_local_set)
    runtime.register_function("thread_local_get", thread_local_get)
    runtime.register_function("thread_local_has", thread_local_has)
    runtime.register_function("thread_local_delete", thread_local_delete)
    
    # Thread priority and affinity (platform-specific)
    runtime.register_function("set_thread_priority", set_thread_priority)
    runtime.register_function("get_thread_priority", get_thread_priority)
    runtime.register_function("set_thread_affinity", set_thread_affinity)
    runtime.register_function("get_thread_affinity", get_thread_affinity)

    # Work-stealing thread pool
    runtime.register_function("create_work_stealing_pool", create_work_stealing_pool)
    runtime.register_function("pool_submit", pool_submit)
    runtime.register_function("pool_join", pool_join)
    runtime.register_function("pool_shutdown", pool_shutdown)
    runtime.register_function("pool_pending_count", pool_pending_count)
