"""
NLPL Compiler Benchmarking Framework
=====================================

Measures and compares performance across:
- Compilation time
- Execution time
- Binary size
- Memory usage

Supports comparison between:
- NexusLang interpreter
- NexusLang compiled (-O0, -O2, -O3)
- Equivalent C code
- Other languages (Python, Rust, etc.)
"""

import time
import subprocess
import os
import sys
import statistics
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass


@dataclass
class BenchmarkResult:
    """Results from a single benchmark run."""
    name: str
    compile_time: float  # seconds
    execution_time: float  # seconds
    binary_size: int  # bytes
    memory_peak: int  # KB
    optimization_level: str
    iterations: int
    
    def speedup_vs(self, baseline: 'BenchmarkResult') -> float:
        """Calculate speedup compared to baseline."""
        if baseline.execution_time == 0:
            return float('inf')
        return baseline.execution_time / self.execution_time
    
    def size_ratio_vs(self, baseline: 'BenchmarkResult') -> float:
        """Calculate size ratio compared to baseline."""
        if baseline.binary_size == 0:
            return float('inf')
        return self.binary_size / baseline.binary_size


class BenchmarkRunner:
    """Runs benchmarks and collects performance metrics."""
    
    def __init__(self, nxl_compiler: str = 'python src/main.py'):
        """
        Initialize benchmark runner.
        
        Args:
            nxl_compiler: Command to invoke NexusLang compiler
        """
        self.nxl_compiler = nxl_compiler
        self.results: List[BenchmarkResult] = []
    
    def benchmark_nxl_interpreted(self, source_file: str, iterations: int = 10) -> BenchmarkResult:
        """
        Benchmark NexusLang in interpreter mode.
        
        Args:
            source_file: Path to .nlpl file
            iterations: Number of runs to average
            
        Returns:
            BenchmarkResult with metrics
        """
        print(f"Benchmarking {source_file} (interpreter)...")
        
        execution_times = []
        
        for i in range(iterations):
            start = time.time()
            result = subprocess.run(
                f'{self.nxl_compiler} {source_file}'.split(),
                capture_output=True,
                text=True
            )
            elapsed = time.time() - start
            
            if result.returncode != 0:
                print(f"  Warning: Run {i+1} failed: {result.stderr}")
                continue
            
            execution_times.append(elapsed)
        
        if not execution_times:
            raise RuntimeError(f"All benchmark runs failed for {source_file}")
        
        avg_time = statistics.median(execution_times)
        
        return BenchmarkResult(
            name=Path(source_file).stem,
            compile_time=0.0,  # Interpreted, no compilation
            execution_time=avg_time,
            binary_size=0,
            memory_peak=0,
            optimization_level='interpreter',
            iterations=len(execution_times)
        )
    
    def benchmark_nxl_compiled(
        self, 
        source_file: str, 
        optimization_level: str = 'O2',
        iterations: int = 10
    ) -> BenchmarkResult:
        """
        Benchmark NexusLang in compiled mode.
        
        Args:
            source_file: Path to .nlpl file
            optimization_level: 'O0', 'O1', 'O2', 'O3', or 'Os'
            iterations: Number of execution runs to average
            
        Returns:
            BenchmarkResult with metrics
        """
        print(f"Benchmarking {source_file} (compiled -{optimization_level})...")
        
        # Compile
        output_binary = f'build/{Path(source_file).stem}_{optimization_level}'
        
        compile_start = time.time()
        compile_result = subprocess.run(
            f'{self.nxl_compiler} {source_file} --compile -{optimization_level} -o {output_binary}'.split(),
            capture_output=True,
            text=True
        )
        compile_time = time.time() - compile_start
        
        if compile_result.returncode != 0:
            raise RuntimeError(f"Compilation failed: {compile_result.stderr}")
        
        # Get binary size
        binary_size = os.path.getsize(output_binary) if os.path.exists(output_binary) else 0
        
        # Execute multiple times
        execution_times = []
        
        for i in range(iterations):
            start = time.time()
            exec_result = subprocess.run(
                [output_binary],
                capture_output=True,
                text=True
            )
            elapsed = time.time() - start
            
            if exec_result.returncode != 0:
                print(f"  Warning: Run {i+1} failed")
                continue
            
            execution_times.append(elapsed)
        
        if not execution_times:
            raise RuntimeError(f"All execution runs failed for {source_file}")
        
        avg_time = statistics.median(execution_times)
        
        return BenchmarkResult(
            name=Path(source_file).stem,
            compile_time=compile_time,
            execution_time=avg_time,
            binary_size=binary_size,
            memory_peak=0,  # TODO: measure with /usr/bin/time -v
            optimization_level=optimization_level,
            iterations=len(execution_times)
        )
    
    def benchmark_c_code(
        self, 
        source_file: str,
        optimization_level: str = 'O2',
        iterations: int = 10
    ) -> BenchmarkResult:
        """
        Benchmark equivalent C code for comparison.
        
        Args:
            source_file: Path to .c file
            optimization_level: gcc optimization level
            iterations: Number of execution runs
            
        Returns:
            BenchmarkResult with metrics
        """
        print(f"Benchmarking {source_file} (C -{optimization_level})...")
        
        # Compile with gcc
        output_binary = f'build/{Path(source_file).stem}_c_{optimization_level}'
        
        compile_start = time.time()
        compile_result = subprocess.run(
            ['gcc', f'-{optimization_level}', source_file, '-o', output_binary],
            capture_output=True,
            text=True
        )
        compile_time = time.time() - compile_start
        
        if compile_result.returncode != 0:
            raise RuntimeError(f"C compilation failed: {compile_result.stderr}")
        
        # Get binary size
        binary_size = os.path.getsize(output_binary)
        
        # Execute multiple times
        execution_times = []
        
        for i in range(iterations):
            start = time.time()
            exec_result = subprocess.run(
                [output_binary],
                capture_output=True,
                text=True
            )
            elapsed = time.time() - start
            
            if exec_result.returncode != 0:
                print(f"  Warning: Run {i+1} failed")
                continue
            
            execution_times.append(elapsed)
        
        avg_time = statistics.median(execution_times)
        
        return BenchmarkResult(
            name=Path(source_file).stem + '_c',
            compile_time=compile_time,
            execution_time=avg_time,
            binary_size=binary_size,
            memory_peak=0,
            optimization_level=optimization_level,
            iterations=len(execution_times)
        )
    
    def run_benchmark_suite(
        self, 
        benchmark_dir: str = 'benchmarks',
        optimization_levels: List[str] = ['O0', 'O2', 'O3']
    ):
        """
        Run full benchmark suite on all programs in directory.
        
        Args:
            benchmark_dir: Directory containing benchmark programs
            optimization_levels: List of optimization levels to test
        """
        benchmark_files = list(Path(benchmark_dir).glob('*.nxl'))
        
        if not benchmark_files:
            print(f"No benchmark files found in {benchmark_dir}")
            return
        
        print(f"\n{'='*60}")
        print(f"Running benchmark suite: {len(benchmark_files)} programs")
        print(f"Optimization levels: {', '.join(optimization_levels)}")
        print(f"{'='*60}\n")
        
        for nxl_file in benchmark_files:
            print(f"\n--- Benchmarking: {nxl_file.name} ---")
            
            # Interpreter baseline
            try:
                result = self.benchmark_nxl_interpreted(str(nxl_file))
                self.results.append(result)
            except Exception as e:
                print(f"  Interpreter benchmark failed: {e}")
            
            # Compiled versions
            for opt_level in optimization_levels:
                try:
                    result = self.benchmark_nxl_compiled(str(nxl_file), opt_level)
                    self.results.append(result)
                except Exception as e:
                    print(f"  Compiled -{opt_level} benchmark failed: {e}")
            
            # C comparison (if exists)
            c_file = nxl_file.with_suffix('.c')
            if c_file.exists():
                try:
                    result = self.benchmark_c_code(str(c_file), 'O2')
                    self.results.append(result)
                except Exception as e:
                    print(f"  C benchmark failed: {e}")
        
        print(f"\n{'='*60}")
        print(f"Benchmark suite complete: {len(self.results)} results")
        print(f"{'='*60}\n")
    
    def generate_report(self, output_file: str = 'benchmarks/report.md'):
        """Generate markdown report from benchmark results."""
        
        if not self.results:
            print("No results to report")
            return
        
        report_lines = [
            "# NexusLang Compiler Performance Benchmark Report",
            "",
            f"**Generated**: {time.strftime('%Y-%m-%d %H:%M:%S')}",
            f"**Total Benchmarks**: {len(self.results)}",
            "",
            "## Summary",
            "",
            "| Program | Mode | Opt Level | Compile Time | Execution Time | Binary Size | Speedup vs Interpreter |",
            "|---------|------|-----------|--------------|----------------|-------------|------------------------|",
        ]
        
        # Group results by program
        by_program = {}
        for result in self.results:
            base_name = result.name.replace('_c', '')
            if base_name not in by_program:
                by_program[base_name] = []
            by_program[base_name].append(result)
        
        # Generate table rows
        for program, results in sorted(by_program.items()):
            # Find interpreter baseline
            interpreter = next((r for r in results if r.optimization_level == 'interpreter'), None)
            
            for result in results:
                speedup = ''
                if interpreter and result.optimization_level != 'interpreter':
                    speedup = f"{result.speedup_vs(interpreter):.2f}x"
                
                compile_time_str = f"{result.compile_time:.3f}s" if result.compile_time > 0 else "N/A"
                binary_size_str = f"{result.binary_size // 1024}KB" if result.binary_size > 0 else "N/A"
                
                mode = "C" if result.name.endswith('_c') else "NexusLang"
                
                report_lines.append(
                    f"| {program} | {mode} | {result.optimization_level} | "
                    f"{compile_time_str} | {result.execution_time:.3f}s | "
                    f"{binary_size_str} | {speedup} |"
                )
        
        # Add detailed analysis
        report_lines.extend([
            "",
            "## Detailed Analysis",
            "",
            "### Optimization Level Comparison",
            "",
        ])
        
        # Calculate averages per optimization level
        by_opt_level = {}
        for result in self.results:
            if result.name.endswith('_c'):
                continue  # Skip C for NexusLang comparison
            
            level = result.optimization_level
            if level not in by_opt_level:
                by_opt_level[level] = []
            by_opt_level[level].append(result)
        
        report_lines.append("| Optimization Level | Avg Compile Time | Avg Execution Time | Avg Binary Size |")
        report_lines.append("|-------------------|------------------|---------------------|-----------------|")
        
        for level in ['interpreter', 'O0', 'O1', 'O2', 'O3', 'Os']:
            if level not in by_opt_level:
                continue
            
            results_for_level = by_opt_level[level]
            avg_compile = statistics.mean(r.compile_time for r in results_for_level)
            avg_exec = statistics.mean(r.execution_time for r in results_for_level)
            avg_size = statistics.mean(r.binary_size for r in results_for_level if r.binary_size > 0)
            
            compile_str = f"{avg_compile:.3f}s" if avg_compile > 0 else "N/A"
            size_str = f"{int(avg_size) // 1024}KB" if avg_size > 0 else "N/A"
            
            report_lines.append(
                f"| {level} | {compile_str} | {avg_exec:.3f}s | {size_str} |"
            )
        
        # Write report
        with open(output_file, 'w') as f:
            f.write('\n'.join(report_lines))
        
        print(f"Report written to {output_file}")
        
        # Print summary to console
        print("\n" + "\n".join(report_lines[:20]))


def main():
    """Run benchmarks from command line."""
    import argparse
    
    parser = argparse.ArgumentParser(description='NLPL Compiler Benchmark Suite')
    parser.add_argument('--dir', default='benchmarks', help='Directory containing benchmark programs')
    parser.add_argument('--iterations', type=int, default=10, help='Number of runs per benchmark')
    parser.add_argument('--levels', nargs='+', default=['O0', 'O2', 'O3'], help='Optimization levels to test')
    parser.add_argument('--output', default='benchmarks/report.md', help='Output report file')
    
    args = parser.parse_args()
    
    runner = BenchmarkRunner()
    runner.run_benchmark_suite(args.dir, args.levels)
    runner.generate_report(args.output)


if __name__ == '__main__':
    main()
