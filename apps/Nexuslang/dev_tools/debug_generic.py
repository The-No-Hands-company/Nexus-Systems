#!/usr/bin/env python3
"""Debug generic class parsing."""

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser

code = '''
define a class called Container with T as a type parameter
    properties:
        value: T
    methods:
        define a method called get that returns T
            return self.value
'''

lexer = Lexer(code)
tokens = lexer.tokenize()

print("=== Tokens ===")
for i, tok in enumerate(tokens):
    print(f"{i}: {tok.type.name:20} '{tok.lexeme}'")

print("\n=== Parsing ===")
try:
    parser = Parser(tokens)
    ast = parser.parse()
    print("SUCCESS!")
    print(ast)
except Exception as e:
    print(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
