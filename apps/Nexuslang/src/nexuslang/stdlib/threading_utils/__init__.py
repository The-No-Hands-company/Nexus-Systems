"""
Threading and Multiprocessing utilities for NexusLang.

Provides:
- Thread creation and management
- Thread pools
- Locks, semaphores, events
- Process creation and management
- Process pools
- Shared memory and queues
"""

import threading
import multiprocessing
from multiprocessing import Queue, Pool, Process
from queue import Queue as ThreadQueue
import time
from typing import Any, Callable, List, Tuple

# Global storage for threads and processes
_threads = {}
_thread_counter = 0
_processes = {}
_process_counter = 0
_locks = {}
_lock_counter = 0
_thread_pools = {}
_process_pools = {}
_pool_counter = 0


def create_thread(func: Callable, args: tuple = (), daemon: bool = False) -> int:
    """Create and start a new thread.
    
    Args:
        func: Function to run in thread
        args: Tuple of arguments to pass to function
        daemon: Whether thread should be daemon
        
    Returns:
        Thread ID
    """
    global _thread_counter
    
    thread = threading.Thread(target=func, args=args, daemon=daemon)
    thread.start()
    
    thread_id = _thread_counter
    _threads[thread_id] = thread
    _thread_counter += 1
    
    return thread_id


def thread_join(thread_id: int, timeout: float = None) -> bool:
    """Wait for thread to complete.
    
    Args:
        thread_id: ID of thread to wait for
        timeout: Maximum time to wait in seconds (None = wait forever)
        
    Returns:
        True if thread completed, False if timeout occurred
    """
    if thread_id not in _threads:
        raise ValueError(f"Thread {thread_id} not found")
    
    thread = _threads[thread_id]
    thread.join(timeout)
    
    return not thread.is_alive()


def thread_is_alive(thread_id: int) -> bool:
    """Check if thread is still running.
    
    Args:
        thread_id: ID of thread to check
        
    Returns:
        True if thread is alive, False otherwise
    """
    if thread_id not in _threads:
        return False
    
    return _threads[thread_id].is_alive()


def thread_count() -> int:
    """Get number of active threads.
    
    Returns:
        Number of alive threads
    """
    return sum(1 for t in _threads.values() if t.is_alive())


def create_lock() -> int:
    """Create a new lock for thread synchronization.
    
    Returns:
        Lock ID
    """
    global _lock_counter
    
    lock = threading.Lock()
    lock_id = _lock_counter
    _locks[lock_id] = lock
    _lock_counter += 1
    
    return lock_id


def lock_acquire(lock_id: int, blocking: bool = True, timeout: float = -1) -> bool:
    """Acquire a lock.
    
    Args:
        lock_id: ID of lock to acquire
        blocking: Whether to block waiting for lock
        timeout: Maximum time to wait (-1 = wait forever)
        
    Returns:
        True if lock acquired, False otherwise
    """
    if lock_id not in _locks:
        raise ValueError(f"Lock {lock_id} not found")
    
    lock = _locks[lock_id]
    
    if timeout == -1:
        return lock.acquire(blocking)
    else:
        return lock.acquire(blocking, timeout)


def lock_release(lock_id: int) -> None:
    """Release a lock.
    
    Args:
        lock_id: ID of lock to release
    """
    if lock_id not in _locks:
        raise ValueError(f"Lock {lock_id} not found")
    
    _locks[lock_id].release()


def create_thread_pool(num_workers: int = None) -> int:
    """Create a thread pool for parallel execution.
    
    Args:
        num_workers: Number of worker threads (None = CPU count)
        
    Returns:
        Pool ID
    """
    global _pool_counter
    
    from concurrent.futures import ThreadPoolExecutor
    
    pool = ThreadPoolExecutor(max_workers=num_workers)
    pool_id = _pool_counter
    _thread_pools[pool_id] = pool
    _pool_counter += 1
    
    return pool_id


def thread_pool_submit(pool_id: int, func: Callable, args: tuple = ()) -> Any:
    """Submit a task to thread pool.
    
    Args:
        pool_id: ID of thread pool
        func: Function to execute
        args: Arguments to pass to function
        
    Returns:
        Future object representing the task
    """
    if pool_id not in _thread_pools:
        raise ValueError(f"Thread pool {pool_id} not found")
    
    pool = _thread_pools[pool_id]
    future = pool.submit(func, *args)
    
    return future


def thread_pool_map(pool_id: int, func: Callable, items: List[Any]) -> List[Any]:
    """Map function over items using thread pool.
    
    Args:
        pool_id: ID of thread pool
        func: Function to apply to each item
        items: List of items to process
        
    Returns:
        List of results
    """
    if pool_id not in _thread_pools:
        raise ValueError(f"Thread pool {pool_id} not found")
    
    pool = _thread_pools[pool_id]
    results = list(pool.map(func, items))
    
    return results


def thread_pool_shutdown(pool_id: int, wait: bool = True) -> None:
    """Shutdown a thread pool.
    
    Args:
        pool_id: ID of thread pool to shutdown
        wait: Whether to wait for pending tasks to complete
    """
    if pool_id not in _thread_pools:
        raise ValueError(f"Thread pool {pool_id} not found")
    
    pool = _thread_pools[pool_id]
    pool.shutdown(wait=wait)
    del _thread_pools[pool_id]


def create_process(func: Callable, args: tuple = (), daemon: bool = False) -> int:
    """Create and start a new process.
    
    Args:
        func: Function to run in process
        args: Tuple of arguments to pass to function
        daemon: Whether process should be daemon
        
    Returns:
        Process ID
    """
    global _process_counter
    
    process = Process(target=func, args=args, daemon=daemon)
    process.start()
    
    process_id = _process_counter
    _processes[process_id] = process
    _process_counter += 1
    
    return process_id


def process_join(process_id: int, timeout: float = None) -> bool:
    """Wait for process to complete.
    
    Args:
        process_id: ID of process to wait for
        timeout: Maximum time to wait in seconds (None = wait forever)
        
    Returns:
        True if process completed, False if timeout occurred
    """
    if process_id not in _processes:
        raise ValueError(f"Process {process_id} not found")
    
    process = _processes[process_id]
    process.join(timeout)
    
    return not process.is_alive()


def process_is_alive(process_id: int) -> bool:
    """Check if process is still running.
    
    Args:
        process_id: ID of process to check
        
    Returns:
        True if process is alive, False otherwise
    """
    if process_id not in _processes:
        return False
    
    return _processes[process_id].is_alive()


def process_terminate(process_id: int) -> None:
    """Terminate a process.
    
    Args:
        process_id: ID of process to terminate
    """
    if process_id not in _processes:
        raise ValueError(f"Process {process_id} not found")
    
    _processes[process_id].terminate()


def create_process_pool(num_workers: int = None) -> int:
    """Create a process pool for parallel execution.
    
    Args:
        num_workers: Number of worker processes (None = CPU count)
        
    Returns:
        Pool ID
    """
    global _pool_counter
    
    pool = Pool(processes=num_workers)
    pool_id = _pool_counter
    _process_pools[pool_id] = pool
    _pool_counter += 1
    
    return pool_id


def process_pool_map(pool_id: int, func: Callable, items: List[Any]) -> List[Any]:
    """Map function over items using process pool.
    
    Args:
        pool_id: ID of process pool
        func: Function to apply to each item
        items: List of items to process
        
    Returns:
        List of results
    """
    if pool_id not in _process_pools:
        raise ValueError(f"Process pool {pool_id} not found")
    
    pool = _process_pools[pool_id]
    results = pool.map(func, items)
    
    return results


def process_pool_close(pool_id: int) -> None:
    """Close a process pool (no more tasks accepted).
    
    Args:
        pool_id: ID of process pool to close
    """
    if pool_id not in _process_pools:
        raise ValueError(f"Process pool {pool_id} not found")
    
    pool = _process_pools[pool_id]
    pool.close()


def process_pool_terminate(pool_id: int) -> None:
    """Terminate a process pool immediately.
    
    Args:
        pool_id: ID of process pool to terminate
    """
    if pool_id not in _process_pools:
        raise ValueError(f"Process pool {pool_id} not found")
    
    pool = _process_pools[pool_id]
    pool.terminate()
    del _process_pools[pool_id]


def cpu_count() -> int:
    """Get number of CPU cores.
    
    Returns:
        Number of CPU cores available
    """
    return multiprocessing.cpu_count()


def current_thread_name() -> str:
    """Get name of current thread.
    
    Returns:
        Name of current thread
    """
    return threading.current_thread().name


def current_thread_id() -> int:
    """Get ID of current thread.
    
    Returns:
        Native thread ID
    """
    return threading.get_ident()


def sleep(seconds: float) -> None:
    """Sleep for specified number of seconds.
    
    Args:
        seconds: Number of seconds to sleep
    """
    time.sleep(seconds)


def register_threading_functions(runtime):
    """Register threading functions with the NexusLang runtime."""
    # Thread management
    runtime.register_function("create_thread", create_thread)
    runtime.register_function("thread_join", thread_join)
    runtime.register_function("thread_is_alive", thread_is_alive)
    runtime.register_function("thread_count", thread_count)
    runtime.register_function("current_thread_name", current_thread_name)
    runtime.register_function("current_thread_id", current_thread_id)
    
    # Thread synchronization
    runtime.register_function("create_lock", create_lock)
    runtime.register_function("lock_acquire", lock_acquire)
    runtime.register_function("lock_release", lock_release)
    
    # Thread pools
    runtime.register_function("create_thread_pool", create_thread_pool)
    runtime.register_function("thread_pool_submit", thread_pool_submit)
    runtime.register_function("thread_pool_map", thread_pool_map)
    runtime.register_function("thread_pool_shutdown", thread_pool_shutdown)
    
    # Process management
    runtime.register_function("create_process", create_process)
    runtime.register_function("process_join", process_join)
    runtime.register_function("process_is_alive", process_is_alive)
    runtime.register_function("process_terminate", process_terminate)
    
    # Process pools
    runtime.register_function("create_process_pool", create_process_pool)
    runtime.register_function("process_pool_map", process_pool_map)
    runtime.register_function("process_pool_close", process_pool_close)
    runtime.register_function("process_pool_terminate", process_pool_terminate)
    
    # System info
    runtime.register_function("cpu_count", cpu_count)
    runtime.register_function("sleep", sleep)
