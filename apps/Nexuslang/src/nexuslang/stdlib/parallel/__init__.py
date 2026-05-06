"""
Parallel computing standard library for NexusLang.

Provides parallel variants of common collection operations using thread and
process pools, plus task-graph scheduling and basic SIMD-style batch processing.
"""
from __future__ import annotations

import os
import math
import heapq
import functools
import itertools
import concurrent.futures as _cf
from typing import Any, Callable, List, Optional, Tuple


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _cpu_count() -> int:
    return os.cpu_count() or 1


def _chunk(seq: list, n: int) -> List[list]:
    """Split seq into n roughly equal chunks."""
    k, m = divmod(len(seq), n)
    chunks = []
    start = 0
    for i in range(n):
        end = start + k + (1 if i < m else 0)
        chunks.append(seq[start:end])
        start = end
    return [c for c in chunks if c]


# ---------------------------------------------------------------------------
# Parallel map / filter / reduce
# ---------------------------------------------------------------------------

def parallel_map(runtime: Any, fn: Any, collection: list,
                 num_workers: Optional[int] = None) -> list:
    """Apply fn to every item in collection concurrently.

    Returns a list of results in the same order as the input.
    """
    if not collection:
        return []
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    chunks = _chunk(list(collection), workers)

    def map_chunk(chunk: list) -> list:
        return [_call(runtime, fn, item) for item in chunk]

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(map_chunk, c) for c in chunks]
        results: list = []
        for f in futures:
            results.extend(f.result())
    return results


def parallel_filter(runtime: Any, fn: Any, collection: list,
                    num_workers: Optional[int] = None) -> list:
    """Keep only items for which fn returns a truthy value, evaluated in parallel."""
    if not collection:
        return []
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    chunks = _chunk(list(collection), workers)

    def filter_chunk(chunk: list) -> list:
        return [item for item in chunk if _call(runtime, fn, item)]

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(filter_chunk, c) for c in chunks]
        results: list = []
        for f in futures:
            results.extend(f.result())
    return results


def parallel_reduce(runtime: Any, fn: Any, collection: list,
                    initial: Any = None,
                    num_workers: Optional[int] = None) -> Any:
    """Reduce collection using fn in a parallel tree-reduction pattern.

    The reduction is performed in two phases:
      1. Each worker reduces its own chunk sequentially.
      2. The per-chunk results are reduced sequentially on the calling thread.

    This preserves associativity but not commutativity; callers whose fn is
    not commutative should set num_workers=1.
    """
    items = list(collection)
    if not items:
        return initial
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(items))
    chunks = _chunk(items, workers)

    def reduce_chunk(chunk: list) -> Any:
        acc = chunk[0]
        for item in chunk[1:]:
            acc = _call(runtime, fn, acc, item)
        return acc

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(reduce_chunk, c) for c in chunks]
        partials = [f.result() for f in futures]

    if initial is not None:
        partials.insert(0, initial)

    acc = partials[0]
    for p in partials[1:]:
        acc = _call(runtime, fn, acc, p)
    return acc


def parallel_for_each(runtime: Any, fn: Any, collection: list,
                      num_workers: Optional[int] = None) -> None:
    """Call fn(item) for every item in collection concurrently, discarding results."""
    if not collection:
        return
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    items = list(collection)

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(_call, runtime, fn, item) for item in items]
        for f in futures:
            f.result()  # propagate exceptions


# ---------------------------------------------------------------------------
# Parallel sort
# ---------------------------------------------------------------------------

def parallel_sort(runtime: Any, collection: list,
                  key_fn: Optional[Any] = None,
                  reverse: bool = False,
                  num_workers: Optional[int] = None) -> list:
    """Sort a collection in parallel using a merge-sort style approach.

    Chunks are sorted concurrently; the sorted chunks are merged on one thread.
    """
    items = list(collection)
    if len(items) <= 1:
        return items
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(items))
    chunks = _chunk(items, workers)

    if key_fn is not None:
        def sort_chunk(chunk: list) -> list:
            return sorted(chunk,
                          key=lambda x: _call(runtime, key_fn, x),
                          reverse=bool(reverse))
    else:
        def sort_chunk(chunk: list) -> list:
            return sorted(chunk, reverse=bool(reverse))

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(sort_chunk, c) for c in chunks]
        sorted_chunks = [f.result() for f in futures]

    # k-way merge
    if key_fn is not None:
        merged = list(heapq.merge(*sorted_chunks,
                                  key=lambda x: _call(runtime, key_fn, x),
                                  reverse=bool(reverse)))
    else:
        merged = list(heapq.merge(*sorted_chunks, reverse=bool(reverse)))
    return merged


# ---------------------------------------------------------------------------
# Parallel search / aggregation
# ---------------------------------------------------------------------------

def parallel_find(runtime: Any, fn: Any, collection: list,
                  num_workers: Optional[int] = None) -> Any:
    """Return the first item for which fn returns truthy, or None."""
    if not collection:
        return None
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    items = list(collection)
    stop_flag = [False]
    result_holder: list = [None]

    def search_chunk(chunk: list) -> Any:
        for item in chunk:
            if stop_flag[0]:
                return None
            if _call(runtime, fn, item):
                stop_flag[0] = True
                result_holder[0] = item
                return item
        return None

    chunks = _chunk(items, workers)
    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(search_chunk, c) for c in chunks]
        for f in futures:
            v = f.result()
            if v is not None and result_holder[0] is None:
                result_holder[0] = v

    return result_holder[0]


def parallel_count(runtime: Any, fn: Any, collection: list,
                   num_workers: Optional[int] = None) -> int:
    """Count items for which fn returns truthy, evaluated in parallel."""
    if not collection:
        return 0
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    chunks = _chunk(list(collection), workers)

    def count_chunk(chunk: list) -> int:
        return sum(1 for item in chunk if _call(runtime, fn, item))

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(count_chunk, c) for c in chunks]
        return sum(f.result() for f in futures)


def parallel_all(runtime: Any, fn: Any, collection: list,
                 num_workers: Optional[int] = None) -> bool:
    """Return True iff fn returns truthy for all items (short-circuits)."""
    if not collection:
        return True
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    chunks = _chunk(list(collection), workers)
    fail_flag = [False]

    def check_chunk(chunk: list) -> bool:
        for item in chunk:
            if fail_flag[0]:
                return False
            if not _call(runtime, fn, item):
                fail_flag[0] = True
                return False
        return True

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(check_chunk, c) for c in chunks]
        return all(f.result() for f in futures)


def parallel_any(runtime: Any, fn: Any, collection: list,
                 num_workers: Optional[int] = None) -> bool:
    """Return True iff fn returns truthy for at least one item (short-circuits)."""
    if not collection:
        return False
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    chunks = _chunk(list(collection), workers)
    found_flag = [False]

    def check_chunk(chunk: list) -> bool:
        for item in chunk:
            if found_flag[0]:
                return True
            if _call(runtime, fn, item):
                found_flag[0] = True
                return True
        return False

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(check_chunk, c) for c in chunks]
        return any(f.result() for f in futures)


# ---------------------------------------------------------------------------
# Parallel batch processing
# ---------------------------------------------------------------------------

def parallel_batch(runtime: Any, fn: Any, collection: list,
                   batch_size: int = 64,
                   num_workers: Optional[int] = None) -> list:
    """Apply fn(batch: list) to non-overlapping batches of collection in parallel.

    fn receives a list (the batch) and should return a list of results.
    The results from all batches are concatenated in order.
    """
    items = list(collection)
    if not items:
        return []
    size = max(1, int(batch_size))
    batches = [items[i:i + size] for i in range(0, len(items), size)]
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(batches))

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(_call, runtime, fn, b) for b in batches]
        results: list = []
        for f in futures:
            r = f.result()
            if isinstance(r, list):
                results.extend(r)
            else:
                results.append(r)
    return results


# ---------------------------------------------------------------------------
# Parallel zip / flat_map
# ---------------------------------------------------------------------------

def parallel_flat_map(runtime: Any, fn: Any, collection: list,
                      num_workers: Optional[int] = None) -> list:
    """Map fn over collection in parallel; fn must return a list.  Results are flattened."""
    if not collection:
        return []
    workers = int(num_workers) if num_workers else min(_cpu_count(), len(collection))
    items = list(collection)

    with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [pool.submit(_call, runtime, fn, item) for item in items]
        results: list = []
        for f in futures:
            r = f.result()
            if isinstance(r, list):
                results.extend(r)
            else:
                results.append(r)
    return results


# ---------------------------------------------------------------------------
# Task graph (DAG) scheduler
# ---------------------------------------------------------------------------

class ParallelTask:
    """A node in a parallel task graph."""

    def __init__(self, fn: Any, args: list, task_id: str):
        self.fn = fn
        self.args = args
        self.task_id = task_id
        self.deps: List["ParallelTask"] = []
        self.result: Any = None
        self._done = False

    def add_dependency(self, task: "ParallelTask") -> None:
        self.deps.append(task)


class ParallelTaskGraph:
    """Simple DAG task scheduler: tasks run as soon as all dependencies complete."""

    def __init__(self) -> None:
        self._tasks: List[ParallelTask] = []

    def add_task(self, task: ParallelTask) -> ParallelTask:
        self._tasks.append(task)
        return task

    def execute(self, runtime: Any, num_workers: Optional[int] = None) -> dict:
        workers = int(num_workers) if num_workers else _cpu_count()
        remaining = list(self._tasks)
        completed: dict = {}

        with _cf.ThreadPoolExecutor(max_workers=workers) as pool:
            in_flight: dict = {}

            def try_submit() -> None:
                for task in list(remaining):
                    if task.task_id in in_flight:
                        continue
                    if all(d.task_id in completed for d in task.deps):
                        remaining.remove(task)
                        f = pool.submit(_call, runtime, task.fn, *task.args)
                        in_flight[task.task_id] = (f, task)

            while remaining or in_flight:
                try_submit()
                if not in_flight:
                    break
                done, _ = _cf.wait(list(in_flight[k][0] for k in in_flight),
                                   return_when=_cf.FIRST_COMPLETED)
                for f in done:
                    for tid, (ft, task) in list(in_flight.items()):
                        if ft is f:
                            task.result = f.result()
                            task._done = True
                            completed[tid] = task.result
                            del in_flight[tid]
                            break
        return completed


# ---------------------------------------------------------------------------
# Utilities exposed to NexusLang
# ---------------------------------------------------------------------------

def create_task_graph(_runtime: Any) -> ParallelTaskGraph:
    return ParallelTaskGraph()


def task_graph_add_task(runtime: Any, graph: ParallelTaskGraph,
                        task_id: str, fn: Any, args: list) -> ParallelTask:
    task = ParallelTask(fn, list(args), str(task_id))
    return graph.add_task(task)


def task_graph_add_dependency(_runtime: Any, task: ParallelTask,
                               dep: ParallelTask) -> None:
    task.add_dependency(dep)


def task_graph_execute(runtime: Any, graph: ParallelTaskGraph,
                       num_workers: Optional[int] = None) -> dict:
    return graph.execute(runtime, num_workers)


# ---------------------------------------------------------------------------
# Internal call helper – invokes NexusLang callables or Python callables
# ---------------------------------------------------------------------------

def _call(runtime: Any, fn: Any, *args: Any) -> Any:
    """Call fn, which may be an NexusLang callable or a plain Python callable."""
    if callable(fn):
        return fn(*args)
    # NexusLang function object: has 'call' or '__call__' via runtime
    if hasattr(runtime, "call_function"):
        return runtime.call_function(fn, list(args))
    raise TypeError(f"parallel: not a callable: {type(fn)}")


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_parallel_functions(runtime: Any) -> None:
    """Register all parallel computing functions with the NexusLang runtime."""

    rt = runtime

    runtime.register_function("parallel_map",
        lambda *a: parallel_map(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_filter",
        lambda *a: parallel_filter(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_reduce",
        lambda *a: parallel_reduce(rt, a[0], a[1],
                                   a[2] if len(a) > 2 else None,
                                   a[3] if len(a) > 3 else None))
    runtime.register_function("parallel_for_each",
        lambda *a: parallel_for_each(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_sort",
        lambda *a: parallel_sort(rt, a[0],
                                 a[1] if len(a) > 1 else None,
                                 a[2] if len(a) > 2 else False,
                                 a[3] if len(a) > 3 else None))
    runtime.register_function("parallel_find",
        lambda *a: parallel_find(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_count",
        lambda *a: parallel_count(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_all",
        lambda *a: parallel_all(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_any",
        lambda *a: parallel_any(rt, a[0], a[1], a[2] if len(a) > 2 else None))
    runtime.register_function("parallel_batch",
        lambda *a: parallel_batch(rt, a[0], a[1],
                                  a[2] if len(a) > 2 else 64,
                                  a[3] if len(a) > 3 else None))
    runtime.register_function("parallel_flat_map",
        lambda *a: parallel_flat_map(rt, a[0], a[1], a[2] if len(a) > 2 else None))

    # Task graph
    runtime.register_function("create_task_graph",
        lambda *a: create_task_graph(rt))
    runtime.register_function("task_graph_add_task",
        lambda *a: task_graph_add_task(rt, a[0], a[1], a[2], list(a[3]) if len(a) > 3 else []))
    runtime.register_function("task_graph_add_dependency",
        lambda *a: task_graph_add_dependency(rt, a[0], a[1]))
    runtime.register_function("task_graph_execute",
        lambda *a: task_graph_execute(rt, a[0], a[1] if len(a) > 1 else None))

    # Utility: number of logical CPUs visible to this process
    runtime.register_function("parallel_cpu_count",
        lambda *_a: _cpu_count())
    runtime.register_function("parallel_optimal_workers",
        lambda *a: min(_cpu_count(), int(a[0])) if a else _cpu_count())
