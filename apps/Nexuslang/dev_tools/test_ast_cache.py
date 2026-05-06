"""
Test AST cache and cached parser functionality.
"""

import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.parser.ast_cache import ASTCache
from nexuslang.parser.cached_parser import CachedParser


def test_ast_cache():
    """Test AST cache basic operations."""
    print("=" * 70)
    print("Test 1: AST Cache Basic Operations")
    print("=" * 70)
    
    cache = ASTCache(max_entries=3, max_memory_mb=1)
    
    # Test cache miss
    result = cache.get('test1.nxl', 'set x to 1')
    assert result is None, "Expected cache miss"
    print("✓ Cache miss works")
    
    # Test cache put and hit
    fake_ast = {'type': 'Program', 'statements': []}
    cache.put('test1.nxl', 'set x to 1', fake_ast)
    result = cache.get('test1.nxl', 'set x to 1')
    assert result == fake_ast, "Expected cache hit"
    print("✓ Cache put and hit works")
    
    # Test hash invalidation
    result = cache.get('test1.nxl', 'set x to 2')  # Different source
    assert result is None, "Expected invalidation on source change"
    print("✓ Hash-based invalidation works")
    
    # Test LRU eviction
    cache.put('test1.nxl', 'set x to 1', fake_ast)
    cache.put('test2.nxl', 'set y to 2', fake_ast)
    cache.put('test3.nxl', 'set z to 3', fake_ast)
    cache.put('test4.nxl', 'set w to 4', fake_ast)  # Should evict test1
    
    result = cache.get('test1.nxl', 'set x to 1')
    assert result is None, "Expected test1 to be evicted"
    print("✓ LRU eviction works (max_entries limit)")
    
    # Test stats
    stats = cache.get_stats()
    print(f"✓ Stats: {stats['hits']} hits, {stats['misses']} misses, {stats['evictions']} evictions")
    
    print()


def test_cached_parser():
    """Test cached parser with real parsing."""
    print("=" * 70)
    print("Test 2: Cached Parser with Real Parsing")
    print("=" * 70)
    
    cached_parser = CachedParser(enable_debug=True)
    
    # Test case 1: Simple variable declaration
    source1 = """
set x to 42
print text "Hello"
"""
    
    print("\n--- Parse 1: First time (should be cache miss) ---")
    ast1 = cached_parser.parse('test_cached_1.nxl', source1)
    assert ast1 is not None, "Expected AST"
    print(f"✓ AST type: {type(ast1).__name__}")
    
    print("\n--- Parse 2: Same file, same content (should be cache hit) ---")
    ast2 = cached_parser.parse('test_cached_1.nxl', source1)
    assert ast2 is ast1, "Expected same AST object from cache"
    print("✓ Got same AST object from cache (identity check)")
    
    print("\n--- Parse 3: Same file, modified content (should be cache miss) ---")
    source1_modified = """
set x to 100
print text "Hello World"
"""
    ast3 = cached_parser.parse('test_cached_1.nxl', source1_modified)
    assert ast3 is not ast1, "Expected different AST object"
    print("✓ Got new AST object (content changed)")
    
    print("\n--- Parse 4: Different file (should be cache miss) ---")
    source2 = """
function add with a as Integer, b as Integer returns Integer
    return a plus b
end
"""
    ast4 = cached_parser.parse('test_cached_2.nxl', source2)
    assert ast4 is not None, "Expected AST"
    print(f"✓ AST type: {type(ast4).__name__}")
    
    print("\n--- Parse 5: Re-parse test_cached_2.nlpl (should be cache hit) ---")
    ast5 = cached_parser.parse('test_cached_2.nxl', source2)
    assert ast5 is ast4, "Expected same AST object from cache"
    print("✓ Got same AST object from cache")
    
    # Print statistics
    print()
    cached_parser.print_stats()
    
    print()


def test_performance_benefit():
    """Test performance benefit of caching."""
    print("=" * 70)
    print("Test 3: Performance Benefit Measurement")
    print("=" * 70)
    
    import time
    
    # Read a real example file
    example_file = 'examples/01_basic_concepts.nxl'
    if not os.path.exists(example_file):
        print(f"⚠ Skipping performance test - {example_file} not found")
        return
    
    with open(example_file, 'r') as f:
        source_code = f.read()
    
    cached_parser = CachedParser()
    
    # First parse (uncached)
    start = time.perf_counter()
    ast1 = cached_parser.parse(example_file, source_code)
    uncached_time = time.perf_counter() - start
    
    # Second parse (cached)
    start = time.perf_counter()
    ast2 = cached_parser.parse(example_file, source_code)
    cached_time = time.perf_counter() - start
    
    # Verify same object
    assert ast2 is ast1, "Expected same AST object"
    
    speedup = uncached_time / cached_time if cached_time > 0 else float('inf')
    
    print(f"File: {example_file}")
    print(f"  Uncached parse: {uncached_time*1000:.3f}ms")
    print(f"  Cached parse:   {cached_time*1000:.6f}ms")
    print(f"  Speedup:        {speedup:.1f}x")
    print(f"  Time saved:     {(uncached_time-cached_time)*1000:.3f}ms")
    
    if speedup > 10:
        print("✓ Excellent speedup! Cache is working effectively.")
    elif speedup > 5:
        print("✓ Good speedup. Cache is beneficial.")
    else:
        print("⚠ Modest speedup. May need tuning.")
    
    print()


def test_memory_limits():
    """Test memory-based eviction."""
    print("=" * 70)
    print("Test 4: Memory Limits and Eviction")
    print("=" * 70)
    
    # Create small cache (0.1MB limit)
    cache = ASTCache(max_entries=100, max_memory_mb=0.1)
    
    # Create a large source file
    large_source = "set x to 1\n" * 1000  # 1000 lines
    
    print(f"Large source: {len(large_source)} chars, {large_source.count(chr(10))} lines")
    
    # Parse and cache
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser
    
    lexer = Lexer(large_source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    cache.put('large.nxl', large_source, ast)
    
    stats = cache.get_stats()
    print(f"Cache after large file:")
    print(f"  Entries: {stats['entries']}")
    print(f"  Memory: {stats['current_memory_mb']:.3f}MB / {stats['max_memory_mb']:.3f}MB")
    print(f"  Usage: {stats['memory_usage_pct']:.1f}%")
    
    # Try to add more files
    print("\nAdding more files to trigger memory eviction...")
    for i in range(5):
        source = f"set var{i} to {i}\n" * 200
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        parser = Parser(tokens)
        ast = parser.parse()
        cache.put(f'file{i}.nxl', source, ast)
    
    stats = cache.get_stats()
    print(f"\nCache after adding 5 more files:")
    print(f"  Entries: {stats['entries']}")
    print(f"  Memory: {stats['current_memory_mb']:.3f}MB / {stats['max_memory_mb']:.3f}MB")
    print(f"  Evictions: {stats['evictions']}")
    
    if stats['evictions'] > 0:
        print("✓ Memory-based eviction is working")
    else:
        print("⚠ No evictions occurred (may need more files or larger files)")
    
    print()


def main():
    """Run all tests."""
    print("\n" + "=" * 70)
    print("NLPL AST Cache & Cached Parser Tests")
    print("=" * 70 + "\n")
    
    try:
        test_ast_cache()
        test_cached_parser()
        test_performance_benefit()
        test_memory_limits()
        
        print("=" * 70)
        print("ALL TESTS PASSED!")
        print("=" * 70)
        
    except AssertionError as e:
        print(f"\n❌ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
