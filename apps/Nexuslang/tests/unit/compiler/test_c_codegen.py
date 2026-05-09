#!/usr/bin/env python3
"""
Test C code generation for array indexing.
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def test_array_indexing_c_gen():
    """Test C code generation for array indexing."""
    code = """
set numbers to [1, 2, 3, 4, 5]
set first to numbers[0]
set second to numbers[1]
"""
    
    print("=== Testing C Code Generation for Array Indexing ===\n")
    print(f"NLPL Code:\n{code}\n")
    
    # Parse the code
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    print(f" Parsed successfully: {len(ast.statements)} statements\n")
    
    # Generate C code
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)
    
    print("Generated C Code:")
    print("=" * 60)
    print(c_code)
    print("=" * 60)
    
    # Check if the expected patterns are in the generated code
    checks = [
        ("Array literal", "{1, 2, 3, 4, 5}"),
        ("First index", "numbers[0]"),
        ("Second index", "numbers[1]"),
    ]
    
    print("\nVerification:")
    missing_patterns = []
    for name, pattern in checks:
        if pattern in c_code:
            print(f"   {name}: Found '{pattern}'")
        else:
            print(f"   {name}: Missing '{pattern}'")
            missing_patterns.append((name, pattern))
    
    if not missing_patterns:
        print("\n All checks passed!")
    else:
        print("\n Some checks failed")

    assert not missing_patterns, f"Missing generated C patterns: {missing_patterns}"

if __name__ == "__main__":
    try:
        test_array_indexing_c_gen()
    except AssertionError:
        sys.exit(1)
    sys.exit(0)
