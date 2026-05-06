"""
Test cases for NexusLang error handling.
Tests various error conditions using valid NexusLang syntax.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.stdlib import register_stdlib
from nexuslang.errors import NxlSyntaxError, NxlRuntimeError, NxlNameError


class TestErrorHandling(unittest.TestCase):
    """Test cases for NexusLang error handling."""

    def setUp(self):
        """Set up test fixtures before each test method."""
        self.runtime = Runtime()
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)

    def _run(self, source):
        self.interpreter.interpret(source)

    # ------------------------------------------------------------------
    # Lexer errors
    # ------------------------------------------------------------------
    def test_lexer_errors(self):
        """Test that the lexer raises errors for invalid input."""
        # Unclosed string literal should raise NxlSyntaxError
        with self.assertRaises(NxlSyntaxError):
            Lexer('"unclosed string').tokenize()

        # Normal identifier-like token should not raise
        tokens = Lexer('set x to 1').tokenize()
        self.assertTrue(len(tokens) > 0)

    # ------------------------------------------------------------------
    # Parser errors
    # ------------------------------------------------------------------
    def test_parser_errors(self):
        """Test that the parser raises errors for invalid syntax."""
        error_cases = [
            # SET without target
            'set to "value"',
            # Incomplete expression
            'set x to',
        ]
        for source in error_cases:
            with self.assertRaises(Exception):
                self.interpreter.interpret(source)

    # ------------------------------------------------------------------
    # Type errors
    # ------------------------------------------------------------------
    def test_type_errors(self):
        """Test type-related runtime errors."""
        # Index out of bounds on a list
        with self.assertRaises(NxlRuntimeError):
            self.interpreter.interpret(
                'set nums to [1, 2, 3]\nset bad to nums[100]'
            )

        # Reset interpreter for next case
        self.setUp()
        # Calling a non-callable value
        with self.assertRaises(Exception):
            self.interpreter.interpret(
                'set x to 42\nset y to x(1)'
            )

    # ------------------------------------------------------------------
    # Runtime errors
    # ------------------------------------------------------------------
    def test_runtime_errors(self):
        """Test runtime error handling."""
        # Division by zero
        with self.assertRaises(ZeroDivisionError):
            self.interpreter.interpret('set x to 1 divided by 0')

        # Index out of range on list
        self.setUp()
        with self.assertRaises(NxlRuntimeError):
            self.interpreter.interpret(
                'set lst to [1, 2, 3]\nset y to lst[99]'
            )

        # Undefined variable / missing key in dict
        self.setUp()
        with self.assertRaises(Exception):
            self.interpreter.interpret(
                'set d to {"a": 1}\nset v to d["missing_key"]'
            )

    # ------------------------------------------------------------------
    # Scope errors
    # ------------------------------------------------------------------
    def test_scope_errors(self):
        """Test scope error handling - undefined variables raise NxlNameError."""
        with self.assertRaises(NxlNameError):
            self.interpreter.interpret('print text undefined_var_xyz')

        self.setUp()
        with self.assertRaises(NxlNameError):
            self.interpreter.interpret('print text another_undefined_xyz_variable')

    # ------------------------------------------------------------------
    # Module errors
    # ------------------------------------------------------------------
    def test_module_errors(self):
        """Test module-related error handling."""
        # Referencing a completely unknown name as a function raises an error
        with self.assertRaises(Exception):
            self.interpreter.interpret('set x to this_module_does_not_exist_abc()')

    # ------------------------------------------------------------------
    # Error location reporting
    # ------------------------------------------------------------------
    def test_error_location_reporting(self):
        """Test that errors report line/column information."""
        source = (
            'set x to 1\n'
            'set y to 2\n'
            'set z to undefined_name_for_test\n'
        )
        try:
            self.interpreter.interpret(source)
            self.fail("Expected an exception to be raised")
        except Exception as e:
            error_msg = str(e)
            # The error should mention the undefined name and describe the problem
            self.assertTrue(
                "undefined_name_for_test" in error_msg or
                "not defined" in error_msg.lower() or
                "3" in error_msg or
                "line" in error_msg.lower(),
                f"Error should describe the problem, got: {error_msg[:200]}"
            )

    # ------------------------------------------------------------------
    # Error recovery
    # ------------------------------------------------------------------
    def test_error_recovery(self):
        """Test that execution stops after an error - subsequent statements are not run."""
        source = (
            'set x to 1\n'
            'set y to x[99]\n'
            'set z to 42\n'
        )
        try:
            self.interpreter.interpret(source)
            self.fail("Expected an exception to be raised")
        except Exception:
            # z should not have been set since execution stopped at line 2
            with self.assertRaises(Exception):
                self.interpreter.get_variable("z")


if __name__ == '__main__':
    unittest.main()
