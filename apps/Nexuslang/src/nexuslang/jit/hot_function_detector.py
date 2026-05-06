"""
Hot Function Detector
=====================

Tracks function call counts and identifies "hot" functions
that are candidates for JIT compilation.
"""

from typing import Dict, Set, Optional
from dataclasses import dataclass, field
import time


@dataclass
class FunctionStats:
    """Statistics for a single function."""
    name: str
    call_count: int = 0
    total_time: float = 0.0
    last_call_time: float = 0.0
    is_jit_compiled: bool = False
    jit_compile_time: Optional[float] = None
    
    @property
    def avg_time(self) -> float:
        """Average execution time per call."""
        return self.total_time / self.call_count if self.call_count > 0 else 0.0


class HotFunctionDetector:
    """
    Detects functions that are called frequently enough to benefit from JIT.
    
    Uses call count threshold and execution time heuristics to identify
    hot functions worth compiling.
    """
    
    def __init__(self, hot_threshold: int = 100, time_weight: float = 0.3):
        """
        Initialize the hot function detector.
        
        Args:
            hot_threshold: Minimum call count to consider function "hot"
            time_weight: Weight given to execution time vs call count (0.0 to 1.0)
        """
        self.hot_threshold = hot_threshold
        self.time_weight = time_weight
        self.function_stats: Dict[str, FunctionStats] = {}
        self.hot_functions: Set[str] = set()
        self.jit_compiled_functions: Set[str] = set()
    
    def record_call(self, function_name: str, execution_time: Optional[float] = None):
        """
        Record a function call.
        
        Args:
            function_name: Name of the called function
            execution_time: Execution time in seconds (optional)
        """
        if function_name not in self.function_stats:
            self.function_stats[function_name] = FunctionStats(name=function_name)
        
        stats = self.function_stats[function_name]
        stats.call_count += 1
        stats.last_call_time = time.time()
        
        if execution_time is not None:
            stats.total_time += execution_time
        
        # Check if function became hot
        if not stats.is_jit_compiled and stats.call_count >= self.hot_threshold:
            self.hot_functions.add(function_name)
    
    def is_hot(self, function_name: str) -> bool:
        """Check if a function is considered hot."""
        return function_name in self.hot_functions
    
    def is_jit_compiled(self, function_name: str) -> bool:
        """Check if a function has been JIT compiled."""
        return function_name in self.jit_compiled_functions
    
    def mark_jit_compiled(self, function_name: str, compile_time: float):
        """
        Mark a function as JIT compiled.
        
        Args:
            function_name: Name of the compiled function
            compile_time: Time taken to compile (seconds)
        """
        if function_name in self.function_stats:
            stats = self.function_stats[function_name]
            stats.is_jit_compiled = True
            stats.jit_compile_time = compile_time
        
        self.jit_compiled_functions.add(function_name)
        self.hot_functions.discard(function_name)  # Remove from hot list
    
    def get_hot_functions(self, limit: Optional[int] = None) -> list[str]:
        """
        Get list of hot functions, optionally limited to top N.
        
        Args:
            limit: Maximum number of functions to return (None = all)
        
        Returns:
            List of function names sorted by "hotness" (call count * time)
        """
        # Calculate hotness score
        scored = []
        for name in self.hot_functions:
            if name in self.function_stats:
                stats = self.function_stats[name]
                # Score = weighted combination of call count and execution time
                score = (
                    stats.call_count * (1.0 - self.time_weight) +
                    stats.total_time * self.time_weight * 1000  # Scale time to be comparable
                )
                scored.append((name, score))
        
        # Sort by score descending
        scored.sort(key=lambda x: x[1], reverse=True)
        
        # Return top N
        if limit:
            scored = scored[:limit]
        
        return [name for name, _ in scored]
    
    def get_stats(self, function_name: str) -> Optional[FunctionStats]:
        """Get statistics for a specific function."""
        return self.function_stats.get(function_name)
    
    def get_all_stats(self) -> Dict[str, FunctionStats]:
        """Get all function statistics."""
        return self.function_stats.copy()
    
    def print_report(self, top_n: int = 10):
        """Print a report of hot functions and JIT compilation."""
        print("\n" + "="*70)
        print("JIT Hot Function Report")
        print("="*70)
        
        print(f"\nHot Threshold: {self.hot_threshold} calls")
        print(f"Hot Functions: {len(self.hot_functions)}")
        print(f"JIT Compiled: {len(self.jit_compiled_functions)}")
        
        # Top JIT compiled functions
        if self.jit_compiled_functions:
            print(f"\nTop {top_n} JIT Compiled Functions:")
            compiled_sorted = sorted(
                [name for name in self.jit_compiled_functions if name in self.function_stats],
                key=lambda x: self.function_stats[x].call_count,
                reverse=True
            )[:top_n]
            
            for name in compiled_sorted:
                stats = self.function_stats[name]
                print(f"  {name:30} {stats.call_count:>6} calls  "
                      f"{stats.avg_time*1000:>8.3f}ms avg  "
                      f"(compiled in {stats.jit_compile_time*1000:.1f}ms)")
        
        # Top hot functions not yet compiled
        hot_list = self.get_hot_functions(limit=top_n)
        if hot_list:
            print(f"\nTop {len(hot_list)} Hot Functions (not yet JIT compiled):")
            for name in hot_list:
                stats = self.function_stats[name]
                print(f"  {name:30} {stats.call_count:>6} calls  "
                      f"{stats.avg_time*1000:>8.3f}ms avg")
        
        print("="*70)
