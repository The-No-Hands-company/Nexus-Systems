#!/usr/bin/env python3
"""
Test and benchmark the scope optimizer module.
"""

import sys
import os
import timeit
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.interpreter.scope_optimizer import enable_scope_optimization, disable_scope_optimization
from nexuslang.runtime.runtime import Runtime
from nexuslang.stdlib import register_stdlib

def test_scope_optimizer_correctness():
 """Test that scope optimizer produces correct results."""
 print("="*80)
 print("Testing Scope Optimizer Correctness")
 print("="*80)
 
 # Test program with nested scopes and variable shadowing
 source = """
set x to 10
set y to 20

function outer with n as Integer returns Integer
 set x to n
 
 function inner with m as Integer returns Integer
 set y to m
 return x plus y
 end
 
 set result to call inner with 5
 return result
end

set final to call outer with 100
print text final
"""
 
 # Parse once
 lexer = Lexer(source)
 tokens = lexer.tokenize()
 parser = Parser(tokens)
 ast = parser.parse()
 
 # Test WITHOUT optimization
 print("\n--- Without Scope Optimization ---")
 runtime1 = Runtime()
 interpreter1 = Interpreter(runtime1)
 register_stdlib(runtime1)
 
 result1 = None
 try:
 result1 = interpreter1.interpret(ast)
 print(" Standard scope lookup: SUCCESS")
 except Exception as e:
 print(f" Standard scope lookup: FAILED - {e}")
 return False
 
 # Test WITH optimization
 print("\n--- With Scope Optimization ---")
 runtime2 = Runtime()
 interpreter2 = Interpreter(runtime2)
 register_stdlib(runtime2)
 
 # Enable optimization
 enable_scope_optimization(interpreter2)
 
 result2 = None
 try:
 result2 = interpreter2.interpret(ast)
 print(" Optimized scope lookup: SUCCESS")
 except Exception as e:
 print(f" Optimized scope lookup: FAILED - {e}")
 return False
 
 # Print cache stats
 if hasattr(interpreter2, '_scope_cache'):
 print("\nScope Cache Statistics:")
 interpreter2._scope_cache.print_stats()
 
 # Results should match
 if result1 == result2:
 print("\n PASS: Results match!")
 return True
 else:
 print(f"\n FAIL: Results differ! Standard: {result1}, Optimized: {result2}")
 return False

def benchmark_scope_optimizer():
 """Benchmark scope optimizer performance."""
 print("\n" + "="*80)
 print("Benchmarking Scope Optimizer Performance")
 print("="*80)
 
 # Variable-heavy program to stress test scope lookups
 source = """
set a to 1
set b to 2
set c to 3
set d to 4
set e to 5

set i to 0
while i is less than 100
 set sum to a plus b plus c plus d plus e
 set product to a times b
 set i to i plus 1
end
"""
 
 lexer = Lexer(source)
 tokens = lexer.tokenize()
 parser = Parser(tokens)
 ast = parser.parse()
 
 NUMBER = 50
 
 # Benchmark WITHOUT optimization
 def run_standard():
 runtime = Runtime()
 interpreter = Interpreter(runtime)
 register_stdlib(runtime)
 interpreter.interpret(ast)
 
 print("\n--- Standard Scope Lookup ---")
 time_standard = timeit.timeit(run_standard, number=NUMBER)
 print(f" Total time ({NUMBER} runs): {time_standard:.4f}s")
 print(f" Average per run: {time_standard/NUMBER*1000:.2f}ms")
 
 # Benchmark WITH optimization
 def run_optimized():
 runtime = Runtime()
 interpreter = Interpreter(runtime)
 register_stdlib(runtime)
 enable_scope_optimization(interpreter)
 interpreter.interpret(ast)
 
 print("\n--- Optimized Scope Lookup ---")
 time_optimized = timeit.timeit(run_optimized, number=NUMBER)
 print(f" Total time ({NUMBER} runs): {time_optimized:.4f}s")
 print(f" Average per run: {time_optimized/NUMBER*1000:.2f}ms")
 
 # Calculate speedup
 speedup = time_standard / time_optimized
 improvement = ((time_standard - time_optimized) / time_standard) * 100
 
 print("\n" + "="*80)
 print("Performance Comparison")
 print("="*80)
 print(f" Standard: {time_standard:.4f}s")
 print(f" Optimized: {time_optimized:.4f}s")
 print(f" Speedup: {speedup:.2f}x")
 print(f" Improvement: {improvement:.1f}%")
 
 if speedup > 1.0:
 print(f"\n Optimization provides {speedup:.2f}x speedup!")
 else:
 print(f"\n Optimization overhead detected ({speedup:.2f}x)")

def test_enable_disable():
 """Test enabling and disabling optimization at runtime."""
 print("\n" + "="*80)
 print("Testing Enable/Disable Functionality")
 print("="*80)
 
 source = """
set x to 42
print text x
"""
 
 lexer = Lexer(source)
 tokens = lexer.tokenize()
 parser = Parser(tokens)
 ast = parser.parse()
 
 runtime = Runtime()
 interpreter = Interpreter(runtime)
 register_stdlib(runtime)
 
 # Run without optimization
 print("\n1. Running without optimization...")
 interpreter.interpret(ast)
 print(" Standard execution works")
 
 # Enable optimization
 print("\n2. Enabling optimization...")
 enable_scope_optimization(interpreter)
 print(" Optimization enabled")
 
 # Run with optimization
 lexer = Lexer(source)
 tokens = lexer.tokenize()
 parser = Parser(tokens)
 ast = parser.parse()
 
 print("\n3. Running with optimization...")
 interpreter.interpret(ast)
 print(" Optimized execution works")
 
 # Disable optimization
 print("\n4. Disabling optimization...")
 disable_scope_optimization(interpreter)
 print(" Optimization disabled")
 
 # Run again without optimization
 lexer = Lexer(source)
 tokens = lexer.tokenize()
 parser = Parser(tokens)
 ast = parser.parse()
 
 print("\n5. Running without optimization again...")
 interpreter.interpret(ast)
 print(" Standard execution works after disable")
 
 print("\n PASS: Enable/disable cycle works correctly!")

if __name__ == "__main__":
 print("NLPL Scope Optimizer Test Suite\n")
 
 # Run tests
 success = True
 
 try:
 if not test_scope_optimizer_correctness():
 success = False
 except Exception as e:
 print(f" Correctness test failed with exception: {e}")
 import traceback
 traceback.print_exc()
 success = False
 
 try:
 test_enable_disable()
 except Exception as e:
 print(f" Enable/disable test failed with exception: {e}")
 import traceback
 traceback.print_exc()
 success = False
 
 try:
 benchmark_scope_optimizer()
 except Exception as e:
 print(f" Benchmark failed with exception: {e}")
 import traceback
 traceback.print_exc()
 success = False
 
 print("\n" + "="*80)
 if success:
 print(" ALL TESTS PASSED")
 else:
 print(" SOME TESTS FAILED")
 print("="*80)
