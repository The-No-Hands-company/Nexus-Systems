#!/usr/bin/env python3
"""
LSP Performance Profiler
=========================

Profiles workspace indexing and symbol lookup performance for the NexusLang LSP server.

Usage:
    python dev_tools/profile_lsp.py [workspace_path]
    
Example:
    python dev_tools/profile_lsp.py examples/
"""

import sys
import os
import time
import cProfile
import pstats
from io import StringIO

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.lsp.workspace_index import WorkspaceIndex


def profile_workspace_indexing(workspace_path):
    """Profile workspace indexing performance."""
    print(f"\n{'='*60}")
    print(f"Profiling Workspace Indexing: {workspace_path}")
    print(f"{'='*60}\n")
    
    # Create profiler
    profiler = cProfile.Profile()
    
    # Profile workspace scan
    print("Phase 1: Workspace Scan")
    print("-" * 40)
    sys.stdout.flush()  # Force flush
    
    index = WorkspaceIndex(workspace_path)
    
    start = time.time()
    profiler.enable()
    index.scan_workspace()
    profiler.disable()
    elapsed = time.time() - start
    sys.stdout.flush()  # Force flush
    
    stats = index.get_statistics()
    
    print(f"  Files indexed:    {stats['files_indexed']}")
    print(f"  Total symbols:    {stats['total_symbols']}")
    print(f"  Functions:        {stats['functions']}")
    print(f"  Classes:          {stats['classes']}")
    print(f"  Methods:          {stats['methods']}")
    print(f"  Structs:          {stats['structs']}")
    print(f"  Variables:        {stats['variables']}")
    print(f"  Time elapsed:     {elapsed:.3f}s")
    print(f"  Files/sec:        {stats['files_indexed'] / elapsed:.1f}")
    print(f"  Symbols/sec:      {stats['total_symbols'] / elapsed:.1f}")
    
    # Print profiling results (top functions only)
    print("\nTop 20 Hotspots (workspace scan):")
    print("-" * 40)
    s = StringIO()
    ps = pstats.Stats(profiler, stream=s).sort_stats('cumulative')
    ps.print_stats(20)
    
    # Print first 2000 chars to avoid hanging on large output
    output = s.getvalue()
    if len(output) > 2000:
        print(output[:2000] + "\n... (output truncated)")
    else:
        print(output)
    
    return index, stats


def profile_symbol_lookup(index, stats):
    """Profile symbol lookup performance."""
    print(f"\n{'='*60}")
    print("Profiling Symbol Lookup")
    print(f"{'='*60}\n")
    
    # Get some symbols to search for
    all_symbols = []
    for file_uri in list(index.indexed_files)[:5]:  # Sample from first 5 files
        all_symbols.extend(index.get_symbols_in_file(file_uri))
    
    if not all_symbols:
        print("No symbols found to profile")
        return
    
    # Profile get_symbol (exact match)
    print("Phase 2: Exact Symbol Lookup (get_symbol)")
    print("-" * 40)
    
    test_symbols = [s.name for s in all_symbols[:100]]  # Test first 100
    
    profiler = cProfile.Profile()
    start = time.time()
    profiler.enable()
    
    for sym_name in test_symbols:
        _ = index.get_symbol(sym_name)
    
    profiler.disable()
    elapsed = time.time() - start
    
    print(f"  Lookups performed: {len(test_symbols)}")
    print(f"  Time elapsed:      {elapsed:.3f}s")
    print(f"  Avg lookup time:   {elapsed/len(test_symbols)*1000:.3f}ms")
    print(f"  Lookups/sec:       {len(test_symbols)/elapsed:.1f}")
    
    print("\nTop 10 Hotspots (exact lookup):")
    print("-" * 40)
    s = StringIO()
    ps = pstats.Stats(profiler, stream=s).sort_stats('cumulative')
    ps.print_stats(10)
    output = s.getvalue()
    if len(output) > 1500:
        print(output[:1500] + "\n... (output truncated)")
    else:
        print(output)
    
    # Profile find_symbols (fuzzy search)
    print("\nPhase 3: Fuzzy Symbol Search (find_symbols)")
    print("-" * 40)
    
    test_queries = ['calc', 'test', 'func', 'main', 'class', 'my', 'get', 'set']
    
    profiler = cProfile.Profile()
    start = time.time()
    profiler.enable()
    
    results_count = 0
    for query in test_queries:
        results = index.find_symbols(query)
        results_count += len(results)
    
    profiler.disable()
    elapsed = time.time() - start
    
    print(f"  Queries performed: {len(test_queries)}")
    print(f"  Results returned:  {results_count}")
    print(f"  Time elapsed:      {elapsed:.3f}s")
    print(f"  Avg query time:    {elapsed/len(test_queries)*1000:.3f}ms")
    print(f"  Queries/sec:       {len(test_queries)/elapsed:.1f}")
    
    print("\nTop 10 Hotspots (fuzzy search):")
    print("-" * 40)
    s = StringIO()
    ps = pstats.Stats(profiler, stream=s).sort_stats('cumulative')
    ps.print_stats(10)
    output = s.getvalue()
    if len(output) > 1500:
        print(output[:1500] + "\n... (output truncated)")
    else:
        print(output)


def profile_incremental_update(index):
    """Profile incremental file re-indexing."""
    print(f"\n{'='*60}")
    print("Profiling Incremental Re-indexing")
    print(f"{'='*60}\n")
    
    if not index.indexed_files:
        print("No files to re-index")
        return
    
    # Pick first file
    file_uri = list(index.indexed_files)[0]
    file_path = index._uri_to_path(file_uri)
    
    print(f"Re-indexing: {os.path.basename(file_path)}")
    print("-" * 40)
    
    # Profile re-indexing
    profiler = cProfile.Profile()
    
    iterations = 10
    start = time.time()
    profiler.enable()
    
    for _ in range(iterations):
        index.index_file(file_uri, file_path)
    
    profiler.disable()
    elapsed = time.time() - start
    
    print(f"  Iterations:       {iterations}")
    print(f"  Total time:       {elapsed:.3f}s")
    print(f"  Avg re-index:     {elapsed/iterations*1000:.3f}ms")
    print(f"  Re-indexes/sec:   {iterations/elapsed:.1f}")
    
    print("\nTop 10 Hotspots (incremental re-index):")
    print("-" * 40)
    s = StringIO()
    ps = pstats.Stats(profiler, stream=s).sort_stats('cumulative')
    ps.print_stats(10)
    output = s.getvalue()
    if len(output) > 1500:
        print(output[:1500] + "\n... (output truncated)")
    else:
        print(output)


def analyze_memory_usage(index, stats):
    """Analyze memory usage of workspace index."""
    print(f"\n{'='*60}")
    print("Memory Usage Analysis")
    print(f"{'='*60}\n")
    
    import sys
    
    # Estimate symbol storage size
    symbols_size = sys.getsizeof(index.symbols)
    files_size = sys.getsizeof(index.files)
    imports_size = sys.getsizeof(index.imports)
    
    # Rough estimate of symbol data
    sample_symbols = []
    for file_uri in list(index.indexed_files)[:10]:
        sample_symbols.extend(index.get_symbols_in_file(file_uri)[:10])
    
    if sample_symbols:
        avg_symbol_size = sum(sys.getsizeof(s) for s in sample_symbols) / len(sample_symbols)
        estimated_symbols_data_size = avg_symbol_size * stats['total_symbols']
    else:
        estimated_symbols_data_size = 0
    
    total_estimated = symbols_size + files_size + imports_size + estimated_symbols_data_size
    
    print(f"  Symbol dictionary:    {symbols_size / 1024:.1f} KB")
    print(f"  File dictionary:      {files_size / 1024:.1f} KB")
    print(f"  Imports dictionary:   {imports_size / 1024:.1f} KB")
    print(f"  Symbol data (est):    {estimated_symbols_data_size / 1024:.1f} KB")
    print(f"  {'─'*40}")
    print(f"  Total (estimated):    {total_estimated / 1024:.1f} KB")
    print(f"                        {total_estimated / (1024*1024):.2f} MB")
    print()
    print(f"  Bytes per symbol:     {total_estimated / stats['total_symbols']:.1f}")
    print(f"  Bytes per file:       {total_estimated / stats['files_indexed']:.1f}")


def print_summary(stats, workspace_path):
    """Print performance summary and recommendations."""
    print(f"\n{'='*60}")
    print("Performance Summary")
    print(f"{'='*60}\n")
    
    print(f"Workspace:     {workspace_path}")
    print(f"Files:         {stats['files_indexed']}")
    print(f"Symbols:       {stats['total_symbols']}")
    print()
    
    # Performance assessment
    symbols_per_file = stats['total_symbols'] / max(stats['files_indexed'], 1)
    
    print("Assessment:")
    print(f"  Symbols/file:  {symbols_per_file:.1f}")
    
    if symbols_per_file < 10:
        print("  Density:       Low (small files)")
    elif symbols_per_file < 50:
        print("  Density:       Medium (typical)")
    else:
        print("  Density:       High (large files)")
    
    print()
    print("Recommendations:")
    
    if stats['files_indexed'] > 100:
        print("  - Consider persistent caching for large workspaces")
        print("  - Enable incremental parsing for better responsiveness")
    
    if symbols_per_file > 100:
        print("  - Large files detected - consider file splitting")
        print("  - Implement lazy symbol loading for file-local symbols")
    
    print()


def main():
    """Main profiling entry point."""
    if len(sys.argv) < 2:
        workspace_path = "examples/"
        print(f"No workspace specified, using: {workspace_path}")
    else:
        workspace_path = sys.argv[1]
    
    if not os.path.exists(workspace_path):
        print(f"Error: Workspace path does not exist: {workspace_path}")
        sys.exit(1)
    
    print(f"\nNLPL LSP Performance Profiler")
    print(f"Python {sys.version}")
    
    # Run profiling phases
    index, stats = profile_workspace_indexing(workspace_path)
    profile_symbol_lookup(index, stats)
    profile_incremental_update(index)
    analyze_memory_usage(index, stats)
    print_summary(stats, workspace_path)
    
    print(f"\n{'='*60}")
    print("Profiling Complete")
    print(f"{'='*60}\n")


if __name__ == '__main__':
    main()
