"""
Memory and collection benchmark suite.

Covers: allocation patterns, comprehensions, copying, large collections.
"""
from __future__ import annotations

import copy
import random
from typing import List

from benchmarks.benchmark_ci import BenchmarkCase

CATEGORY = "memory"


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

_SORTED_10K   = list(range(10_000))
_RANDOM_10K   = random.sample(range(100_000), 10_000)
_RANDOM_100K  = random.sample(range(1_000_000), 100_000)
_DICT_10K     = {str(i): i for i in range(10_000)}
_LIST_1K_OBJS = [{"id": i, "v": i * 2, "s": str(i)} for i in range(1_000)]


# ---------------------------------------------------------------------------
# List allocation
# ---------------------------------------------------------------------------

def _bench_list_alloc_10k() -> None:
    x = [0] * 10_000
    assert len(x) == 10_000


def _bench_list_alloc_100k() -> None:
    x = [0] * 100_000
    assert len(x) == 100_000


def _bench_list_alloc_1m() -> None:
    x = [0] * 1_000_000
    assert len(x) == 1_000_000


def _bench_list_append_10k() -> None:
    x = []
    for i in range(10_000):
        x.append(i)


def _bench_list_extend_10k() -> None:
    chunk = list(range(100))
    x = []
    for _ in range(100):
        x.extend(chunk)


def _bench_list_comprehension_10k() -> None:
    x = [i * 2 for i in range(10_000)]
    assert len(x) == 10_000


def _bench_list_comprehension_100k() -> None:
    x = [i * 2 for i in range(100_000)]
    assert len(x) == 100_000


# ---------------------------------------------------------------------------
# Generator vs list
# ---------------------------------------------------------------------------

def _bench_generator_sum_100k() -> None:
    total = sum(i * 2 for i in range(100_000))
    assert total >= 0


def _bench_list_sum_100k() -> None:
    data = [i * 2 for i in range(100_000)]
    total = sum(data)
    assert total >= 0


# ---------------------------------------------------------------------------
# Dict allocation and access
# ---------------------------------------------------------------------------

def _bench_dict_alloc_10k() -> None:
    d = {str(i): i for i in range(10_000)}
    assert len(d) == 10_000


def _bench_dict_comprehension_10k() -> None:
    d = {k: v * 2 for k, v in _DICT_10K.items()}
    assert len(d) == 10_000


def _bench_dict_merge_1k() -> None:
    a = {str(i): i for i in range(500)}
    b = {str(i): i for i in range(500, 1_000)}
    merged = {**a, **b}
    assert len(merged) == 1_000


def _bench_dict_update_1k() -> None:
    d = {}
    for i in range(1_000):
        d[str(i)] = i


# ---------------------------------------------------------------------------
# Set operations
# ---------------------------------------------------------------------------

def _bench_set_alloc_10k() -> None:
    s = set(range(10_000))
    assert len(s) == 10_000


def _bench_set_intersection_10k() -> None:
    a = set(range(10_000))
    b = set(range(5_000, 15_000))
    result = a & b
    assert len(result) == 5_000


def _bench_set_union_10k() -> None:
    a = set(range(10_000))
    b = set(range(5_000, 15_000))
    result = a | b
    assert len(result) == 15_000


def _bench_set_difference_10k() -> None:
    a = set(range(10_000))
    b = set(range(5_000, 15_000))
    result = a - b
    assert len(result) == 5_000


# ---------------------------------------------------------------------------
# Copy semantics
# ---------------------------------------------------------------------------

def _bench_list_copy_10k() -> None:
    copy.copy(_RANDOM_10K)


def _bench_list_deepcopy_1k() -> None:
    copy.deepcopy(_LIST_1K_OBJS)


def _bench_dict_copy_10k() -> None:
    _DICT_10K.copy()


def _bench_dict_deepcopy_1k() -> None:
    data = {str(i): {"v": i, "sub": [i, i + 1]} for i in range(1_000)}
    copy.deepcopy(data)


# ---------------------------------------------------------------------------
# Tuple vs list
# ---------------------------------------------------------------------------

def _bench_tuple_alloc_10k() -> None:
    t = tuple(range(10_000))
    assert len(t) == 10_000


def _bench_list_sort_in_place_10k() -> None:
    data = _RANDOM_10K.copy()
    data.sort()


def _bench_sorted_copy_10k() -> None:
    sorted(_RANDOM_10K)


# ---------------------------------------------------------------------------
# Object instantiation
# ---------------------------------------------------------------------------

class _SimpleObj:
    __slots__ = ("x", "y", "z")

    def __init__(self, x: int, y: int, z: float) -> None:
        self.x = x
        self.y = y
        self.z = z


def _bench_object_alloc_10k() -> None:
    objs = [_SimpleObj(i, i + 1, float(i)) for i in range(10_000)]
    assert len(objs) == 10_000


# ---------------------------------------------------------------------------
# Memory churn (allocation + GC pressure)
# ---------------------------------------------------------------------------

def _bench_temporary_allocations_1k() -> None:
    total = 0
    for _ in range(1_000):
        buf = list(range(100))
        total += sum(buf)
    assert total >= 0


# ---------------------------------------------------------------------------
# Case list
# ---------------------------------------------------------------------------

MEMORY_CASES = [
    # List allocation
    BenchmarkCase("list_alloc_10k",         CATEGORY, _bench_list_alloc_10k),
    BenchmarkCase("list_alloc_100k",        CATEGORY, _bench_list_alloc_100k),
    BenchmarkCase("list_alloc_1m",          CATEGORY, _bench_list_alloc_1m),
    BenchmarkCase("list_append_10k",        CATEGORY, _bench_list_append_10k),
    BenchmarkCase("list_extend_10k",        CATEGORY, _bench_list_extend_10k),
    BenchmarkCase("list_comp_10k",          CATEGORY, _bench_list_comprehension_10k),
    BenchmarkCase("list_comp_100k",         CATEGORY, _bench_list_comprehension_100k),
    # Generator vs list
    BenchmarkCase("generator_sum_100k",     CATEGORY, _bench_generator_sum_100k),
    BenchmarkCase("list_sum_100k",          CATEGORY, _bench_list_sum_100k),
    # Dict
    BenchmarkCase("dict_alloc_10k",         CATEGORY, _bench_dict_alloc_10k),
    BenchmarkCase("dict_comprehension_10k", CATEGORY, _bench_dict_comprehension_10k),
    BenchmarkCase("dict_merge_1k",          CATEGORY, _bench_dict_merge_1k),
    BenchmarkCase("dict_update_1k",         CATEGORY, _bench_dict_update_1k),
    # Set
    BenchmarkCase("set_alloc_10k",          CATEGORY, _bench_set_alloc_10k),
    BenchmarkCase("set_intersection_10k",   CATEGORY, _bench_set_intersection_10k),
    BenchmarkCase("set_union_10k",          CATEGORY, _bench_set_union_10k),
    BenchmarkCase("set_difference_10k",     CATEGORY, _bench_set_difference_10k),
    # Copy
    BenchmarkCase("list_copy_10k",          CATEGORY, _bench_list_copy_10k),
    BenchmarkCase("list_deepcopy_1k",       CATEGORY, _bench_list_deepcopy_1k),
    BenchmarkCase("dict_copy_10k",          CATEGORY, _bench_dict_copy_10k),
    BenchmarkCase("dict_deepcopy_1k",       CATEGORY, _bench_dict_deepcopy_1k),
    # Tuple / sort
    BenchmarkCase("tuple_alloc_10k",        CATEGORY, _bench_tuple_alloc_10k),
    BenchmarkCase("list_sort_inplace_10k",  CATEGORY, _bench_list_sort_in_place_10k),
    BenchmarkCase("sorted_copy_10k",        CATEGORY, _bench_sorted_copy_10k),
    # Object
    BenchmarkCase("object_alloc_10k",       CATEGORY, _bench_object_alloc_10k),
    BenchmarkCase("temporary_alloc_1k",     CATEGORY, _bench_temporary_allocations_1k),
]
