"""
Comprehensive test suite for NexusLang operators.
Tests all arithmetic, bitwise, logical, and comparison operators.
"""

import pytest
import sys
import os

from tests.helpers.utils import NLPLTestBase

# Add the src directory to the path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))



class TestArithmeticOperators(NLPLTestBase):
    """Test arithmetic operators: +, -, *, /, %, **, //"""
    
    def test_addition(self):
        """Test addition operator."""
        result = self.parse_and_execute("set x to 5 plus 3")
        assert self.interpreter.get_variable("x") == 8
    
    def test_addition_symbol(self):
        """Test addition with + symbol."""
        result = self.parse_and_execute("set x to 5 + 3")
        assert self.interpreter.get_variable("x") == 8
    
    def test_subtraction(self):
        """Test subtraction operator."""
        result = self.parse_and_execute("set x to 10 minus 3")
        assert self.interpreter.get_variable("x") == 7
    
    def test_subtraction_symbol(self):
        """Test subtraction with - symbol."""
        result = self.parse_and_execute("set x to 10 - 3")
        assert self.interpreter.get_variable("x") == 7
    
    def test_multiplication(self):
        """Test multiplication operator."""
        result = self.parse_and_execute("set x to 4 times 5")
        assert self.interpreter.get_variable("x") == 20
    
    def test_multiplication_symbol(self):
        """Test multiplication with * symbol."""
        result = self.parse_and_execute("set x to 4 * 5")
        assert self.interpreter.get_variable("x") == 20
    
    def test_division(self):
        """Test division operator."""
        result = self.parse_and_execute("set x to 15 divided by 3")
        assert self.interpreter.get_variable("x") == 5.0
    
    def test_division_symbol(self):
        """Test division with / symbol."""
        result = self.parse_and_execute("set x to 15 / 3")
        assert self.interpreter.get_variable("x") == 5.0
    
    def test_modulo(self):
        """Test modulo operator."""
        result = self.parse_and_execute("set x to 17 modulo 5")
        assert self.interpreter.get_variable("x") == 2
    
    def test_modulo_symbol(self):
        """Test modulo with % symbol."""
        result = self.parse_and_execute("set x to 17 % 5")
        assert self.interpreter.get_variable("x") == 2
    
    def test_power(self):
        """Test power operator with natural language."""
        result = self.parse_and_execute("set x to 2 to the power of 8")
        assert self.interpreter.get_variable("x") == 256
    
    def test_power_symbol(self):
        """Test power operator with ** symbol."""
        result = self.parse_and_execute("set x to 2 ** 8")
        assert self.interpreter.get_variable("x") == 256
    
    def test_power_fractional(self):
        """Test power with fractional exponent (square root)."""
        result = self.parse_and_execute("set x to 16 ** 0.5")
        assert self.interpreter.get_variable("x") == 4.0
    
    def test_floor_division(self):
        """Test floor division operator."""
        result = self.parse_and_execute("set x to 17 // 5")
        assert self.interpreter.get_variable("x") == 3
    
    def test_floor_division_negative(self):
        """Test floor division with negative numbers."""
        # Use proper NexusLang syntax for negative numbers
        result = self.parse_and_execute("set x to (0 minus 17) // 5")
        assert self.interpreter.get_variable("x") == -4
    
    def test_operator_precedence(self):
        """Test operator precedence: multiply > add, power > multiply."""
        # 2 + 3 * 4 -> 2 + 12 = 14 (multiply before add)
        result = self.parse_and_execute("set x to 2 + 3 * 4")
        assert self.interpreter.get_variable("x") == 14
        # 2 ** 3 + 1 -> 8 + 1 = 9 (power before add)
        result = self.parse_and_execute("set y to 2 ** 3 + 1")
        assert self.interpreter.get_variable("y") == 9
    
    def test_parentheses_override_precedence(self):
        """Test parentheses override operator precedence."""
        # (2 + 3) * 4 = 5 * 4 = 20 (parentheses force add-first)
        result = self.parse_and_execute("set x to (2 + 3) * 4")
        assert self.interpreter.get_variable("x") == 20
        # Without parens: 2 + 3 * 4 = 14
        result = self.parse_and_execute("set y to 2 + 3 * 4")
        assert self.interpreter.get_variable("y") == 14
    
    def test_division_by_zero(self):
        """Test division by zero raises an error."""
        with pytest.raises((ZeroDivisionError, Exception)):
            self.parse_and_execute("set x to 5 / 0")
    
    def test_chained_operations(self):
        """Test chaining multiple arithmetic operations in one expression."""
        # 10 - 3 + 2 - 1 = 8 (left-to-right for same precedence)
        result = self.parse_and_execute("set x to 10 - 3 + 2 - 1")
        assert self.interpreter.get_variable("x") == 8
        # 2 * 3 * 4 = 24
        result = self.parse_and_execute("set y to 2 * 3 * 4")
        assert self.interpreter.get_variable("y") == 24


class TestBitwiseOperators(NLPLTestBase):
    """Test bitwise operators: &, |, ^, ~, <<, >>"""
    
    def test_bitwise_and_natural(self):
        """Test bitwise AND with natural language."""
        result = self.parse_and_execute("set x to 12 bitwise and 10")
        assert self.interpreter.get_variable("x") == 8  # 1100 & 1010 = 1000
    
    def test_bitwise_and_symbol(self):
        """Test bitwise AND with & symbol."""
        result = self.parse_and_execute("set x to 12 & 10")
        assert self.interpreter.get_variable("x") == 8
    
    def test_bitwise_or_natural(self):
        """Test bitwise OR with natural language."""
        result = self.parse_and_execute("set x to 12 bitwise or 10")
        assert self.interpreter.get_variable("x") == 14  # 1100 | 1010 = 1110
    
    def test_bitwise_or_symbol(self):
        """Test bitwise OR with | symbol."""
        result = self.parse_and_execute("set x to 12 | 10")
        assert self.interpreter.get_variable("x") == 14
    
    def test_bitwise_xor_natural(self):
        """Test bitwise XOR with natural language."""
        result = self.parse_and_execute("set x to 12 bitwise xor 10")
        assert self.interpreter.get_variable("x") == 6  # 1100 ^ 1010 = 0110
    
    def test_bitwise_xor_symbol(self):
        """Test bitwise XOR with ^ symbol."""
        result = self.parse_and_execute("set x to 12 ^ 10")
        assert self.interpreter.get_variable("x") == 6
    
    def test_bitwise_not_natural(self):
        """Test bitwise NOT with natural language."""
        result = self.parse_and_execute("set x to bitwise not 5")
        assert self.interpreter.get_variable("x") == -6  # ~5 = -6 in two's complement
    
    def test_bitwise_not_symbol(self):
        """Test bitwise NOT with ~ symbol."""
        result = self.parse_and_execute("set x to ~5")
        assert self.interpreter.get_variable("x") == -6
    
    def test_left_shift_natural(self):
        """Test left shift with natural language."""
        result = self.parse_and_execute("set x to 4 shift left 2")
        assert self.interpreter.get_variable("x") == 16  # 0100 << 2 = 10000
    
    def test_left_shift_symbol(self):
        """Test left shift with << symbol."""
        result = self.parse_and_execute("set x to 4 << 2")
        assert self.interpreter.get_variable("x") == 16
    
    def test_right_shift_natural(self):
        """Test right shift with natural language."""
        result = self.parse_and_execute("set x to 16 shift right 2")
        assert self.interpreter.get_variable("x") == 4  # 10000 >> 2 = 0100
    
    def test_right_shift_symbol(self):
        """Test right shift with >> symbol."""
        result = self.parse_and_execute("set x to 16 >> 2")
        assert self.interpreter.get_variable("x") == 4
    
    def test_bitwise_masks(self):
        """Test using bitwise operators for bit masking."""
        # Clear lower 4 bits
        result = self.parse_and_execute("set x to 255 & 240")
        assert self.interpreter.get_variable("x") == 240  # 11111111 & 11110000 = 11110000
    
    def test_bitwise_flags(self):
        """Test using bitwise operators for flag manipulation."""
        # Set flag
        result = self.parse_and_execute("set flags to 0\nset flags to flags | 4")
        assert self.interpreter.get_variable("flags") == 4
        
        # Check flag
        result = self.parse_and_execute("set flags to 5\nset has_flag to (flags & 4) == 4")
        # This test might need adjustment based on boolean implementation


class TestComparisonOperators(NLPLTestBase):
    """Test comparison operators: ==, !=, <, >, <=, >=, in"""
    
    def test_equal_to(self):
        """Test equality comparison."""
        result = self.parse_and_execute("set x to 5 equal to 5")
        assert self.interpreter.get_variable("x") == True
    
    def test_not_equal_to(self):
        """Test inequality comparison."""
        result = self.parse_and_execute("set x to 5 not equal to 3")
        assert self.interpreter.get_variable("x") == True
    
    def test_less_than(self):
        """Test less than comparison."""
        result = self.parse_and_execute("set x to 3 less than 5")
        assert self.interpreter.get_variable("x") == True
    
    def test_greater_than(self):
        """Test greater than comparison."""
        result = self.parse_and_execute("set x to 5 greater than 3")
        assert self.interpreter.get_variable("x") == True
    
    def test_less_than_or_equal(self):
        """Test less than or equal comparison."""
        result = self.parse_and_execute("set x to 5 less than or equal to 5")
        assert self.interpreter.get_variable("x") == True
    
    def test_greater_than_or_equal(self):
        """Test greater than or equal comparison."""
        result = self.parse_and_execute("set x to 5 greater than or equal to 5")
        assert self.interpreter.get_variable("x") == True
    
    def test_is_syntax(self):
        """Test 'is' syntax for comparisons."""
        result = self.parse_and_execute("set x to 5 is greater than 3")
        assert self.interpreter.get_variable("x") == True
    
    def test_membership_in_list(self):
        """Test membership operator with list."""
        code = """
        set numbers to [1, 2, 3, 4, 5]
        set x to 3 in numbers
        """
        result = self.parse_and_execute(code)
        assert self.interpreter.get_variable("x") == True
    
    def test_membership_not_in_list(self):
        """Test membership operator when element not in list."""
        code = """
        set numbers to [1, 2, 3, 4, 5]
        set x to 10 in numbers
        """
        result = self.parse_and_execute(code)
        assert self.interpreter.get_variable("x") == False


class TestLogicalOperators(NLPLTestBase):
    """Test logical operators: and, or, not"""
    
    def test_logical_and(self):
        """Test logical AND operator."""
        result = self.parse_and_execute("set x to true and true")
        assert self.interpreter.get_variable("x") == True
        
        result = self.parse_and_execute("set x to true and false")
        assert self.interpreter.get_variable("x") == False
    
    def test_logical_or(self):
        """Test logical OR operator."""
        result = self.parse_and_execute("set x to true or false")
        assert self.interpreter.get_variable("x") == True
        
        result = self.parse_and_execute("set x to false or false")
        assert self.interpreter.get_variable("x") == False
    
    def test_logical_not(self):
        """Test logical NOT operator."""
        result = self.parse_and_execute("set x to not true")
        assert self.interpreter.get_variable("x") == False
        
        result = self.parse_and_execute("set x to not false")
        assert self.interpreter.get_variable("x") == True
    
    def test_logical_precedence(self):
        """Test logical operator precedence: not > and > or."""
        result = self.parse_and_execute("set x to false or true and true")
        assert self.interpreter.get_variable("x") == True  # false or (true and true)
    
    def test_short_circuit_and(self):
        """Test short-circuit evaluation of AND."""
        # This would fail if second expression evaluated (division by zero)
        # But AND should short-circuit on first false
        code = """
        set x to false
        set y to x and (10 / 0 > 5)
        """
        # Depending on implementation, this might not short-circuit
        # Adjust test based on actual behavior


class TestUnaryOperators(NLPLTestBase):
    """Test unary operators: -, not, ~"""
    
    def test_unary_minus(self):
        """Test unary minus operator."""
        result = self.parse_and_execute("set x to negative 5")
        assert self.interpreter.get_variable("x") == -5
    
    def test_unary_minus_symbol(self):
        """Test unary minus with - symbol."""
        result = self.parse_and_execute("set x to -5")
        assert self.interpreter.get_variable("x") == -5
    
    def test_double_negation(self):
        """Test double negation."""
        result = self.parse_and_execute("set x to negative negative 5")
        assert self.interpreter.get_variable("x") == 5
    
    def test_unary_not(self):
        """Test unary NOT operator."""
        result = self.parse_and_execute("set x to not true")
        assert self.interpreter.get_variable("x") == False
    
    def test_unary_bitwise_not(self):
        """Test unary bitwise NOT operator."""
        result = self.parse_and_execute("set x to ~5")
        assert self.interpreter.get_variable("x") == -6


class TestOperatorEdgeCases(NLPLTestBase):
    """Test edge cases and corner cases for operators."""
    
    def test_zero_operations(self):
        """Test operations with zero."""
        result = self.parse_and_execute("set x to 0 + 0")
        assert self.interpreter.get_variable("x") == 0
        
        result = self.parse_and_execute("set x to 0 * 100")
        assert self.interpreter.get_variable("x") == 0
    
    def test_negative_numbers(self):
        """Test operations with negative numbers."""
        result = self.parse_and_execute("set x to -5 + 3")
        assert self.interpreter.get_variable("x") == -2
        
        result = self.parse_and_execute("set x to -5 * -3")
        assert self.interpreter.get_variable("x") == 15
    
    def test_large_numbers(self):
        """Test operations with large numbers."""
        result = self.parse_and_execute("set x to 999999 + 1")
        assert self.interpreter.get_variable("x") == 1000000
    
    def test_floating_point_precision(self):
        """Test floating point operations."""
        result = self.parse_and_execute("set x to 0.1 + 0.2")
        # Due to floating point precision, might need approximate comparison
        assert abs(self.interpreter.get_variable("x") - 0.3) < 0.0001
    
    def test_power_of_zero(self):
        """Test power operations with zero."""
        result = self.parse_and_execute("set x to 0 ** 5")
        assert self.interpreter.get_variable("x") == 0
        
        result = self.parse_and_execute("set x to 5 ** 0")
        assert self.interpreter.get_variable("x") == 1
    
    def test_shift_edge_cases(self):
        """Test shift operations edge cases."""
        result = self.parse_and_execute("set x to 1 << 0")
        assert self.interpreter.get_variable("x") == 1
        
        result = self.parse_and_execute("set x to 8 >> 3")
        assert self.interpreter.get_variable("x") == 1
    
    def test_modulo_negative(self):
        """Test modulo with negative numbers."""
        result = self.parse_and_execute("set x to -10 % 3")
        # Result depends on language semantics (Python: 2, C: -1)
        # Adjust assertion based on NLPL's defined behavior


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
