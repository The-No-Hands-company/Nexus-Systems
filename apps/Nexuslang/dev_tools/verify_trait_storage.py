#!/usr/bin/env python3
"""Debug script to verify trait storage in interpreter."""

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime

code = '''
define a trait called Comparable
    that requires a method called compare with other returning Integer
end trait

define a trait called Printable
    that requires a method called to_string returning String
end trait
'''

# Parse
lexer = Lexer(code)
tokens = lexer.tokenize()
parser = Parser(tokens)
ast = parser.parse()

# Interpret
runtime = Runtime()
interpreter = Interpreter(runtime)
interpreter.interpret(ast)

# Check trait storage
print("Traits stored in interpreter:")
if hasattr(interpreter, 'traits'):
    for name, trait_def in interpreter.traits.items():
        print(f"  {name}:")
        print(f"    Required methods: {[m.name for m in trait_def.required_methods]}")
        print(f"    Provided methods: {[m.name for m in trait_def.provided_methods]}")
else:
    print("  No traits attribute found")
