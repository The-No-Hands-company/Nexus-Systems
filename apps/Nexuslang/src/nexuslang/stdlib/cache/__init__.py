"""
Caching utilities for NexusLang.

Provides in-memory caching, memoization, and cache management.

Features:
- Simple key-value cache
- TTL (time-to-live) support
- LRU (Least Recently Used) eviction
- Memoization decorators
- Cache statistics

Example usage in NexusLang:
    # Set cache value
    cache_set with "user_123" and user_data and 300
    
    # Get cache value
    set cached to cache_get with "user_123"
    
    # Clear cache
    cache_clear()
"""

from typing import OrderedDict
from ...runtime.runtime import Runtime
import time
from collections import OrderedDict
from functools import wraps


# Global cache storage
_cache = OrderedDict()
_cache_metadata = {}  # {key: {"expires_at": timestamp, "access_count": int}}
_cache_stats = {"hits": 0, "misses": 0, "evictions": 0}
_max_cache_size = 1000


def cache_set(key, value, ttl=None):
    """
    Set a value in the cache.
    
    Args:
        key: Cache key
        value: Value to cache
        ttl: Time to live in seconds (None for no expiration)
    
    Returns:
        True if set successfully
    """
    global _cache, _cache_metadata
    
    # Evict if cache is full
    if len(_cache) >= _max_cache_size and key not in _cache:
        _evict_lru()
    
    _cache[key] = value
    _cache[key] = _cache.pop(key)  # Move to end (most recent)
    
    # Set metadata
    metadata = {"access_count": 0}
    if ttl is not None:
        metadata["expires_at"] = time.time() + ttl
    else:
        metadata["expires_at"] = None
    
    _cache_metadata[key] = metadata
    
    return True


def cache_get(key, default=None):
    """
    Get a value from the cache.
    
    Args:
        key: Cache key
        default: Default value if not found
    
    Returns:
        Cached value or default
    """
    global _cache, _cache_metadata, _cache_stats
    
    # Check if key exists
    if key not in _cache:
        _cache_stats["misses"] += 1
        return default
    
    # Check if expired
    metadata = _cache_metadata.get(key, {})
    if metadata.get("expires_at") and time.time() > metadata["expires_at"]:
        # Expired - remove and return default
        del _cache[key]
        del _cache_metadata[key]
        _cache_stats["misses"] += 1
        return default
    
    # Valid cache hit
    _cache_stats["hits"] += 1
    metadata["access_count"] = metadata.get("access_count", 0) + 1
    
    # Move to end (most recently accessed)
    value = _cache[key]
    _cache[key] = _cache.pop(key)
    
    return value


def cache_has(key):
    """
    Check if a key exists in the cache.
    
    Args:
        key: Cache key
    
    Returns:
        True if key exists and not expired
    """
    if key not in _cache:
        return False
    
    # Check if expired
    metadata = _cache_metadata.get(key, {})
    if metadata.get("expires_at") and time.time() > metadata["expires_at"]:
        # Expired
        del _cache[key]
        del _cache_metadata[key]
        return False
    
    return True


def cache_delete(key):
    """
    Delete a key from the cache.
    
    Args:
        key: Cache key to delete
    
    Returns:
        True if deleted, False if not found
    """
    global _cache, _cache_metadata
    
    if key in _cache:
        del _cache[key]
        if key in _cache_metadata:
            del _cache_metadata[key]
        return True
    
    return False


def cache_clear():
    """
    Clear all cache entries.
    
    Returns:
        Number of entries cleared
    """
    global _cache, _cache_metadata
    
    count = len(_cache)
    _cache.clear()
    _cache_metadata.clear()
    
    return count


def cache_size():
    """
    Get the current number of cache entries.
    
    Returns:
        Number of cache entries
    """
    return len(_cache)


def cache_set_max_size(size):
    """
    Set the maximum cache size.
    
    Args:
        size: Maximum number of entries
    """
    global _max_cache_size
    _max_cache_size = size


def cache_get_stats():
    """
    Get cache statistics.
    
    Returns:
        Dictionary with hits, misses, evictions, hit_rate
    """
    global _cache_stats
    
    total = _cache_stats["hits"] + _cache_stats["misses"]
    hit_rate = (_cache_stats["hits"] / total * 100) if total > 0 else 0
    
    return {
        "hits": _cache_stats["hits"],
        "misses": _cache_stats["misses"],
        "evictions": _cache_stats["evictions"],
        "size": len(_cache),
        "hit_rate": round(hit_rate, 2)
    }


def cache_reset_stats():
    """
    Reset cache statistics.
    """
    global _cache_stats
    _cache_stats = {"hits": 0, "misses": 0, "evictions": 0}


def _evict_lru():
    """Evict the least recently used cache entry."""
    global _cache, _cache_metadata, _cache_stats
    
    if not _cache:
        return
    
    # Remove first item (oldest)
    key = next(iter(_cache))
    del _cache[key]
    if key in _cache_metadata:
        del _cache_metadata[key]
    
    _cache_stats["evictions"] += 1


def cache_get_or_set(key, compute_function, ttl=None):
    """
    Get value from cache, or compute and cache it if not present.
    
    Args:
        key: Cache key
        compute_function: Function to compute value if not cached
        ttl: Time to live in seconds
    
    Returns:
        Cached or computed value
    """
    # Check cache first
    value = cache_get(key)
    
    if value is None:
        # Compute value
        value = compute_function()
        # Cache it
        cache_set(key, value, ttl)
    
    return value


def cache_cleanup_expired():
    """
    Remove all expired cache entries.
    
    Returns:
        Number of entries removed
    """
    global _cache, _cache_metadata
    
    current_time = time.time()
    expired_keys = []
    
    for key, metadata in _cache_metadata.items():
        if metadata.get("expires_at") and current_time > metadata["expires_at"]:
            expired_keys.append(key)
    
    # Remove expired entries
    for key in expired_keys:
        del _cache[key]
        del _cache_metadata[key]
    
    return len(expired_keys)


def cache_get_keys():
    """
    Get all cache keys.
    
    Returns:
        List of cache keys
    """
    return list(_cache.keys())


def cache_get_all():
    """
    Get all cache entries as a dictionary.
    
    Returns:
        Dictionary of all cache entries
    """
    # Clean up expired entries first
    cache_cleanup_expired()
    return dict(_cache)


# Memoization support
_memoize_caches = {}  # {function_name: {args_key: result}}


def create_memoized_function(function_name, max_size=128):
    """
    Create a memoization cache for a function.
    
    Args:
        function_name: Name of the function to memoize
        max_size: Maximum number of cached results
    
    Returns:
        Cache ID
    """
    global _memoize_caches
    
    if function_name not in _memoize_caches:
        _memoize_caches[function_name] = {
            "cache": OrderedDict(),
            "max_size": max_size
        }
    
    return function_name


def memoize_get(function_name, args_key):
    """
    Get memoized result for function call.
    
    Args:
        function_name: Name of memoized function
        args_key: String key representing arguments
    
    Returns:
        Cached result or None
    """
    if function_name not in _memoize_caches:
        return None
    
    cache_data = _memoize_caches[function_name]
    return cache_data["cache"].get(args_key)


def memoize_set(function_name, args_key, result):
    """
    Cache a function result.
    
    Args:
        function_name: Name of function
        args_key: String key representing arguments
        result: Result to cache
    """
    if function_name not in _memoize_caches:
        create_memoized_function(function_name)
    
    cache_data = _memoize_caches[function_name]
    cache = cache_data["cache"]
    max_size = cache_data["max_size"]
    
    # Evict oldest if at max size
    if len(cache) >= max_size and args_key not in cache:
        cache.popitem(last=False)
    
    cache[args_key] = result
    # Move to end (most recent)
    if args_key in cache:
        cache[args_key] = cache.pop(args_key)


def memoize_clear(function_name):
    """
    Clear memoization cache for a function.
    
    Args:
        function_name: Name of function
    
    Returns:
        Number of entries cleared
    """
    if function_name in _memoize_caches:
        count = len(_memoize_caches[function_name]["cache"])
        _memoize_caches[function_name]["cache"].clear()
        return count
    return 0


def register_cache_functions(runtime: Runtime) -> None:
    """Register cache functions with the runtime."""
    
    # Basic cache operations
    runtime.register_function("cache_set", cache_set)
    runtime.register_function("cache_get", cache_get)
    runtime.register_function("cache_has", cache_has)
    runtime.register_function("cache_delete", cache_delete)
    runtime.register_function("cache_clear", cache_clear)
    
    # Cache info
    runtime.register_function("cache_size", cache_size)
    runtime.register_function("cache_get_keys", cache_get_keys)
    runtime.register_function("cache_get_all", cache_get_all)
    
    # Cache configuration
    runtime.register_function("cache_set_max_size", cache_set_max_size)
    
    # Cache statistics
    runtime.register_function("cache_get_stats", cache_get_stats)
    runtime.register_function("cache_reset_stats", cache_reset_stats)
    
    # Advanced operations
    runtime.register_function("cache_get_or_set", cache_get_or_set)
    runtime.register_function("cache_cleanup_expired", cache_cleanup_expired)
    
    # Memoization
    runtime.register_function("create_memoized_function", create_memoized_function)
    runtime.register_function("memoize_get", memoize_get)
    runtime.register_function("memoize_set", memoize_set)
    runtime.register_function("memoize_clear", memoize_clear)
