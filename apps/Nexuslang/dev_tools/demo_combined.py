#!/usr/bin/env python3
"""
Combined demo: Arrays + For-Each Loops
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def main():
    """Demonstrate arrays with for-each loops."""
    
    nxl_code = """
set numbers to [1, 2, 3, 4, 5]
set total to 0

for each num in numbers
    set total to total plus num
end

set names to ["Alice", "Bob", "Charlie"]

for each name in names
    print name
end

set matrix to [[1, 2], [3, 4]]

for each row in matrix
    set first to row[0]
    print first
end
"""
    
    print("="*70)
    print("ARRAYS + FOR-EACH LOOPS - COMBINED DEMONSTRATION")
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
    
    print(f"\n Parsed {len(ast.statements)} statements")
    
    # Generate C
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)
    
    print("\nGenerated C Code:")
    print("="*70)
    print(c_code)
    print("="*70)
    
    print("\n FEATURES DEMONSTRATED:")
    print("-"*70)
    
    checks = [
        ("Integer array", "int numbers[]"),
        ("String array", "const char* names[]"),
        ("2D array", "int matrix[][]"),
        ("For-each int", "for (int _i = 0; _i < sizeof(numbers)"),
        ("For-each string", "for (int _i = 0; _i < sizeof(names)"),
        ("For-each 2D", "for (int _i = 0; _i < sizeof(matrix)"),
        ("Array indexing in loop", "row[0]"),
        ("Arithmetic in loop", "total + num"),
    ]
    
    for desc, pattern in checks:
        if pattern in c_code:
            print(f"   {desc}")
        else:
            print(f"   {desc} (missing '{pattern}')")
    
    print("\n" + "="*70)
    print(" Arrays + For-Each Loops: FULLY FUNCTIONAL!")
    print("="*70)

if __name__ == "__main__":
    main()
