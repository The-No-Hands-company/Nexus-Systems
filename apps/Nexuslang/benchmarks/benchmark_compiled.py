#!/usr/bin/env python3
"""
Benchmark NexusLang compiled mode vs interpreter mode vs C.

Measures the three standard benchmarks (fibonacci, matrix_sum, sieve)
through all execution modes and updates perf-baseline.json with compiled
results.

Usage:
    python benchmarks/benchmark_compiled.py
"""

import json
import os
import subprocess
import sys
import tempfile
import time
import io
from datetime import datetime

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator
from nexuslang.interpreter.interpreter import Interpreter
from nexuslang.runtime.runtime import Runtime

BENCHMARK_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(BENCHMARK_DIR)

# ---------------------------------------------------------------------------
# NexusLang programs adapted for compiler (need explicit main() function, and must
# use integer output directly — no 'convert ... to string' since the compiler
# handles integer-to-string via sprintf already).
# ---------------------------------------------------------------------------

# fib(40) = 102334155 -- fits in 64-bit, big enough to be interesting
FIBONACCI_SRC = """
function fibonacci with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    set a to 0
    set b to 1
    set i to 2
    while i is less than or equal to n
        set temp to a plus b
        set a to b
        set b to temp
        set i to i plus 1
    end
    return b
end

function main returns Integer
    set result to fibonacci with 40
    print text result
    return 0
end
"""

# Interpreter uses top-level call instead of main()
FIBONACCI_INTERP_SRC = """
function fibonacci with n as Integer returns Integer
    if n is less than or equal to 1
        return n
    end
    set a to 0
    set b to 1
    set i to 2
    while i is less than or equal to n
        set temp to a plus b
        set a to b
        set b to temp
        set i to i plus 1
    end
    return b
end

set result to fibonacci with 40
print text result
"""

MATRIX_SRC = """
function matrix_sum with size as Integer returns Integer
    set sum to 0
    set i to 0
    while i is less than size
        set j to 0
        while j is less than size
            set value to i times size plus j
            set sum to sum plus value
            set j to j plus 1
        end
        set i to i plus 1
    end
    return sum
end

function main returns Integer
    set result to matrix_sum with 200
    print text result
    return 0
end
"""

MATRIX_INTERP_SRC = """
function matrix_sum with size as Integer returns Integer
    set sum to 0
    set i to 0
    while i is less than size
        set j to 0
        while j is less than size
            set value to i times size plus j
            set sum to sum plus value
            set j to j plus 1
        end
        set i to i plus 1
    end
    return sum
end

set result to matrix_sum with 200
print text result
"""

# Sieve using modulo operator -- count primes up to 200
SIEVE_SRC = """
function count_primes with limit as Integer returns Integer
    set count to 0
    set candidate to 2
    while candidate is less than or equal to limit
        set is_prime to 1
        set divisor to 2
        while divisor times divisor is less than or equal to candidate
            if candidate modulo divisor equals 0
                set is_prime to 0
            end
            set divisor to divisor plus 1
        end
        if is_prime equals 1
            set count to count plus 1
        end
        set candidate to candidate plus 1
    end
    return count
end

function main returns Integer
    set result to count_primes with 200
    print text result
    return 0
end
"""

SIEVE_INTERP_SRC = """
function count_primes with limit as Integer returns Integer
    set count to 0
    set candidate to 2
    while candidate is less than or equal to limit
        set is_prime to 1
        set divisor to 2
        while divisor times divisor is less than or equal to candidate
            if candidate modulo divisor equals 0
                set is_prime to 0
            end
            set divisor to divisor plus 1
        end
        if is_prime equals 1
            set count to count plus 1
        end
        set candidate to candidate plus 1
    end
    return count
end

set result to count_primes with 200
print text result
"""

BENCHMARKS = [
    ("fibonacci_iterative_40",
     "Iterative Fibonacci(40) -- loop + integer arithmetic",
     FIBONACCI_SRC,
     FIBONACCI_INTERP_SRC,
     "102334155"),
    ("matrix_sum_200x200",
     "Matrix sum 200x200 -- nested loops + arithmetic",
     MATRIX_SRC,
     MATRIX_INTERP_SRC,
     "799980000"),
    ("prime_count_200",
     "Naive prime counting up to 200 -- divisibility check loops",
     SIEVE_SRC,
     SIEVE_INTERP_SRC,
     "46"),
]

RUNS = 5  # measurements per benchmark


# ---------------------------------------------------------------------------
# Compilation helpers
# ---------------------------------------------------------------------------

def compile_nxl_to_binary(name: str, source: str, opt_level: int = 0) -> str | None:
    """Compile NexusLang source to a native binary (in a temp dir, caller manages).
    
    Returns path to the compiled executable, or None on failure.
    """
    import shutil
    if not all(shutil.which(t) for t in ("llc", "opt", "clang")):
        return None

    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()
    gen = LLVMIRGenerator()
    gen.generate(ast, source_file=f"{name}.nxl")

    # We need to keep the tmpdir alive; store on gen as a side channel
    tmpdir = tempfile.mkdtemp(prefix="nxl_bench_")
    exe = os.path.join(tmpdir, name)

    saved_stdout = sys.stdout
    sys.stdout = io.StringIO()
    try:
        ok = gen.compile_to_executable(exe, opt_level=opt_level)
    finally:
        sys.stdout = saved_stdout

    if ok and os.path.exists(exe):
        return exe
    return None


# ---------------------------------------------------------------------------
# Timing helper
# ---------------------------------------------------------------------------

def measure_ms(fn, runs: int) -> tuple[float, list[float]]:
    """Return (median_ms, [all_ms])."""
    timings = []
    for _ in range(runs):
        t0 = time.perf_counter()
        fn()
        timings.append((time.perf_counter() - t0) * 1000.0)
    timings.sort()
    return timings[len(timings) // 2], timings


def run_binary_n(exe: str, runs: int) -> tuple[float, str]:
    """Time running the binary *runs* times, return (median_ms, last_stdout)."""
    timings = []
    last_out = ""
    for _ in range(runs):
        t0 = time.perf_counter()
        result = subprocess.run([exe], capture_output=True, text=True, timeout=30)
        timings.append((time.perf_counter() - t0) * 1000.0)
        last_out = result.stdout.strip()
    timings.sort()
    return timings[len(timings) // 2], last_out


def run_interpreter(source: str, runs: int) -> tuple[float, str]:
    """Run a program through the NexusLang interpreter, return (median_ms, stdout)."""
    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()

    timings = []
    last_out = ""
    for _ in range(runs):
        runtime = Runtime()
        interp = Interpreter(runtime)
        captured = io.StringIO()
        saved = sys.stdout
        sys.stdout = captured
        t0 = time.perf_counter()
        interp.interpret(ast)
        elapsed = (time.perf_counter() - t0) * 1000.0
        sys.stdout = saved
        timings.append(elapsed)
        last_out = captured.getvalue().strip()
    timings.sort()
    return timings[len(timings) // 2], last_out


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    print("NLPL Compiled vs Interpreter Benchmark")
    print("=" * 60)

    results = {}

    for bench_name, description, source, interp_source, expected_output in BENCHMARKS:
        print(f"\n--- {bench_name} ---")
        print(f"    {description}")
        entry = {"description": description}

        # Interpreter timing (uses interp_source which has top-level call)
        print("  Interpreter: ", end="", flush=True)
        try:
            interp_ms, interp_out = run_interpreter(interp_source, RUNS)
            # Strip GLFW warning lines
            real_out = "\n".join(
                line for line in interp_out.splitlines()
                if "GLFW" not in line and "PyOpenGL" not in line and "Install with" not in line
            ).strip()
            entry["interpreter_ms"] = round(interp_ms, 3)
            entry["interpreter_output"] = real_out
            match = " OK" if real_out == expected_output else f" WRONG (expected {expected_output!r}, got {real_out!r})"
            print(f"{interp_ms:.3f} ms{match}")
        except Exception as e:
            print(f"FAILED: {e}")
            entry["interpreter_ms"] = None
            entry["interpreter_error"] = str(e)

        # Compiled O0 timing
        print("  Compiled O0: ", end="", flush=True)
        exe = compile_nxl_to_binary(bench_name, source, opt_level=0)
        if exe:
            try:
                compiled_ms, compiled_out = run_binary_n(exe, RUNS)
                entry["compiled_O0_ms"] = round(compiled_ms, 3)
                entry["compiled_O0_output"] = compiled_out
                match = " OK" if compiled_out == expected_output else f" WRONG (expected {expected_output!r}, got {compiled_out!r})"
                print(f"{compiled_ms:.3f} ms{match}")
                import shutil
                shutil.rmtree(os.path.dirname(exe), ignore_errors=True)
            except Exception as e:
                print(f"FAILED: {e}")
                entry["compiled_O0_ms"] = None
                entry["compiled_O0_error"] = str(e)
        else:
            print("SKIPPED (LLVM tools unavailable)")
            entry["compiled_O0_ms"] = None

        # Compiled O3 timing
        print("  Compiled O3: ", end="", flush=True)
        exe3 = compile_nxl_to_binary(bench_name, source, opt_level=3)
        if exe3:
            try:
                compiled3_ms, compiled3_out = run_binary_n(exe3, RUNS)
                entry["compiled_O3_ms"] = round(compiled3_ms, 3)
                entry["compiled_O3_output"] = compiled3_out
                match = " OK" if compiled3_out == expected_output else f" WRONG (expected {expected_output!r}, got {compiled3_out!r})"
                print(f"{compiled3_ms:.3f} ms{match}")
                import shutil
                shutil.rmtree(os.path.dirname(exe3), ignore_errors=True)
            except Exception as e:
                print(f"FAILED: {e}")
                entry["compiled_O3_ms"] = None
                entry["compiled_O3_error"] = str(e)
        else:
            print("SKIPPED")
            entry["compiled_O3_ms"] = None

        # Speedup ratios
        interp = entry.get("interpreter_ms")
        comp0 = entry.get("compiled_O0_ms")
        comp3 = entry.get("compiled_O3_ms")

        if interp and comp0 and comp0 > 0:
            ratio = interp / comp0
            entry["compiled_O0_speedup_vs_interp"] = round(ratio, 1)
            print(f"  Speedup (compiled O0 vs interpreter): {ratio:.1f}x")
        if interp and comp3 and comp3 > 0:
            ratio3 = interp / comp3
            entry["compiled_O3_speedup_vs_interp"] = round(ratio3, 1)
            print(f"  Speedup (compiled O3 vs interpreter): {ratio3:.1f}x")
        if comp0 and comp3 and comp3 > 0:
            ir = comp0 / comp3
            entry["compiled_O3_vs_O0"] = round(ir, 2)
            print(f"  Speedup (compiled O3 vs O0): {ir:.2f}x")

        results[bench_name] = entry

    # Load existing baseline and merge compiled data
    baseline_path = os.path.join(BENCHMARK_DIR, "perf-baseline.json")
    try:
        with open(baseline_path) as f:
            baseline = json.load(f)
    except FileNotFoundError:
        baseline = {"meta": {}, "baseline": {}}

    baseline["meta"]["compiled_benchmark_date"] = datetime.now().isoformat()
    baseline["meta"]["compiled_benchmark_commit"] = (
        subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                       capture_output=True, text=True, cwd=PROJECT_ROOT)
        .stdout.strip()
    )

    for bench_name, entry in results.items():
        if bench_name not in baseline["baseline"]:
            baseline["baseline"][bench_name] = {}
        baseline["baseline"][bench_name].update(entry)

    with open(baseline_path, "w") as f:
        json.dump(baseline, f, indent=2)

    print(f"\nBaseline updated: {baseline_path}")
    print("\nSummary")
    print("-" * 60)
    for bench_name, entry in results.items():
        interp = entry.get("interpreter_ms", "N/A")
        comp0 = entry.get("compiled_O0_ms", "N/A")
        comp3 = entry.get("compiled_O3_ms", "N/A")
        sp0 = entry.get("compiled_O0_speedup_vs_interp", "N/A")
        sp3 = entry.get("compiled_O3_speedup_vs_interp", "N/A")
        print(f"  {bench_name}:")
        print(f"    interpreter: {interp} ms")
        print(f"    compiled O0: {comp0} ms  (x{sp0} vs interpreter)")
        print(f"    compiled O3: {comp3} ms  (x{sp3} vs interpreter)")


if __name__ == "__main__":
    main()
