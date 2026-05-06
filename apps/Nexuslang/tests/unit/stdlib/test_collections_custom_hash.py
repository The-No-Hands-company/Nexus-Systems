"""Tests for CustomHashMap — HashMap with user-supplied hash/equality functions."""

import pytest
from nexuslang.stdlib.collections import (
    CustomHashMap,
    custom_hash_map_create,
    custom_hash_map_set,
    custom_hash_map_get,
    custom_hash_map_has,
    custom_hash_map_remove,
    custom_hash_map_keys,
    custom_hash_map_values,
    custom_hash_map_items,
    custom_hash_map_size,
    custom_hash_map_clear,
)


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def default_map():
    """New empty CustomHashMap with default hash/eq."""
    return CustomHashMap()


@pytest.fixture
def case_insensitive_map():
    """CustomHashMap where string keys are treated case-insensitively."""
    return CustomHashMap(hash_fn=lambda k: hash(k.lower()), eq_fn=lambda a, b: a.lower() == b.lower())


# ---------------------------------------------------------------------------
# Construction
# ---------------------------------------------------------------------------

class TestConstruction:
    def test_default_construction(self):
        m = CustomHashMap()
        assert m.size() == 0

    def test_custom_hash_fn(self):
        m = CustomHashMap(hash_fn=lambda k: hash(k.lower()))
        assert m.size() == 0

    def test_custom_eq_fn(self):
        m = CustomHashMap(eq_fn=lambda a, b: a.lower() == b.lower())
        assert m.size() == 0

    def test_both_custom_fns(self):
        m = CustomHashMap(
            hash_fn=lambda k: hash(k.lower()),
            eq_fn=lambda a, b: a.lower() == b.lower(),
        )
        assert m.size() == 0

    def test_create_helper(self):
        m = custom_hash_map_create()
        assert isinstance(m, CustomHashMap)
        assert m.size() == 0

    def test_create_helper_with_fns(self):
        m = custom_hash_map_create(hash_fn=lambda k: hash(k % 10))
        assert isinstance(m, CustomHashMap)


# ---------------------------------------------------------------------------
# Basic set / get / has
# ---------------------------------------------------------------------------

class TestBasicOperations:
    def test_set_and_get(self, default_map):
        assert custom_hash_map_set(default_map, "a", 1) is True
        assert custom_hash_map_get(default_map, "a") == 1

    def test_get_missing_returns_none(self, default_map):
        assert custom_hash_map_get(default_map, "missing") is None

    def test_get_missing_returns_default(self, default_map):
        assert custom_hash_map_get(default_map, "missing", 99) == 99

    def test_has_present(self, default_map):
        custom_hash_map_set(default_map, "k", "v")
        assert custom_hash_map_has(default_map, "k") is True

    def test_has_absent(self, default_map):
        assert custom_hash_map_has(default_map, "nope") is False

    def test_set_returns_false_on_update(self, default_map):
        custom_hash_map_set(default_map, "x", 10)
        result = custom_hash_map_set(default_map, "x", 20)
        assert result is False
        assert custom_hash_map_get(default_map, "x") == 20

    def test_size_increments_on_new_key(self, default_map):
        custom_hash_map_set(default_map, 1, "a")
        custom_hash_map_set(default_map, 2, "b")
        assert custom_hash_map_size(default_map) == 2

    def test_size_stable_on_update(self, default_map):
        custom_hash_map_set(default_map, "k", 1)
        custom_hash_map_set(default_map, "k", 2)
        assert custom_hash_map_size(default_map) == 1


# ---------------------------------------------------------------------------
# Case-insensitive string keys
# ---------------------------------------------------------------------------

class TestCaseInsensitiveKeys:
    def test_lookup_with_different_case(self, case_insensitive_map):
        custom_hash_map_set(case_insensitive_map, "Hello", 1)
        assert custom_hash_map_get(case_insensitive_map, "hello") == 1
        assert custom_hash_map_get(case_insensitive_map, "HELLO") == 1

    def test_update_with_different_case(self, case_insensitive_map):
        custom_hash_map_set(case_insensitive_map, "Key", 10)
        custom_hash_map_set(case_insensitive_map, "KEY", 20)
        assert custom_hash_map_size(case_insensitive_map) == 1
        assert custom_hash_map_get(case_insensitive_map, "key") == 20

    def test_has_with_different_case(self, case_insensitive_map):
        custom_hash_map_set(case_insensitive_map, "FOO", "bar")
        assert custom_hash_map_has(case_insensitive_map, "foo") is True

    def test_remove_with_different_case(self, case_insensitive_map):
        custom_hash_map_set(case_insensitive_map, "Apple", 1)
        result = custom_hash_map_remove(case_insensitive_map, "APPLE")
        assert result is True
        assert custom_hash_map_has(case_insensitive_map, "apple") is False


# ---------------------------------------------------------------------------
# Custom hash causing deliberate collisions
# ---------------------------------------------------------------------------

class TestCollisionHandling:
    def test_colliding_keys_stored_separately(self):
        # hash_fn always returns 0 -> every key collides
        m = CustomHashMap(hash_fn=lambda k: 0)
        custom_hash_map_set(m, "a", 1)
        custom_hash_map_set(m, "b", 2)
        custom_hash_map_set(m, "c", 3)
        assert custom_hash_map_size(m) == 3
        assert custom_hash_map_get(m, "a") == 1
        assert custom_hash_map_get(m, "b") == 2
        assert custom_hash_map_get(m, "c") == 3

    def test_update_in_collision_bucket(self):
        m = CustomHashMap(hash_fn=lambda k: 0)
        custom_hash_map_set(m, "a", 1)
        custom_hash_map_set(m, "b", 2)
        custom_hash_map_set(m, "a", 99)  # update "a"
        assert custom_hash_map_size(m) == 2
        assert custom_hash_map_get(m, "a") == 99

    def test_remove_from_collision_bucket(self):
        m = CustomHashMap(hash_fn=lambda k: 0)
        custom_hash_map_set(m, "x", 10)
        custom_hash_map_set(m, "y", 20)
        custom_hash_map_remove(m, "x")
        assert custom_hash_map_has(m, "x") is False
        assert custom_hash_map_has(m, "y") is True


# ---------------------------------------------------------------------------
# Remove
# ---------------------------------------------------------------------------

class TestRemove:
    def test_remove_present_key(self, default_map):
        custom_hash_map_set(default_map, "rm", 7)
        assert custom_hash_map_remove(default_map, "rm") is True
        assert custom_hash_map_has(default_map, "rm") is False
        assert custom_hash_map_size(default_map) == 0

    def test_remove_absent_key(self, default_map):
        assert custom_hash_map_remove(default_map, "ghost") is False

    def test_size_decrements_on_remove(self, default_map):
        custom_hash_map_set(default_map, 1, "a")
        custom_hash_map_set(default_map, 2, "b")
        custom_hash_map_remove(default_map, 1)
        assert custom_hash_map_size(default_map) == 1


# ---------------------------------------------------------------------------
# Keys / values / items
# ---------------------------------------------------------------------------

class TestIterators:
    def test_keys(self, default_map):
        custom_hash_map_set(default_map, "p", 1)
        custom_hash_map_set(default_map, "q", 2)
        assert sorted(custom_hash_map_keys(default_map)) == ["p", "q"]

    def test_values(self, default_map):
        custom_hash_map_set(default_map, "p", 10)
        custom_hash_map_set(default_map, "q", 20)
        assert sorted(custom_hash_map_values(default_map)) == [10, 20]

    def test_items(self, default_map):
        custom_hash_map_set(default_map, "a", 1)
        custom_hash_map_set(default_map, "b", 2)
        items = custom_hash_map_items(default_map)
        assert sorted(items) == [("a", 1), ("b", 2)]

    def test_empty_keys_values_items(self, default_map):
        assert custom_hash_map_keys(default_map) == []
        assert custom_hash_map_values(default_map) == []
        assert custom_hash_map_items(default_map) == []


# ---------------------------------------------------------------------------
# Clear
# ---------------------------------------------------------------------------

class TestClear:
    def test_clear_empties_map(self, default_map):
        custom_hash_map_set(default_map, 1, "a")
        custom_hash_map_set(default_map, 2, "b")
        custom_hash_map_clear(default_map)
        assert custom_hash_map_size(default_map) == 0
        assert custom_hash_map_keys(default_map) == []

    def test_clear_returns_same_map(self, default_map):
        returned = custom_hash_map_clear(default_map)
        assert returned is default_map

    def test_can_use_after_clear(self, default_map):
        custom_hash_map_set(default_map, "x", 1)
        custom_hash_map_clear(default_map)
        custom_hash_map_set(default_map, "y", 2)
        assert custom_hash_map_size(default_map) == 1
        assert custom_hash_map_get(default_map, "y") == 2


# ---------------------------------------------------------------------------
# Python protocols
# ---------------------------------------------------------------------------

class TestPythonProtocols:
    def test_len(self, default_map):
        custom_hash_map_set(default_map, "a", 1)
        custom_hash_map_set(default_map, "b", 2)
        assert len(default_map) == 2

    def test_repr_empty(self, default_map):
        r = repr(default_map)
        assert r.startswith("CustomHashMap{")

    def test_repr_nonempty(self, default_map):
        custom_hash_map_set(default_map, "k", 42)
        r = repr(default_map)
        assert "'k'" in r
        assert "42" in r


# ---------------------------------------------------------------------------
# Guard: standalone helpers reject wrong types gracefully
# ---------------------------------------------------------------------------

class TestTypeGuards:
    def test_set_on_wrong_type(self):
        assert custom_hash_map_set({}, "k", 1) is False

    def test_get_on_wrong_type(self):
        assert custom_hash_map_get({}, "k") is None

    def test_has_on_wrong_type(self):
        assert custom_hash_map_has({}, "k") is False

    def test_remove_on_wrong_type(self):
        assert custom_hash_map_remove({}, "k") is False

    def test_keys_on_wrong_type(self):
        assert custom_hash_map_keys([]) == []

    def test_values_on_wrong_type(self):
        assert custom_hash_map_values([]) == []

    def test_items_on_wrong_type(self):
        assert custom_hash_map_items([]) == []

    def test_size_on_wrong_type(self):
        assert custom_hash_map_size(None) == 0
