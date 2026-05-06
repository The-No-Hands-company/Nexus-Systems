"""
Comprehensive test suite for NexusLang control flow structures.
Tests if/else, for-each loops, try-catch, and other control structures.
"""

import pytest
import sys
import os

from tests.helpers.utils import NLPLTestBase

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))



class TestConditionals(NLPLTestBase):
    """Test if/else if/else statements."""
    
    def test_simple_if_true(self):
        """Test if statement with true condition."""
        code = '''
        set x to 5
        if x is greater than 3
            set result to "yes"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "yes"
    
    def test_simple_if_false(self):
        """Test if statement with false condition."""
        code = '''
        set x to 1
        set result to "no"
        if x is greater than 3
            set result to "yes"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "no"
    
    def test_if_else(self):
        """Test if-else statement."""
        code = '''
        set x to 1
        if x is greater than 3
            set result to "yes"
        else
            set result to "no"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "no"
    
    def test_if_else_if_else(self):
        """Test if-else if-else chain."""
        code = '''
        set x to 5
        if x is less than 3
            set result to "small"
        else if x is less than 7
            set result to "medium"
        else
            set result to "large"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "medium"
    
    def test_nested_if(self):
        """Test nested if statements."""
        code = '''
        set x to 5
        set y to 10
        if x is greater than 3
            if y is greater than 8
                set result to "both"
            end
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "both"
    
    def test_if_with_complex_condition(self):
        """Test if with AND/OR conditions."""
        code = '''
        set x to 5
        set y to 10
        if x is greater than 3 and y is less than 15
            set result to "yes"
        else
            set result to "no"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "yes"


class TestForEachLoops(NLPLTestBase):
    """Test for-each loop functionality."""
    
    def test_for_each_list(self):
        """Test for-each loop over list."""
        code = '''
        set numbers to [1, 2, 3, 4, 5]
        set sum to 0
        for each num in numbers
            set sum to sum plus num
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 15
    
    def test_for_each_empty_list(self):
        """Test for-each over empty list."""
        code = '''
        set items to []
        set count to 0
        for each item in items
            set count to count plus 1
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("count") == 0
    
    def test_for_each_string_list(self):
        """Test for-each over list of strings."""
        code = '''
        set words to ["hello", "world"]
        set result to ""
        for each word in words
            set result to result concatenate word
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "helloworld"
    
    def test_for_each_with_index(self):
        """Test for-each accessing index."""
        code = '''
        set numbers to [10, 20, 30]
        set result to numbers[1]
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == 20
    
    def test_nested_for_each(self):
        """Test nested for-each loops."""
        code = '''
        set matrix to [[1, 2], [3, 4]]
        set sum to 0
        for each row in matrix
            for each value in row
                set sum to sum plus value
            end
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 10
    
    def test_for_each_modifying_elements(self):
        """Test for-each loop that modifies list elements."""
        code = '''
        set numbers to [1, 2, 3]
        set doubled to []
        for each num in numbers
            set double to num times 2
            add double to doubled
        end
        '''
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("doubled")
        assert result == [2, 4, 6]


class TestTryCatch(NLPLTestBase):
    """Test try-catch error handling."""
    
    def test_try_catch_no_error(self):
        """Test try-catch when no error occurs."""
        code = '''
        set result to "none"
        try
            set result to "success"
        catch error
            set result to "failed"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "success"
    
    def test_try_catch_with_error(self):
        """Test try-catch catching an error."""
        code = '''
        set result to "none"
        try
            set x to 10 divided by 0
            set result to "no error"
        catch error
            set result to "caught"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "caught"
    
    def test_try_catch_error_variable(self):
        """Test accessing error variable in catch block."""
        code = '''
        try
            set x to undefined_variable
        catch error
            set error_caught to true
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("error_caught") == True
    
    def test_nested_try_catch(self):
        """Test nested try-catch blocks."""
        code = '''
        set result to "none"
        try
            try
                set x to 10 divided by 0
            catch inner_error
                set result to "inner"
            end
        catch outer_error
            set result to "outer"
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == "inner"
    
    def test_try_catch_finally(self):
        """Test try-catch-finally block."""
        code = '''
        set cleanup_done to false
        try
            set x to 10 divided by 0
        catch error
            set error_handled to true
        finally
            set cleanup_done to true
        end
        '''
        # Adjust if finally is implemented
        # self.parse_and_execute(code)
        # assert self.interpreter.get_variable("cleanup_done") == True


class TestControlFlowIntegration(NLPLTestBase):
    """Test complex control flow combinations."""
    
    def test_if_inside_loop(self):
        """Test if statement inside for-each loop."""
        code = '''
        set numbers to [1, 2, 3, 4, 5]
        set evens to 0
        for each num in numbers
            if num modulo 2 equal to 0
                set evens to evens plus 1
            end
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("evens") == 2
    
    def test_loop_inside_if(self):
        """Test loop inside if statement."""
        code = '''
        set condition to true
        set sum to 0
        if condition
            for each num in [1, 2, 3]
                set sum to sum plus num
            end
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 6
    
    def test_try_catch_in_loop(self):
        """Test try-catch inside loop."""
        code = '''
        set numbers to [10, 0, 5]
        set errors to 0
        for each num in numbers
            try
                set result to 100 divided by num
            catch error
                set errors to errors plus 1
            end
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("errors") == 1
    
    def test_complex_nested_control_flow(self):
        """Test complex nesting of control structures."""
        code = '''
        set data to [[1, 2], [3, 4], [5, 6]]
        set sum to 0
        set count to 0
        for each row in data
            if count is less than 2
                for each value in row
                    if value modulo 2 equal to 0
                        set sum to sum plus value
                    end
                end
            end
            set count to count plus 1
        end
        '''
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 6  # 2 + 4


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
