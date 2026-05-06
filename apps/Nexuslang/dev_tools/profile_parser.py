#!/usr/bin/env python3
"""
Performance profiling for NexusLang parser.
Measures parsing time, memory usage, and identifies bottlenecks.
"""

import sys
import os
import time
import tracemalloc
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / "src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def profile_parse(source_file):
    """Profile parsing a source file."""
    print(f"\n{'='*70}")
    print(f"Profiling: {source_file}")
    print(f"{'='*70}")
    
    # Read source
    with open(source_file, 'r') as f:
        source_code = f.read()
    
    lines = source_code.count('\n') + 1
    chars = len(source_code)
    print(f"File size: {lines} lines, {chars} characters")
    
    # Profile lexer
    print("\n--- Lexer Performance ---")
    tracemalloc.start()
    start_time = time.perf_counter()
    
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    
    lexer_time = time.perf_counter() - start_time
    lexer_memory = tracemalloc.get_traced_memory()[1] / 1024 / 1024  # Peak MB
    tracemalloc.stop()
    
    print(f"Tokens generated: {len(tokens)}")
    print(f"Lexer time: {lexer_time*1000:.2f}ms")
    print(f"Lexer memory: {lexer_memory:.2f}MB")
    print(f"Tokens/sec: {len(tokens)/lexer_time:.0f}")
    
    # Profile parser
    print("\n--- Parser Performance ---")
    tracemalloc.start()
    start_time = time.perf_counter()
    
    parser = Parser(tokens)
    ast = parser.parse()
    
    parser_time = time.perf_counter() - start_time
    parser_memory = tracemalloc.get_traced_memory()[1] / 1024 / 1024  # Peak MB
    tracemalloc.stop()
    
    print(f"AST generated: {type(ast).__name__}")
    print(f"Parser time: {parser_time*1000:.2f}ms")
    print(f"Parser memory: {parser_memory:.2f}MB")
    
    # Total
    total_time = lexer_time + parser_time
    total_memory = lexer_memory + parser_memory
    
    print("\n--- Total Performance ---")
    print(f"Total time: {total_time*1000:.2f}ms")
    print(f"Total memory: {total_memory:.2f}MB")
    print(f"Lines/sec: {lines/total_time:.0f}")
    print(f"Chars/sec: {chars/total_time:.0f}")
    
    return {
        'file': source_file,
        'lines': lines,
        'chars': chars,
        'tokens': len(tokens),
        'lexer_time': lexer_time,
        'parser_time': parser_time,
        'total_time': total_time,
        'lexer_memory': lexer_memory,
        'parser_memory': parser_memory,
        'total_memory': total_memory
    }


def benchmark_multiple_files():
    """Benchmark multiple files of varying sizes."""
    test_files = [
        'examples/01_basics/01_basic_concepts.nxl',
        'examples/10_concurrency_and_async.nxl',
        'examples/09_feature_patterns/07_performance_optimization.nxl',
        'test_programs/unit/stdlib/test_json.nxl',
    ]
    
    results = []
    for test_file in test_files:
        if os.path.exists(test_file):
            try:
                result = profile_parse(test_file)
                results.append(result)
            except Exception as e:
                print(f"Error profiling {test_file}: {e}")
    
    # Summary
    if results:
        print(f"\n{'='*70}")
        print("SUMMARY")
        print(f"{'='*70}")
        print(f"{'File':<40} {'Lines':>8} {'Time(ms)':>10} {'Mem(MB)':>10}")
        print(f"{'-'*70}")
        for r in results:
            filename = os.path.basename(r['file'])
            print(f"{filename:<40} {r['lines']:>8} {r['total_time']*1000:>10.2f} {r['total_memory']:>10.2f}")
        
        # Averages
        avg_lines_per_sec = sum(r['lines']/r['total_time'] for r in results) / len(results)
        avg_time = sum(r['total_time'] for r in results) / len(results)
        avg_memory = sum(r['total_memory'] for r in results) / len(results)
        
        print(f"{'-'*70}")
        print(f"Average: {avg_lines_per_sec:.0f} lines/sec, {avg_time*1000:.2f}ms/file, {avg_memory:.2f}MB")


def test_incremental_simulation():
    """Simulate incremental parsing by parsing the same file with small changes."""
    print(f"\n{'='*70}")
    print("INCREMENTAL PARSING SIMULATION")
    print(f"{'='*70}")
    
    # Test with a small file
    test_file = 'examples/01_basic_concepts.nxl'
    if not os.path.exists(test_file):
        print(f"Test file not found: {test_file}")
        return
    
    with open(test_file, 'r') as f:
        original_source = f.read()
    
    # Parse original
    print("\nParsing original file...")
    start = time.perf_counter()
    lexer = Lexer(original_source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    baseline_time = time.perf_counter() - start
    print(f"Baseline parse: {baseline_time*1000:.2f}ms")
    
    # Simulate editing: add a comment at the end
    modified_source = original_source + "\n# Modified line\n"
    
    print("\nParsing modified file (full reparse)...")
    start = time.perf_counter()
    lexer = Lexer(modified_source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    reparse_time = time.perf_counter() - start
    print(f"Full reparse: {reparse_time*1000:.2f}ms")
    
    # Calculate potential savings
    lines_changed = 1
    total_lines = original_source.count('\n') + 1
    theoretical_speedup = total_lines / lines_changed
    
    print(f"\nTheoretical incremental savings:")
    print(f"  Lines changed: {lines_changed}/{total_lines} ({lines_changed/total_lines*100:.1f}%)")
    print(f"  Theoretical speedup: {theoretical_speedup:.1f}x")
    print(f"  Potential time with incremental: {reparse_time/theoretical_speedup*1000:.2f}ms")
    print(f"  Potential savings: {(reparse_time - reparse_time/theoretical_speedup)*1000:.2f}ms")


if __name__ == '__main__':
    if len(sys.argv) > 1:
        # Profile specific file
        profile_parse(sys.argv[1])
    else:
        # Run full benchmark
        benchmark_multiple_files()
        test_incremental_simulation()
