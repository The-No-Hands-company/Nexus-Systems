#!/usr/bin/env python3
"""Simple performance comparison: Native vs Python overhead."""

import subprocess
import time
import statistics

RUNS = 10

def benchmark_compiled(source_file, optimize_level):
    """Benchmark compiled execution."""
    # Compile once
    binary_name = f"bench_o{optimize_level}"
    compile_result = subprocess.run(
        ["./nlplc", source_file, "-o", binary_name, f"--optimize", str(optimize_level)],
        capture_output=True,
        timeout=30
    )
    
    if compile_result.returncode != 0:
        print(f"Compilation failed: {compile_result.stderr.decode()}")
        return None
    
    # Run multiple times
    times = []
    for _ in range(RUNS):
        start = time.time()
        result = subprocess.run(
            [f"./{binary_name}"],
            capture_output=True,
            timeout=10
        )
        elapsed = time.time() - start
        if result.returncode == 0:
            times.append(elapsed)
    
    return times

print("=" * 70)
print("NLPL Compilation Performance Benchmark")
print("=" * 70)
print(f"Test: Fibonacci computation (30 iterations)")
print(f"Runs: {RUNS} per optimization level")
print()

# Test O0
print("Compiling with -O0 (no optimization)...", end=" ", flush=True)
times_o0 = benchmark_compiled("benchmark_fib.nxl", 0)
if times_o0:
    median_o0 = statistics.median(times_o0)
    mean_o0 = statistics.mean(times_o0)
    stdev_o0 = statistics.stdev(times_o0) if len(times_o0) > 1 else 0
    print(f"Done")
    print(f"  Median: {median_o0*1000:.2f}ms")
    print(f"  Mean:   {mean_o0*1000:.2f}ms ± {stdev_o0*1000:.2f}ms")
else:
    print("FAILED")
    exit(1)

# Test O2
print("\nCompiling with -O2 (optimized)...", end=" ", flush=True)
times_o2 = benchmark_compiled("benchmark_fib.nxl", 2)
if times_o2:
    median_o2 = statistics.median(times_o2)
    mean_o2 = statistics.mean(times_o2)
    stdev_o2 = statistics.stdev(times_o2) if len(times_o2) > 1 else 0
    print(f"Done")
    print(f"  Median: {median_o2*1000:.2f}ms")
    print(f"  Mean:   {mean_o2*1000:.2f}ms ± {stdev_o2*1000:.2f}ms")
else:
    print("FAILED")
    exit(1)

# Test O3
print("\nCompiling with -O3 (aggressive optimization)...", end=" ", flush=True)
times_o3 = benchmark_compiled("benchmark_fib.nxl", 3)
if times_o3:
    median_o3 = statistics.median(times_o3)
    mean_o3 = statistics.mean(times_o3)
    stdev_o3 = statistics.stdev(times_o3) if len(times_o3) > 1 else 0
    print(f"Done")
    print(f"  Median: {median_o3*1000:.2f}ms")
    print(f"  Mean:   {mean_o3*1000:.2f}ms ± {stdev_o3*1000:.2f}ms")
else:
    print("FAILED")

print()
print("=" * 70)
print("OPTIMIZATION IMPACT")
print("=" * 70)
print(f"O0 baseline:  {median_o0*1000:.2f}ms")
print(f"O2 speedup:   {median_o0/median_o2:.2f}x faster ({median_o2*1000:.2f}ms)")
if times_o3:
    print(f"O3 speedup:   {median_o0/median_o3:.2f}x faster ({median_o3*1000:.2f}ms)")
print("=" * 70)
