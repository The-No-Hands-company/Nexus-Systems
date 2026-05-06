"""
Concurrency benchmark suite.

Covers: threading primitives, ThreadPoolExecutor, asyncio tasks and queues.

NOTE: Process-based benchmarks are skipped because spawning subprocesses in a
CI tight-loop creates noise and cross-platform portability issues (Windows
requires `if __name__ == "__main__"` guards).
"""
from __future__ import annotations

import asyncio
import queue
import threading
from concurrent.futures import ThreadPoolExecutor
from typing import List

from benchmarks.benchmark_ci import BenchmarkCase

CATEGORY = "concurrency"


# ---------------------------------------------------------------------------
# Thread primitives
# ---------------------------------------------------------------------------

def _bench_thread_create_join_10() -> None:
    """Spawn 10 threads, each doing minimal work, then join."""
    def noop() -> None:
        pass

    threads = [threading.Thread(target=noop) for _ in range(10)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def _bench_thread_create_join_100() -> None:
    """Spawn 100 threads, each doing minimal work, then join."""
    def noop() -> None:
        pass

    threads = [threading.Thread(target=noop) for _ in range(100)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()


def _bench_lock_acquire_release_1k() -> None:
    """Acquire and release an uncontested lock 1000 times."""
    lock = threading.Lock()
    for _ in range(1_000):
        with lock:
            pass


def _bench_rlock_acquire_release_1k() -> None:
    """Acquire and release an RLock (recursive) 1000 times."""
    lock = threading.RLock()
    for _ in range(1_000):
        with lock:
            pass


def _bench_event_set_wait_1k() -> None:
    """Set and wait on a threading.Event 1000 times."""
    event = threading.Event()
    for _ in range(1_000):
        event.set()
        event.wait()
        event.clear()


def _bench_semaphore_1k() -> None:
    """Acquire and release a Semaphore 1000 times."""
    sem = threading.Semaphore(1)
    for _ in range(1_000):
        sem.acquire()
        sem.release()


def _bench_threading_queue_1k() -> None:
    """Put / get 1000 items through a thread-safe Queue without threads."""
    q: queue.Queue[int] = queue.Queue()
    for i in range(1_000):
        q.put(i)
    for _ in range(1_000):
        q.get_nowait()


# ---------------------------------------------------------------------------
# ThreadPoolExecutor
# ---------------------------------------------------------------------------

def _cpu_task(n: int) -> int:
    total = 0
    for i in range(n):
        total += i
    return total


def _bench_thread_pool_map_50_small() -> None:
    """Submit 50 small tasks to a thread pool and collect results."""
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(_cpu_task, [500] * 50))
    assert len(results) == 50


def _bench_thread_pool_map_20_medium() -> None:
    """Submit 20 medium tasks to a thread pool and collect results."""
    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(_cpu_task, [5_000] * 20))
    assert len(results) == 20


def _bench_thread_pool_submit_100() -> None:
    """Submit 100 futures individually and wait for completion."""
    with ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(_cpu_task, 200) for _ in range(100)]
        results = [f.result() for f in futures]
    assert len(results) == 100


# ---------------------------------------------------------------------------
# asyncio
# ---------------------------------------------------------------------------

async def _async_noop() -> int:
    await asyncio.sleep(0)
    return 0


async def _gather_100() -> None:
    tasks = [asyncio.create_task(_async_noop()) for _ in range(100)]
    await asyncio.gather(*tasks)


async def _gather_1k() -> None:
    tasks = [asyncio.create_task(_async_noop()) for _ in range(1_000)]
    await asyncio.gather(*tasks)


async def _asyncio_queue_roundtrip_1k() -> None:
    q: asyncio.Queue[int] = asyncio.Queue()
    for i in range(1_000):
        await q.put(i)
    for _ in range(1_000):
        await q.get()


def _bench_asyncio_gather_100() -> None:
    asyncio.run(_gather_100())


def _bench_asyncio_gather_1k() -> None:
    asyncio.run(_gather_1k())


def _bench_asyncio_queue_1k() -> None:
    asyncio.run(_asyncio_queue_roundtrip_1k())


# ---------------------------------------------------------------------------
# Async event / condition
# ---------------------------------------------------------------------------

async def _asyncio_event_1k() -> None:
    event = asyncio.Event()
    for _ in range(1_000):
        event.set()
        await asyncio.sleep(0)  # yield
        event.clear()


def _bench_asyncio_event_1k() -> None:
    asyncio.run(_asyncio_event_1k())


# ---------------------------------------------------------------------------
# Producer-consumer pattern
# ---------------------------------------------------------------------------

def _bench_producer_consumer_threading_1k() -> None:
    """Classic producer-consumer with a bounded thread-safe queue."""
    results: List[int] = []
    q: queue.Queue[int] = queue.Queue(maxsize=50)

    def producer() -> None:
        for i in range(1_000):
            q.put(i)
        q.put(-1)  # sentinel

    def consumer() -> None:
        while True:
            item = q.get()
            if item == -1:
                break
            results.append(item)

    prod = threading.Thread(target=producer)
    cons = threading.Thread(target=consumer)
    prod.start()
    cons.start()
    prod.join()
    cons.join()
    assert len(results) == 1_000


# ---------------------------------------------------------------------------
# Case list
# ---------------------------------------------------------------------------

CONCURRENCY_CASES = [
    # Thread primitives
    BenchmarkCase("thread_create_10",       CATEGORY, _bench_thread_create_join_10),
    BenchmarkCase("thread_create_100",      CATEGORY, _bench_thread_create_join_100),
    BenchmarkCase("lock_1k",                CATEGORY, _bench_lock_acquire_release_1k),
    BenchmarkCase("rlock_1k",               CATEGORY, _bench_rlock_acquire_release_1k),
    BenchmarkCase("event_1k",               CATEGORY, _bench_event_set_wait_1k),
    BenchmarkCase("semaphore_1k",           CATEGORY, _bench_semaphore_1k),
    BenchmarkCase("queue_put_get_1k",       CATEGORY, _bench_threading_queue_1k),
    # ThreadPoolExecutor
    BenchmarkCase("pool_map_50_small",      CATEGORY, _bench_thread_pool_map_50_small),
    BenchmarkCase("pool_map_20_medium",     CATEGORY, _bench_thread_pool_map_20_medium),
    BenchmarkCase("pool_submit_100",        CATEGORY, _bench_thread_pool_submit_100),
    # asyncio
    BenchmarkCase("asyncio_gather_100",     CATEGORY, _bench_asyncio_gather_100),
    BenchmarkCase("asyncio_gather_1k",      CATEGORY, _bench_asyncio_gather_1k, bench_iters=10),
    BenchmarkCase("asyncio_queue_1k",       CATEGORY, _bench_asyncio_queue_1k),
    BenchmarkCase("asyncio_event_1k",       CATEGORY, _bench_asyncio_event_1k),
    # Patterns
    BenchmarkCase("producer_consumer_1k",   CATEGORY, _bench_producer_consumer_threading_1k),
]
