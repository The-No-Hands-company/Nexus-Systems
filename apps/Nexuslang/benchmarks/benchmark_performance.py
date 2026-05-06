#!/usr/bin/env python3
"""Performance benchmark: Native compiled vs Interpreted execution."""

import subprocess
import time
import statistics

# Examples to benchmark
EXAMPLES = [
    "01_basics/01_basic_concepts.nxl",
    "03_type_system/01_type_system_basics.nxl",
    "03_type_system/05_type_features_index.nxl",
    "02_oop/03_traits.nxl",
    "07_low_level/03_ffi_c_interop.nxl",
    "07_low_level/04_ffi_struct_marshalling.nxl",
    "09_feature_patterns/09_feature_showcase.nxl"
]

RUNS = 5  # Number of runs per example

def benchmark_interpreted(example_path):
    """Benchmark interpreted execution."""
    times = []
    for _ in range(RUNS):
        start = time.time()
        result = subprocess.run(
            ["python3", "src/main.py", example_path],
            capture_output=True,
            timeout=10
        )
        elapsed = time.time() - start
        if result.returncode == 0:
            times.append(elapsed)
    return times

def benchmark_compiled(example_path, optimize_level=0):
    """Benchmark compiled execution."""
    # First compile
    compile_result = subprocess.run(
        ["./nlplc", example_path, "-o", "benchmark_test", f"--optimize", str(optimize_level)],
        capture_output=True,
        timeout=30
    )
    
    if compile_result.returncode != 0:
        return None, None
    
    # Measure compile time
    compile_start = time.time()
    subprocess.run(
        ["./nlplc", example_path, "-o", "benchmark_test", f"--optimize", str(optimize_level)],
        capture_output=True,
        timeout=30
    )
    compile_time = time.time() - compile_start
    
    # Measure execution times
    times = []
    for _ in range(RUNS):
        start = time.time()
        result = subprocess.run(
            ["./benchmark_test"],
            capture_output=True,
            timeout=10
        )
        elapsed = time.time() - start
        if result.returncode == 0:
            times.append(elapsed)
    
    return compile_time, times

def format_time(seconds):
    """Format time in milliseconds."""
    return f"{seconds * 1000:.2f}ms"

def calculate_speedup(interpreted_median, compiled_median):
    """Calculate speedup factor."""
    if compiled_median == 0:
        return float('inf')
    return interpreted_median / compiled_median

print("=" * 80)
print("NLPL Performance Benchmark: Native Compiled vs Interpreted")
print("=" * 80)
print(f"Runs per example: {RUNS}")
print(f"Optimization levels tested: O0, O2")
print()

results = []

for example in EXAMPLES:
    example_path = f"examples/{example}"
    print(f"Benchmarking: {example}")
    print("-" * 80)
    
    # Interpreted
    print("  Running interpreted...", end=" ", flush=True)
    interp_times = benchmark_interpreted(example_path)
    if not interp_times:
        print("FAILED")
        continue
    interp_median = statistics.median(interp_times)
    print(f"median: {format_time(interp_median)}")
    
    # Compiled O0
    print("  Compiling O0...", end=" ", flush=True)
    compile_time_o0, exec_times_o0 = benchmark_compiled(example_path, 0)
    if exec_times_o0 is None:
        print("FAILED")
        continue
    exec_median_o0 = statistics.median(exec_times_o0)
    speedup_o0 = calculate_speedup(interp_median, exec_median_o0)
    print(f"compile: {format_time(compile_time_o0)}, exec median: {format_time(exec_median_o0)}, speedup: {speedup_o0:.2f}x")
    
    # Compiled O2
    print("  Compiling O2...", end=" ", flush=True)
    compile_time_o2, exec_times_o2 = benchmark_compiled(example_path, 2)
    if exec_times_o2 is None:
        print("FAILED")
        continue
    exec_median_o2 = statistics.median(exec_times_o2)
    speedup_o2 = calculate_speedup(interp_median, exec_median_o2)
    print(f"compile: {format_time(compile_time_o2)}, exec median: {format_time(exec_median_o2)}, speedup: {speedup_o2:.2f}x")
    print()
    
    results.append({
        'example': example,
        'interpreted': interp_median,
        'compiled_o0': exec_median_o0,
        'compiled_o2': exec_median_o2,
        'speedup_o0': speedup_o0,
        'speedup_o2': speedup_o2,
        'compile_time_o0': compile_time_o0,
        'compile_time_o2': compile_time_o2
    })

# Summary table
print("=" * 80)
print("SUMMARY")
print("=" * 80)
print(f"{'Example':<40} {'Interpreted':>12} {'O0':>12} {'O2':>12} {'Speedup O0':>12} {'Speedup O2':>12}")
print("-" * 80)

for r in results:
    print(f"{r['example']:<40} {format_time(r['interpreted']):>12} {format_time(r['compiled_o0']):>12} "
          f"{format_time(r['compiled_o2']):>12} {r['speedup_o0']:>11.2f}x {r['speedup_o2']:>11.2f}x")

if results:
    avg_speedup_o0 = statistics.mean([r['speedup_o0'] for r in results])
    avg_speedup_o2 = statistics.mean([r['speedup_o2'] for r in results])
    print("-" * 80)
    print(f"{'AVERAGE SPEEDUP':<40} {'':<12} {'':<12} {'':<12} {avg_speedup_o0:>11.2f}x {avg_speedup_o2:>11.2f}x")

print()
print("Compilation Times:")
print(f"  O0 (no optimization): {format_time(statistics.mean([r['compile_time_o0'] for r in results]))}")
print(f"  O2 (optimized):       {format_time(statistics.mean([r['compile_time_o2'] for r in results]))}")
print("=" * 80)
