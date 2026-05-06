"""
Shared test utilities for NexusLang test suite.
Common helper functions and fixtures.
"""

import sys
import os
import textwrap

# Add src directory to path for imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


class NLPLTestBase:
    """Base class for NexusLang tests with common utilities."""
    
    def setup_method(self):
        """Set up test environment before each test."""
        self.runtime = Runtime()
        # Register standard library functions
        register_stdlib(self.runtime)
        self.interpreter = Interpreter(self.runtime)
    
    def parse_and_execute(self, code):
        """
        Helper method to parse and execute NexusLang code.
        
        Args:
            code: NexusLang source code to execute
            
        Returns:
            Result of interpretation
        """
        # Remove leading whitespace from test code (handles triple-quoted strings with indentation)
        code = textwrap.dedent(code)
        
        # Ensure code ends with newline for proper parsing
        if not code.endswith('\n'):
            code += '\n'
        
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        return self.interpreter.interpret(ast)
    
    def get_variable(self, name):
        """Get a variable value from the interpreter."""
        return self.interpreter.get_variable(name)
    
    def assert_variable(self, name, expected_value):
        """Assert that a variable has the expected value."""
        actual = self.get_variable(name)
        assert actual == expected_value, f"Expected {name}={expected_value}, got {actual}"


def parse_nxl(code):
    """
    Parse NexusLang code and return AST.
    
    Args:
        code: NexusLang source code
        
    Returns:
        AST representing the code
    """
    if not code.endswith('\n'):
        code += '\n'
    
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    return parser.parse()


def execute_nxl(code):
    """
    Execute NexusLang code and return runtime and interpreter.
    
    Args:
        code: NexusLang source code
        
    Returns:
        Tuple of (runtime, interpreter)
    """
    if not code.endswith('\n'):
        code += '\n'
    
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    interpreter.interpret(ast)
    
    return runtime, interpreter
