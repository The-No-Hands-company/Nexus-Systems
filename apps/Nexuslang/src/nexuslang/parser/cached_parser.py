"""
Cached Parser Wrapper for NexusLang
================================

Provides intelligent caching for parsed ASTs to improve performance
in scenarios like LSP where files are parsed repeatedly.
"""

from typing import Optional
from pathlib import Path

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast_cache import ASTCache, get_global_cache


class CachedParser:
    """
    Parser wrapper with intelligent AST caching.
    
    Features:
    - Automatic cache management (hash-based invalidation)
    - Transparent fallback to full parsing
    - Statistics tracking
    - Thread-safe operations
    
    Example:
        cached_parser = CachedParser()
        
        # First parse - cache miss, will parse
        ast = cached_parser.parse(file_path, source_code)
        
        # Second parse with same content - cache hit, instant
        ast = cached_parser.parse(file_path, source_code)
        
        # Parse with modified content - cache miss, will reparse
        ast = cached_parser.parse(file_path, modified_source)
    """
    
    def __init__(self, cache: Optional[ASTCache] = None, enable_debug: bool = False):
        """
        Initialize cached parser.
        
        Args:
            cache: Custom AST cache instance (uses global cache if None)
            enable_debug: Enable debug output
        """
        self.cache = cache if cache is not None else get_global_cache()
        self.enable_debug = enable_debug
        
        # Statistics
        self.total_parses = 0
        self.cache_hits = 0
        self.cache_misses = 0
    
    def parse(self, file_path: str, source_code: str, debug: bool = False) -> any:
        """
        Parse source code with caching.
        
        Args:
            file_path: Path to source file (for cache key)
            source_code: NexusLang source code to parse
            debug: Enable debug output (shows tokens and AST)
            
        Returns:
            Parsed AST (Program node)
        """
        self.total_parses += 1
        
        # Normalize path
        file_path = str(Path(file_path).resolve())
        
        # Try cache first
        cached_ast = self.cache.get(file_path, source_code)
        if cached_ast is not None:
            self.cache_hits += 1
            if self.enable_debug or debug:
                print(f"[CachedParser] Cache HIT for {file_path}")
            return cached_ast
        
        # Cache miss - parse from scratch
        self.cache_misses += 1
        if self.enable_debug or debug:
            print(f"[CachedParser] Cache MISS for {file_path} - parsing...")
        
        # Parse
        lexer = Lexer(source_code)
        tokens = lexer.tokenize()
        
        if debug:
            print("Tokens:")
            for token in tokens:
                print(f"  {token}")
        
        parser = Parser(tokens)
        ast = parser.parse()
        
        if debug:
            print("\nAST:")
            self._print_ast(ast)
        
        # Store in cache
        self.cache.put(file_path, source_code, ast)
        
        return ast
    
    def parse_from_file(self, file_path: str, debug: bool = False) -> any:
        """
        Parse NexusLang file with caching.
        
        Args:
            file_path: Path to .nlpl file
            debug: Enable debug output
            
        Returns:
            Parsed AST
        """
        # Read file
        with open(file_path, 'r', encoding='utf-8') as f:
            source_code = f.read()
        
        return self.parse(file_path, source_code, debug)
    
    def invalidate(self, file_path: str) -> bool:
        """
        Manually invalidate cache entry.
        
        Args:
            file_path: Path to file
            
        Returns:
            True if entry was invalidated
        """
        return self.cache.invalidate(file_path)
    
    def clear_cache(self) -> None:
        """Clear all cached ASTs."""
        self.cache.clear()
    
    def get_stats(self) -> dict:
        """
        Get parser statistics.
        
        Returns:
            Dictionary with parser and cache statistics
        """
        cache_stats = self.cache.get_stats()
        
        return {
            'parser': {
                'total_parses': self.total_parses,
                'cache_hits': self.cache_hits,
                'cache_misses': self.cache_misses,
                'cache_hit_rate': (self.cache_hits / self.total_parses * 100) if self.total_parses > 0 else 0
            },
            'cache': cache_stats
        }
    
    def print_stats(self) -> None:
        """Print parser and cache statistics."""
        stats = self.get_stats()
        
        print("\nCached Parser Statistics:")
        print(f"  Total parses: {stats['parser']['total_parses']}")
        print(f"  Cache hits: {stats['parser']['cache_hits']}")
        print(f"  Cache misses: {stats['parser']['cache_misses']}")
        print(f"  Cache hit rate: {stats['parser']['cache_hit_rate']:.1f}%")
        print()
        
        self.cache.print_stats()
    
    def _print_ast(self, node, indent=0):
        """Print AST for debugging."""
        indent_str = "  " * indent
        if hasattr(node, 'statements'):
            print(f"{indent_str}{node}")
            for stmt in node.statements:
                self._print_ast(stmt, indent + 1)
        elif hasattr(node, 'body') and isinstance(node.body, list):
            print(f"{indent_str}{node}")
            for stmt in node.body:
                self._print_ast(stmt, indent + 1)
        else:
            print(f"{indent_str}{node}")


# Global cached parser instance
_global_cached_parser: Optional[CachedParser] = None


def get_cached_parser() -> CachedParser:
    """Get the global cached parser instance."""
    global _global_cached_parser
    if _global_cached_parser is None:
        _global_cached_parser = CachedParser()
    return _global_cached_parser


def parse_with_cache(file_path: str, source_code: str, debug: bool = False) -> any:
    """
    Convenience function for parsing with global cache.
    
    Args:
        file_path: Path to source file
        source_code: NexusLang source code
        debug: Enable debug output
        
    Returns:
        Parsed AST
    """
    return get_cached_parser().parse(file_path, source_code, debug)
