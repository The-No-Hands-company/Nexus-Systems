#!/usr/bin/env python3
"""
Test array indexing feature.
"""

import sys
sys.path.insert(0, '/run/media/zajferx/Data/dev/The-No-hands-Company/projects/NexusLang/src')

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import IndexExpression, VariableDeclaration

def test_array_indexing():
    """Test parsing array indexing expressions."""
    code = """
set numbers to [1, 2, 3, 4, 5]
set first to numbers[0]
"""
    
    print(f"Testing code:\n{code}\n")
    
    lexer = Lexer(code)
    tokens = lexer.tokenize()
    
    print(f"Tokens: {len(tokens)} tokens")
    for i, tok in enumerate(tokens[:15]):  # Show first 15 tokens
        print(f"  {i}: {tok}")
    print()
    
    parser = Parser(tokens)
    ast = parser.parse()
    
    print(f"AST: {len(ast.statements)} statements")
    for i, stmt in enumerate(ast.statements):
        print(f"  {i}: {type(stmt).__name__}")
        if isinstance(stmt, VariableDeclaration):
            print(f"      variable: {stmt.name}")
            print(f"      value type: {type(stmt.value).__name__}")
            if isinstance(stmt.value, IndexExpression):
                print(f"      array: {type(stmt.value.array_expr).__name__}")
                print(f"      index: {type(stmt.value.index_expr).__name__}")
                print("       Array indexing parsed successfully!")
    
if __name__ == "__main__":
    test_array_indexing()
