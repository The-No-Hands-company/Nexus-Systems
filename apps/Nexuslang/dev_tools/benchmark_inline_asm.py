#!/usr/bin/env python3
"""
NLPL Inline Assembly Performance Benchmark Runner

Compiles and runs inline assembly benchmarks with detailed profiling.
Measures compilation overhead and runtime performance.
"""

import os
import sys
import time
import subprocess
import statistics
from pathlib import Path

# Add src to path
SCRIPT_DIR = Path(__file__).parent
NLPL_ROOT = SCRIPT_DIR.parent
sys.path.insert(0, str(NLPL_ROOT / "src"))

from nexuslang.compiler.profiling import CompilerProfiler, enable_profiling


BENCHMARK_FILE = NLPL_ROOT / "test_programs" / "benchmarks" / "bench_inline_asm.nxl"
OUTPUT_FILE = NLPL_ROOT / "build" / "bench_inline_asm"
COMPILER = NLPL_ROOT / "dev_tools" / "nlplc"


def compile_with_profiling(source_file: Path, output_file: Path, runs: int = 5):
    """Compile benchmark with profiling enabled."""
    
    print(f"\n{'='*70}")
    print(f"COMPILING: {source_file.name}")
    print(f"{'='*70}\n")
    
    compile_times = []
    
    for i in range(runs):
        print(f"Compilation run {i+1}/{runs}...", end=" ", flush=True)
        
        start = time.perf_counter()
        result = subprocess.run(
            [str(COMPILER), str(source_file), "-o", str(output_file)],
            capture_output=True,
            text=True
        )
        elapsed = time.perf_counter() - start
        compile_times.append(elapsed)
        
        if result.returncode != 0:
            print(f"FAILED")
            print(f"\nCompilation error:\n{result.stderr}")
            return None
        
        print(f"{elapsed*1000:.2f} ms")
    
    # Statistics
    print(f"\n📊 Compilation Time Statistics (n={runs}):")
    print(f"  Mean:   {statistics.mean(compile_times)*1000:.2f} ms")
    print(f"  Median: {statistics.median(compile_times)*1000:.2f} ms")
    print(f"  StdDev: {statistics.stdev(compile_times)*1000:.2f} ms")
    print(f"  Min:    {min(compile_times)*1000:.2f} ms")
    print(f"  Max:    {max(compile_times)*1000:.2f} ms")
    
    return compile_times


def run_benchmark(output_file: Path, runs: int = 10):
    """Run compiled benchmark and measure execution time."""
    
    print(f"\n{'='*70}")
    print(f"RUNNING BENCHMARK: {output_file.name}")
    print(f"{'='*70}\n")
    
    if not output_file.exists():
        print(f"Error: Binary not found: {output_file}")
        return None
    
    run_times = []
    
    for i in range(runs):
        print(f"Execution run {i+1}/{runs}...", end=" ", flush=True)
        
        start = time.perf_counter()
        result = subprocess.run(
            [str(output_file)],
            capture_output=True,
            text=True
        )
        elapsed = time.perf_counter() - start
        run_times.append(elapsed)
        
        if result.returncode != 0:
            print(f"FAILED")
            print(f"\nExecution error:\n{result.stderr}")
            return None
        
        print(f"{elapsed*1000:.2f} ms")
    
    # Show output from last run
    print(f"\n📝 Benchmark Output (last run):")
    print("-" * 70)
    print(result.stdout)
    print("-" * 70)
    
    # Statistics
    print(f"\n⏱️  Execution Time Statistics (n={runs}):")
    print(f"  Mean:   {statistics.mean(run_times)*1000:.2f} ms")
    print(f"  Median: {statistics.median(run_times)*1000:.2f} ms")
    print(f"  StdDev: {statistics.stdev(run_times)*1000:.2f} ms")
    print(f"  Min:    {min(run_times)*1000:.2f} ms")
    print(f"  Max:    {max(run_times)*1000:.2f} ms")
    
    return run_times


def get_file_size(file_path: Path) -> int:
    """Get file size in bytes."""
    if file_path.exists():
        return file_path.stat().st_size
    return 0


def analyze_binary(binary_path: Path):
    """Analyze compiled binary."""
    
    print(f"\n{'='*70}")
    print(f"BINARY ANALYSIS: {binary_path.name}")
    print(f"{'='*70}\n")
    
    size = get_file_size(binary_path)
    print(f"📦 Binary Size: {size:,} bytes ({size/1024:.2f} KB)")
    
    # Try to get section sizes with objdump
    try:
        result = subprocess.run(
            ["objdump", "-h", str(binary_path)],
            capture_output=True,
            text=True
        )
        
        if result.returncode == 0:
            print(f"\n📋 Section Sizes:")
            lines = result.stdout.split('\n')
            for line in lines:
                if '.text' in line or '.data' in line or '.rodata' in line or '.bss' in line:
                    parts = line.split()
                    if len(parts) >= 3:
                        section = parts[1]
                        size_hex = parts[2]
                        size_dec = int(size_hex, 16)
                        print(f"  {section:12s}: {size_dec:>8,} bytes")
    except FileNotFoundError:
        print("  (objdump not available)")
    
    # Try to count inline assembly instructions with objdump
    try:
        result = subprocess.run(
            ["objdump", "-d", str(binary_path)],
            capture_output=True,
            text=True
        )
        
        if result.returncode == 0:
            # Count lines that look like assembly instructions
            asm_lines = [l for l in result.stdout.split('\n') if ':' in l and '\t' in l]
            print(f"\n⚙️  Disassembly:")
            print(f"  Total instructions: ~{len(asm_lines)}")
    except FileNotFoundError:
        pass


def compare_with_c_baseline():
    """
    Compare NexusLang inline asm performance with C inline asm.
    
    This would require creating equivalent C benchmarks.
    For now, just a placeholder.
    """
    print(f"\n{'='*70}")
    print("COMPARISON WITH C BASELINE")
    print(f"{'='*70}\n")
    print("Note: C baseline comparison requires equivalent C benchmarks.")
    print("TODO: Create C versions of benchmarks for comparison.")


def main():
    print("""
╔══════════════════════════════════════════════════════════════════╗
║                                                                  ║
║       NexusLang INLINE ASSEMBLY PERFORMANCE BENCHMARK SUITE           ║
║                                                                  ║
║  Tests compilation speed and runtime performance of inline       ║
║  assembly features with comprehensive profiling.                 ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
    """)
    
    # Check benchmark file exists
    if not BENCHMARK_FILE.exists():
        print(f"Error: Benchmark file not found: {BENCHMARK_FILE}")
        return 1
    
    source_size = get_file_size(BENCHMARK_FILE)
    print(f"📄 Source file: {BENCHMARK_FILE.name}")
    print(f"📦 Source size: {source_size:,} bytes ({source_size/1024:.2f} KB)")
    
    # Compile with profiling
    compile_times = compile_with_profiling(BENCHMARK_FILE, OUTPUT_FILE, runs=5)
    if compile_times is None:
        return 1
    
    # Analyze binary
    analyze_binary(OUTPUT_FILE)
    
    # Run benchmarks
    run_times = run_benchmark(OUTPUT_FILE, runs=10)
    if run_times is None:
        return 1
    
    # Overall summary
    print(f"\n{'='*70}")
    print("OVERALL SUMMARY")
    print(f"{'='*70}\n")
    
    print(f"✅ Compilation successful")
    print(f"   - Average compile time: {statistics.mean(compile_times)*1000:.2f} ms")
    print(f"   - Compile time stddev:  {statistics.stdev(compile_times)*1000:.2f} ms")
    
    print(f"\n✅ Execution successful")
    print(f"   - Average runtime:      {statistics.mean(run_times)*1000:.2f} ms")
    print(f"   - Runtime stddev:       {statistics.stdev(run_times)*1000:.2f} ms")
    
    binary_size = get_file_size(OUTPUT_FILE)
    print(f"\n📦 Code size metrics:")
    print(f"   - Source:  {source_size:>8,} bytes")
    print(f"   - Binary:  {binary_size:>8,} bytes")
    print(f"   - Ratio:   {binary_size/source_size:>8.2f}x")
    
    # Performance assessment
    avg_compile = statistics.mean(compile_times) * 1000
    avg_runtime = statistics.mean(run_times) * 1000
    
    print(f"\n🎯 Performance Assessment:")
    if avg_compile < 100:
        print(f"   Compilation: EXCELLENT (<100ms)")
    elif avg_compile < 500:
        print(f"   Compilation: GOOD (<500ms)")
    else:
        print(f"   Compilation: NEEDS OPTIMIZATION (>500ms)")
    
    if avg_runtime < 10:
        print(f"   Runtime:     EXCELLENT (<10ms)")
    elif avg_runtime < 50:
        print(f"   Runtime:     GOOD (<50ms)")
    else:
        print(f"   Runtime:     ACCEPTABLE")
    
    print(f"\n{'='*70}\n")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
