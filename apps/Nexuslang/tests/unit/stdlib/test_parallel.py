"""Tests for the parallel computing standard library module."""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))


# ---------------------------------------------------------------------------
# Minimal mock runtime for testing
# ---------------------------------------------------------------------------

class MockRuntime:
    def __init__(self):
        self._funcs = {}

    def register_function(self, name, fn):
        self._funcs[name] = fn

    def call_function(self, fn, args):
        return fn(*args)


# ---------------------------------------------------------------------------
# Import the module under test
# ---------------------------------------------------------------------------

from nexuslang.stdlib.parallel import (
    parallel_map,
    parallel_filter,
    parallel_reduce,
    parallel_for_each,
    parallel_sort,
    parallel_find,
    parallel_count,
    parallel_all,
    parallel_any,
    parallel_batch,
    parallel_flat_map,
    register_parallel_functions,
    _cpu_count,
    _chunk,
    ParallelTaskGraph,
    ParallelTask,
    task_graph_add_task,
    task_graph_execute,
)


# ---------------------------------------------------------------------------
# _chunk helper
# ---------------------------------------------------------------------------

class TestChunk:
    def test_even_split(self):
        result = _chunk([1, 2, 3, 4], 2)
        assert result == [[1, 2], [3, 4]]

    def test_uneven_split(self):
        result = _chunk([1, 2, 3, 4, 5], 3)
        # Should produce chunks of sizes 2, 2, 1
        assert sum(len(c) for c in result) == 5
        assert sorted(x for chunk in result for x in chunk) == [1, 2, 3, 4, 5]

    def test_single_chunk(self):
        result = _chunk([1, 2, 3], 1)
        assert result == [[1, 2, 3]]

    def test_more_chunks_than_items(self):
        result = _chunk([1, 2], 5)
        # Each item in its own chunk; empty chunks dropped
        assert len(result) == 2
        assert sum(len(c) for c in result) == 2

    def test_empty_list(self):
        result = _chunk([], 3)
        assert result == []


# ---------------------------------------------------------------------------
# parallel_map
# ---------------------------------------------------------------------------

class TestParallelMap:
    def test_empty(self):
        rt = MockRuntime()
        assert parallel_map(rt, lambda x: x * 2, []) == []

    def test_double(self):
        rt = MockRuntime()
        result = parallel_map(rt, lambda x: x * 2, [1, 2, 3, 4])
        assert result == [2, 4, 6, 8]

    def test_preserves_order(self):
        rt = MockRuntime()
        items = list(range(100))
        result = parallel_map(rt, lambda x: x, items)
        assert result == items

    def test_single_worker(self):
        rt = MockRuntime()
        result = parallel_map(rt, lambda x: x + 1, [10, 20, 30], num_workers=1)
        assert result == [11, 21, 31]

    def test_string_transform(self):
        rt = MockRuntime()
        result = parallel_map(rt, str.upper, ['a', 'b', 'c'])
        assert result == ['A', 'B', 'C']


# ---------------------------------------------------------------------------
# parallel_filter
# ---------------------------------------------------------------------------

class TestParallelFilter:
    def test_empty(self):
        rt = MockRuntime()
        assert parallel_filter(rt, lambda x: True, []) == []

    def test_even_numbers(self):
        rt = MockRuntime()
        result = parallel_filter(rt, lambda x: x % 2 == 0, [1, 2, 3, 4, 5, 6])
        assert sorted(result) == [2, 4, 6]

    def test_all_pass(self):
        rt = MockRuntime()
        result = parallel_filter(rt, lambda x: True, [1, 2, 3])
        assert sorted(result) == [1, 2, 3]

    def test_none_pass(self):
        rt = MockRuntime()
        result = parallel_filter(rt, lambda x: False, [1, 2, 3])
        assert result == []


# ---------------------------------------------------------------------------
# parallel_reduce
# ---------------------------------------------------------------------------

class TestParallelReduce:
    def test_sum(self):
        rt = MockRuntime()
        result = parallel_reduce(rt, lambda a, b: a + b, [1, 2, 3, 4, 5])
        assert result == 15

    def test_single_item(self):
        rt = MockRuntime()
        result = parallel_reduce(rt, lambda a, b: a + b, [42])
        assert result == 42

    def test_with_initial(self):
        rt = MockRuntime()
        result = parallel_reduce(rt, lambda a, b: a + b, [1, 2, 3], initial=10)
        assert result == 16

    def test_empty_with_initial(self):
        rt = MockRuntime()
        result = parallel_reduce(rt, lambda a, b: a + b, [], initial=99)
        assert result == 99

    def test_product(self):
        rt = MockRuntime()
        result = parallel_reduce(rt, lambda a, b: a * b, [1, 2, 3, 4], num_workers=1)
        assert result == 24


# ---------------------------------------------------------------------------
# parallel_sort
# ---------------------------------------------------------------------------

class TestParallelSort:
    def test_basic_sort(self):
        rt = MockRuntime()
        result = parallel_sort(rt, [3, 1, 4, 1, 5, 9, 2, 6])
        assert result == sorted([3, 1, 4, 1, 5, 9, 2, 6])

    def test_reverse(self):
        rt = MockRuntime()
        result = parallel_sort(rt, [3, 1, 4, 1, 5], reverse=True)
        assert result == [9, 5, 4, 3, 1, 1] or result == sorted([3, 1, 4, 1, 5], reverse=True)

    def test_empty(self):
        rt = MockRuntime()
        assert parallel_sort(rt, []) == []

    def test_single_element(self):
        rt = MockRuntime()
        assert parallel_sort(rt, [7]) == [7]

    def test_already_sorted(self):
        rt = MockRuntime()
        items = [1, 2, 3, 4, 5]
        assert parallel_sort(rt, items) == items

    def test_with_key(self):
        rt = MockRuntime()
        words = ['banana', 'apple', 'cherry', 'date']
        result = parallel_sort(rt, words, key_fn=len)
        # Each word sorted by its length
        assert result[0] == 'date' or len(result[0]) <= len(result[1])


# ---------------------------------------------------------------------------
# parallel_find
# ---------------------------------------------------------------------------

class TestParallelFind:
    def test_find_existing(self):
        rt = MockRuntime()
        result = parallel_find(rt, lambda x: x > 5, [1, 2, 3, 6, 7])
        assert result in (6, 7)

    def test_find_missing(self):
        rt = MockRuntime()
        result = parallel_find(rt, lambda x: x > 100, [1, 2, 3])
        assert result is None

    def test_empty(self):
        rt = MockRuntime()
        assert parallel_find(rt, lambda x: True, []) is None


# ---------------------------------------------------------------------------
# parallel_count
# ---------------------------------------------------------------------------

class TestParallelCount:
    def test_count_evens(self):
        rt = MockRuntime()
        result = parallel_count(rt, lambda x: x % 2 == 0, [1, 2, 3, 4, 5, 6])
        assert result == 3

    def test_count_all(self):
        rt = MockRuntime()
        assert parallel_count(rt, lambda x: True, [1, 2, 3]) == 3

    def test_count_none(self):
        rt = MockRuntime()
        assert parallel_count(rt, lambda x: False, [1, 2, 3]) == 0

    def test_empty(self):
        rt = MockRuntime()
        assert parallel_count(rt, lambda x: True, []) == 0


# ---------------------------------------------------------------------------
# parallel_all / parallel_any
# ---------------------------------------------------------------------------

class TestParallelAllAny:
    def test_all_true(self):
        rt = MockRuntime()
        assert parallel_all(rt, lambda x: x > 0, [1, 2, 3]) is True

    def test_all_false_when_one_fails(self):
        rt = MockRuntime()
        assert parallel_all(rt, lambda x: x > 0, [1, -1, 3]) is False

    def test_all_empty(self):
        rt = MockRuntime()
        assert parallel_all(rt, lambda x: False, []) is True

    def test_any_true(self):
        rt = MockRuntime()
        assert parallel_any(rt, lambda x: x > 5, [1, 2, 6]) is True

    def test_any_false(self):
        rt = MockRuntime()
        assert parallel_any(rt, lambda x: x > 100, [1, 2, 3]) is False

    def test_any_empty(self):
        rt = MockRuntime()
        assert parallel_any(rt, lambda x: True, []) is False


# ---------------------------------------------------------------------------
# parallel_for_each
# ---------------------------------------------------------------------------

class TestParallelForEach:
    def test_side_effects(self):
        rt = MockRuntime()
        results = []
        import threading
        lock = threading.Lock()
        def collect(x):
            with lock:
                results.append(x)
        parallel_for_each(rt, collect, [1, 2, 3, 4])
        assert sorted(results) == [1, 2, 3, 4]

    def test_empty(self):
        rt = MockRuntime()
        # Should not raise
        parallel_for_each(rt, lambda x: x, [])


# ---------------------------------------------------------------------------
# parallel_batch
# ---------------------------------------------------------------------------

class TestParallelBatch:
    def test_batch_map(self):
        rt = MockRuntime()
        result = parallel_batch(rt, lambda b: [x * 2 for x in b], [1, 2, 3, 4, 5, 6], batch_size=2)
        assert sorted(result) == [2, 4, 6, 8, 10, 12]

    def test_empty(self):
        rt = MockRuntime()
        assert parallel_batch(rt, lambda b: b, [], batch_size=10) == []


# ---------------------------------------------------------------------------
# parallel_flat_map
# ---------------------------------------------------------------------------

class TestParallelFlatMap:
    def test_expand(self):
        rt = MockRuntime()
        result = parallel_flat_map(rt, lambda x: [x, x * 2], [1, 2, 3])
        assert sorted(result) == sorted([1, 2, 2, 4, 3, 6])

    def test_empty(self):
        rt = MockRuntime()
        assert parallel_flat_map(rt, lambda x: [x], []) == []


# ---------------------------------------------------------------------------
# Task graph
# ---------------------------------------------------------------------------

class TestParallelTaskGraph:
    def test_independent_tasks(self):
        rt = MockRuntime()
        graph = ParallelTaskGraph()
        task_graph_add_task(rt, graph, 'a', lambda: 1, [])
        task_graph_add_task(rt, graph, 'b', lambda: 2, [])
        results = task_graph_execute(rt, graph)
        assert results.get('a') == 1
        assert results.get('b') == 2

    def test_dependent_tasks(self):
        rt = MockRuntime()
        graph = ParallelTaskGraph()
        t1 = task_graph_add_task(rt, graph, 'first', lambda: 10, [])
        t2 = ParallelTask(lambda: 20, [], 'second')
        t2.add_dependency(t1)
        graph.add_task(t2)
        results = task_graph_execute(rt, graph)
        assert results.get('first') == 10
        assert results.get('second') == 20


# ---------------------------------------------------------------------------
# register_parallel_functions
# ---------------------------------------------------------------------------

class TestRegisterParallelFunctions:
    def test_registration_succeeds(self):
        rt = MockRuntime()
        register_parallel_functions(rt)
        expected = [
            'parallel_map', 'parallel_filter', 'parallel_reduce', 'parallel_for_each',
            'parallel_sort', 'parallel_find', 'parallel_count', 'parallel_all',
            'parallel_any', 'parallel_batch', 'parallel_flat_map',
            'create_task_graph', 'task_graph_add_task', 'task_graph_add_dependency',
            'task_graph_execute', 'parallel_cpu_count', 'parallel_optimal_workers',
        ]
        for name in expected:
            assert name in rt._funcs, f"Missing: {name}"

    def test_cpu_count(self):
        rt = MockRuntime()
        register_parallel_functions(rt)
        count = rt._funcs['parallel_cpu_count']()
        assert isinstance(count, int)
        assert count >= 1
