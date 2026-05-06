#!/usr/bin/env python3
"""Debug script to check class inheritance in LLVM IR generator."""

import sys
sys.path.insert(0, 'src')
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator

print("Starting debug...", flush=True)

code = '''
class Animal
    property name as String
    property age as Integer

class Dog extends Animal
    property breed as String

set dog to new Dog
set dog.name to "Buddy"
set dog.age to 3
set dog.breed to "Golden Retriever"

print text "Dog name: "
print text dog.name

print text "Dog age: "
print dog.age

print text "Dog breed: "
print text dog.breed
'''

lexer = Lexer(code)
tokens = lexer.tokenize()
parser = Parser(tokens)
ast = parser.parse()
gen = LLVMIRGenerator()
ir = gen.generate(ast)

# Print class types info
print('=== Class Types ===')
for name, info in gen.class_types.items():
    print(f'{name}:')
    print(f'  properties: {info["properties"]}')
    print(f'  parent: {info.get("parent", None)}')
    print(f'  _get_all_class_properties: {gen._get_all_class_properties(name)}')
    print()

# Look for the struct definition in IR
print('=== Struct Definitions in IR ===')
lines = ir.split('\n')
for i, line in enumerate(lines):
    if '%Dog = type' in line or '%Animal = type' in line:
        print(f'Line {i}: {line}')
        
# Print first 100 lines of IR
print('\n=== First 100 lines of IR ===')
for i, line in enumerate(lines[:100]):
    print(f'{i}: {line}')
