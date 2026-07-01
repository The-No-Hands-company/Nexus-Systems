#!/usr/bin/env python3
"""Quick compiler script for testing."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator

def main():
    if len(sys.argv) < 2:
        print("Usage: python compile.py <file.nlpl> [output.ll]")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else "build/output.ll"
    
    # Read source
    with open(input_file, 'r') as f:
        source = f.read()
    
    # Compile
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    generator = LLVMIRGenerator()
    llvm_ir = generator.generate(ast)
    
    # Write output
    with open(output_file, 'w') as f:
        f.write(llvm_ir)
    
    print(f"Compiled {input_file} → {output_file}")

if __name__ == '__main__':
    main()
