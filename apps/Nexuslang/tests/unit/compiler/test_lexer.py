"""
Test cases for the NexusLang lexer.
Tests token recognition, error handling, and edge cases.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest
from nexuslang.parser.lexer import Lexer, TokenType

class TestLexer(unittest.TestCase):
    """Test cases for the NexusLang lexer."""

    def setUp(self):
        """Set up test fixtures before each test method."""
        self.lexer = None

    def test_keywords(self):
        """Test recognition of core keywords (lower-case)."""
        keywords = {
            "if": TokenType.IF,
            "else": TokenType.ELSE,
            "while": TokenType.WHILE,
            "for": TokenType.FOR,
            "function": TokenType.FUNCTION,
            "return": TokenType.RETURN,
            "class": TokenType.CLASS,
            "import": TokenType.IMPORT,
            "from": TokenType.FROM,
            "as": TokenType.AS,
            "returns": TokenType.RETURNS,
            "integer": TokenType.INTEGER,
            "float": TokenType.FLOAT,
            "string": TokenType.STRING,
            "boolean": TokenType.BOOLEAN,
            "null": TokenType.NULL,
            "list": TokenType.LIST,
            "dictionary": TokenType.DICTIONARY,
        }

        for keyword, expected_type in keywords.items():
            self.lexer = Lexer(keyword)
            token = self.lexer.get_next_token()
            self.assertEqual(token.type, expected_type,
                             msg=f"Keyword '{keyword}' should produce {expected_type}")
            self.assertEqual(token.line, 1)

    def test_operators(self):
        """Test recognition of operators."""
        operators = {
            "+": TokenType.PLUS,
            "-": TokenType.MINUS,
            "*": TokenType.TIMES,
            "/": TokenType.DIVIDED_BY,
            "=": TokenType.EQUALS,
            "==": TokenType.EQUAL_TO,
            "!=": TokenType.NOT_EQUAL_TO,
            "<": TokenType.LESS_THAN,
            "<=": TokenType.LESS_THAN_OR_EQUAL_TO,
            ">": TokenType.GREATER_THAN,
            ">=": TokenType.GREATER_THAN_OR_EQUAL_TO,
            "and": TokenType.AND,
            "or": TokenType.OR,
            "not": TokenType.NOT,
        }

        for operator, expected_type in operators.items():
            self.lexer = Lexer(operator)
            token = self.lexer.get_next_token()
            self.assertEqual(token.type, expected_type,
                             msg=f"Operator '{operator}' should produce {expected_type}")

    def test_identifiers(self):
        """Test recognition of valid identifiers."""
        identifiers = [
            "variable",
            "myVariable",
            "my_variable",
            "variable123",
            "_variable",
            "v123",
        ]

        for identifier in identifiers:
            self.lexer = Lexer(identifier)
            token = self.lexer.get_next_token()
            self.assertEqual(token.type, TokenType.IDENTIFIER)
            self.assertEqual(token.value, identifier)

    def test_numbers(self):
        """Test recognition of integer and float literals."""
        numbers = [
            ("123", TokenType.INTEGER_LITERAL, 123),
            ("123.456", TokenType.FLOAT_LITERAL, 123.456),
            ("0", TokenType.INTEGER_LITERAL, 0),
            ("0.0", TokenType.FLOAT_LITERAL, 0.0),
        ]

        for number_str, expected_type, expected_value in numbers:
            self.lexer = Lexer(number_str)
            token = self.lexer.get_next_token()
            self.assertEqual(token.type, expected_type,
                             msg=f"'{number_str}' should produce {expected_type}")
            self.assertEqual(token.value, expected_value)

    def test_strings(self):
        """Test recognition of string literals."""
        strings = [
            ('"hello"', "hello"),
            ("'world'", "world"),
            ('"hello\\nworld"', "hello\nworld"),
            ('"hello\\tworld"', "hello\tworld"),
        ]

        for string_str, expected_value in strings:
            self.lexer = Lexer(string_str)
            token = self.lexer.get_next_token()
            self.assertEqual(token.type, TokenType.STRING_LITERAL)
            self.assertEqual(token.value, expected_value)

    def test_whitespace(self):
        """Test handling of whitespace."""
        self.lexer = Lexer("  \t\n  ")
        token = self.lexer.get_next_token()
        self.assertEqual(token.type, TokenType.EOF)

    def test_comments(self):
        """Test handling of comments."""
        self.lexer = Lexer("# This is a comment\nvariable")
        token = self.lexer.get_next_token()
        self.assertEqual(token.type, TokenType.IDENTIFIER)
        self.assertEqual(token.value, "variable")

    def test_error_handling(self):
        """Test error handling for truly invalid characters."""
        invalid_chars = ["$"]

        for char in invalid_chars:
            self.lexer = Lexer(char)
            with self.assertRaises(Exception) as context:
                self.lexer.get_next_token()
            err = str(context.exception)
            self.assertTrue(
                "Unexpected character" in err or "character" in err.lower(),
                msg=f"Expected character error for '{char}', got: {err}",
            )

    def test_line_and_column_tracking(self):
        """Test line and column tracking."""
        source = "variable\n  another_variable\n    third_variable"
        self.lexer = Lexer(source)

        # First line
        token = self.lexer.get_next_token()
        self.assertEqual(token.line, 1)

        # Second line (get_next_token skips INDENT)
        token = self.lexer.get_next_token()
        self.assertEqual(token.line, 2)

        # Third line (get_next_token skips INDENT)
        token = self.lexer.get_next_token()
        self.assertEqual(token.line, 3)

    def test_complex_program(self):
        """Test lexing a NexusLang program produces expected core token types."""
        source = """
set x to 42
set greeting to "Hello"
if x is greater than 0
    print text greeting
end if
"""
        self.lexer = Lexer(source)
        tokens = []
        while True:
            token = self.lexer.get_next_token()
            if token.type == TokenType.EOF:
                break
            tokens.append(token)

        # Verify key token types appear
        types = {t.type for t in tokens}
        self.assertIn(TokenType.SET, types)
        self.assertIn(TokenType.IF, types)
        self.assertIn(TokenType.PRINT, types)
        self.assertIn(TokenType.IDENTIFIER, types)
        self.assertIn(TokenType.INTEGER_LITERAL, types)
        self.assertIn(TokenType.STRING_LITERAL, types)


if __name__ == "__main__":
    unittest.main()
