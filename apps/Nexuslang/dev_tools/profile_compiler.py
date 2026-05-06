#!/usr/bin/env python3
"""
Profile LLVM compiler backend to identify optimization opportunities.
"""

import cProfile
import pstats
import io
import sys
import os
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


def profile_compiler(source_file: str):
    """Profile LLVM compiler execution of a source file."""
    print(f"=== Profiling NexusLang Compiler: {source_file} ===\n")
    
    # Read source code
    with open(source_file, 'r') as f:
        source_code = f.read()
    
    # Setup profiler
    profiler = cProfile.Profile()
    
    # Profile lexing & parsing (for context)
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # Profile LLVM IR generation
    print("Profiling LLVM IR generation...")
    profiler.enable()
    generator = LLVMIRGenerator(module_name="test")
    ir_code = generator.generate(ast)
    profiler.disable()
    
    compiler_stats = io.StringIO()
    ps = pstats.Stats(profiler, stream=compiler_stats)
    ps.sort_stats('cumulative')
    ps.print_stats(40)
    
    print(f"\n--- Compiler Stats (Top 40) ---")
    print(compiler_stats.getvalue())
    
    print(f"\n--- Generated IR Length: {len(ir_code)} characters ---")
    
    # Summary
    print("\n" + "="*80)
    print("COMPILER PERFORMANCE SUMMARY")
    print("="*80)
    
    # Analyze stats for common bottlenecks
    ps = pstats.Stats(profiler)
    ps.sort_stats('cumulative')
    
    print("\nTop time consumers:")
    for func, (cc, nc, tt, ct, callers) in list(ps.stats.items())[:15]:
        filename, line, func_name = func
        if 'nlpl' in filename:  # Only show NLPL-related functions
            print(f"  {func_name:50} {ct:8.4f}s")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python profile_compiler.py <nxl_file>")
        sys.exit(1)
    
    source_file = sys.argv[1]
    if not os.path.exists(source_file):
        print(f"Error: File '{source_file}' not found")
        sys.exit(1)
    
    profile_compiler(source_file)
