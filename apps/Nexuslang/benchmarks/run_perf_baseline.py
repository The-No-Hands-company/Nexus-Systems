"""
NLPL Performance Baseline Runner
==================================

Measures interpreter performance across multiple benchmark programs
and compares to C reference implementations. Produces perf-baseline.json
with NLPL-vs-C ratios and optimization-level comparisons.

Usage:
    python benchmarks/run_perf_baseline.py
    python benchmarks/run_perf_baseline.py --output perf-baseline.json
    python benchmarks/run_perf_baseline.py --runs 20 --verbose

Output format (perf-baseline.json):
{
    "meta": { "date": "...", "git_commit": "...", "nxl_version": "..." },
    "dispatch_speedup": 22.1,
    "benchmarks": {
        "fibonacci_iterative": {
            "nxl_O0_ms": 0.85,
            "nxl_O1_ms": 0.72,
            "nxl_O2_ms": 0.68,
            "nxl_O3_ms": 0.65,
            "c_o3_ms": 0.002,
            "nxl_vs_c_ratio": 425.0
        },
        ...
    }
}
"""

import sys
import os
import time
import json
import statistics
import subprocess
import argparse
import re
import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Project root
PROJ_ROOT = Path(__file__).parent.parent
BENCH_DIR = PROJ_ROOT / "benchmarks"
SRC_DIR = PROJ_ROOT / "src"

sys.path.insert(0, str(SRC_DIR))


# ---------------------------------------------------------------------------
# Python-level NexusLang interpreter timing (no subprocess overhead)
# ---------------------------------------------------------------------------

def time_nxl_program(source_code: str, optimization_level: int = 0, runs: int = 5) -> Tuple[float, float]:
    """
    Time an NexusLang program using the interpreter directly.

    Returns:
        (median_ms, stdev_ms) - execution times in milliseconds
    """
    from nexuslang.interpreter.interpreter import Interpreter
    from nexuslang.runtime.runtime import Runtime
    from nexuslang.stdlib import register_stdlib
    from nexuslang.parser.lexer import Lexer
    from nexuslang.parser.parser import Parser

    # Pre-parse to separate parse time from execution time
    lexer = Lexer(source_code)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source_code)
    ast = parser.parse()

    times = []
    for _ in range(runs):
        rt = Runtime()
        register_stdlib(rt)
        interp = Interpreter(rt, source=source_code)
        t0 = time.perf_counter()
        interp.interpret(ast, optimization_level=optimization_level)
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)

    med = statistics.median(times)
    std = statistics.stdev(times) if len(times) > 1 else 0.0
    return med, std


def time_nxl_file(nxl_path: str, optimization_level: int = 0, runs: int = 5) -> Tuple[float, float]:
    """Time an NexusLang file."""
    with open(nxl_path) as f:
        source = f.read()
    return time_nxl_program(source, optimization_level=optimization_level, runs=runs)


# ---------------------------------------------------------------------------
# C benchmark compilation and timing
# ---------------------------------------------------------------------------

def compile_c(c_path: str, output_path: str, opt_level: str = "O3") -> bool:
    """Compile a C file. Returns True on success."""
    cmd = ["gcc", f"-{opt_level}", "-o", output_path, c_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  C compilation failed: {result.stderr.strip()}")
        return False
    return True


def time_c_binary(binary_path: str, runs: int = 20) -> Tuple[float, float]:
    """
    Time a compiled C binary.

    Extracts the time reported by the binary itself if it outputs
    'Time: X.XXXXXXXXX seconds', otherwise measures wall clock.

    Returns:
        (median_ms, stdev_ms)
    """
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        result = subprocess.run([binary_path], capture_output=True, text=True, timeout=10)
        t1 = time.perf_counter()

        # Try to extract printed time if available
        m = re.search(r"Time:\s*([\d.]+)\s*seconds", result.stdout)
        if m:
            times.append(float(m.group(1)) * 1000)
        else:
            times.append((t1 - t0) * 1000)

    med = statistics.median(times)
    std = statistics.stdev(times) if len(times) > 1 else 0.0
    return med, std


# ---------------------------------------------------------------------------
# Python benchmark timing
# ---------------------------------------------------------------------------

def time_python_file(py_path: str, runs: int = 20) -> Tuple[float, float]:
    """
    Time a Python benchmark file by importing and calling its function directly.
    Extracts the time reported by the script if it outputs
    'Time: X.XXXXXXXXX seconds', otherwise measures wall clock.

    Returns:
        (median_ms, stdev_ms)
    """
    times = []
    for _ in range(runs):
        result = subprocess.run(
            [sys.executable, py_path],
            capture_output=True, text=True, timeout=30
        )
        m = re.search(r"Time:\s*([\d.]+)\s*seconds", result.stdout)
        if m:
            times.append(float(m.group(1)) * 1000)
        else:
            # Fallback: measure wall-clock of subprocess
            t0 = time.perf_counter()
            subprocess.run([sys.executable, py_path], capture_output=True, timeout=30)
            t1 = time.perf_counter()
            times.append((t1 - t0) * 1000)
    med = statistics.median(times)
    std = statistics.stdev(times) if len(times) > 1 else 0.0
    return med, std


# ---------------------------------------------------------------------------
# Rust benchmark compilation and timing
# ---------------------------------------------------------------------------

def compile_rust(rs_path: str, output_path: str) -> bool:
    """Compile a Rust file with --release optimizations. Returns True on success."""
    cmd = ["rustc", "-C", "opt-level=3", "-o", output_path, rs_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  Rust compilation failed: {result.stderr.strip()[:300]}")
        return False
    return True


def time_rust_binary(binary_path: str, runs: int = 20) -> Tuple[float, float]:
    """
    Time a compiled Rust binary.
    Extracts the time reported by the binary if it outputs
    'Time: X.XXXXXXXXX seconds', otherwise measures wall clock.

    Returns:
        (median_ms, stdev_ms)
    """
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        result = subprocess.run([binary_path], capture_output=True, text=True, timeout=10)
        t1 = time.perf_counter()
        m = re.search(r"Time:\s*([\d.]+)\s*seconds", result.stdout)
        if m:
            times.append(float(m.group(1)) * 1000)
        else:
            times.append((t1 - t0) * 1000)
    med = statistics.median(times)
    std = statistics.stdev(times) if len(times) > 1 else 0.0
    return med, std


# ---------------------------------------------------------------------------
# Dispatch speedup measurement
# ---------------------------------------------------------------------------

def measure_dispatch_speedup() -> float:
    """Measure the speedup from the static dispatch table vs old regex approach."""
    import timeit
    import re as _re

    from nexuslang.interpreter.interpreter import Interpreter
    from nexuslang.runtime.runtime import Runtime
    from nexuslang.stdlib import register_stdlib

    rt = Runtime()
    register_stdlib(rt)
    _ = Interpreter(rt)  # trigger table build
    table = Interpreter._DISPATCH_TABLE

    node_types = [
        "BinaryOperation", "VariableDeclaration", "IfStatement",
        "FunctionCall", "Literal", "Identifier", "WhileLoop", "ReturnStatement",
        "FunctionDefinition", "PrintStatement",
    ]

    def old_dispatch(nt):
        return "execute_" + _re.sub(r"(?<!^)(?=[A-Z])", "_", nt).lower()

    def new_dispatch(nt):
        return table.get(nt)

    n = 50000
    t_old = timeit.timeit(lambda: [old_dispatch(t) for t in node_types], number=n // len(node_types))
    t_new = timeit.timeit(lambda: [new_dispatch(t) for t in node_types], number=n // len(node_types))
    return t_old / t_new if t_new > 0 else 1.0


# ---------------------------------------------------------------------------
# Benchmark definitions
# ---------------------------------------------------------------------------

BENCHMARKS = [
    {
        "name": "fibonacci_iterative",
        "description": "Iterative Fibonacci(1000) - loop + integer arithmetic",
        "nxl_file": str(BENCH_DIR / "bench_fibonacci.nxl"),
        "c_file": str(BENCH_DIR / "bench_fibonacci_iter.c"),
        "c_opt": "O3",
        "python_file": str(BENCH_DIR / "bench_fibonacci_iter.py"),
        "rust_file": str(BENCH_DIR / "bench_fibonacci_iter.rs"),
    },
    {
        "name": "matrix_sum",
        "description": "Matrix sum (200x200) - nested loops, arithmetic",
        "nxl_file": str(BENCH_DIR / "bench_matrix.nxl"),
        "c_file": str(BENCH_DIR / "bench_matrix.c"),
        "c_opt": "O3",
        "python_file": str(BENCH_DIR / "bench_matrix.py"),
        "rust_file": str(BENCH_DIR / "bench_matrix.rs"),
    },
    {
        "name": "sieve_of_eratosthenes",
        "description": "Prime sieve up to 1000 - array access, conditional branching",
        "nxl_file": str(BENCH_DIR / "bench_sieve.nxl"),
        "c_file": str(BENCH_DIR / "bench_sieve.c"),
        "c_opt": "O3",
        "python_file": str(BENCH_DIR / "bench_sieve.py"),
        "rust_file": str(BENCH_DIR / "bench_sieve.rs"),
    },
]


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

def run_benchmark(bench: dict, runs: int, verbose: bool,
                  args_skip_python: bool = False,
                  args_skip_rust: bool = False) -> dict:
    """Run a single benchmark and return results dict."""
    name = bench["name"]
    print(f"\n  [{name}] {bench['description']}")

    result = {
        "description": bench["description"],
        "nxl_O0_ms": None,
        "nxl_O1_ms": None,
        "nxl_O2_ms": None,
        "nxl_O3_ms": None,
        "c_o3_ms": None,
        "python_ms": None,
        "rust_release_ms": None,
        "nxl_vs_c_ratio": None,
        "nxl_vs_python_ratio": None,
        "nxl_vs_rust_ratio": None,
        "nxl_O3_vs_O0_speedup": None,
        "errors": [],
    }

    nxl_file = bench.get("nxl_file")
    c_file = bench.get("c_file")

    # Time NexusLang at each optimization level
    if nxl_file and Path(nxl_file).exists():
        for opt in [0, 1, 2, 3]:
            key = f"nxl_O{opt}_ms"
            try:
                med, std = time_nxl_file(nxl_file, optimization_level=opt, runs=runs)
                result[key] = round(med, 3)
                if verbose:
                    print(f"    NexusLang -O{opt}: {med:.3f} ms  (stdev={std:.3f})")
            except Exception as e:
                result["errors"].append(f"nlpl O{opt}: {e}")
                if verbose:
                    print(f"    NexusLang -O{opt}: ERROR - {e}")
    else:
        result["errors"].append(f"NLPL file not found: {nxl_file}")
        print(f"    SKIP: NexusLang file not found")

    # Time C reference at O3
    if c_file and Path(c_file).exists():
        with tempfile.NamedTemporaryFile(suffix="", delete=False) as f:
            c_out = f.name
        try:
            if compile_c(c_file, c_out, opt_level=bench.get("c_opt", "O3")):
                med, std = time_c_binary(c_out, runs=runs * 4)
                result["c_o3_ms"] = round(med, 6)
                if verbose:
                    print(f"    C -O3:    {med:.6f} ms  (stdev={std:.6f})")
            else:
                result["errors"].append("C compilation failed")
        except Exception as e:
            result["errors"].append(f"C benchmark: {e}")
            if verbose:
                print(f"    C -O3: ERROR - {e}")
        finally:
            try:
                os.unlink(c_out)
            except Exception:
                pass
    else:
        if verbose:
            print(f"    C reference: not available")

    # Time Python reference
    python_file = bench.get("python_file")
    if not args_skip_python and python_file and Path(python_file).exists():
        try:
            med, std = time_python_file(python_file, runs=max(runs, 5))
            result["python_ms"] = round(med, 3)
            if verbose:
                print(f"    Python:   {med:.3f} ms  (stdev={std:.3f})")
        except Exception as e:
            result["errors"].append(f"Python benchmark: {e}")
            if verbose:
                print(f"    Python: ERROR - {e}")
    elif verbose:
        print(f"    Python reference: not available")

    # Time Rust reference (--release = opt-level=3)
    rust_file = bench.get("rust_file")
    if not args_skip_rust and rust_file and Path(rust_file).exists():
        with tempfile.NamedTemporaryFile(suffix="", delete=False) as f:
            rust_out = f.name
        try:
            if compile_rust(rust_file, rust_out):
                med, std = time_rust_binary(rust_out, runs=runs * 4)
                result["rust_release_ms"] = round(med, 6)
                if verbose:
                    print(f"    Rust -O3: {med:.6f} ms  (stdev={std:.6f})")
            else:
                result["errors"].append("Rust compilation failed")
        except Exception as e:
            result["errors"].append(f"Rust benchmark: {e}")
            if verbose:
                print(f"    Rust: ERROR - {e}")
        finally:
            try:
                os.unlink(rust_out)
            except Exception:
                pass
    elif verbose:
        print(f"    Rust reference: not available")

    # Compute ratios
    nxl_best = result.get("nxl_O3_ms") or result.get("nxl_O2_ms") or result.get("nxl_O0_ms")
    nxl_o0 = result.get("nxl_O0_ms")
    nxl_o3 = result.get("nxl_O3_ms")

    if nxl_best and result["c_o3_ms"] and result["c_o3_ms"] > 0:
        result["nxl_vs_c_ratio"] = round(nxl_best / result["c_o3_ms"], 1)
        if verbose:
            print(f"    NexusLang/C ratio:      {result['nxl_vs_c_ratio']}x slower")

    if nxl_best and result["python_ms"] and result["python_ms"] > 0:
        result["nxl_vs_python_ratio"] = round(nxl_best / result["python_ms"], 3)
        if verbose:
            print(f"    NexusLang/Python ratio: {result['nxl_vs_python_ratio']}x")

    if nxl_best and result["rust_release_ms"] and result["rust_release_ms"] > 0:
        result["nxl_vs_rust_ratio"] = round(nxl_best / result["rust_release_ms"], 1)
        if verbose:
            print(f"    NexusLang/Rust ratio:   {result['nxl_vs_rust_ratio']}x slower")

    if nxl_o0 and nxl_o3 and nxl_o0 > 0:
        result["nxl_O3_vs_O0_speedup"] = round(nxl_o0 / nxl_o3, 3)
        if verbose:
            print(f"    O3 vs O0 speedup:  {result['nxl_O3_vs_O0_speedup']}x")

    return result


def get_git_commit() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=str(PROJ_ROOT), text=True
        ).strip()
    except Exception:
        return "unknown"


def main():
    parser = argparse.ArgumentParser(description="NLPL Performance Baseline Runner")
    parser.add_argument("--output", default=str(PROJ_ROOT / "benchmarks" / "perf-baseline.json"),
                        help="Output JSON file (default: benchmarks/perf-baseline.json)")
    parser.add_argument("--runs", type=int, default=10,
                        help="Number of runs per measurement (default: 10)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print detailed timing for each benchmark")
    parser.add_argument("--skip-c", action="store_true",
                        help="Skip C compilation/benchmarks")
    parser.add_argument("--skip-python", action="store_true",
                        help="Skip Python benchmarks")
    parser.add_argument("--skip-rust", action="store_true",
                        help="Skip Rust compilation/benchmarks")
    args = parser.parse_args()

    print("NLPL Performance Baseline")
    print("=" * 50)
    print(f"Runs per measurement: {args.runs}")
    print(f"Output: {args.output}")

    # Measure dispatch speedup
    print("\nMeasuring dispatch table speedup...")
    dispatch_speedup = measure_dispatch_speedup()
    print(f"  Dispatch speedup: {dispatch_speedup:.1f}x (regex -> dict lookup)")

    # Run benchmarks
    bench_results = {}
    for bench in BENCHMARKS:
        # Honor --skip-c, --skip-python, --skip-rust
        bench = dict(bench)
        if args.skip_c:
            bench["c_file"] = None
        if args.skip_python:
            bench["python_file"] = None
        if args.skip_rust:
            bench["rust_file"] = None
        bench_results[bench["name"]] = run_benchmark(
            bench, args.runs, args.verbose,
            args_skip_python=args.skip_python,
            args_skip_rust=args.skip_rust,
        )

    # Build output
    from datetime import datetime

    # Collect language availability
    languages_available = ["nlpl", "c"]
    if not args.skip_python:
        languages_available.append("python")
    if not args.skip_rust:
        languages_available.append("rust")

    def _compiler_version(cmd: list) -> str:
        try:
            return subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True).splitlines()[0]
        except Exception:
            return "unavailable"

    output = {
        "meta": {
            "date": datetime.now().isoformat(),
            "git_commit": get_git_commit(),
            "runs_per_measurement": args.runs,
            "python_version": sys.version.split()[0],
            "gcc_version": _compiler_version(["gcc", "--version"]) if not args.skip_c else "skipped",
            "rustc_version": _compiler_version(["rustc", "--version"]) if not args.skip_rust else "skipped",
            "platform": sys.platform,
            "languages_compared": languages_available,
            "optimization_description": {
                "O0": "No AST optimization",
                "O1": "Basic: constant folding + DCE",
                "O2": "Standard: O1 + strength reduction + loop unrolling + inlining",
                "O3": "Aggressive: O2 + CSE + TCO + aggressive inlining",
            },
        },
        "dispatch_speedup_x": round(dispatch_speedup, 1),
        "baseline": bench_results,
    }

    # Save
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w") as f:
        json.dump(output, f, indent=2)

    # Summary
    print("\n" + "=" * 50)
    print("SUMMARY")
    print("=" * 50)
    print(f"Dispatch speedup: {dispatch_speedup:.1f}x")
    print()
    for bench_name, r in bench_results.items():
        print(f"{bench_name}:")
        o0 = r.get("nxl_O0_ms")
        o3 = r.get("nxl_O3_ms")
        c = r.get("c_o3_ms")
        py = r.get("python_ms")
        rs = r.get("rust_release_ms")
        ratio_c = r.get("nxl_vs_c_ratio")
        ratio_py = r.get("nxl_vs_python_ratio")
        ratio_rs = r.get("nxl_vs_rust_ratio")
        speedup = r.get("nxl_O3_vs_O0_speedup")
        if o0:
            print(f"  NexusLang O0:        {o0:.3f} ms")
        if o3:
            print(f"  NexusLang O3:        {o3:.3f} ms", end="")
            if speedup:
                print(f"  ({speedup:.2f}x vs O0)", end="")
            print()
        if c:
            print(f"  C -O3:          {c:.6f} ms")
        if py:
            print(f"  Python:         {py:.3f} ms")
        if rs:
            print(f"  Rust --release: {rs:.6f} ms")
        if ratio_c:
            print(f"  NexusLang/C ratio:   {ratio_c}x slower than C")
        if ratio_py:
            direction = "faster" if ratio_py < 1 else "slower"
            print(f"  NexusLang/Python:    {ratio_py}x {direction} than Python")
        if ratio_rs:
            print(f"  NexusLang/Rust:      {ratio_rs}x slower than Rust")
        if r.get("errors"):
            for e in r["errors"]:
                print(f"  WARNING: {e}")
        print()

    print(f"Results saved to: {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
