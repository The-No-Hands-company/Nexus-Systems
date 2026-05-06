#!/usr/bin/env python3
"""Debug script to trace for-each loop code execution."""

import sys
import os

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator

# Read test file
with open('test_foreach_minimal.nxl', 'r') as f:
    source = f.read()

# Parse
lexer = Lexer(source)
tokens = lexer.tokenize()
parser = Parser(tokens)
ast = parser.parse()

print("=== AST Structure ===")
for stmt in ast.statements:
    print(f"Statement type: {type(stmt).__name__}")
    if hasattr(stmt, '__dict__'):
        print(f"  Attributes: {stmt.__dict__}")
print()

# Generate LLVM IR with tracing
generator = LLVMIRGenerator()

# Monkey-patch to trace method calls
original_generate_for_loop = generator._generate_for_loop
original_generate_foreach_loop = generator._generate_foreach_loop

def traced_generate_for_loop(node, indent='  '):
    print(f"TRACE: _generate_for_loop called")
    print(f"  node type: {type(node).__name__}")
    print(f"  node.iterator: {getattr(node, 'iterator', 'N/A')}")
    print(f"  node.iterable type: {type(node.iterable).__name__ if hasattr(node, 'iterable') else 'N/A'}")
    print(f"  hasattr(node, 'start'): {hasattr(node, 'start')}")
    print(f"  hasattr(node, 'end'): {hasattr(node, 'end')}")
    result = original_generate_for_loop(node, indent)
    print(f"  returned from _generate_for_loop")
    return result

def traced_generate_foreach_loop(node, indent='  '):
    print(f"TRACE: _generate_foreach_loop called")
    print(f"  node.iterator: {node.iterator}")
    print(f"  node.iterable: {node.iterable}")
    result = original_generate_foreach_loop(node, indent)
    print(f"  returned from _generate_foreach_loop")
    return result

generator._generate_for_loop = traced_generate_for_loop
generator._generate_foreach_loop = traced_generate_foreach_loop

print("=== Generating LLVM IR ===")
llvm_ir = generator.generate(ast)

print("\n=== Generated IR (loop section) ===")
lines = llvm_ir.split('\n')
in_main = False
for i, line in enumerate(lines):
    if 'define i64 @main()' in line:
        in_main = True
    if in_main:
        print(f"{i:4d}: {line}")
    if in_main and line.strip().startswith('ret '):
        break
