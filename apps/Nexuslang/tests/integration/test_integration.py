"""
Integration test cases for the NexusLang compiler pipeline.
Tests the entire process from source code to execution.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest
import tempfile
import shutil
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


class TestIntegration(unittest.TestCase):
    """Integration test cases for the NexusLang compiler pipeline."""

    def setUp(self):
        """Set up test fixtures before each test method."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)

        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        """Clean up after each test method."""
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_hello_world(self):
        """Test a Hello World program runs without error."""
        source = 'print text "Hello, World!"'
        # Just verify no exception is raised
        self.interpreter.interpret(source)

    def test_calculator(self):
        """Test a simple calculator using NexusLang functions."""
        source = """
function add with a as Integer and b as Integer returns Integer
    return a plus b
end

function sub with a as Integer and b as Integer returns Integer
    return a minus b
end

function mul with a as Integer and b as Integer returns Integer
    return a times b
end

set x to 10
set y to 3

set sum to add with a: x and b: y
set diff to sub with a: x and b: y
set product to mul with a: x and b: y
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("sum"), 13)
        self.assertEqual(self.interpreter.get_variable("diff"), 7)
        self.assertEqual(self.interpreter.get_variable("product"), 30)

    def test_file_operations(self):
        """Test file write and read using stdlib functions."""
        file_path = os.path.join(self.temp_dir, "test.txt")
        content = "Hello, NLPL!"

        source = f"""
set file_path to "{file_path}"
set content to "{content}"
set ok to write_file(file_path, content)
set read_back to read_file(file_path)
set exists to file_exists(file_path)
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("read_back"), content)
        self.assertTrue(self.interpreter.get_variable("exists"))

    def test_math_operations(self):
        """Test math stdlib functions."""
        source = """
set sqrt_result to sqrt(25.0)
set abs_result to abs(-42)
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("sqrt_result"), 5.0)
        self.assertEqual(self.interpreter.get_variable("abs_result"), 42)

    def test_string_operations(self):
        """Test string stdlib functions."""
        source = """
set s to "hello"
set upper to uppercase(s)
set lower to lowercase("WORLD")
set len_val to length(s)
set concat to s plus " world"
"""
        self.interpreter.interpret(source)

        self.assertEqual(self.interpreter.get_variable("upper"), "HELLO")
        self.assertEqual(self.interpreter.get_variable("lower"), "world")
        self.assertEqual(self.interpreter.get_variable("len_val"), 5)
        self.assertEqual(self.interpreter.get_variable("concat"), "hello world")

    def test_complex_program(self):
        """Test a complete program using multiple features."""
        source = """
function fibonacci with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    set n1 to n minus 1
    set n2 to n minus 2
    set f1 to fibonacci with n: n1
    set f2 to fibonacci with n: n2
    return f1 plus f2
end

set numbers to [0, 1, 2, 3, 4, 5, 6, 7]
set results to []
for each n in numbers
    set fib_n to fibonacci with n: n
    append fib_n to results
end

set total to 0
for each r in results
    set total to total plus r
end
"""
        self.interpreter.interpret(source)

        expected_fibs = [0, 1, 1, 2, 3, 5, 8, 13]
        self.assertEqual(self.interpreter.get_variable("results"), expected_fibs)
        self.assertEqual(self.interpreter.get_variable("total"), sum(expected_fibs))


if __name__ == "__main__":
    unittest.main()
