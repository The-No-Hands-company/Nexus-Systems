#!/usr/bin/env python3
"""
Script to update test files to use NLPLTestBase.
"""

import re
import sys

def update_test_file(filepath):
    """Update a test file to use NLPLTestBase."""
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Remove old setup_method and parse_and_execute methods
    # Pattern to match setup_method
    setup_pattern = r'    def setup_method\(self\):.*?        self\.interpreter = Interpreter\(self\.runtime\)\s*\n'
    content = re.sub(setup_pattern, '', content, flags=re.DOTALL)
    
    # Pattern to match parse_and_execute
    parse_pattern = r'    def parse_and_execute\(self, code\):.*?        return self\.interpreter\.interpret\(ast\)\s*\n'
    content = re.sub(parse_pattern, '', content, flags=re.DOTALL)
    
    # Update class definitions to inherit from NLPLTestBase
    content = re.sub(r'class (Test\w+):', r'class \1(NLPLTestBase):', content)
    
    # Remove duplicate imports that test_utils already has
    lines_to_remove = [
        'from src.nlpl.parser.lexer import',
        'from src.nlpl.parser.parser import',
        'from src.nlpl.interpreter.interpreter import',
        'from src.nlpl.runtime.runtime import'
    ]
    
    lines = content.split('\n')
    filtered_lines = []
    skip_next = False
    
    for line in lines:
        if any(line.strip().startswith(prefix) for prefix in lines_to_remove):
            skip_next = True
            continue
        filtered_lines.append(line)
    
    content = '\n'.join(filtered_lines)
    
    # Remove references to undefined Runtime, Lexer, Parser, etc
    content = content.replace('Runtime()', 'Runtime()  # from test_utils')
    content = content.replace('Lexer(', 'Lexer(  # from test_utils')
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"Updated {filepath}")

if __name__ == '__main__':
    files = [
        'tests/test_operators.py',
        'tests/test_while_loops.py',
        'tests/test_dictionaries.py',
        'tests/test_strings.py',
        'tests/test_control_flow.py'
    ]
    
    for filepath in files:
        try:
            update_test_file(filepath)
        except Exception as e:
            print(f"Error updating {filepath}: {e}")
