#!/usr/bin/env python3
"""
Compiled-path benchmark runner.

Compiles bench_fibonacci.nlpl, bench_matrix.nlpl, and bench_sieve.nlpl
through the LLVM IR backend, measures execution times, and updates
perf-baseline.json with the results.

Usage:
    cd /path/to/NLPL
    .venv/bin/python benchmarks/bench_compiled.py [--runs N] [--update-json]

Output:
    Prints a markdown-style table comparing compiled NexusLang vs interpreter vs C.
    With --update-json, writes results into benchmarks/perf-baseline.json.
"""

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


# ---------------------------------------------------------------------------
# Benchmark programs
# ---------------------------------------------------------------------------

BENCHMARKS = [
    {
        "key": "fibonacci_iterative",
        "name": "Fibonacci(1000) iterative",
        "nxl_file": REPO / "benchmarks" / "bench_fibonacci.nxl",
        "c_o3_binary": REPO / "benchmarks" / "bench_fib_c_o3",
        "c_o0_binary": REPO / "benchmarks" / "bench_fib_c_o0",
        "expected_output_contains": "817770325994397771",
    },
    {
        "key": "matrix_sum",
        "name": "Matrix sum 200x200",
        "nxl_file": REPO / "benchmarks" / "bench_matrix.nxl",
        "c_o3_binary": REPO / "benchmarks" / "bench_matrix_c_o3",
        "c_o0_binary": REPO / "benchmarks" / "bench_matrix_c_o0",
        "expected_output_contains": "799980000",
    },
    {
        "key": "sieve_of_eratosthenes",
        "name": "Sieve of Eratosthenes (limit=1000)",
        "nxl_file": REPO / "benchmarks" / "bench_sieve.nxl",
        "c_o3_binary": REPO / "benchmarks" / "bench_sieve_c_o3",
        "c_o0_binary": REPO / "benchmarks" / "bench_sieve_c_o0",
        "expected_output_contains": "168",
    },
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def measure_binary(binary_path: Path, runs: int = 10) -> dict:
    """Run a pre-compiled binary N times and return timing statistics (ms)."""
    times_ms = []
    last_stdout = ""
    for _ in range(runs):
        t0 = time.perf_counter()
        r = subprocess.run(
            [str(binary_path)],
            capture_output=True, text=True, timeout=30,
        )
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        if r.returncode == 0:
            times_ms.append(elapsed_ms)
            last_stdout = r.stdout.strip()
        else:
            return {"error": f"exit {r.returncode}: {r.stderr[:200]}"}

    if not times_ms:
        return {"error": "no successful runs"}

    return {
        "median_ms": statistics.median(times_ms),
        "mean_ms": statistics.mean(times_ms),
        "min_ms": min(times_ms),
        "max_ms": max(times_ms),
        "stdev_ms": statistics.stdev(times_ms) if len(times_ms) > 1 else 0.0,
        "runs": len(times_ms),
        "stdout": last_stdout,
    }


def compile_nxl_to_binary(source_path: Path, opt_level: int, tmpdir: str) -> tuple[bool, Path, str]:
    """Compile an NexusLang source file through the LLVM backend.

    Returns (success, exe_path, error_message).
    """
    src = source_path.read_text()
    try:
        tokens = Lexer(src).tokenize()
        ast = Parser(tokens).parse()
    except Exception as exc:
        return False, Path(), f"parse error: {exc}"

    try:
        gen = LLVMIRGenerator()
        import io
        captured = io.StringIO()
        old_stdout = sys.stdout
        sys.stdout = captured
        try:
            gen.generate(ast, source_file=str(source_path))
        finally:
            sys.stdout = old_stdout
    except Exception as exc:
        return False, Path(), f"IR generation error: {exc}"

    exe = Path(tmpdir) / f"{source_path.stem}_O{opt_level}"
    try:
        captured2 = io.StringIO()
        old_stdout2 = sys.stdout
        sys.stdout = captured2
        try:
            ok = gen.compile_to_executable(str(exe), opt_level=opt_level)
        finally:
            sys.stdout = old_stdout2
    except Exception as exc:
        return False, Path(), f"compile error: {exc}"

    if not ok or not exe.exists():
        return False, Path(), "executable not created"

    return True, exe, ""


def measure_interpreter(source_path: Path, runs: int = 5) -> dict:
    """Measure interpreter execution time (includes Python startup)."""
    venv_py = REPO / ".venv" / "bin" / "python"
    interpreter = REPO / "src" / "nlpl" / "main.py"
    times_ms = []
    last_stdout = ""
    for _ in range(runs):
        t0 = time.perf_counter()
        r = subprocess.run(
            [str(venv_py), str(interpreter), str(source_path)],
            capture_output=True, text=True, timeout=60, cwd=str(REPO),
        )
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        if r.returncode == 0:
            times_ms.append(elapsed_ms)
            last_stdout = r.stdout.strip()
        else:
            return {"error": f"interpreter exit {r.returncode}: {r.stderr[:300]}"}

    if not times_ms:
        return {"error": "no successful interpreter runs"}

    return {
        "median_ms": statistics.median(times_ms),
        "mean_ms": statistics.mean(times_ms),
        "min_ms": min(times_ms),
        "stdout": last_stdout,
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def run_all(runs: int, update_json: bool, verbose: bool) -> dict:
    results = {}

    with tempfile.TemporaryDirectory(prefix="nxl_bench_") as tmpdir:
        for bench in BENCHMARKS:
            key = bench["key"]
            name = bench["name"]
            nxl_file = bench["nxl_file"]
            c_o3_bin = bench["c_o3_binary"]
            c_o0_bin = bench["c_o0_binary"]
            expected = bench["expected_output_contains"]

            print(f"\n{'='*70}")
            print(f"Benchmark: {name}")
            print(f"{'='*70}")

            entry = {"description": name}

            # -- C reference timings ------------------------------------------
            for label, binary in [("c_o0", c_o0_bin), ("c_o3", c_o3_bin)]:
                if binary.exists():
                    print(f"  Measuring C -{label[-2:].upper()} ({runs} runs)... ", end="", flush=True)
                    stats = measure_binary(binary, runs=runs)
                    if "error" in stats:
                        print(f"FAILED: {stats['error']}")
                    else:
                        entry[f"{label}_ms"] = round(stats["median_ms"], 6)
                        entry[f"{label}_stdout"] = stats["stdout"]
                        print(f"{stats['median_ms']:.4f} ms  output: {stats['stdout'][:60]!r}")
                else:
                    print(f"  C {label[-2:].upper()} binary not found: {binary}")

            # -- Interpreter timing  ------------------------------------------
            print(f"  Measuring interpreter ({min(runs, 5)} runs)... ", end="", flush=True)
            interp_stats = measure_interpreter(nxl_file, runs=min(runs, 5))
            if "error" in interp_stats:
                print(f"FAILED: {interp_stats['error']}")
                entry["interpreter_error"] = interp_stats["error"]
            else:
                entry["interpreter_ms"] = round(interp_stats["median_ms"], 3)
                entry["interpreter_stdout"] = interp_stats["stdout"]
                print(f"{interp_stats['median_ms']:.1f} ms  output: {interp_stats['stdout'][:60]!r}")

            # -- Compiled NexusLang timings ----------------------------------------
            for opt_level in [0, 3]:
                label = f"compiled_O{opt_level}"
                print(f"  Compiling NexusLang -O{opt_level}... ", end="", flush=True)
                ok, exe_path, err = compile_nxl_to_binary(nxl_file, opt_level, tmpdir)
                if not ok:
                    print(f"FAILED: {err}")
                    entry[f"{label}_error"] = err
                    continue
                print(f"ok, measuring ({runs} runs)... ", end="", flush=True)
                stats = measure_binary(exe_path, runs=runs)
                if "error" in stats:
                    print(f"RUN FAILED: {stats['error']}")
                    entry[f"{label}_error"] = stats["error"]
                    continue

                entry[f"{label}_ms"] = round(stats["median_ms"], 6)
                entry[f"{label}_stdout"] = stats["stdout"]
                print(f"{stats['median_ms']:.4f} ms  output: {stats['stdout'][:60]!r}")

                # Correctness check
                if expected and expected not in stats["stdout"]:
                    print(f"    WARNING: expected '{expected}' in output but got {stats['stdout']!r}")
                    entry[f"{label}_correctness"] = "MISMATCH"
                else:
                    entry[f"{label}_correctness"] = "ok"

            # -- Speedup calculations ----------------------------------------
            interp_ms = entry.get("interpreter_ms")
            c_o3_ms = entry.get("c_o3_ms")

            for opt_level in [0, 3]:
                label = f"compiled_O{opt_level}"
                compiled_ms = entry.get(f"{label}_ms")
                if compiled_ms and interp_ms:
                    speedup = interp_ms / compiled_ms
                    entry[f"{label}_speedup_vs_interpreter"] = round(speedup, 1)
                if compiled_ms and c_o3_ms:
                    ratio = compiled_ms / c_o3_ms
                    entry[f"{label}_ratio_vs_c_o3"] = round(ratio, 2)

            # -- Summary line -------------------------------------------------
            co3 = entry.get("c_o3_ms")
            compiled_o3 = entry.get("compiled_O3_ms")
            if co3 and compiled_o3:
                ratio = compiled_o3 / co3
                target = "MEETS TARGET (<= 3x)" if ratio <= 3.0 else f"MISSES TARGET ({ratio:.1f}x C -O3)"
                print(f"\n  COMPILED-O3 vs C-O3: {ratio:.2f}x  -- {target}")
            if interp_ms and compiled_o3:
                speedup = interp_ms / compiled_o3
                print(f"  Speedup vs interpreter: {speedup:.0f}x")

            results[key] = entry

    return results


def print_summary_table(results: dict):
    print(f"\n\n{'='*80}")
    print("SUMMARY: Compiled NexusLang vs Interpreter vs C -O3")
    print(f"{'='*80}")
    print(f"{'Benchmark':<32} {'Interp(ms)':>12} {'Comp-O0(ms)':>12} {'Comp-O3(ms)':>12} {'C-O3(ms)':>10} {'Comp-O3 vs C':>14}")
    print(f"{'-'*80}")
    for key, entry in results.items():
        name = entry.get("description", key)[:30]
        interp = f"{entry.get('interpreter_ms', 'N/A')}" if 'interpreter_ms' in entry else "err"
        co0 = f"{entry.get('compiled_O0_ms', 'N/A'):.4f}" if 'compiled_O0_ms' in entry else "err"
        co3 = f"{entry.get('compiled_O3_ms', 'N/A'):.4f}" if 'compiled_O3_ms' in entry else "err"
        c = f"{entry.get('c_o3_ms', 'N/A'):.6f}" if 'c_o3_ms' in entry else "N/A"
        ratio = entry.get("compiled_O3_ratio_vs_c_o3")
        ratio_str = f"{ratio:.2f}x" if ratio is not None else "N/A"
        print(f"{name:<32} {interp:>12} {co0:>12} {co3:>12} {c:>10} {ratio_str:>14}")


def update_perf_baseline(results: dict, runs: int):
    """Merge results into benchmarks/perf-baseline.json."""
    baseline_path = REPO / "benchmarks" / "perf-baseline.json"
    baseline = json.loads(baseline_path.read_text())

    import datetime, subprocess as sp
    try:
        commit = sp.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=str(REPO), stderr=sp.DEVNULL, timeout=5,
        ).decode().strip()
    except Exception:
        commit = "unknown"

    baseline["meta"]["compiled_benchmark_date"] = datetime.datetime.now().isoformat()
    baseline["meta"]["compiled_benchmark_commit"] = commit
    baseline["meta"]["compiled_benchmark_runs"] = runs

    for key, entry in results.items():
        if key not in baseline["baseline"]:
            baseline["baseline"][key] = {}
        target = baseline["baseline"][key]

        # Overwrite compiled and interpreter fields; leave legacy C/Python/Rust entries
        for field in [
            "interpreter_ms", "interpreter_stdout",
            "compiled_O0_ms", "compiled_O0_stdout", "compiled_O0_correctness",
            "compiled_O0_speedup_vs_interpreter", "compiled_O0_ratio_vs_c_o3",
            "compiled_O3_ms", "compiled_O3_stdout", "compiled_O3_correctness",
            "compiled_O3_speedup_vs_interpreter", "compiled_O3_ratio_vs_c_o3",
            "c_o0_ms", "c_o3_ms",
        ]:
            if field in entry:
                target[field] = entry[field]

    baseline_path.write_text(json.dumps(baseline, indent=2) + "\n")
    print(f"\nUpdated: {baseline_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--runs", type=int, default=10,
                    help="Number of timing runs per binary (default: 10)")
    ap.add_argument("--update-json", action="store_true",
                    help="Write results into benchmarks/perf-baseline.json")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    print("NLPL Compiled-Path Benchmark")
    print(f"LLVM tools: clang={_tool_version('clang')}, llc={_tool_version('llc')}")
    print(f"Runs per binary: {args.runs}")

    results = run_all(runs=args.runs, update_json=args.update_json, verbose=args.verbose)
    print_summary_table(results)

    if args.update_json:
        update_perf_baseline(results, runs=args.runs)
    else:
        print("\n(Use --update-json to write results to perf-baseline.json)")


def _tool_version(tool: str) -> str:
    try:
        r = subprocess.run([tool, "--version"], capture_output=True, text=True, timeout=3)
        return r.stdout.split("\n")[0][:40]
    except Exception:
        return "not found"


if __name__ == "__main__":
    main()
