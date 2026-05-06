"""
Comprehensive tests for break and continue statements in NexusLang.
Tests various scenarios including nested loops, edge cases, and error conditions.
"""

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))

import pytest
from tests.helpers.utils import NLPLTestBase


class TestBreakStatement(NLPLTestBase):
    """Test break statement functionality."""
    
    def test_break_in_while_loop(self):
        """Test break exits while loop immediately."""
        code = """
        set counter to 0
        while counter is less than 100
            if counter is equal to 5
                break
            end
            set counter to counter plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("counter") == 5
    
    def test_break_in_for_loop(self):
        """Test break exits for loop immediately."""
        code = """
        set sum to 0
        for each num in [1, 2, 3, 4, 5]
            if num is equal to 3
                break
            end
            set sum to sum plus num
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 3  # 1 + 2
    
    def test_break_in_nested_while_loops(self):
        """Test break only exits innermost loop."""
        code = """
        set outer to 0
        set inner_count to 0
        while outer is less than 3
            set inner to 0
            while inner is less than 5
                set inner_count to inner_count plus 1
                if inner is equal to 2
                    break
                end
                set inner to inner plus 1
            end
            set outer to outer plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("outer") == 3  # Outer loop completes
        assert self.interpreter.get_variable("inner_count") == 9  # 3 iterations * 3 times
    
    def test_break_with_accumulator(self):
        """Test break stops accumulation correctly."""
        code = """
        set numbers to [10, 20, 30, 40, 50]
        set total to 0
        for each n in numbers
            set total to total plus n
            if total is greater than 40
                break
            end
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("total") == 60  # 10 + 20 + 30
    
    def test_break_at_loop_start(self):
        """Test break as first statement in loop."""
        code = """
        set executed to false
        set counter to 0
        while counter is less than 10
            break
            set executed to true
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("executed") == False
        assert self.interpreter.get_variable("counter") == 0


class TestContinueStatement(NLPLTestBase):
    """Test continue statement functionality."""
    
    def test_continue_in_while_loop(self):
        """Test continue skips to next iteration in while loop."""
        code = """
        set counter to 0
        set sum to 0
        while counter is less than 5
            set counter to counter plus 1
            if counter is equal to 3
                continue
            end
            set sum to sum plus counter
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 12  # 1 + 2 + 4 + 5 (skip 3)
    
    def test_continue_in_for_loop(self):
        """Test continue skips to next iteration in for loop."""
        code = """
        set sum to 0
        for each num in [1, 2, 3, 4, 5]
            if num modulo 2 equal to 0
                continue
            end
            set sum to sum plus num
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("sum") == 9  # 1 + 3 + 5 (skip evens)
    
    def test_continue_in_nested_loops(self):
        """Test continue only affects innermost loop."""
        code = """
        set outer_count to 0
        set inner_total to 0
        for each i in [1, 2, 3]
            set outer_count to outer_count plus 1
            for each j in [1, 2, 3]
                if j is equal to 2
                    continue
                end
                set inner_total to inner_total plus j
            end
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("outer_count") == 3
        assert self.interpreter.get_variable("inner_total") == 12  # (1+3) * 3
    
    def test_continue_skips_rest_of_iteration(self):
        """Test that continue skips all statements after it."""
        code = """
        set should_not_increment to 0
        for each num in [1, 2, 3]
            if num is equal to 2
                continue
            end
            set should_not_increment to should_not_increment plus 1
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("should_not_increment") == 2  # Skip when num=2
    
    def test_continue_with_complex_condition(self):
        """Test continue with complex conditional logic."""
        code = """
        set result to []
        for each n in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
            if n is less than 3 or n is greater than 7
                continue
            end
            add n to result
        end
        """
        self.parse_and_execute(code)
        result = self.interpreter.get_variable("result")
        assert result == [3, 4, 5, 6, 7]


class TestBreakContinueEdgeCases(NLPLTestBase):
    """Test edge cases and complex scenarios for break and continue."""
    
    def test_deeply_nested_break(self):
        """Test break in deeply nested loops."""
        code = """
        set found to false
        for each i in [1, 2, 3]
            for each j in [1, 2, 3]
                for each k in [1, 2, 3]
                    if i is equal to 2 and j is equal to 2 and k is equal to 2
                        set found to true
                        break
                    end
                end
            end
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("found") == True
    
    def test_continue_at_loop_end(self):
        """Test continue when it's the last statement (no-op)."""
        code = """
        set count to 0
        for each i in [1, 2, 3]
            set count to count plus 1
            continue
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("count") == 3
    
    def test_break_in_while_with_string_building(self):
        """Test break while building a string."""
        code = """
        set text to ""
        set counter to 0
        while counter is less than 100
            set text to text concatenate "a"
            set counter to counter plus 1
            if counter is equal to 5
                break
            end
        end
        """
        self.parse_and_execute(code)
        assert self.interpreter.get_variable("text") == "aaaaa"
    
    def test_continue_with_list_modification(self):
        """Test continue while modifying a list."""
        code = """
        set evens to []
        set counter to 0
        while counter is less than 10
            set counter to counter plus 1
            if counter modulo 2 equal to 1
                continue
            end
            add counter to evens
        end
        """
        self.parse_and_execute(code)
        evens = self.interpreter.get_variable("evens")
        assert evens == [2, 4, 6, 8, 10]


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
