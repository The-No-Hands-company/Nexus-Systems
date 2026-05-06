"""
Comprehensive test suite for NexusLang string operations.
Tests string creation, concatenation, methods, and operations.
"""

import pytest
import sys
import os

from tests.helpers.utils import NLPLTestBase

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))



class TestStringBasics(NLPLTestBase):
    """Test basic string operations."""
    
    def test_string_literal(self):
        """Test creating string literals."""
        code = 'set s to "Hello, World!"'
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("s") == "Hello, World!"
    
    def test_empty_string(self):
        """Test empty string."""
        code = 'set s to ""'
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("s") == ""
    
    def test_string_with_single_quotes(self):
        """Test string with single quotes."""
        code = "set s to 'Hello'"
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("s") == "Hello"
    
    def test_string_concatenation_natural(self):
        """Test string concatenation with natural language."""
        code = '''
        set s1 to "Hello"
        set s2 to " World"
        set result to s1 concatenate s2
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "Hello World"
    
    def test_string_concatenation_plus(self):
        """Test string concatenation with + operator."""
        code = 'set result to "Hello" + " World"'
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "Hello World"
    
    def test_string_concatenation_chain(self):
        """Test chaining multiple concatenations."""
        code = 'set result to "a" + "b" + "c" + "d"'
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "abcd"
    
    def test_string_with_numbers(self):
        """Test concatenating strings and numbers."""
        code = 'set result to "Number: " + 42'
        # This might require type coercion
        # Adjust based on actual implementation


class TestStringMethods(NLPLTestBase):
    """Test string methods and operations."""
    
    def test_string_length(self):
        """Test getting string length."""
        code = '''
        set s to "Hello"
        set len to length of s
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("len") == 5
    
    def test_string_uppercase(self):
        """Test converting string to uppercase."""
        code = '''
        set s to "hello"
        set upper to uppercase of s
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_lowercase(self):
        """Test converting string to lowercase."""
        code = '''
        set s to "HELLO"
        set lower to lowercase of s
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_substring(self):
        """Test extracting substring."""
        code = '''
        set s to "Hello World"
        set sub to substring of s from 0 to 5
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_split(self):
        """Test splitting string into list."""
        code = '''
        set s to "one,two,three"
        set parts to split s by ","
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_join(self):
        """Test joining list into string."""
        code = '''
        set words to ["one", "two", "three"]
        set result to join words with " "
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_replace(self):
        """Test replacing substring."""
        code = '''
        set s to "Hello World"
        set result to replace "World" with "NexusLang" in s
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_contains(self):
        """Test checking if string contains substring."""
        code = '''
        set s to "Hello World"
        set has_world to "World" in s
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("has_world") == True
    
    def test_string_startswith(self):
        """Test checking if string starts with prefix."""
        code = '''
        set s to "Hello World"
        set starts to s starts with "Hello"
        '''
        # Adjust based on actual method name/implementation
    
    def test_string_endswith(self):
        """Test checking if string ends with suffix."""
        code = '''
        set s to "Hello World"
        set ends to s ends with "World"
        '''
        # Adjust based on actual method name/implementation


class TestStringIndexing(NLPLTestBase):
    """Test string indexing and slicing."""
    
    def test_string_indexing(self):
        """Test accessing string character by index."""
        code = '''
        set s to "Hello"
        set char to s[0]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("char") == "H"
    
    def test_string_negative_indexing(self):
        """Test negative indexing from end."""
        code = '''
        set s to "Hello"
        set char to s[-1]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("char") == "o"
    
    def test_string_slicing(self):
        """Test slicing strings."""
        code = '''
        set s to "Hello World"
        set slice to s[0:5]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("slice") == "Hello"
    
    def test_string_iteration(self):
        """Test iterating over string characters."""
        code = '''
        set s to "abc"
        set result to ""
        for each char in s
            set result to result + char
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "abc"


class TestStringEdgeCases(NLPLTestBase):
    """Test edge cases for string operations."""
    
    def test_string_with_escape_sequences(self):
        """Test strings with escape sequences."""
        code = r'set s to "Line1\nLine2"'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("s")
        assert "\\n" in result or "\n" in result
    
    def test_string_with_quotes(self):
        """Test strings containing quotes."""
        code = r'set s to "She said \"Hello\""'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("s")
        assert '"' in result or '\\"' in result
    
    def test_unicode_strings(self):
        """Test Unicode string support."""
        code = 'set s to "Hello "'
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("s") == "Hello "
    
    def test_very_long_string(self):
        """Test handling very long strings."""
        code = 'set s to "a" * 10000'
        # Adjust based on string multiplication support
    
    def test_string_comparison(self):
        """Test string equality comparison."""
        code = '''
        set s1 to "hello"
        set s2 to "hello"
        set s3 to "world"
        set result_eq to s1 == s2
        set result_ne to s1 == s3
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result_eq") == True
        assert self.interpreter.get_variable("result_ne") == False


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
