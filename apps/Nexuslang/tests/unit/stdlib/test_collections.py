"""
Tests for collections (Vec, HashMap, Set).

Production-ready test suite - comprehensive coverage.
"""

import pytest
from nexuslang.stdlib.collections import Vec, HashMap, Set
from nexuslang.stdlib.option_result import Some, NoneValue


class TestVec:
    """Tests for Vec<T>."""
    
    def test_vec_creation(self):
        """Test creating a Vec."""
        vec = Vec()
        assert vec.len() == 0
        assert vec.is_empty()
    
    def test_vec_push_pop(self):
        """Test push and pop operations."""
        vec = Vec()
        vec.push(1)
        vec.push(2)
        vec.push(3)
        
        assert vec.len() == 3
        assert not vec.is_empty()
        
        # Pop returns Option
        last = vec.pop()
        assert last == Some(3)
        assert vec.len() == 2
    
    def test_vec_pop_empty(self):
        """Test popping from empty Vec."""
        vec = Vec()
        result = vec.pop()
        assert result == NoneValue()
    
    def test_vec_get(self):
        """Test getting elements."""
        vec = Vec()
        vec.push(10)
        vec.push(20)
        
        assert vec.get(0) == Some(10)
        assert vec.get(1) == Some(20)
        assert vec.get(2) == NoneValue()  # Out of bounds
    
    def test_vec_set(self):
        """Test setting elements."""
        vec = Vec()
        vec.push(1)
        vec.push(2)
        
        assert vec.set(0, 100)
        assert vec.get(0) == Some(100)
        assert not vec.set(5, 999)  # Out of bounds
    
    def test_vec_clear(self):
        """Test clearing Vec."""
        vec = Vec()
        vec.push(1)
        vec.push(2)
        vec.clear()
        
        assert vec.len() == 0
        assert vec.is_empty()
    
    def test_vec_iteration(self):
        """Test iterating over Vec."""
        vec = Vec()
        vec.push(1)
        vec.push(2)
        vec.push(3)
        
        values = list(vec)
        assert values == [1, 2, 3]


class TestHashMap:
    """Tests for HashMap<K,V>."""
    
    def test_hashmap_creation(self):
        """Test creating a HashMap."""
        map = HashMap()
        assert map.len() == 0
        assert map.is_empty()
    
    def test_hashmap_insert_get(self):
        """Test insert and get operations."""
        map = HashMap()
        
        # Insert returns None for new keys
        assert map.insert("name", "Alice") == NoneValue()
        assert map.insert("age", 30) == NoneValue()
        
        # Get returns Some for existing keys
        assert map.get("name") == Some("Alice")
        assert map.get("age") == Some(30)
        assert map.get("missing") == NoneValue()
    
    def test_hashmap_insert_overwrite(self):
        """Test overwriting values."""
        map = HashMap()
        map.insert("key", "value1")
        
        # Inserting again returns old value
        old = map.insert("key", "value2")
        assert old == Some("value1")
        assert map.get("key") == Some("value2")
    
    def test_hashmap_remove(self):
        """Test removing entries."""
        map = HashMap()
        map.insert("key", "value")
        
        removed = map.remove("key")
        assert removed == Some("value")
        assert map.get("key") == NoneValue()
        assert map.remove("key") == NoneValue()  # Already removed
    
    def test_hashmap_contains_key(self):
        """Test checking for keys."""
        map = HashMap()
        map.insert("exists", 1)
        
        assert map.contains_key("exists")
        assert not map.contains_key("missing")
    
    def test_hashmap_clear(self):
        """Test clearing HashMap."""
        map = HashMap()
        map.insert("a", 1)
        map.insert("b", 2)
        map.clear()
        
        assert map.len() == 0
        assert map.is_empty()
    
    def test_hashmap_iteration(self):
        """Test iterating over HashMap."""
        map = HashMap()
        map.insert("a", 1)
        map.insert("b", 2)
        
        keys = list(map.keys())
        assert set(keys) == {"a", "b"}
        
        values = list(map.values())
        assert set(values) == {1, 2}


class TestSet:
    """Tests for Set<T>."""
    
    def test_set_creation(self):
        """Test creating a Set."""
        s = Set()
        assert s.len() == 0
        assert s.is_empty()
    
    def test_set_add(self):
        """Test adding elements."""
        s = Set()
        
        assert s.add(1)  # New element
        assert s.add(2)
        assert not s.add(1)  # Duplicate
        
        assert s.len() == 2
    
    def test_set_contains(self):
        """Test checking membership."""
        s = Set()
        s.add(1)
        s.add(2)
        
        assert s.contains(1)
        assert s.contains(2)
        assert not s.contains(3)
    
    def test_set_remove(self):
        """Test removing elements."""
        s = Set()
        s.add(1)
        s.add(2)
        
        assert s.remove(1)
        assert not s.contains(1)
        assert not s.remove(1)  # Already removed
    
    def test_set_union(self):
        """Test set union."""
        s1 = Set()
        s1.add(1)
        s1.add(2)
        
        s2 = Set()
        s2.add(2)
        s2.add(3)
        
        union = s1.union(s2)
        assert union.len() == 3
        assert union.contains(1)
        assert union.contains(2)
        assert union.contains(3)
    
    def test_set_intersection(self):
        """Test set intersection."""
        s1 = Set()
        s1.add(1)
        s1.add(2)
        
        s2 = Set()
        s2.add(2)
        s2.add(3)
        
        inter = s1.intersection(s2)
        assert inter.len() == 1
        assert inter.contains(2)
    
    def test_set_difference(self):
        """Test set difference."""
        s1 = Set()
        s1.add(1)
        s1.add(2)
        
        s2 = Set()
        s2.add(2)
        s2.add(3)
        
        diff = s1.difference(s2)
        assert diff.len() == 1
        assert diff.contains(1)
    
    def test_set_clear(self):
        """Test clearing Set."""
        s = Set()
        s.add(1)
        s.add(2)
        s.clear()
        
        assert s.len() == 0
        assert s.is_empty()
    
    def test_set_iteration(self):
        """Test iterating over Set."""
        s = Set()
        s.add(1)
        s.add(2)
        s.add(3)
        
        values = set(s)
        assert values == {1, 2, 3}


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
