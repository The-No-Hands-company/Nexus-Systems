"""
Comprehensive test file for error reporting in the NexusLang parser and lexer.
"""

import sys
import traceback
from nexuslang.parser.lexer import Lexer, TokenType
from nexuslang.parser.parser import Parser

def test_lexer_invalid_character():
    """Test lexer error reporting for invalid characters."""
    print("Testing lexer error for invalid character...")
    source_code = "Create an integer called x and set it to 10.\nCreate an integer called y and set it to $invalid."
    
    lexer = Lexer(source_code)
    
    try:
        tokens = lexer.tokenize()
        print("Expected a lexical error but none was raised")
        raise AssertionError("Expected a lexical error but none was raised")
    except Exception as e:
        error_message = str(e)
        print(f"Lexer error: {error_message}")
        # Check that the error message contains the line and column
        assert "line 2" in error_message, "Error message should contain line number"
        # Check that the error message contains a pointer to the error
        assert "^" in error_message, "Error message should contain pointer"
        print("Lexer error for invalid character test passed!")

def test_parser_missing_type():
    """Test parser error reporting for missing type in variable declaration."""
    print("Testing parser error for missing type...")
    source_code = "Create x and set it to 10."  # Missing type annotation
    
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    
    try:
        parser.parse()
        print("Expected a syntax error but none was raised")
        raise AssertionError("Expected a syntax error but none was raised")
    except Exception as e:
        error_message = str(e)
        print(f"Parser error: {error_message}")
        # Check that the error message contains the line and column
        assert "Syntax Error" in error_message, "Error message should contain 'Syntax Error'"
        # Check that the error message contains the context
        assert "Create x and set it to 10" in error_message, "Error message should contain context"
        # Check that the error message contains a pointer to the error
        assert "^" in error_message, "Error message should contain pointer"
        print("Parser error for missing type test passed!")

def test_parser_invalid_function_call():
    """Test parser error reporting for invalid function call."""
    print("Testing parser error for invalid function call...")
    source_code = """
    Create an integer called x and set it to 10.
    Call undefined_function with x.
    """
    
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    
    try:
        parser.parse()
        print("Expected a syntax error but none was raised")
        raise AssertionError("Expected a syntax error but none was raised")
    except Exception as e:
        error_message = str(e)
        print(f"Parser error: {error_message}")
        # Check that the error message contains the line and column
        assert "Syntax Error" in error_message, "Error message should contain 'Syntax Error'"
        # Check that the error message contains a pointer to the error or location info
        assert ("^" in error_message or "line" in error_message.lower() or "-->" in error_message), \
            "Error message should contain pointer or location info"
        print("Parser error for invalid function call test passed!")

def test_parser_if_statement_error():
    """Test parser error reporting for syntax error in if statement condition."""
    print("Testing parser error for syntax error in if statement condition...")
    # This example has a syntax error in the if statement condition
    source_code = """
    Create an integer called x and set it to 10.
    If x equals then
        Create an integer called y and set it to 20.
    End if.
    """
    
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    
    try:
        parser.parse()
        print("Expected a syntax error but none was raised")
        raise AssertionError("Expected a syntax error but none was raised")
    except Exception as e:
        error_message = str(e)
        print(f"Parser error: {error_message}")
        # Check that the error message contains the line and column
        assert "Syntax Error" in error_message, "Error message should contain 'Syntax Error'"
        # Check that the error message contains a pointer to the error
        assert "^" in error_message, "Error message should contain pointer"
        print("Parser error for if statement condition test passed!")

if __name__ == "__main__":
    try:
        # Run all tests
        test_lexer_invalid_character()
        test_parser_missing_type()
        test_parser_invalid_function_call()
        test_parser_if_statement_error()
        print("\nAll comprehensive error tests passed!")
    except Exception as e:
        print(f"Test failed with error: {e}")
        traceback.print_exc() 