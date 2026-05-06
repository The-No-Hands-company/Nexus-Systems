"""
Script to run the error reporting test.
"""

import sys
import traceback

sys.path.append('.')

try:
    from src.tests.test_error_reporting import test_lexer_error_reporting, test_parser_error_reporting
    
    print("Running lexer error reporting test...")
    test_lexer_error_reporting()
    
    print("\nRunning parser error reporting test...")
    test_parser_error_reporting()
    
    print("\nAll tests passed!")
except Exception as e:
    print(f"Test failed with error: {e}")
    traceback.print_exc() 