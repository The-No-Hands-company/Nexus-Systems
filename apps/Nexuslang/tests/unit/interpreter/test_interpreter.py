"""
Test cases for the NexusLang interpreter.
Tests program execution and runtime behavior using valid NexusLang syntax.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


class TestInterpreter(unittest.TestCase):
    """Test cases for the NexusLang interpreter."""

    def setUp(self):
        """Set up test fixtures before each test method."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)

    def test_variable_declaration_and_assignment(self):
        """Test variable declaration and assignment."""
        source = """
set x to 42
set y to 3.14
set z to "hello"
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("x"), 42)
        self.assertAlmostEqual(self.interpreter.get_variable("y"), 3.14)
        self.assertEqual(self.interpreter.get_variable("z"), "hello")

    def test_function_definition_and_call(self):
        """Test function definition and calling."""
        source = """
function add with a as Integer and b as Integer returns Integer
    return a plus b
end

set result to add with a: 5 and b: 3
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("result"), 8)

    def test_if_statement(self):
        """Test if statement execution (true branch)."""
        source = """
set x to 5
if x is greater than 0
    set result to "Positive"
else
    set result to "Negative"
end
"""
        self.interpreter.interpret(source)
        self.assertEqual(self.interpreter.get_variable("result"), "Positive")

    def test_if_statement_false_branch(self):
        """Test if statement execution (false branch)."""
        source = """
set x to -5
if x is greater than 0
    set result to "Positive"
else
    set result to "Negative"
end
"""
        self.interpreter.interpret(source)
        self.assertEqual(self.interpreter.get_variable("result"), "Negative")

    def test_while_loop(self):
        """Test while loop execution."""
        source = """
set x to 5
set sum to 0
while x is greater than 0
    set sum to sum plus x
    set x to x minus 1
end while
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("sum"), 15)
        self.assertEqual(self.interpreter.get_variable("x"), 0)

    def test_for_loop(self):
        """Test for-each loop execution."""
        source = """
set sum to 0
set numbers to [1, 2, 3, 4, 5]
for each i in numbers
    set sum to sum plus i
end
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("sum"), 15)

    def test_arithmetic_operations(self):
        """Test arithmetic operations."""
        source = """
set a to 10
set b to 3

set sum to a plus b
set diff to a minus b
set product to a times b
set quotient to a divided by b
set remainder to a modulo b
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("sum"), 13)
        self.assertEqual(self.interpreter.get_variable("diff"), 7)
        self.assertEqual(self.interpreter.get_variable("product"), 30)
        self.assertAlmostEqual(self.interpreter.get_variable("quotient"),
                               10 / 3)
        self.assertEqual(self.interpreter.get_variable("remainder"), 1)

    def test_comparison_operations(self):
        """Test comparison operations."""
        source = """
set x to 5
set y to 10

set less_result to x is less than y
set less_eq_result to x is less than or equal to y
set greater_result to x is greater than y
set equal_result to x is equal to y
set not_equal_result to x is not equal to y
"""
        self.interpreter.interpret(source)

        self.assertTrue(self.interpreter.get_variable("less_result"))
        self.assertTrue(self.interpreter.get_variable("less_eq_result"))
        self.assertFalse(self.interpreter.get_variable("greater_result"))
        self.assertFalse(self.interpreter.get_variable("equal_result"))
        self.assertTrue(self.interpreter.get_variable("not_equal_result"))

    def test_logical_operations(self):
        """Test logical operations."""
        source = """
set a to true
set b to false

set and_result to a and b
set or_result to a or b
set not_result to not b
"""
        self.interpreter.interpret(source)

        self.assertFalse(self.interpreter.get_variable("and_result"))
        self.assertTrue(self.interpreter.get_variable("or_result"))
        self.assertTrue(self.interpreter.get_variable("not_result"))

    def test_string_operations(self):
        """Test string concatenation and length."""
        source = """
set str1 to "Hello"
set str2 to "World"

set concat to str1 plus " " plus str2
set length_val to length(str1)
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("concat"), "Hello World")
        self.assertEqual(self.interpreter.get_variable("length_val"), 5)

    def test_error_handling(self):
        """Test that referencing undefined variables raises an error."""
        with self.assertRaises(Exception):
            self.interpreter.interpret("set result to undefined_variable plus 1")

    def test_complex_program(self):
        """Test a recursive factorial function."""
        source = """
function factorial with n as Integer returns Integer
    if n is less than or equal to 1
        return 1
    end
    set nm1 to n minus 1
    set sub to factorial with n: nm1
    return n times sub
end

set fact to factorial with n: 5
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("fact"), 120)


if __name__ == "__main__":
    unittest.main()
