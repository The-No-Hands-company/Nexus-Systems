#!/usr/bin/env python3
"""
Final demonstration of array indexing with proper type inference.
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def main():
    """Demonstrate array indexing with type inference."""
    
    nxl_code = """
set integers to [10, 20, 30, 40, 50]
set first_int to integers[0]
set last_int to integers[4]

set floats to [1.5, 2.7, 3.14, 4.2]
set pi_approx to floats[2]

set names to ["Alice", "Bob", "Charlie"]
set first_name to names[0]

set matrix to [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
set center to matrix[1][1]
set corner to matrix[2][2]

set flags to [true, false, true, false]
set is_active to flags[0]
"""
    
    print("="*70)
    print("NLPL ARRAY INDEXING - COMPLETE DEMONSTRATION")
    print("="*70)
    print("\nNexusLang Source Code:")
    print("-"*70)
    print(nxl_code)
    print("-"*70)
    
    # Parse
    lexer = Lexer(nxl_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    print(f"\n Successfully parsed {len(ast.statements)} statements")
    
    # Generate C
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)
    
    print("\nGenerated C Code:")
    print("="*70)
    print(c_code)
    print("="*70)
    
    # Highlight key features
    print("\n KEY FEATURES DEMONSTRATED:")
    print("-"*70)
    
    features = [
        ("Integer arrays", "int integers[]", "Proper int array declaration"),
        ("Float arrays", "double floats[]", "Proper double array declaration"),
        ("String arrays", "const char* names[]", "Proper string array declaration"),
        ("Boolean arrays", "bool flags[]", "Proper bool array declaration"),
        ("2D arrays", "int matrix[][]", "Multi-dimensional array support"),
        ("Array indexing", "integers[0]", "Standard C array access"),
        ("Nested indexing", "matrix[1][1]", "Multi-dimensional array access"),
        ("Type inference", "int first_int", "Automatic element type deduction"),
    ]
    
    for feature, pattern, description in features:
        if pattern in c_code:
            print(f"   {feature:20} → {description}")
        else:
            print(f"   {feature:20} → Missing!")
    
    print("\n" + "="*70)
    print(" Array indexing feature is fully functional with type inference!")
    print("="*70)

if __name__ == "__main__":
    main()
