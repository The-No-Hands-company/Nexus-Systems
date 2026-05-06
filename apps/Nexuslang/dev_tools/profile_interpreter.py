#!/usr/bin/env python3
"""
Profile the NexusLang interpreter to identify performance bottlenecks.
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
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime


def profile_interpreter(source_file: str):
    """Profile interpreter execution of a source file."""
    print(f"=== Profiling NexusLang Interpreter: {source_file} ===\n")
    
    # Read source code
    with open(source_file, 'r') as f:
        source_code = f.read()
    
    # Setup profiler
    profiler = cProfile.Profile()
    
    # Profile lexing
    print("Profiling lexer...")
    profiler.enable()
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    profiler.disable()
    
    lexer_stats = io.StringIO()
    ps = pstats.Stats(profiler, stream=lexer_stats)
    ps.sort_stats('cumulative')
    ps.print_stats(20)
    
    print(f"\n--- Lexer Stats (Top 20) ---")
    print(lexer_stats.getvalue())
    
    # Profile parsing
    print("\nProfiling parser...")
    profiler = cProfile.Profile()
    profiler.enable()
    parser = Parser(tokens)
    ast = parser.parse()
    profiler.disable()
    
    parser_stats = io.StringIO()
    ps = pstats.Stats(profiler, stream=parser_stats)
    ps.sort_stats('cumulative')
    ps.print_stats(20)
    
    print(f"\n--- Parser Stats (Top 20) ---")
    print(parser_stats.getvalue())
    
    # Profile interpretation
    print("\nProfiling interpreter...")
    profiler = cProfile.Profile()
    runtime = Runtime()
    interpreter = Interpreter(runtime)
    
    # Register stdlib
    from nexuslang.stdlib import register_stdlib
    register_stdlib(runtime)
    
    profiler.enable()
    interpreter.interpret(ast)
    profiler.disable()
    
    interp_stats = io.StringIO()
    ps = pstats.Stats(profiler, stream=interp_stats)
    ps.sort_stats('cumulative')
    ps.print_stats(30)
    
    print(f"\n--- Interpreter Stats (Top 30) ---")
    print(interp_stats.getvalue())
    
    # Summary
    print("\n" + "="*80)
    print("PERFORMANCE BOTTLENECK SUMMARY")
    print("="*80)
    
    # Analyze stats for common bottlenecks
    ps = pstats.Stats(profiler)
    ps.sort_stats('cumulative')
    
    total_time = ps.total_tt
    print(f"\nTotal execution time: {total_time:.4f}s\n")
    
    print("Top time consumers:")
    for func, (cc, nc, tt, ct, callers) in list(ps.stats.items())[:10]:
        filename, line, func_name = func
        print(f"  {func_name:40} {ct:8.4f}s ({ct/total_time*100:5.1f}%)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python profile_interpreter.py <nxl_file>")
        sys.exit(1)
    
    source_file = sys.argv[1]
    if not os.path.exists(source_file):
        print(f"Error: File '{source_file}' not found")
        sys.exit(1)
    
    profile_interpreter(source_file)
