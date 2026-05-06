"""
NLPL Profiler - Performance Analysis Tool

Profiles NexusLang programs to identify performance hotspots:
- Function call counts and timing
- Line-by-line execution counts
- Memory allocation tracking
- Hot path identification
"""

import time
import sys
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, field
from collections import defaultdict
from pathlib import Path


@dataclass
class FunctionProfile:
    """Profile data for a single function."""
    name: str
    call_count: int = 0
    total_time: float = 0.0
    self_time: float = 0.0
    min_time: float = float('inf')
    max_time: float = 0.0
    caller_counts: Dict[str, int] = field(default_factory=lambda: defaultdict(int))


@dataclass
class LineProfile:
    """Profile data for a single line of code."""
    file: str
    line: int
    hit_count: int = 0
    total_time: float = 0.0


class Profiler:
    """
    Runtime profiler for NexusLang programs.
    
    Usage:
        profiler = Profiler()
        profiler.start()
        # ... run your code ...
        profiler.stop()
        profiler.print_report()
    """
    
    def __init__(self):
        self.enabled = False
        self.start_time = 0.0
        self.total_runtime = 0.0
        
        # Function profiles
        self.functions: Dict[str, FunctionProfile] = {}
        self.call_stack: List[Tuple[str, float]] = []
        
        # Line profiles
        self.lines: Dict[Tuple[str, int], LineProfile] = {}
        self.current_file = "<unknown>"
        
        # Memory tracking
        self.allocations = 0
        self.deallocations = 0
        self.peak_memory = 0
        self.current_memory = 0
    
    def start(self):
        """Start profiling."""
        self.enabled = True
        self.start_time = time.perf_counter()
    
    def stop(self):
        """Stop profiling."""
        self.enabled = False
        self.total_runtime = time.perf_counter() - self.start_time
    
    def enter_function(self, name: str, caller: Optional[str] = None):
        """Record entering a function."""
        if not self.enabled:
            return
        
        if name not in self.functions:
            self.functions[name] = FunctionProfile(name=name)
        
        profile = self.functions[name]
        profile.call_count += 1
        
        if caller:
            profile.caller_counts[caller] += 1
        
        self.call_stack.append((name, time.perf_counter()))
    
    def exit_function(self, name: str):
        """Record exiting a function."""
        if not self.enabled or not self.call_stack:
            return
        
        stack_name, start_time = self.call_stack.pop()
        if stack_name != name:
            # Stack mismatch - try to recover
            for i in range(len(self.call_stack) - 1, -1, -1):
                if self.call_stack[i][0] == name:
                    stack_name, start_time = self.call_stack.pop(i)
                    break
        
        elapsed = time.perf_counter() - start_time
        profile = self.functions[name]
        profile.total_time += elapsed
        profile.min_time = min(profile.min_time, elapsed)
        profile.max_time = max(profile.max_time, elapsed)
        
        # Calculate self time (excluding child calls)
        # This is total time minus time spent in child functions
        profile.self_time += elapsed
    
    def record_line(self, file: str, line: int):
        """Record execution of a line."""
        if not self.enabled:
            return
        
        key = (file, line)
        if key not in self.lines:
            self.lines[key] = LineProfile(file=file, line=line)
        
        self.lines[key].hit_count += 1
    
    def record_allocation(self, size: int):
        """Record memory allocation."""
        if not self.enabled:
            return
        
        self.allocations += 1
        self.current_memory += size
        self.peak_memory = max(self.peak_memory, self.current_memory)
    
    def record_deallocation(self, size: int):
        """Record memory deallocation."""
        if not self.enabled:
            return
        
        self.deallocations += 1
        self.current_memory -= size
    
    def get_hottest_functions(self, n: int = 10) -> List[FunctionProfile]:
        """Get the N functions with highest total time."""
        return sorted(
            self.functions.values(),
            key=lambda f: f.total_time,
            reverse=True
        )[:n]
    
    def get_most_called_functions(self, n: int = 10) -> List[FunctionProfile]:
        """Get the N most frequently called functions."""
        return sorted(
            self.functions.values(),
            key=lambda f: f.call_count,
            reverse=True
        )[:n]
    
    def get_hottest_lines(self, n: int = 10) -> List[LineProfile]:
        """Get the N lines executed most frequently."""
        return sorted(
            self.lines.values(),
            key=lambda l: l.hit_count,
            reverse=True
        )[:n]
    
    def print_report(self, output=sys.stdout):
        """Print comprehensive profiling report."""
        print("=" * 80, file=output)
        print("NexusLang PROFILER REPORT", file=output)
        print("=" * 80, file=output)
        print(f"\nTotal Runtime: {self.total_runtime*1000:.2f}ms", file=output)
        print(f"Total Functions: {len(self.functions)}", file=output)
        print(f"Total Lines Executed: {sum(l.hit_count for l in self.lines.values())}", file=output)
        
        # Memory stats
        print(f"\nMemory Statistics:", file=output)
        print(f"  Allocations: {self.allocations}", file=output)
        print(f"  Deallocations: {self.deallocations}", file=output)
        print(f"  Peak Memory: {self.peak_memory / 1024:.2f} KB", file=output)
        print(f"  Current Memory: {self.current_memory / 1024:.2f} KB", file=output)
        
        # Hottest functions by time
        print(f"\n{'=' * 80}", file=output)
        print("TOP 10 FUNCTIONS BY TIME", file=output)
        print("=" * 80, file=output)
        print(f"{'Function':<30} {'Calls':<10} {'Total(ms)':<12} {'Avg(ms)':<12} {'%Time':<8}", file=output)
        print("-" * 80, file=output)
        
        for func in self.get_hottest_functions(10):
            avg_time = func.total_time / func.call_count if func.call_count > 0 else 0
            pct_time = (func.total_time / self.total_runtime * 100) if self.total_runtime > 0 else 0
            print(
                f"{func.name[:30]:<30} "
                f"{func.call_count:<10} "
                f"{func.total_time*1000:<12.2f} "
                f"{avg_time*1000:<12.4f} "
                f"{pct_time:<8.2f}",
                file=output
            )
        
        # Most called functions
        print(f"\n{'=' * 80}", file=output)
        print("TOP 10 MOST CALLED FUNCTIONS", file=output)
        print("=" * 80, file=output)
        print(f"{'Function':<30} {'Calls':<10} {'Avg(μs)':<12}", file=output)
        print("-" * 80, file=output)
        
        for func in self.get_most_called_functions(10):
            avg_time = func.total_time / func.call_count if func.call_count > 0 else 0
            print(
                f"{func.name[:30]:<30} "
                f"{func.call_count:<10} "
                f"{avg_time*1000000:<12.2f}",
                file=output
            )
        
        # Hottest lines
        if self.lines:
            print(f"\n{'=' * 80}", file=output)
            print("TOP 10 HOT LINES", file=output)
            print("=" * 80, file=output)
            print(f"{'File':<40} {'Line':<8} {'Hits':<12}", file=output)
            print("-" * 80, file=output)
            
            for line in self.get_hottest_lines(10):
                file_short = Path(line.file).name if line.file != "<unknown>" else line.file
                print(
                    f"{file_short[:40]:<40} "
                    f"{line.line:<8} "
                    f"{line.hit_count:<12}",
                    file=output
                )
        
        print(f"\n{'=' * 80}\n", file=output)
    
    def export_flamegraph(self, output_file: str):
        """Export data in flamegraph format for visualization."""
        with open(output_file, 'w') as f:
            for func in self.functions.values():
                # Format: stack;frames samples
                f.write(f"{func.name} {int(func.total_time * 1000000)}\n")
    
    def export_json(self, output_file: str):
        """Export profile data as JSON for further analysis."""
        import json
        
        data = {
            'total_runtime': self.total_runtime,
            'functions': {
                name: {
                    'call_count': prof.call_count,
                    'total_time': prof.total_time,
                    'self_time': prof.self_time,
                    'min_time': prof.min_time if prof.min_time != float('inf') else 0,
                    'max_time': prof.max_time,
                    'callers': dict(prof.caller_counts)
                }
                for name, prof in self.functions.items()
            },
            'lines': {
                f"{file}:{line}": {
                    'hit_count': prof.hit_count,
                    'total_time': prof.total_time
                }
                for (file, line), prof in self.lines.items()
            },
            'memory': {
                'allocations': self.allocations,
                'deallocations': self.deallocations,
                'peak_memory': self.peak_memory,
                'current_memory': self.current_memory
            }
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)


# Global profiler instance
_global_profiler: Optional[Profiler] = None


def get_profiler() -> Profiler:
    """Get the global profiler instance."""
    global _global_profiler
    if _global_profiler is None:
        _global_profiler = Profiler()
    return _global_profiler


def enable_profiling():
    """Enable global profiling."""
    get_profiler().start()


def disable_profiling():
    """Disable global profiling."""
    get_profiler().stop()


def print_profile_report():
    """Print the global profiler report."""
    get_profiler().print_report()
