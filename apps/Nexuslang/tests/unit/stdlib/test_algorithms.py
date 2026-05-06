"""
Graph and string algorithm tests.
Split from test_session_features.py.
"""

import sys
import os
import tempfile
import pytest
from pathlib import Path

_SRC = str(Path(__file__).resolve().parent.parent.parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

class TestGraphAlgorithms:
    def test_dfs_import(self):
        from nexuslang.stdlib.algorithms import algo_dfs
        assert callable(algo_dfs)

    def test_bfs_import(self):
        from nexuslang.stdlib.algorithms import algo_bfs
        assert callable(algo_bfs)

    def test_dijkstra_import(self):
        from nexuslang.stdlib.algorithms import algo_dijkstra
        assert callable(algo_dijkstra)

    def test_topological_sort_import(self):
        from nexuslang.stdlib.algorithms import algo_topological_sort
        assert callable(algo_topological_sort)

    def test_dfs_visits_all(self):
        from nexuslang.stdlib.algorithms import algo_dfs
        graph = {0: [1, 2], 1: [3], 2: [3], 3: []}
        visited = algo_dfs(graph, 0)
        assert set(visited) == {0, 1, 2, 3}

    def test_bfs_visits_all(self):
        from nexuslang.stdlib.algorithms import algo_bfs
        graph = {0: [1, 2], 1: [3], 2: [3], 3: []}
        visited = algo_bfs(graph, 0)
        assert set(visited) == {0, 1, 2, 3}

    def test_dijkstra_shortest_path(self):
        from nexuslang.stdlib.algorithms import algo_dijkstra
        # algo_dijkstra expects {node: {neighbour: weight}} format
        graph = {0: {1: 1, 2: 4}, 1: {2: 2, 3: 5}, 2: {3: 1}, 3: {}}
        dist = algo_dijkstra(graph, 0)
        assert dist[3] == 4

    def test_topological_sort_dag(self):
        from nexuslang.stdlib.algorithms import algo_topological_sort
        graph = {0: [1, 2], 1: [3], 2: [3], 3: []}
        order = algo_topological_sort(graph)
        assert order.index(0) < order.index(3)


# ============================================================
# Section 9 - Stdlib: String Algorithms
# ============================================================

class TestStringAlgorithms:
    def test_kmp_search_import(self):
        from nexuslang.stdlib.algorithms import algo_kmp_search
        assert callable(algo_kmp_search)

    def test_rabin_karp_import(self):
        from nexuslang.stdlib.algorithms import algo_rabin_karp
        assert callable(algo_rabin_karp)

    def test_kmp_found(self):
        from nexuslang.stdlib.algorithms import algo_kmp_search
        positions = algo_kmp_search("hello world", "world")
        assert 6 in positions

    def test_kmp_not_found(self):
        from nexuslang.stdlib.algorithms import algo_kmp_search
        positions = algo_kmp_search("hello world", "xyz")
        assert len(positions) == 0

    def test_kmp_multiple_occurrences(self):
        from nexuslang.stdlib.algorithms import algo_kmp_search
        positions = algo_kmp_search("ababab", "ab")
        assert len(positions) == 3

    def test_rabin_karp_found(self):
        from nexuslang.stdlib.algorithms import algo_rabin_karp
        positions = algo_rabin_karp("hello world", "world")
        assert 6 in positions

    def test_rabin_karp_not_found(self):
        from nexuslang.stdlib.algorithms import algo_rabin_karp
        positions = algo_rabin_karp("hello world", "xyz")
        assert len(positions) == 0


# ============================================================
# Section 10 - Stdlib: Buffered I/O
# ============================================================

