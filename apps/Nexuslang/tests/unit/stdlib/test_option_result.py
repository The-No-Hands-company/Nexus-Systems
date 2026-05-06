"""
Tests for Option<T> and Result<T,E> types.

Production-ready test suite with comprehensive coverage.
"""

import pytest
from nexuslang.stdlib.option_result import (
    Some, NoneValue, Ok, Err, Option, Result
)


class TestOption:
    """Tests for Option<T> type."""
    
    def test_some_creation(self):
        """Test creating Some values."""
        opt = Some(42)
        assert opt.is_some()
        assert not opt.is_none()
        assert opt.unwrap() == 42
    
    def test_none_creation(self):
        """Test creating None values."""
        opt = NoneValue()
        assert opt.is_none()
        assert not opt.is_some()
    
    def test_unwrap_some(self):
        """Test unwrapping Some values."""
        opt = Some("hello")
        assert opt.unwrap() == "hello"
    
    def test_unwrap_none_raises(self):
        """Test that unwrapping None raises error."""
        opt = NoneValue()
        with pytest.raises(RuntimeError, match="Called unwrap\\(\\) on a None value"):
            opt.unwrap()
    
    def test_unwrap_or(self):
        """Test unwrap_or with default values."""
        some = Some(42)
        none = NoneValue()
        
        assert some.unwrap_or(0) == 42
        assert none.unwrap_or(0) == 0
    
    def test_map_some(self):
        """Test mapping over Some values."""
        opt = Some(5)
        mapped = opt.map(lambda x: x * 2)
        
        assert mapped.is_some()
        assert mapped.unwrap() == 10
    
    def test_map_none(self):
        """Test mapping over None values."""
        opt = NoneValue()
        mapped = opt.map(lambda x: x * 2)
        
        assert mapped.is_none()
    
    def test_and_then_some(self):
        """Test and_then with Some values."""
        opt = Some(5)
        result = opt.and_then(lambda x: Some(x * 2) if x > 0 else NoneValue())
        
        assert result.is_some()
        assert result.unwrap() == 10
    
    def test_and_then_none(self):
        """Test and_then with None values."""
        opt = NoneValue()
        result = opt.and_then(lambda x: Some(x * 2))
        
        assert result.is_none()
    
    def test_filter_true(self):
        """Test filter with passing predicate."""
        opt = Some(5)
        filtered = opt.filter(lambda x: x > 0)
        
        assert filtered.is_some()
        assert filtered.unwrap() == 5
    
    def test_filter_false(self):
        """Test filter with failing predicate."""
        opt = Some(-5)
        filtered = opt.filter(lambda x: x > 0)
        
        assert filtered.is_none()
    
    def test_equality(self):
        """Test Option equality."""
        assert Some(42) == Some(42)
        assert NoneValue() == NoneValue()
        assert Some(42) != Some(43)
        assert Some(42) != NoneValue()


class TestResult:
    """Tests for Result<T,E> type."""
    
    def test_ok_creation(self):
        """Test creating Ok values."""
        res = Ok(42)
        assert res.is_ok()
        assert not res.is_err()
        assert res.unwrap() == 42
    
    def test_err_creation(self):
        """Test creating Err values."""
        res = Err("error")
        assert res.is_err()
        assert not res.is_ok()
        assert res.unwrap_err() == "error"
    
    def test_unwrap_ok(self):
        """Test unwrapping Ok values."""
        res = Ok("success")
        assert res.unwrap() == "success"
    
    def test_unwrap_err_raises(self):
        """Test that unwrapping Err raises error."""
        res = Err("failed")
        with pytest.raises(RuntimeError, match="Called unwrap\\(\\) on an Err value"):
            res.unwrap()
    
    def test_unwrap_or(self):
        """Test unwrap_or with default values."""
        ok = Ok(42)
        err = Err("error")
        
        assert ok.unwrap_or(0) == 42
        assert err.unwrap_or(0) == 0
    
    def test_map_ok(self):
        """Test mapping over Ok values."""
        res = Ok(5)
        mapped = res.map(lambda x: x * 2)
        
        assert mapped.is_ok()
        assert mapped.unwrap() == 10
    
    def test_map_err(self):
        """Test mapping over Err values."""
        res = Err("error")
        mapped = res.map(lambda x: x * 2)
        
        assert mapped.is_err()
        assert mapped.unwrap_err() == "error"
    
    def test_map_err_transform(self):
        """Test transforming Err values."""
        res = Err("error")
        mapped = res.map_err(lambda e: f"Failed: {e}")
        
        assert mapped.is_err()
        assert mapped.unwrap_err() == "Failed: error"
    
    def test_and_then_ok(self):
        """Test and_then with Ok values."""
        res = Ok(5)
        result = res.and_then(lambda x: Ok(x * 2) if x > 0 else Err("negative"))
        
        assert result.is_ok()
        assert result.unwrap() == 10
    
    def test_and_then_err(self):
        """Test and_then with Err values."""
        res = Err("error")
        result = res.and_then(lambda x: Ok(x * 2))
        
        assert result.is_err()
        assert result.unwrap_err() == "error"
    
    def test_equality(self):
        """Test Result equality."""
        assert Ok(42) == Ok(42)
        assert Err("error") == Err("error")
        assert Ok(42) != Ok(43)
        assert Ok(42) != Err("error")


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
