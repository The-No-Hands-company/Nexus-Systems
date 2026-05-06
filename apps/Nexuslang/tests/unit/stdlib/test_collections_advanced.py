"""
Advanced collection types: BTreeMap, BTreeSet, LinkedList, VecDeque, heaps.
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

class TestBTreeMap:
    def _get(self):
        from nexuslang.stdlib.collections import BTreeMap
        return BTreeMap()

    def test_insert_and_get(self):
        m = self._get()
        m.insert("a", 1)
        assert m.get("a") == 1

    def test_contains_key_true(self):
        m = self._get()
        m.insert("x", 42)
        assert m.contains_key("x")

    def test_contains_key_false(self):
        m = self._get()
        assert not m.contains_key("missing")

    def test_len(self):
        m = self._get()
        m.insert("a", 1)
        m.insert("b", 2)
        assert m.len() == 2

    def test_keys(self):
        m = self._get()
        m.insert("b", 2)
        m.insert("a", 1)
        keys = list(m.keys())
        assert "a" in keys and "b" in keys

    def test_remove(self):
        m = self._get()
        m.insert("k", 99)
        m.remove("k")
        assert not m.contains_key("k")

    def test_get_missing_returns_none(self):
        m = self._get()
        assert m.get("nope") is None

    def test_sorted_keys_order(self):
        m = self._get()
        for k in ["c", "a", "b"]:
            m.insert(k, ord(k))
        keys = list(m.keys())
        assert keys == sorted(keys)

class TestBTreeSet:
    def _get(self):
        from nexuslang.stdlib.collections import BTreeSet
        return BTreeSet()

    def test_insert_and_contains(self):
        s = self._get()
        s.insert(10)
        assert s.contains(10)

    def test_not_contains(self):
        s = self._get()
        assert not s.contains(99)

    def test_remove(self):
        s = self._get()
        s.insert(5)
        s.remove(5)
        assert not s.contains(5)

    def test_to_list(self):
        s = self._get()
        s.insert(3)
        s.insert(1)
        s.insert(2)
        lst = s.to_list()
        assert sorted(lst) == [1, 2, 3]

    def test_len(self):
        s = self._get()
        s.insert(7)
        s.insert(8)
        assert s.len() == 2

    def test_sorted_order(self):
        s = self._get()
        for v in [5, 3, 4, 1, 2]:
            s.insert(v)
        assert s.to_list() == [1, 2, 3, 4, 5]


# ============================================================
# Section 7 - Stdlib: LinkedList, VecDeque, MinHeap, MaxHeap
# ============================================================

class TestLinkedList:
    def _get(self):
        from nexuslang.stdlib.collections import LinkedList
        return LinkedList()

    def test_push_and_len(self):
        ll = self._get()
        ll.push_back(1)
        ll.push_back(2)
        assert ll.len() == 2

    def test_pop_front(self):
        ll = self._get()
        ll.push_back(10)
        ll.push_back(20)
        val = ll.pop_front()
        assert val == 10

    def test_push_front(self):
        ll = self._get()
        ll.push_back(2)
        ll.push_front(1)
        assert ll.pop_front() == 1

    def test_to_list(self):
        ll = self._get()
        for v in [1, 2, 3]:
            ll.push_back(v)
        assert ll.to_list() == [1, 2, 3]

    def test_empty_len(self):
        ll = self._get()
        assert ll.len() == 0

class TestVecDeque:
    def _get(self):
        from nexuslang.stdlib.collections import VecDeque
        return VecDeque()

    def test_push_back_pop_front(self):
        vd = self._get()
        vd.push_back(1)
        vd.push_back(2)
        assert vd.pop_front() == 1

    def test_push_front_pop_back(self):
        vd = self._get()
        vd.push_front(1)
        vd.push_front(2)
        assert vd.pop_back() == 1

    def test_len(self):
        vd = self._get()
        vd.push_back(5)
        assert vd.len() == 1

    def test_to_list(self):
        vd = self._get()
        for v in [1, 2, 3]:
            vd.push_back(v)
        assert vd.to_list() == [1, 2, 3]

class TestMinHeap:
    def _get(self):
        from nexuslang.stdlib.collections import MinHeap
        return MinHeap()

    def test_push_and_pop(self):
        h = self._get()
        h.push(5)
        h.push(1)
        h.push(3)
        assert h.pop() == 1

    def test_peek(self):
        h = self._get()
        h.push(10)
        h.push(4)
        assert h.peek() == 4

    def test_len(self):
        h = self._get()
        h.push(1)
        h.push(2)
        assert h.len() == 2

    def test_min_order(self):
        h = self._get()
        for v in [9, 3, 7, 1, 5]:
            h.push(v)
        results = [h.pop() for _ in range(5)]
        assert results == [1, 3, 5, 7, 9]

class TestMaxHeap:
    def _get(self):
        from nexuslang.stdlib.collections import MaxHeap
        return MaxHeap()

    def test_push_and_pop(self):
        h = self._get()
        h.push(5)
        h.push(1)
        h.push(3)
        assert h.pop() == 5

    def test_peek(self):
        h = self._get()
        h.push(2)
        h.push(8)
        assert h.peek() == 8

    def test_max_order(self):
        h = self._get()
        for v in [3, 1, 4, 1, 5, 9]:
            h.push(v)
        assert h.pop() == 9


# ============================================================
# Section 8 - Stdlib: Graph Algorithms
# ============================================================

