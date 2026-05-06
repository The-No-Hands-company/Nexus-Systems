#!/usr/bin/env python3
"""
Comprehensive test for array indexing feature.
Tests parsing, AST construction, and C code generation.
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import IndexExpression, ListExpression, VariableDeclaration
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def run_test(name, code, expected_patterns):
    """Run a single test case."""
    print(f"\n{'='*70}")
    print(f"TEST: {name}")
    print(f"{'='*70}")
    print(f"NLPL:\n{code}\n")
    
    # Parse
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # Generate C
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)
    
    # Extract main body only
    lines = c_code.split('\n')
    main_start = next(i for i, line in enumerate(lines) if 'int main' in line)
    main_body = '\n'.join(lines[main_start+1:-1])  # Skip main declaration and closing brace
    
    print(f"Generated C (main body):\n{main_body}\n")
    
    # Check patterns
    all_passed = True
    for pattern in expected_patterns:
        if pattern in main_body:
            print(f"   Found: {pattern}")
        else:
            print(f"   Missing: {pattern}")
            all_passed = False
    
    return all_passed

def main():
    """Run all test cases."""
    print("="*70)
    print("COMPREHENSIVE ARRAY INDEXING TEST SUITE")
    print("="*70)
    
    tests = [
        (
            "Simple array indexing",
            "set numbers to [10, 20, 30]\nset x to numbers[0]",
            ["{10, 20, 30}", "numbers[0]"]
        ),
        (
            "Nested array (matrix)",
            "set matrix to [[1, 2], [3, 4]]\nset val to matrix[1][0]",
            ["{{1, 2}, {3, 4}}", "matrix[1][0]"]
        ),
        (
            "Array indexing in sequence",
            "set arr to [5, 6, 7]\nset a to arr[0]\nset b to arr[1]\nset c to arr[2]",
            ["arr[0]", "arr[1]", "arr[2]"]
        ),
        (
            "Mixed expressions",
            "set data to [100, 200]\nset first to data[0]",
            ["{100, 200}", "data[0]"]
        ),
    ]
    
    results = []
    for name, code, patterns in tests:
        passed = run_test(name, code, patterns)
        results.append((name, passed))
    
    # Summary
    print("\n" + "="*70)
    print("TEST SUMMARY")
    print("="*70)
    
    passed_count = sum(1 for _, passed in results if passed)
    total_count = len(results)
    
    for name, passed in results:
        status = " PASS" if passed else " FAIL"
        print(f"{status}: {name}")
    
    print(f"\nResult: {passed_count}/{total_count} tests passed")
    
    if passed_count == total_count:
        print("\n ALL TESTS PASSED! Array indexing feature is complete.")
        return 0
    else:
        print(f"\n  {total_count - passed_count} test(s) failed")
        return 1

if __name__ == "__main__":
    sys.exit(main())
