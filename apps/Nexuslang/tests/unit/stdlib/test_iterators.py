"""
Tests for Iterator protocol with Option<T>.

Production-ready test suite - comprehensive coverage, no shortcuts.
"""

import pytest
from nexuslang.stdlib.iterators import (
    RangeIterator, create_range, iterator_map, iterator_filter,
    iterator_reduce, iterator_take, iterator_skip
)
from nexuslang.stdlib.option_result import Some, NoneValue


class TestRangeIterator:
    """Tests for RangeIterator with Option<T>."""
    
    def test_exclusive_range(self):
        """Test exclusive range (1..5)."""
        r = create_range(1, 5, inclusive=False)
        
        assert r.has_next()
        assert r.next() == Some(1)
        assert r.next() == Some(2)
        assert r.next() == Some(3)
        assert r.next() == Some(4)
        assert not r.has_next()
        assert r.next() == NoneValue()
    
    def test_inclusive_range(self):
        """Test inclusive range (1..=5)."""
        r = create_range(1, 5, inclusive=True)
        
        values = []
        while r.has_next():
            opt = r.next()
            if opt.is_some():
                values.append(opt.unwrap())
        
        assert values == [1, 2, 3, 4, 5]
    
    def test_empty_range(self):
        """Test empty range."""
        r = create_range(5, 5, inclusive=False)
        
        assert not r.has_next()
        assert r.next() == NoneValue()
    
    def test_single_element_inclusive(self):
        """Test single element inclusive range."""
        r = create_range(5, 5, inclusive=True)
        
        assert r.has_next()
        assert r.next() == Some(5)
        assert not r.has_next()
    
    def test_python_iterator_protocol(self):
        """Test Python iterator protocol compatibility."""
        r = create_range(1, 4, inclusive=False)
        values = list(r)
        
        assert values == [1, 2, 3]
    
    def test_type_error_on_non_int(self):
        """Test that non-integer bounds raise TypeError."""
        with pytest.raises(TypeError, match="Range requires integer bounds"):
            create_range(1.5, 5.5)
    
    def test_exhausted_iterator(self):
        """Test that exhausted iterator keeps returning None."""
        r = create_range(1, 2, inclusive=False)
        
        assert r.next() == Some(1)
        assert r.next() == NoneValue()
        assert r.next() == NoneValue()  # Still None
        assert r.next() == NoneValue()  # Still None


class TestIteratorFunctions:
    """Tests for iterator utility functions."""
    
    def test_map(self):
        """Test map function."""
        result = iterator_map([1, 2, 3], lambda x: x * 2)
        assert result == [2, 4, 6]
    
    def test_map_type_error(self):
        """Test map with non-callable."""
        with pytest.raises(TypeError, match="map\\(\\) requires a callable"):
            iterator_map([1, 2, 3], "not a function")
    
    def test_filter(self):
        """Test filter function."""
        result = iterator_filter([1, 2, 3, 4, 5], lambda x: x % 2 == 0)
        assert result == [2, 4]
    
    def test_filter_type_error(self):
        """Test filter with non-callable."""
        with pytest.raises(TypeError, match="filter\\(\\) requires a callable"):
            iterator_filter([1, 2, 3], 42)
    
    def test_reduce(self):
        """Test reduce function."""
        result = iterator_reduce([1, 2, 3, 4], 0, lambda acc, x: acc + x)
        assert result == 10
    
    def test_reduce_type_error(self):
        """Test reduce with non-callable."""
        with pytest.raises(TypeError, match="reduce\\(\\) requires a callable"):
            iterator_reduce([1, 2, 3], 0, None)
    
    def test_take(self):
        """Test take function."""
        result = iterator_take([1, 2, 3, 4, 5], 3)
        assert result == [1, 2, 3]
    
    def test_take_more_than_available(self):
        """Test take with n > length."""
        result = iterator_take([1, 2], 5)
        assert result == [1, 2]
    
    def test_take_zero(self):
        """Test take with n=0."""
        result = iterator_take([1, 2, 3], 0)
        assert result == []
    
    def test_take_negative_error(self):
        """Test take with negative n."""
        with pytest.raises(ValueError, match="take\\(\\) requires non-negative integer"):
            iterator_take([1, 2, 3], -1)
    
    def test_skip(self):
        """Test skip function."""
        result = iterator_skip([1, 2, 3, 4, 5], 2)
        assert result == [3, 4, 5]
    
    def test_skip_all(self):
        """Test skip with n >= length."""
        result = iterator_skip([1, 2, 3], 5)
        assert result == []
    
    def test_skip_zero(self):
        """Test skip with n=0."""
        result = iterator_skip([1, 2, 3], 0)
        assert result == [1, 2, 3]
    
    def test_skip_negative_error(self):
        """Test skip with negative n."""
        with pytest.raises(ValueError, match="skip\\(\\) requires non-negative integer"):
            iterator_skip([1, 2, 3], -1)


class TestIteratorChaining:
    """Tests for chaining iterator operations."""
    
    def test_map_filter_chain(self):
        """Test chaining map and filter."""
        numbers = [1, 2, 3, 4, 5]
        doubled = iterator_map(numbers, lambda x: x * 2)
        evens = iterator_filter(doubled, lambda x: x > 5)
        
        assert evens == [6, 8, 10]
    
    def test_range_map_reduce(self):
        """Test range -> map -> reduce chain."""
        r = create_range(1, 6, inclusive=False)
        values = list(r)
        doubled = iterator_map(values, lambda x: x * 2)
        sum_val = iterator_reduce(doubled, 0, lambda acc, x: acc + x)
        
        assert sum_val == 30  # (1+2+3+4+5)*2 = 30


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
