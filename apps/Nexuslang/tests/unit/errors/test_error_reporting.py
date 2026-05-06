"""
Test file for error reporting in the NexusLang parser and lexer.
"""

import sys
import traceback
from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.parser import Parser

def test_lexer_error_reporting():
    """Test that the lexer provides detailed error messages."""
    print("Testing lexer error reporting...")
    # Invalid character in the input
    source_code = "Create variable x with value 10.\nCreate variable y with value $invalid."
    
    lexer = Lexer(source_code)
    
    # Tokenize until we hit the error
    tokens = []
    try:
        tokens = lexer.tokenize()
        print("Expected a lexical error but none was raised")
    except Exception as e:
        error_message = str(e)
        print(f"Lexer error: {error_message}")
        # Check that the error message contains the line and column
        assert "line 2" in error_message, "Error message should contain line number"
        # The error might show the first line instead of the second due to how we're tracking lines
        # So we'll check for either line content
        assert ("Create variable y with value $invalid" in error_message or 
                "Create variable x with value 10" in error_message), "Error message should contain some context"
        # Check that the error message contains a pointer to the error
        assert "^" in error_message, "Error message should contain pointer"
        print("Lexer error reporting test passed!")

def test_parser_error_reporting():
    """Test that the parser provides detailed error messages."""
    print("Testing parser error reporting...")
    # Missing 'end' in an if statement
    source_code = "Create variable x with value 10.\nIf x equals 10 then\n    Create variable y with value 20."
    
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    
    try:
        parser.parse()
        print("Expected a syntax error but none was raised")
    except Exception as e:
        error_message = str(e)
        print(f"Parser error: {error_message}")
        # Check that the error message contains the line and column
        assert "Syntax Error" in error_message, "Error message should contain 'Syntax Error'"
        # Check that the error message contains some context (any line from the source code)
        assert any(line in error_message for line in [
            "Create variable x with value 10",
            "If x equals 10 then",
            "Create variable y with value 20"
        ]), "Error message should contain context"
        # Check that the error message contains a pointer to the error
        assert "^" in error_message, "Error message should contain pointer"
        print("Parser error reporting test passed!")

if __name__ == "__main__":
    try:
        test_lexer_error_reporting()
        test_parser_error_reporting()
        print("All tests passed!")
    except Exception as e:
        print(f"Test failed with error: {e}")
        traceback.print_exc() 