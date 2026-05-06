#!/usr/bin/env python3
"""
Test nested array indexing (matrix[i][j]).
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.c_generator import CCodeGenerator

def test_nested_indexing():
    """Test nested array indexing."""
    code = """
set matrix to [[1, 2], [3, 4]]
set value to matrix[0][1]
set row to matrix[1]
set cell to row[0]
"""
    
    print("=== Testing Nested Array Indexing ===\n")
    print(f"NLPL Code:\n{code}\n")
    
    # Parse
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    print(f" Parsed: {len(ast.statements)} statements\n")
    
    # Analyze AST for nested indexing
    from nexuslang.parser.ast import IndexExpression, VariableDeclaration
    for i, stmt in enumerate(ast.statements):
        if isinstance(stmt, VariableDeclaration):
            print(f"Statement {i}: set {stmt.name} to {type(stmt.value).__name__}")
            if isinstance(stmt.value, IndexExpression):
                print(f"  → Indexing {type(stmt.value.array_expr).__name__}[{type(stmt.value.index_expr).__name__}]")
                # Check if nested
                if isinstance(stmt.value.array_expr, IndexExpression):
                    print(f"  → NESTED: {type(stmt.value.array_expr.array_expr).__name__}[...][...]")
    
    print()
    
    # Generate C
    generator = CCodeGenerator(target="c")
    c_code = generator.generate(ast)
    
    print("Generated C Code:")
    print("=" * 60)
    print(c_code)
    print("=" * 60)
    
    # Verify patterns
    checks = [
        ("Nested literal", "{{1, 2}, {3, 4}}"),
        ("Nested indexing", "matrix[0][1]"),
        ("Single index", "matrix[1]"),
        ("Chained access", "row[0]"),
    ]
    
    print("\nVerification:")
    for name, pattern in checks:
        if pattern in c_code:
            print(f"   {name}: '{pattern}'")
        else:
            print(f"   {name}: Missing '{pattern}'")

if __name__ == "__main__":
    test_nested_indexing()
