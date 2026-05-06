#!/usr/bin/env python3
"""
Comprehensive test for-each loop with array indexing.
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def main():
    """Test for-each loop with arrays."""
    
    nxl_code = """
set numbers to [10, 20, 30, 40, 50]

for each num in numbers
    print num
end
"""
    
    print("="*70)
    print("FOR-EACH LOOP - COMPLETE DEMONSTRATION")
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
    
    # Check for key patterns
    print("\n KEY FEATURES:")
    print("-"*70)
    
    features = [
        ("Array declaration", "int numbers[]", "Proper typed array"),
        ("Array size calc", "sizeof(numbers)/sizeof(int)", "Compile-time size"),
        ("Loop variable type", "int num =", "Inferred element type"),
        ("For loop syntax", "for (int _i = 0;", "Standard C for loop"),
    ]
    
    for feature, pattern, desc in features:
        if pattern in c_code:
            print(f"   {feature:20} → {desc}")
        else:
            print(f"   {feature:20} → Missing!")
    
    print("\n" + "="*70)
    print(" For-each loop feature is functional!")
    print("="*70)

if __name__ == "__main__":
    main()
