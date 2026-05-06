"""
LSP Performance Benchmark
==========================

Measures LSP server performance with and without AST caching.
Simulates realistic editing scenarios.
"""

import sys
import os
import time

# Add parent directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.parser.cached_parser import CachedParser, get_cached_parser
from nexuslang.parser.ast_cache import get_global_cache, set_cache_limits


def simulate_lsp_editing_session(file_path: str, num_edits: int = 10):
    """
    Simulate a realistic LSP editing session.
    
    Scenario:
    1. Initial file open (cold start - cache miss)
    2. User types, triggering diagnostics on each keystroke
    3. Small edits (1-2 lines changed at a time)
    
    Args:
        file_path: Path to NexusLang file
        num_edits: Number of edits to simulate
        
    Returns:
        Performance metrics dictionary
    """
    # Read original file
    with open(file_path, 'r') as f:
        original_source = f.read()
    
    lines = original_source.split('\n')
    
    # Configure cache
    set_cache_limits(max_entries=50, max_memory_mb=25)
    cached_parser = get_cached_parser()
    cached_parser.clear_cache()
    
    # Metrics
    times = []
    
    print(f"Simulating LSP editing session: {file_path}")
    print(f"  File size: {len(lines)} lines, {len(original_source)} chars")
    print(f"  Edits: {num_edits}")
    print()
    
    # Edit 1: Initial file open (cold start)
    print("Edit 1: Initial file open (cold start)")
    start = time.perf_counter()
    ast = cached_parser.parse(file_path, original_source)
    elapsed = time.perf_counter() - start
    times.append(('cold_start', elapsed))
    print(f"  Time: {elapsed*1000:.3f}ms")
    print()
    
    # Edits 2-N: Incremental edits (cache hits expected)
    for i in range(2, num_edits + 1):
        # Simulate small edit (change 1 line)
        edit_line = (i * 7) % len(lines)  # Pseudo-random line
        modified_lines = lines.copy()
        
        # Add a comment or modify a line
        if i % 2 == 0:
            modified_lines[edit_line] = f"# Edit {i}: Modified line {edit_line}"
        else:
            modified_lines[edit_line] = modified_lines[edit_line] + " # edit"
        
        modified_source = '\n'.join(modified_lines)
        
        print(f"Edit {i}: Modify line {edit_line} (cache miss expected - content changed)")
        start = time.perf_counter()
        ast = cached_parser.parse(file_path, modified_source)
        elapsed = time.perf_counter() - start
        times.append(('edit_miss', elapsed))
        print(f"  Time: {elapsed*1000:.3f}ms")
        
        # Simulate diagnostic re-check without change (cache hit)
        print(f"  Re-check diagnostics (cache hit expected)")
        start = time.perf_counter()
        ast = cached_parser.parse(file_path, modified_source)
        elapsed = time.perf_counter() - start
        times.append(('recheck_hit', elapsed))
        print(f"  Time: {elapsed*1000:.3f}ms")
        print()
    
    # Calculate statistics
    cold_start_time = times[0][1]
    edit_miss_times = [t[1] for t in times if t[0] == 'edit_miss']
    recheck_hit_times = [t[1] for t in times if t[0] == 'recheck_hit']
    
    avg_edit_miss = sum(edit_miss_times) / len(edit_miss_times) if edit_miss_times else 0
    avg_recheck_hit = sum(recheck_hit_times) / len(recheck_hit_times) if recheck_hit_times else 0
    
    speedup = avg_edit_miss / avg_recheck_hit if avg_recheck_hit > 0 else 0
    
    # Print summary
    print("=" * 70)
    print("PERFORMANCE SUMMARY")
    print("=" * 70)
    print(f"Cold start:           {cold_start_time*1000:.3f}ms")
    print(f"Avg edit (miss):      {avg_edit_miss*1000:.3f}ms")
    print(f"Avg recheck (hit):    {avg_recheck_hit*1000:.6f}ms")
    print(f"Cache speedup:        {speedup:.1f}x")
    print(f"Time saved per hit:   {(avg_edit_miss-avg_recheck_hit)*1000:.3f}ms")
    print()
    
    # Cache statistics
    cached_parser.print_stats()
    
    return {
        'file': file_path,
        'lines': len(lines),
        'chars': len(original_source),
        'edits': num_edits,
        'cold_start_ms': cold_start_time * 1000,
        'avg_edit_miss_ms': avg_edit_miss * 1000,
        'avg_recheck_hit_ms': avg_recheck_hit * 1000,
        'speedup': speedup,
        'time_saved_ms': (avg_edit_miss - avg_recheck_hit) * 1000
    }


def benchmark_multiple_files():
    """Benchmark multiple example files."""
    print("\n" + "=" * 70)
    print("LSP PERFORMANCE BENCHMARK - MULTIPLE FILES")
    print("=" * 70 + "\n")
    
    test_files = [
        'examples/01_basic_concepts.nxl',
        'examples/02_functions.nxl',
        'examples/03_loops.nxl',
        'test_programs/unit/stdlib/test_json.nxl'
    ]
    
    results = []
    
    for file_path in test_files:
        if not os.path.exists(file_path):
            print(f"⚠ Skipping {file_path} - not found\n")
            continue
        
        result = simulate_lsp_editing_session(file_path, num_edits=5)
        results.append(result)
    
    # Overall summary
    if results:
        print("\n" + "=" * 70)
        print("OVERALL SUMMARY")
        print("=" * 70)
        
        avg_speedup = sum(r['speedup'] for r in results) / len(results)
        avg_time_saved = sum(r['time_saved_ms'] for r in results) / len(results)
        
        print(f"Files tested: {len(results)}")
        print(f"Average cache speedup: {avg_speedup:.1f}x")
        print(f"Average time saved per cache hit: {avg_time_saved:.3f}ms")
        print()
        
        print("Per-file results:")
        for r in results:
            print(f"  {os.path.basename(r['file']):<30} {r['speedup']:>6.1f}x speedup, {r['time_saved_ms']:>7.3f}ms saved")


def benchmark_cache_vs_no_cache():
    """
    Direct comparison: cached vs uncached parsing.
    """
    print("\n" + "=" * 70)
    print("DIRECT COMPARISON: CACHED vs UNCACHED")
    print("=" * 70 + "\n")
    
    file_path = 'examples/01_basic_concepts.nxl'
    if not os.path.exists(file_path):
        print(f"⚠ Skipping - {file_path} not found")
        return
    
    with open(file_path, 'r') as f:
        source_code = f.read()
    
    # Uncached parsing
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser
    
    print("Uncached parsing (10 iterations):")
    uncached_times = []
    for i in range(10):
        start = time.perf_counter()
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        elapsed = time.perf_counter() - start
        uncached_times.append(elapsed)
    
    avg_uncached = sum(uncached_times) / len(uncached_times)
    print(f"  Average: {avg_uncached*1000:.3f}ms")
    print(f"  Min: {min(uncached_times)*1000:.3f}ms")
    print(f"  Max: {max(uncached_times)*1000:.3f}ms")
    print()
    
    # Cached parsing (after warmup)
    cached_parser = CachedParser()
    cached_parser.clear_cache()
    
    # Warmup
    cached_parser.parse(file_path, source_code)
    
    print("Cached parsing (10 iterations, warmed up):")
    cached_times = []
    for i in range(10):
        start = time.perf_counter()
        ast = cached_parser.parse(file_path, source_code)
        elapsed = time.perf_counter() - start
        cached_times.append(elapsed)
    
    avg_cached = sum(cached_times) / len(cached_times)
    print(f"  Average: {avg_cached*1000:.6f}ms")
    print(f"  Min: {min(cached_times)*1000:.6f}ms")
    print(f"  Max: {max(cached_times)*1000:.6f}ms")
    print()
    
    speedup = avg_uncached / avg_cached if avg_cached > 0 else 0
    print(f"Speedup: {speedup:.1f}x")
    print(f"Time saved: {(avg_uncached-avg_cached)*1000:.3f}ms per cache hit")


def main():
    """Run all benchmarks."""
    benchmark_multiple_files()
    benchmark_cache_vs_no_cache()
    
    print("\n" + "=" * 70)
    print("LSP PERFORMANCE BENCHMARK COMPLETE")
    print("=" * 70)


if __name__ == '__main__':
    main()
