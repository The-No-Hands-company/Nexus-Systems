#!/usr/bin/env python3
"""
Test type inference for different array element types.
"""

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def test_type_inference():
    """Test type inference for various array types."""
    
    test_cases = [
        ("Integer array", 'set nums to [1, 2, 3]', "int nums[]"),
        ("Float array", 'set floats to [1.5, 2.5, 3.5]', "double floats[]"),
        ("String array", 'set words to ["hello", "world"]', 'const char* words[]'),
        ("Boolean array", 'set flags to [true, false, true]', "bool flags[]"),
        ("Mixed numeric (int + float)", 'set mixed to [1, 2.5, 3]', "double mixed[]"),
        ("2D int array", 'set grid to [[1, 2], [3, 4]]', "int grid[2][2]"),
    ]
    
    print("="*70)
    print("TYPE INFERENCE TEST SUITE")
    print("="*70)
    
    all_passed = True
    for name, code, expected_decl in test_cases:
        print(f"\nTest: {name}")
        print(f"Code: {code}")
        
        # Parse and generate
        lexer = Lexer(code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        generator = CCodeGenerator(target="c")
        c_code = generator.generate(ast)
        
        # Check if expected declaration is in the code
        if expected_decl in c_code:
            print(f"   PASS: Found '{expected_decl}'")
        else:
            print(f"   FAIL: Expected '{expected_decl}'")
            # Show what we actually got
            lines = [l.strip() for l in c_code.split('\n') if '[]' in l or 'double' in l or 'const char*' in l]
            if lines:
                print(f"      Got: {lines[0]}")
            all_passed = False
    
    print("\n" + "="*70)
    if all_passed:
        print(" ALL TYPE INFERENCE TESTS PASSED!")
    else:
        print(" Some type inference tests failed")
    print("="*70)

    assert all_passed, "One or more type inference tests failed (see output above)"

if __name__ == "__main__":
    import sys
    test_type_inference()
    sys.exit(0)
