#!/usr/bin/env python3
"""
Compare interpreter performance before and after optimization.
Uses timeit for accurate benchmarking.
"""

import sys
import os
import timeit
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))


def benchmark_simple_program():
    """Benchmark a simple NexusLang program with basic operations."""
    source = """
set x to 0
while x is less than 100
    set x to x plus 1
end
"""
    
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser
    from nexuslang.interpreter.interpreter import Interpreter
    from nexuslang.runtime.runtime import Runtime
    
    # Parse once
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    # Time interpretation only
    def run_interpreter():
        runtime = Runtime()
        interpreter = Interpreter(runtime)
        from nexuslang.stdlib import register_stdlib
        register_stdlib(runtime)
        interpreter.interpret(ast)
    
    return run_interpreter


def benchmark_function_calls():
    """Benchmark function call overhead."""
    source = """
function my_add with x as Integer and y as Integer returns Integer
    return x plus y
end

set i to 0
while i is less than 50
    set result to call my_add with i and 1
    set i to i plus 1
end
"""
    
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser
    from nexuslang.interpreter.interpreter import Interpreter
    from nexuslang.runtime.runtime import Runtime
    
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    def run_interpreter():
        runtime = Runtime()
        interpreter = Interpreter(runtime)
        from nexuslang.stdlib import register_stdlib
        register_stdlib(runtime)
        interpreter.interpret(ast)
    
    return run_interpreter


def benchmark_arithmetic():
    """Benchmark arithmetic operations."""
    source = """
set sum to 0
set i to 0
while i is less than 100
    set sum to sum plus i
    set product to i times 2
    set i to i plus 1
end
"""
    
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser
    from nexuslang.interpreter.interpreter import Interpreter
    from nexuslang.runtime.runtime import Runtime
    
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    
    def run_interpreter():
        runtime = Runtime()
        interpreter = Interpreter(runtime)
        from nexuslang.stdlib import register_stdlib
        register_stdlib(runtime)
        interpreter.interpret(ast)
    
    return run_interpreter


if __name__ == "__main__":
    print("="*80)
    print("NLPL Interpreter Performance Benchmark")
    print("="*80)
    print()
    
    # Number of runs for each benchmark
    NUMBER = 100
    
    benchmarks = [
        ("Simple Loop (100 iterations)", benchmark_simple_program),
        ("Function Calls (50 calls)", benchmark_function_calls),
        ("Arithmetic Operations (100 ops)", benchmark_arithmetic),
    ]
    
    for name, benchmark_func in benchmarks:
        print(f"Benchmark: {name}")
        func = benchmark_func()
        
        # Time the benchmark
        time_taken = timeit.timeit(func, number=NUMBER)
        avg_time = time_taken / NUMBER
        
        print(f"  Total time ({NUMBER} runs): {time_taken:.4f}s")
        print(f"  Average per run: {avg_time*1000:.2f}ms")
        print(f"  Runs per second: {NUMBER/time_taken:.2f}")
        print()
    
    print("="*80)
    print("Benchmark Complete")
    print("="*80)
