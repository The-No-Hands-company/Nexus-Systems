"""
Script to run the comprehensive error reporting test.
"""

import sys
import traceback

sys.path.append('.')

try:
    from src.tests.test_comprehensive_errors import (
        test_lexer_invalid_character,
        test_parser_missing_type,
        test_parser_invalid_function_call,
        test_parser_if_statement_error
    )
    
    success = True
    
    print("Running comprehensive error tests...\n")
    
    # Run all tests
    success = test_lexer_invalid_character() and success
    print()
    success = test_parser_missing_type() and success
    print()
    success = test_parser_invalid_function_call() and success
    print()
    success = test_parser_if_statement_error() and success
    
    if success:
        print("\nAll comprehensive error tests passed!")
    else:
        print("\nSome tests failed!")
        
except Exception as e:
    print(f"Test failed with error: {e}")
    traceback.print_exc() 