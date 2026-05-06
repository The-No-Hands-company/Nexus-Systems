"""
Algorithm benchmark suite.

Covers: sorting, searching, graph traversal, dynamic programming.
"""
from __future__ import annotations

import heapq
import random
from collections import deque
from typing import Any, Dict, List, Optional, Tuple

from benchmarks.benchmark_ci import BenchmarkCase

CATEGORY = "algorithms"


# ---------------------------------------------------------------------------
# Sorting
# ---------------------------------------------------------------------------

def _make_list(n: int, seed: int = 42) -> List[int]:
    rng = random.Random(seed)
    return [rng.randint(0, n * 10) for _ in range(n)]


def _bench_builtin_sort_1k() -> None:
    lst = _make_list(1_000)
    lst.sort()


def _bench_builtin_sort_10k() -> None:
    lst = _make_list(10_000)
    lst.sort()


def _bench_builtin_sort_100k() -> None:
    lst = _make_list(100_000)
    lst.sort()


def _bench_sorted_1k() -> None:
    lst = _make_list(1_000)
    sorted(lst)


def _bench_sorted_10k() -> None:
    lst = _make_list(10_000)
    sorted(lst)


def _bench_heapsort_1k() -> None:
    lst = _make_list(1_000)
    heapq.nsmallest(len(lst), lst)


def _bench_mergesort_1k() -> None:
    """Python-level merge sort (pure, not Timsort)."""
    def merge_sort(arr: List[int]) -> List[int]:
        if len(arr) <= 1:
            return arr
        mid = len(arr) // 2
        left = merge_sort(arr[:mid])
        right = merge_sort(arr[mid:])
        result: List[int] = []
        i = j = 0
        while i < len(left) and j < len(right):
            if left[i] <= right[j]:
                result.append(left[i])
                i += 1
            else:
                result.append(right[j])
                j += 1
        result.extend(left[i:])
        result.extend(right[j:])
        return result

    merge_sort(_make_list(1_000))


def _bench_quicksort_1k() -> None:
    """Iterative quicksort."""
    def quicksort(arr: List[int]) -> None:
        stack = [(0, len(arr) - 1)]
        while stack:
            lo, hi = stack.pop()
            if lo >= hi:
                continue
            pivot = arr[hi]
            i = lo - 1
            for j in range(lo, hi):
                if arr[j] <= pivot:
                    i += 1
                    arr[i], arr[j] = arr[j], arr[i]
            arr[i + 1], arr[hi] = arr[hi], arr[i + 1]
            p = i + 1
            stack.append((lo, p - 1))
            stack.append((p + 1, hi))

    quicksort(_make_list(1_000))


# ---------------------------------------------------------------------------
# Searching
# ---------------------------------------------------------------------------

_SORTED_1K = sorted(_make_list(1_000))
_SORTED_100K = sorted(_make_list(100_000))


def _bench_linear_search_1k() -> None:
    target = _SORTED_1K[500]
    for x in _SORTED_1K:
        if x == target:
            break


def _bench_binary_search_1k() -> None:
    import bisect
    target = _SORTED_1K[500]
    bisect.bisect_left(_SORTED_1K, target)


def _bench_binary_search_100k() -> None:
    import bisect
    target = _SORTED_100K[50_000]
    bisect.bisect_left(_SORTED_100K, target)


def _bench_dict_lookup_10k() -> None:
    d = {str(i): i for i in range(10_000)}
    for i in range(0, 10_000, 100):
        _ = d[str(i)]


# ---------------------------------------------------------------------------
# Graph traversal
# ---------------------------------------------------------------------------

def _make_graph(n: int, edges_per_node: int = 4, seed: int = 42) -> Dict[int, List[int]]:
    rng = random.Random(seed)
    g: Dict[int, List[int]] = {i: [] for i in range(n)}
    for i in range(n):
        for _ in range(edges_per_node):
            j = rng.randint(0, n - 1)
            if j != i:
                g[i].append(j)
    return g


_GRAPH_500 = _make_graph(500)
_GRAPH_2K = _make_graph(2_000)


def _bench_bfs_500() -> None:
    g = _GRAPH_500
    visited = set()
    queue = deque([0])
    while queue:
        node = queue.popleft()
        if node in visited:
            continue
        visited.add(node)
        for nb in g[node]:
            if nb not in visited:
                queue.append(nb)


def _bench_dfs_500() -> None:
    g = _GRAPH_500
    visited = set()
    stack = [0]
    while stack:
        node = stack.pop()
        if node in visited:
            continue
        visited.add(node)
        for nb in g[node]:
            if nb not in visited:
                stack.append(nb)


def _bench_dijkstra_500() -> None:
    """Dijkstra shortest paths from node 0."""
    g = {i: [(nb, 1) for nb in nbs] for i, nbs in _GRAPH_500.items()}
    dist = {i: float("inf") for i in g}
    dist[0] = 0
    heap = [(0, 0)]
    while heap:
        d, u = heapq.heappop(heap)
        if d > dist[u]:
            continue
        for v, w in g[u]:
            nd = d + w
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(heap, (nd, v))


def _bench_bfs_2k() -> None:
    g = _GRAPH_2K
    visited = set()
    queue = deque([0])
    while queue:
        node = queue.popleft()
        if node in visited:
            continue
        visited.add(node)
        for nb in g[node]:
            if nb not in visited:
                queue.append(nb)


# ---------------------------------------------------------------------------
# Dynamic programming
# ---------------------------------------------------------------------------

def _bench_fibonacci_dp_50() -> None:
    n = 50
    dp = [0] * (n + 1)
    dp[1] = 1
    for i in range(2, n + 1):
        dp[i] = dp[i - 1] + dp[i - 2]


def _bench_knapsack_100() -> None:
    n = 100
    capacity = 500
    rng = random.Random(7)
    weights = [rng.randint(1, 50) for _ in range(n)]
    values = [rng.randint(1, 100) for _ in range(n)]
    dp = [0] * (capacity + 1)
    for i in range(n):
        for w in range(capacity, weights[i] - 1, -1):
            dp[w] = max(dp[w], dp[w - weights[i]] + values[i])


def _bench_lcs_100() -> None:
    """Longest common subsequence of two 100-char strings."""
    rng = random.Random(13)
    chars = "abcdefghijklmnopqrstuvwxyz"
    a = [rng.choice(chars) for _ in range(100)]
    b = [rng.choice(chars) for _ in range(100)]
    m, n = len(a), len(b)
    dp = [[0] * (n + 1) for _ in range(m + 1)]
    for i in range(1, m + 1):
        for j in range(1, n + 1):
            if a[i - 1] == b[j - 1]:
                dp[i][j] = dp[i - 1][j - 1] + 1
            else:
                dp[i][j] = max(dp[i - 1][j], dp[i][j - 1])


# ---------------------------------------------------------------------------
# Case list
# ---------------------------------------------------------------------------

ALGORITHM_CASES: List[BenchmarkCase] = [
    # Sorting
    BenchmarkCase("sort_builtin_1k",   CATEGORY, _bench_builtin_sort_1k),
    BenchmarkCase("sort_builtin_10k",  CATEGORY, _bench_builtin_sort_10k),
    BenchmarkCase("sort_builtin_100k", CATEGORY, _bench_builtin_sort_100k, bench_iters=10),
    BenchmarkCase("sorted_1k",         CATEGORY, _bench_sorted_1k),
    BenchmarkCase("sorted_10k",        CATEGORY, _bench_sorted_10k),
    BenchmarkCase("heapsort_1k",       CATEGORY, _bench_heapsort_1k),
    BenchmarkCase("mergesort_1k",      CATEGORY, _bench_mergesort_1k),
    BenchmarkCase("quicksort_1k",      CATEGORY, _bench_quicksort_1k),
    # Searching
    BenchmarkCase("linear_search_1k",   CATEGORY, _bench_linear_search_1k),
    BenchmarkCase("binary_search_1k",   CATEGORY, _bench_binary_search_1k),
    BenchmarkCase("binary_search_100k", CATEGORY, _bench_binary_search_100k),
    BenchmarkCase("dict_lookup_10k",    CATEGORY, _bench_dict_lookup_10k),
    # Graph
    BenchmarkCase("bfs_500",     CATEGORY, _bench_bfs_500),
    BenchmarkCase("dfs_500",     CATEGORY, _bench_dfs_500),
    BenchmarkCase("dijkstra_500", CATEGORY, _bench_dijkstra_500),
    BenchmarkCase("bfs_2k",      CATEGORY, _bench_bfs_2k),
    # Dynamic programming
    BenchmarkCase("fibonacci_dp_50", CATEGORY, _bench_fibonacci_dp_50),
    BenchmarkCase("knapsack_100",    CATEGORY, _bench_knapsack_100),
    BenchmarkCase("lcs_100",         CATEGORY, _bench_lcs_100),
]
