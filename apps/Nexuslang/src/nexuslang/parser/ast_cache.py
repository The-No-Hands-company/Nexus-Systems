"""
AST Cache System for NexusLang
==========================

Intelligent caching of parsed ASTs with:
- File hash tracking for invalidation
- LRU eviction policy
- Memory limits
- Thread-safe operations
"""

import hashlib
import time
from typing import Dict, Optional, Tuple, Any, OrderedDict
from collections import OrderedDict
from threading import Lock
from dataclasses import dataclass
from pathlib import Path


@dataclass
class CacheEntry:
    """Entry in the AST cache."""
    ast: Any  # The parsed AST
    source_hash: str  # SHA-256 hash of source code
    file_path: str  # Absolute path to file
    timestamp: float  # When cached
    access_count: int  # Number of times accessed
    last_access: float  # Last access time
    memory_size: int  # Estimated memory size in bytes
    
    def touch(self):
        """Update access statistics."""
        self.access_count += 1
        self.last_access = time.time()


class ASTCache:
    """
    LRU cache for parsed ASTs.
    
    Features:
    - File hash-based invalidation
    - Memory-based eviction
    - Thread-safe operations
    - Statistics tracking
    
    Example:
        cache = ASTCache(max_memory_mb=100)
        
        # Try to get from cache
        ast = cache.get(file_path, source_code)
        if ast is None:
            # Cache miss - parse and store
            ast = parse_source(source_code)
            cache.put(file_path, source_code, ast)
    """
    
    def __init__(self, max_entries: int = 100, max_memory_mb: float = 50):
        """
        Initialize AST cache.
        
        Args:
            max_entries: Maximum number of cached ASTs
            max_memory_mb: Maximum memory usage in MB
        """
        self.max_entries = max_entries
        self.max_memory_bytes = int(max_memory_mb * 1024 * 1024)
        
        # OrderedDict for LRU behavior
        self.cache: OrderedDict[str, CacheEntry] = OrderedDict()
        self.lock = Lock()
        
        # Statistics
        self.hits = 0
        self.misses = 0
        self.evictions = 0
        self.current_memory = 0
    
    def _compute_hash(self, source_code: str) -> str:
        """Compute SHA-256 hash of source code."""
        return hashlib.sha256(source_code.encode('utf-8')).hexdigest()
    
    def _estimate_size(self, ast: Any) -> int:
        """
        Estimate memory size of AST in bytes.
        
        This is approximate - uses a simple heuristic based on:
        - Number of AST nodes
        - Average size per node
        """
        # Rough estimate: 200 bytes per AST node on average
        # (includes Python object overhead, references, attributes)
        node_count = self._count_nodes(ast)
        return node_count * 200
    
    def _count_nodes(self, node: Any, visited: Optional[set] = None) -> int:
        """Recursively count AST nodes."""
        if visited is None:
            visited = set()
        
        # Avoid infinite recursion
        node_id = id(node)
        if node_id in visited:
            return 0
        visited.add(node_id)
        
        count = 1  # Current node
        
        # Check if it's an AST node (has __dict__)
        if hasattr(node, '__dict__'):
            for attr_name, attr_value in node.__dict__.items():
                # Skip certain attributes to avoid cycles
                if attr_name in ('parent', 'scope', 'line_number', 'column'):
                    continue
                
                if isinstance(attr_value, list):
                    for item in attr_value:
                        if hasattr(item, '__dict__'):
                            count += self._count_nodes(item, visited)
                elif hasattr(attr_value, '__dict__'):
                    count += self._count_nodes(attr_value, visited)
        
        return count
    
    def get(self, file_path: str, source_code: str) -> Optional[Any]:
        """
        Get AST from cache if valid.
        
        Args:
            file_path: Absolute path to file
            source_code: Current source code
            
        Returns:
            Cached AST if valid, None if cache miss or invalidated
        """
        with self.lock:
            # Normalize path
            file_path = str(Path(file_path).resolve())
            
            if file_path not in self.cache:
                self.misses += 1
                return None
            
            entry = self.cache[file_path]
            
            # Validate hash
            current_hash = self._compute_hash(source_code)
            if entry.source_hash != current_hash:
                # Source changed - invalidate
                self.misses += 1
                self.current_memory -= entry.memory_size
                del self.cache[file_path]
                return None
            
            # Cache hit - update access stats and move to end (most recent)
            entry.touch()
            self.cache.move_to_end(file_path)
            self.hits += 1
            
            return entry.ast
    
    def put(self, file_path: str, source_code: str, ast: Any) -> None:
        """
        Store AST in cache.
        
        Args:
            file_path: Absolute path to file
            source_code: Source code
            ast: Parsed AST
        """
        with self.lock:
            # Normalize path
            file_path = str(Path(file_path).resolve())
            
            # Compute hash and size
            source_hash = self._compute_hash(source_code)
            memory_size = self._estimate_size(ast)
            
            # Check if already cached (update instead of add)
            if file_path in self.cache:
                old_entry = self.cache[file_path]
                self.current_memory -= old_entry.memory_size
                del self.cache[file_path]
            
            # Create entry
            entry = CacheEntry(
                ast=ast,
                source_hash=source_hash,
                file_path=file_path,
                timestamp=time.time(),
                access_count=1,
                last_access=time.time(),
                memory_size=memory_size
            )
            
            # Add to cache
            self.cache[file_path] = entry
            self.current_memory += memory_size
            
            # Evict if necessary
            self._evict_if_needed()
    
    def _evict_if_needed(self) -> None:
        """Evict entries if limits exceeded (LRU policy)."""
        # Evict by count
        while len(self.cache) > self.max_entries:
            # Remove oldest (first in OrderedDict)
            file_path, entry = self.cache.popitem(last=False)
            self.current_memory -= entry.memory_size
            self.evictions += 1
        
        # Evict by memory
        while self.current_memory > self.max_memory_bytes and self.cache:
            # Remove oldest
            file_path, entry = self.cache.popitem(last=False)
            self.current_memory -= entry.memory_size
            self.evictions += 1
    
    def invalidate(self, file_path: str) -> bool:
        """
        Invalidate cache entry for a file.
        
        Args:
            file_path: Path to file
            
        Returns:
            True if entry was invalidated, False if not in cache
        """
        with self.lock:
            file_path = str(Path(file_path).resolve())
            
            if file_path in self.cache:
                entry = self.cache[file_path]
                self.current_memory -= entry.memory_size
                del self.cache[file_path]
                return True
            
            return False
    
    def clear(self) -> None:
        """Clear all cache entries."""
        with self.lock:
            self.cache.clear()
            self.current_memory = 0
            self.evictions = 0
    
    def get_stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        with self.lock:
            total_requests = self.hits + self.misses
            hit_rate = self.hits / total_requests if total_requests > 0 else 0
            
            return {
                'hits': self.hits,
                'misses': self.misses,
                'hit_rate': hit_rate,
                'entries': len(self.cache),
                'evictions': self.evictions,
                'current_memory_mb': self.current_memory / 1024 / 1024,
                'max_memory_mb': self.max_memory_bytes / 1024 / 1024,
                'memory_usage_pct': (self.current_memory / self.max_memory_bytes * 100) if self.max_memory_bytes > 0 else 0
            }
    
    def print_stats(self) -> None:
        """Print cache statistics."""
        stats = self.get_stats()
        print("AST Cache Statistics:")
        print(f"  Hits: {stats['hits']}")
        print(f"  Misses: {stats['misses']}")
        print(f"  Hit rate: {stats['hit_rate']*100:.1f}%")
        print(f"  Entries: {stats['entries']}/{self.max_entries}")
        print(f"  Evictions: {stats['evictions']}")
        print(f"  Memory: {stats['current_memory_mb']:.2f}/{stats['max_memory_mb']:.2f}MB ({stats['memory_usage_pct']:.1f}%)")


# Global cache instance
_global_cache: Optional[ASTCache] = None


def get_global_cache() -> ASTCache:
    """Get the global AST cache instance."""
    global _global_cache
    if _global_cache is None:
        _global_cache = ASTCache(max_entries=100, max_memory_mb=50)
    return _global_cache


def set_cache_limits(max_entries: int, max_memory_mb: float) -> None:
    """Configure global cache limits."""
    global _global_cache
    if _global_cache is None:
        _global_cache = ASTCache(max_entries=max_entries, max_memory_mb=max_memory_mb)
    else:
        _global_cache.max_entries = max_entries
        _global_cache.max_memory_bytes = int(max_memory_mb * 1024 * 1024)
