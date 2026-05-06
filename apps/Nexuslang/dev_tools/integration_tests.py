#!/usr/bin/env python3
"""
NLPL Integration Test & Benchmark Suite
Tests interpreter vs compiled execution and measures performance
"""

import sys
import os
import time
import subprocess
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional

# Add src to path
sys.path.insert(0, str(Path(__file__).parent / 'src'))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib


@dataclass
class BenchmarkResult:
    """Results from a benchmark test."""
    name: str
    interpreter_time: float
    interpreter_result: str
    compiled_time: Optional[float] = None
    compiled_result: Optional[str] = None
    compilation_time: Optional[float] = None
    speedup: Optional[float] = None
    passed: bool = False
    error: Optional[str] = None


class IntegrationTester:
    """Integration tester and benchmarker for NexusLang."""
    
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.results: List[BenchmarkResult] = []
        self.nlplc_path = Path(__file__).parent / "nlplc"
        
    def run_interpreter(self, source_code: str, source_file: str = "<benchmark>") -> tuple:
        """Run code in interpreter and measure time."""
        runtime = Runtime()
        register_stdlib(runtime)
        
        start = time.perf_counter()
        try:
            lexer = Lexer(source_code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            interpreter = Interpreter(runtime, enable_type_checking=True)
            result = interpreter.interpret(ast)
            elapsed = time.perf_counter() - start
            return (elapsed, str(result) if result is not None else "", None)
        except Exception as e:
            elapsed = time.perf_counter() - start
            return (elapsed, "", str(e))
    
    def compile_and_run(self, source_code: str, test_name: str) -> tuple:
        """Compile and run code, measure both compilation and execution time."""
        import tempfile
        
        with tempfile.TemporaryDirectory() as tmpdir:
            source_file = Path(tmpdir) / f"{test_name}.nxl"
            executable = Path(tmpdir) / test_name
            
            # Write source
            source_file.write_text(source_code)
            
            # Compile
            compile_start = time.perf_counter()
            try:
                result = subprocess.run(
                    [str(self.nlplc_path), str(source_file), "-o", str(executable), "-O2"],
                    capture_output=True,
                    text=True,
                    timeout=30
                )
                compile_time = time.perf_counter() - compile_start
                
                if result.returncode != 0:
                    return (compile_time, 0.0, "", f"Compilation failed: {result.stderr}")
                
                # Run executable
                run_start = time.perf_counter()
                result = subprocess.run(
                    [str(executable)],
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                run_time = time.perf_counter() - run_start
                
                if result.returncode != 0:
                    return (compile_time, run_time, "", f"Execution failed: {result.stderr}")
                
                return (compile_time, run_time, result.stdout.strip(), None)
                
            except subprocess.TimeoutExpired:
                return (0.0, 0.0, "", "Timeout")
            except Exception as e:
                return (0.0, 0.0, "", str(e))
    
    def benchmark(self, name: str, source_code: str):
        """Run a benchmark test (interpreter vs compiled)."""
        print(f"\n{'='*60}")
        print(f"Test: {name}")
        print(f"{'='*60}")
        
        # Run in interpreter
        print("Running in interpreter...", end=" ", flush=True)
        interp_time, interp_result, interp_error = self.run_interpreter(source_code, name)
        
        if interp_error:
            print(f"FAILED")
            print(f"  Error: {interp_error}")
            result = BenchmarkResult(
                name=name,
                interpreter_time=interp_time,
                interpreter_result="",
                error=interp_error,
                passed=False
            )
            self.results.append(result)
            return
        
        print(f"OK ({interp_time*1000:.2f}ms)")
        if self.verbose and interp_result:
            print(f"  Output: {interp_result[:100]}")
        
        # Compile and run
        print("Compiling...", end=" ", flush=True)
        compile_time, run_time, compiled_result, compile_error = self.compile_and_run(source_code, name)
        
        if compile_error:
            print(f"FAILED")
            print(f"  Error: {compile_error}")
            result = BenchmarkResult(
                name=name,
                interpreter_time=interp_time,
                interpreter_result=interp_result,
                compilation_time=compile_time,
                error=compile_error,
                passed=False
            )
            self.results.append(result)
            return
        
        print(f"OK ({compile_time*1000:.2f}ms compile, {run_time*1000:.2f}ms run)")
        
        # Check if results match
        # Normalize whitespace for comparison
        interp_normalized = ' '.join(interp_result.split())
        compiled_normalized = ' '.join(compiled_result.split())
        
        results_match = interp_normalized == compiled_normalized
        speedup = interp_time / run_time if run_time > 0 else 0
        
        if results_match:
            print(f"✓ Results match! Speedup: {speedup:.2f}x")
        else:
            print(f"✗ Results differ!")
            print(f"  Interpreter: {interp_result[:100]}")
            print(f"  Compiled:    {compiled_result[:100]}")
        
        result = BenchmarkResult(
            name=name,
            interpreter_time=interp_time,
            interpreter_result=interp_result,
            compiled_time=run_time,
            compiled_result=compiled_result,
            compilation_time=compile_time,
            speedup=speedup,
            passed=results_match
        )
        self.results.append(result)
    
    def print_summary(self):
        """Print summary of all benchmarks."""
        print(f"\n\n{'='*60}")
        print("SUMMARY")
        print(f"{'='*60}\n")
        
        passed = sum(1 for r in self.results if r.passed)
        total = len(self.results)
        
        print(f"Tests passed: {passed}/{total}")
        print()
        
        print(f"{'Test':<30} {'Interp(ms)':<12} {'Compile(ms)':<13} {'Run(ms)':<10} {'Speedup':<10}")
        print("-" * 75)
        
        for result in self.results:
            status = "✓" if result.passed else "✗"
            interp_time = f"{result.interpreter_time*1000:.2f}"
            compile_time = f"{result.compilation_time*1000:.2f}" if result.compilation_time else "N/A"
            run_time = f"{result.compiled_time*1000:.2f}" if result.compiled_time else "N/A"
            speedup = f"{result.speedup:.2f}x" if result.speedup else "N/A"
            
            print(f"{status} {result.name:<28} {interp_time:<12} {compile_time:<13} {run_time:<10} {speedup:<10}")
        
        print()
        
        # Calculate average speedup for successful tests
        speedups = [r.speedup for r in self.results if r.speedup and r.passed]
        if speedups:
            avg_speedup = sum(speedups) / len(speedups)
            print(f"Average speedup: {avg_speedup:.2f}x")


def main():
    import argparse
    parser = argparse.ArgumentParser(description="NLPL Integration Test & Benchmark Suite")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    args = parser.parse_args()
    
    tester = IntegrationTester(verbose=args.verbose)
    
    # Test 1: Simple arithmetic
    tester.benchmark("arithmetic", """
set a to 10
set b to 20
set result to a plus b
print text result
""")
    
    # Test 2: Factorial (recursive)
    tester.benchmark("factorial", """
function factorial with n as Integer returns Integer
    if n is less than or equal to 1
        return 1
    set prev to factorial with n minus 1
    return n times prev

set result to factorial with 10
print text result
""")
    
    # Test 3: Fibonacci
    tester.benchmark("fibonacci", """
function fib with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    set a to fib with n minus 1
    set b to fib with n minus 2
    return a plus b

set result to fib with 20
print text result
""")
    
    # Test 4: Array operations
    tester.benchmark("array_ops", """
set numbers to [1, 2, 3, 4, 5]
set sum to 0
for each num in numbers
    set sum to sum plus num
print text sum
""")
    
    # Test 5: String operations
    tester.benchmark("strings", """
set name to "NexusLang"
set greeting to "Hello, "
set message to greeting plus name
print text message
""")
    
    # Print summary
    tester.print_summary()
    
    # Exit with proper code
    all_passed = all(r.passed for r in tester.results)
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
