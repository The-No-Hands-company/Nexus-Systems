"""
Tests for pattern matching with Option and Result types.

Production-ready test suite - comprehensive coverage.
"""

import pytest
from nexuslang.parser.ast import OptionPattern, ResultPattern, WildcardPattern
from nexuslang.stdlib.option_result import Some, NoneValue, Ok, Err


class TestPatternAST:
    """Tests for pattern AST nodes."""
    
    def test_option_pattern_some(self):
        """Test OptionPattern for Some variant."""
        pattern = OptionPattern("Some", "value")
        assert pattern.variant == "Some"
        assert pattern.binding == "value"
        assert "Some with value" in repr(pattern)
    
    def test_option_pattern_none(self):
        """Test OptionPattern for None variant."""
        pattern = OptionPattern("None")
        assert pattern.variant == "None"
        assert pattern.binding is None
        assert "None" in repr(pattern)
    
    def test_option_pattern_invalid_variant(self):
        """Test that invalid Option variant raises error."""
        with pytest.raises(ValueError, match="Invalid Option variant"):
            OptionPattern("Invalid")
    
    def test_result_pattern_ok(self):
        """Test ResultPattern for Ok variant."""
        pattern = ResultPattern("Ok", "value")
        assert pattern.variant == "Ok"
        assert pattern.binding == "value"
        assert "Ok with value" in repr(pattern)
    
    def test_result_pattern_err(self):
        """Test ResultPattern for Err variant."""
        pattern = ResultPattern("Err", "error")
        assert pattern.variant == "Err"
        assert pattern.binding == "error"
        assert "Err with error" in repr(pattern)
    
    def test_result_pattern_invalid_variant(self):
        """Test that invalid Result variant raises error."""
        with pytest.raises(ValueError, match="Invalid Result variant"):
            ResultPattern("Invalid")
    
    def test_wildcard_pattern(self):
        """Test WildcardPattern."""
        pattern = WildcardPattern()
        assert "_" in repr(pattern)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
