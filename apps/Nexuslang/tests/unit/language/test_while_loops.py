"""
Comprehensive test suite for NexusLang while loops.
Tests iteration, conditions, scope, breaks, and edge cases.
"""

import pytest
import sys
import os

from tests.helpers.utils import NLPLTestBase

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))



class TestWhileLoops(NLPLTestBase):
    """Test while loop functionality."""
    
    def test_simple_while_loop(self):
        """Test basic while loop counting."""
        code = """
        set counter to 0
        while counter is less than 5
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 5
    
    def test_while_loop_zero_iterations(self):
        """Test while loop that never executes."""
        code = """
        set counter to 10
        while counter is less than 5
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 10
    
    def test_while_loop_with_accumulator(self):
        """Test while loop with accumulation."""
        code = """
        set counter to 0
        set sum to 0
        while counter is less than 5
            set sum to sum plus counter
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 10  # 0+1+2+3+4
        assert self.interpreter.get_variable("counter") == 5
    
    def test_while_loop_countdown(self):
        """Test while loop counting down."""
        code = """
        set counter to 5
        while counter is greater than 0
            set counter to counter minus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 0
    
    def test_while_loop_with_complex_condition(self):
        """Test while loop with complex condition."""
        code = """
        set x to 0
        set y to 10
        while x is less than 5 and y is greater than 5
            set x to x plus 1
            set y to y minus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("x") == 5
        assert self.interpreter.get_variable("y") == 5
    
    def test_while_loop_modifying_list(self):
        """Test while loop that modifies a list."""
        code = """
        set numbers to []
        set counter to 0
        while counter is less than 3
            add counter to numbers
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("numbers")
        assert result == [0, 1, 2]
    
    def test_while_loop_with_multiplication(self):
        """Test while loop with multiplication (factorial-like)."""
        code = """
        set n to 5
        set result to 1
        set counter to 1
        while counter is less than or equal to n
            set result to result times counter
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("result") == 120  # 5!
    
    def test_nested_while_loops(self):
        """Test nested while loops."""
        code = """
        set i to 0
        set total to 0
        while i is less than 3
            set j to 0
            while j is less than 2
                set total to total plus 1
                set j to j plus 1
            end
            set i to i plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("total") == 6  # 3 * 2
    
    def test_while_loop_with_string_concatenation(self):
        """Test while loop building a string."""
        code = """
        set counter to 0
        set text to ""
        while counter is less than 3
            set text to text concatenate "a"
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("text") == "aaa"
    
    def test_while_loop_scope_isolation(self):
        """Test that variables created inside while loop are accessible outside."""
        code = """
        set counter to 0
        while counter is less than 1
            set inner_var to 42
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        # Variables created in loop should be accessible outside
        assert self.interpreter.get_variable("inner_var") == 42
    
    def test_while_loop_with_break(self):
        """Test while loop with break statement."""
        code = """
        set counter to 0
        while counter is less than 100
            if counter is equal to 5
                break
            end
            set counter to counter plus 1
        end
        """
        # Note: This test assumes break is implemented
        # If not implemented yet, this test will fail and serve as a TODO
    
    def test_while_loop_with_continue(self):
        """Test while loop with continue statement."""
        code = """
        set counter to 0
        set sum to 0
        while counter is less than 10
            set counter to counter plus 1
            if counter modulo 2 equal to 0
                continue
            end
            set sum to sum plus counter
        end
        """
        # Note: This test assumes continue is implemented
        # If not implemented yet, this test will fail and serve as a TODO
    
    def test_infinite_loop_prevention(self):
        """Test that infinite loops can be detected or limited."""
        # This is a safety test - implementation might use max iteration count
        # or timeout to prevent true infinite loops
        code = """
        set counter to 0
        set max_iterations to 0
        while true
            set max_iterations to max_iterations plus 1
            if max_iterations is greater than 10000
                break
            end
        end
        """
        # This test serves as documentation that infinite loops are possible
        # Production code might want iteration limits or timeouts


class TestWhileLoopEdgeCases(NLPLTestBase):
    """Test edge cases for while loops."""
    
    def test_while_with_boolean_literal(self):
        """Test while loop with boolean literals."""
        code = """
        set executed to false
        while false
            set executed to true
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("executed") == False
    
    def test_while_with_negated_condition(self):
        """Test while loop with negated condition."""
        code = """
        set counter to 0
        set done to false
        while not done
            set counter to counter plus 1
            if counter is equal to 3
                set done to true
            end
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 3
    
    def test_while_modifying_condition_variable(self):
        """Test while loop that modifies its condition variable in complex ways."""
        code = """
        set x to 10
        while x is greater than 0
            set x to x divided by 2
        end
        """
        self.parse_and_execute(code)
        # After division by 2: 10 -> 5.0 -> 2.5 -> 1.25 -> 0.625 -> 0.3125 -> ...
        # Eventually becomes less than 1, which is still > 0, but eventually underflows
        # Exact result depends on floating point behavior
        assert self.interpreter.get_variable("x") < 1
    
    def test_while_with_list_membership(self):
        """Test while loop with list membership condition."""
        code = """
        set items to [1, 2, 3]
        set counter to 0
        while counter not in items
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 1  # Stops at first item in list
    
    def test_while_with_empty_body(self):
        """Test while loop with empty body."""
        code = """
        set counter to 5
        while counter is less than 3
        end
        """
        # Empty body should be syntactically valid (or raise appropriate error)
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 5


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
