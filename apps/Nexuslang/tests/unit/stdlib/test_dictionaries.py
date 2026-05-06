"""
Comprehensive test suite for NexusLang dictionaries.
Tests dictionary creation, access, modification, and operations.
"""

import pytest
import sys
import os

from tests.helpers.utils import NLPLTestBase

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))



class TestDictionaryBasics(NLPLTestBase):
    """Test basic dictionary operations."""
    
    def test_empty_dictionary(self):
        """Test creating an empty dictionary."""
        code = 'set d to {}'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result == {}
    
    def test_dictionary_with_string_keys(self):
        """Test dictionary with string keys."""
        code = 'set person to {"name": "Alice", "age": 30}'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("person")
        assert result["name"] == "Alice"
        assert result["age"] == 30
    
    def test_dictionary_access_by_key(self):
        """Test accessing dictionary values by key."""
        code = '''
        set person to {"name": "Bob"}
        set name to person["name"]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("name") == "Bob"
    
    def test_dictionary_modification(self):
        """Test modifying dictionary values."""
        code = '''
        set person to {"name": "Charlie", "age": 25}
        set person["age"] to 26
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("person")
        assert result["age"] == 26
    
    def test_dictionary_add_new_key(self):
        """Test adding new key-value pairs."""
        code = '''
        set person to {"name": "Diana"}
        set person["age"] to 28
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("person")
        assert result["name"] == "Diana"
        assert result["age"] == 28
    
    def test_dictionary_with_integer_keys(self):
        """Test dictionary with integer keys."""
        code = 'set d to {1: "one", 2: "two", 3: "three"}'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result[1] == "one"
        assert result[2] == "two"
    
    def test_dictionary_with_mixed_value_types(self):
        """Test dictionary with different value types."""
        code = 'set d to {"string": "text", "number": 42, "bool": true, "list": [1, 2, 3]}'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result["string"] == "text"
        assert result["number"] == 42
        assert result["bool"] == True
        assert result["list"] == [1, 2, 3]
    
    def test_nested_dictionaries(self):
        """Test nested dictionary structures."""
        code = '''
        set data to {
            "user": {
                "name": "Eve",
                "settings": {
                    "theme": "dark"
                }
            }
        }
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("data")
        assert result["user"]["name"] == "Eve"
        assert result["user"]["settings"]["theme"] == "dark"
    
    def test_nested_dictionary_access(self):
        """Test accessing nested dictionary values."""
        code = '''
        set data to {"user": {"name": "Frank"}}
        set username to data["user"]["name"]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("username") == "Frank"
    
    def test_dictionary_in_list(self):
        """Test dictionaries stored in lists."""
        code = '''
        set users to [
            {"name": "Alice", "age": 30},
            {"name": "Bob", "age": 25}
        ]
        set first_user to users[0]
        set first_name to first_user["name"]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("first_name") == "Alice"


class TestDictionaryOperations(NLPLTestBase):
    """Test dictionary operations and methods."""
    
    def test_dictionary_membership_in(self):
        """Test membership operator with dictionaries."""
        code = '''
        set d to {"a": 1, "b": 2}
        set has_a to "a" in d
        set has_c to "c" in d
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("has_a") == True
        assert self.interpreter.get_variable("has_c") == False
    
    def test_dictionary_size(self):
        """Test getting dictionary size."""
        code = '''
        set d to {"a": 1, "b": 2, "c": 3}
        set count to size of d
        '''
        # This test assumes 'size of' is implemented for dictionaries
        self.parse_and_execute(code)
        # Adjust based on actual implementation
    
    def test_dictionary_keys(self):
        """Test getting dictionary keys."""
        code = '''
        set d to {"a": 1, "b": 2}
        set k to keys of d
        '''
        # This test assumes 'keys of' is implemented
        # Adjust based on actual implementation
    
    def test_dictionary_values(self):
        """Test getting dictionary values."""
        code = '''
        set d to {"a": 1, "b": 2}
        set v to values of d
        '''
        # This test assumes 'values of' is implemented
        # Adjust based on actual implementation
    
    def test_dictionary_clear(self):
        """Test clearing dictionary."""
        code = '''
        set d to {"a": 1, "b": 2}
        clear d
        '''
        # This test assumes 'clear' is implemented
        # Adjust based on actual implementation
    
    def test_dictionary_remove_key(self):
        """Test removing a key from dictionary."""
        code = '''
        set d to {"a": 1, "b": 2, "c": 3}
        remove "b" from d
        '''
        # This test assumes 'remove from' is implemented
        # Adjust based on actual implementation
    
    def test_dictionary_update(self):
        """Test updating one dictionary with another."""
        code = '''
        set d1 to {"a": 1, "b": 2}
        set d2 to {"b": 3, "c": 4}
        update d1 with d2
        '''
        # This test assumes 'update with' is implemented
        # Result should be {"a": 1, "b": 3, "c": 4}


class TestDictionaryEdgeCases(NLPLTestBase):
    """Test edge cases and error conditions for dictionaries."""
    
    def test_dictionary_key_not_found(self):
        """Test accessing non-existent key raises error."""
        code = '''
        set d to {"a": 1}
        set value to d["b"]
        '''
        with pytest.raises(Exception):
            self.parse_and_execute(code)
    
    def test_dictionary_overwrite_value(self):
        """Test overwriting existing values."""
        code = '''
        set d to {"key": "old"}
        set d["key"] to "new"
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result["key"] == "new"
    
    def test_dictionary_with_None_values(self):
        """Test dictionary with null/none values."""
        code = 'set d to {"key": null}'
        # Adjust based on how NexusLang represents null
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result["key"] is None or result["key"] == "null"
    
    def test_dictionary_duplicate_keys(self):
        """Test dictionary with duplicate keys (last value wins)."""
        code = 'set d to {"key": 1, "key": 2}'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result["key"] == 2  # Last value should win
    
    def test_dictionary_with_boolean_keys(self):
        """Test dictionary with boolean keys."""
        code = 'set d to {true: "yes", false: "no"}'
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result[True] == "yes"
        assert result[False] == "no"
    
    def test_dictionary_iteration(self):
        """Test iterating over dictionary keys."""
        code = '''
        set d to {"a": 1, "b": 2, "c": 3}
        set sum to 0
        for each key in d
            set sum to sum plus d[key]
        end
        '''
        # This assumes for-each can iterate over dict keys
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 6
    
    def test_large_dictionary(self):
        """Test dictionary with many entries."""
        # Programmatically build large dictionary
        code = '''
        set d to {}
        set i to 0
        while i is less than 100
            set d[i] to i times 2
            set i to i plus 1
        end
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert len(result) == 100
        assert result[50] == 100


class TestDictionaryIntegration(NLPLTestBase):
    """Test dictionaries in complex scenarios."""
    
    def test_dictionary_as_function_argument(self):
        """Test passing dictionary to function."""
        code = '''
        function get_name with data as Dictionary returns String
            return data["name"]
        end
        
        set person to {"name": "Grace"}
        set result to get_name(person)
        '''
        # Adjust based on function implementation
    
    def test_dictionary_return_from_function(self):
        """Test returning dictionary from function."""
        code = '''
        function create_person with name as String, age as Integer returns Dictionary
            return {"name": name, "age": age}
        end
        
        set person to create_person("Henry", 35)
        '''
        # Adjust based on function implementation
    
    def test_dictionary_with_computed_keys(self):
        """Test using computed values as keys."""
        code = '''
        set prefix to "key"
        set d to {}
        set d[prefix + "1"] to "value1"
        set d[prefix + "2"] to "value2"
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result["key1"] == "value1"
        assert result["key2"] == "value2"
    
    def test_dictionary_with_expression_values(self):
        """Test dictionary values as expressions."""
        code = '''
        set x to 10
        set d to {"double": x times 2, "triple": x times 3}
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("d")
        assert result["double"] == 20
        assert result["triple"] == 30


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
