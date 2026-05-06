#!/usr/bin/env python3
"""
Comprehensive Benchmark Runner
Measures NexusLang compilation and execution performance vs C
"""

import subprocess
import time
import statistics
import sys
from pathlib import Path

def run_benchmark(executable, runs=10):
    """Run executable multiple times and measure performance."""
    times = []
    
    for _ in range(runs):
        start = time.perf_counter()
        result = subprocess.run(
            [executable],
            capture_output=True,
            text=True,
            timeout=10
        )
        end = time.perf_counter()
        
        if result.returncode != 0:
            print(f"Error running {executable}: {result.stderr}")
            return None
        
        times.append(end - start)
    
    return {
        'median': statistics.median(times),
        'mean': statistics.mean(times),
        'min': min(times),
        'max': max(times),
        'stdev': statistics.stdev(times) if len(times) > 1 else 0
    }

def get_binary_size(executable):
    """Get executable file size in bytes."""
    return Path(executable).stat().st_size

def main():
    print("=" * 80)
    print("NLPL Compiler Optimization Benchmark")
    print("=" * 80)
    print()
    
    benchmarks = [
        {
            'name': 'Fibonacci(1000) - Iterative',
            'nxl_o0': './build/bench_fibonacci_o0',
            'nxl_o2': './build/bench_fibonacci_o2',
            'nxl_o3': './build/bench_fibonacci_o3',
            'c_o0': './benchmarks/bench_fib_c_o0',
            'c_o2': './benchmarks/bench_fib_c_o2',
            'c_o3': './benchmarks/bench_fib_c_o3',
        },
        {
            'name': 'Matrix Sum (200x200)',
            'nxl_o0': './build/bench_matrix_o0',
            'nxl_o2': './build/bench_matrix_o2',
            'nxl_o3': './build/bench_matrix_o3',
            'c_o0': './benchmarks/bench_matrix_c_o0',
            'c_o2': './benchmarks/bench_matrix_c_o2',
            'c_o3': './benchmarks/bench_matrix_c_o3',
        },
    ]
    
    for bench in benchmarks:
        print(f"\n{'=' * 80}")
        print(f"Benchmark: {bench['name']}")
        print(f"{'=' * 80}\n")
        
        results = {}
        
        # Run C benchmarks
        for opt in ['o0', 'o2', 'o3']:
            exe = bench[f'c_{opt}']
            if not Path(exe).exists():
                print(f"Skipping {exe} (not found)")
                continue
            
            print(f"Running C -{opt.upper()}... ", end='', flush=True)
            perf = run_benchmark(exe, runs=20)
            if perf:
                results[f'c_{opt}'] = perf
                results[f'c_{opt}_size'] = get_binary_size(exe)
                print(f"✓ {perf['median']*1000:.3f}ms")
            else:
                print("✗ Failed")
        
        # Run NexusLang benchmarks
        for opt in ['o0', 'o2', 'o3']:
            exe = bench[f'nxl_{opt}']
            if not Path(exe).exists():
                print(f"Skipping {exe} (not found)")
                continue
            
            print(f"Running NexusLang -{opt.upper()}... ", end='', flush=True)
            perf = run_benchmark(exe, runs=20)
            if perf:
                results[f'nxl_{opt}'] = perf
                results[f'nxl_{opt}_size'] = get_binary_size(exe)
                print(f"✓ {perf['median']*1000:.3f}ms")
            else:
                print("✗ Failed")
        
        # Print comparison table
        print(f"\n{'-' * 80}")
        print("Performance Comparison")
        print(f"{'-' * 80}")
        print(f"{'Optimization':<15} {'Median (ms)':<15} {'Mean (ms)':<15} {'Size (KB)':<12} {'vs C-O2':<12}")
        print(f"{'-' * 80}")
        
        c_o2_time = results.get('c_o2', {}).get('median', 1.0)
        
        for lang in ['c', 'nlpl']:
            for opt in ['o0', 'o2', 'o3']:
                key = f'{lang}_{opt}'
                if key in results:
                    perf = results[key]
                    size_kb = results.get(f'{key}_size', 0) / 1024
                    speedup = c_o2_time / perf['median']
                    
                    lang_label = lang.upper()
                    opt_label = f"-{opt.upper()}"
                    
                    print(f"{lang_label + ' ' + opt_label:<15} "
                          f"{perf['median']*1000:>13.3f}  "
                          f"{perf['mean']*1000:>13.3f}  "
                          f"{size_kb:>10.1f}  "
                          f"{speedup:>10.2f}x")
        
        print(f"{'-' * 80}\n")
        
        # Calculate speedups
        if 'nxl_o2' in results and 'c_o2' in results:
            nxl_o2_time = results['nxl_o2']['median']
            c_o2_time = results['c_o2']['median']
            slowdown = nxl_o2_time / c_o2_time
            
            print(f"📊 NexusLang -O2 vs C -O2: {slowdown:.2f}x slower ({1/slowdown:.2f}x of C performance)")
            
            # Check if we meet target
            if slowdown <= 3.0:
                print(f"✓ MEETS TARGET: Within 3x of C performance")
            else:
                print(f"✗ BELOW TARGET: Need to be within 3x of C (currently {slowdown:.2f}x)")
        
        if 'nxl_o3' in results and 'c_o2' in results:
            nxl_o3_time = results['nxl_o3']['median']
            c_o2_time = results['c_o2']['median']
            slowdown = nxl_o3_time / c_o2_time
            
            print(f"📊 NexusLang -O3 vs C -O2: {slowdown:.2f}x slower ({1/slowdown:.2f}x of C performance)")
            
            if slowdown <= 2.0:
                print(f"✓ MEETS STRETCH TARGET: Within 2x of C performance")
            else:
                print(f"  STRETCH TARGET: Aiming for within 2x of C (currently {slowdown:.2f}x)")
        
        # Optimization effectiveness
        if 'nxl_o0' in results and 'nxl_o2' in results:
            speedup = results['nxl_o0']['median'] / results['nxl_o2']['median']
            print(f"\n🚀 NexusLang Optimization Effectiveness: -O2 is {speedup:.2f}x faster than -O0")
        
        if 'nxl_o2' in results and 'nxl_o3' in results:
            speedup = results['nxl_o2']['median'] / results['nxl_o3']['median']
            print(f"🚀 NexusLang -O3 vs -O2: {speedup:.2f}x faster")

if __name__ == '__main__':
    main()
