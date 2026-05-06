#!/usr/bin/env python3
"""
Minimal test to debug parser issue.
"""

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'src')))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime

code = "set x to 5\n"

print("=" * 60)
print("Testing:", repr(code))
print("=" * 60)

# Tokenize
lexer = Lexer(code)
tokens = lexer.tokenize()

print("\nTokens:")
for i, tok in enumerate(tokens):
    print(f"  {i}: {tok.type.name:20s} '{tok.lexeme}'")

print("\n" + "=" * 60)
print("Parsing...")
print("=" * 60 + "\n")

# Parse
parser = Parser(tokens)
try:
    ast = parser.parse()
    print(f"\nAST created with {len(ast.statements)} statements")
    
    # Execute
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    interpreter.interpret(ast)
    
    # Get result
    try:
        result = interpreter.get_variable('x')
        print(f"\nSuccess! x = {result}")
    except NameError as e:
        print(f"\nVariable not found: {e}")
        
except Exception as e:
    print(f"\nError: {e}")
    import traceback
    traceback.print_exc()
