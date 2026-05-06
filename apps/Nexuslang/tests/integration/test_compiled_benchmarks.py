"""
Compiled benchmark integration tests.

Compiles the canonical NexusLang benchmark programs through the full LLVM pipeline
(NLPL source -> LLVM IR -> opt -> llc -> clang -> native binary), runs each
binary, and asserts:

  1. Exit code 0
  2. Output contains the expected result value
  3. Wall-clock time is below a generous ceiling (guards against regressions
     that re-introduce interpreter-like overhead in the compiled path)

These tests double as smoke tests for the LLVM backend on real workloads.

Requirements
------------
- LLVM tools (llc, opt) installed and on PATH
- clang installed and on PATH

Tests are automatically *skipped* when the required tools are absent so CI
environments without LLVM installed do not fail.
"""

from __future__ import annotations

import os
import sys
import time
import tempfile
import subprocess

import pytest

# Ensure the src/ tree is importable regardless of how pytest is invoked.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.compiler.backends.llvm_ir_generator import LLVMIRGenerator


# ---------------------------------------------------------------------------
# Helpers shared with test_compiler_roundtrip.py (duplicated to keep tests
# independent)
# ---------------------------------------------------------------------------

def _tools_available() -> bool:
    """Return True when llc, opt AND clang are all on PATH."""
    import shutil
    return all(shutil.which(t) for t in ("llc", "opt", "clang"))


skip_no_llvm = pytest.mark.skipif(
    not _tools_available(),
    reason="LLVM tools (llc, opt, clang) not available on PATH",
)

# Repository root is two directories above this file (tests/integration/).
_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
_BENCHMARKS_DIR = os.path.join(_REPO_ROOT, "benchmarks")


def _compile_benchmark(source_path: str, opt_level: int, tmpdir: str) -> str:
    """Compile an NexusLang benchmark source file to a native executable.

    Parameters
    ----------
    source_path:
        Absolute path to the ``.nlpl`` benchmark source.
    opt_level:
        LLVM optimisation level (0, 1, 2, or 3).
    tmpdir:
        Writable temporary directory for intermediate files.

    Returns
    -------
    str
        Path to the compiled native executable.
    """
    with open(source_path, "r", encoding="utf-8") as fh:
        source = fh.read()

    tokens = Lexer(source).tokenize()
    ast = Parser(tokens).parse()

    # Suppress irrelevant stdout emitted by the generator / compiler steps.
    _devnull = open(os.devnull, "w")
    old_stdout = sys.stdout
    sys.stdout = _devnull
    try:
        gen = LLVMIRGenerator()
        gen.generate(ast, source_file=source_path)
        exe_path = os.path.join(tmpdir, "bench_out")
        gen.compile_to_executable(exe_path, opt_level=opt_level)
    finally:
        sys.stdout = old_stdout
        _devnull.close()

    return exe_path


def _run_binary(exe_path: str) -> tuple[int, str, float]:
    """Run a compiled binary and measure wall-clock elapsed time.

    Returns
    -------
    (returncode, stdout_stripped, elapsed_seconds)
    """
    t0 = time.perf_counter()
    result = subprocess.run(
        [exe_path],
        capture_output=True,
        text=True,
        timeout=30,
    )
    elapsed = time.perf_counter() - t0
    return result.returncode, result.stdout.strip(), elapsed


# ---------------------------------------------------------------------------
# Benchmark test cases
# ---------------------------------------------------------------------------

@skip_no_llvm
class TestFibonacciBenchmark:
    """Fibonacci(1000) iterative benchmark compiled to native."""

    SOURCE = os.path.join(_BENCHMARKS_DIR, "bench_fibonacci.nxl")
    EXPECTED_VALUE = "817770325994397771"
    # Generous ceiling: even under CI load a trivial binary should finish fast.
    MAX_WALL_SECONDS = 10.0

    def test_compiles_and_runs_O0(self, tmp_path):
        exe = _compile_benchmark(self.SOURCE, opt_level=0, tmpdir=str(tmp_path))
        rc, stdout, elapsed = _run_binary(exe)
        assert rc == 0, f"Exit code {rc}; stderr not captured separately"
        assert self.EXPECTED_VALUE in stdout, (
            f"Expected '{self.EXPECTED_VALUE}' in output, got:\n{stdout}"
        )
        assert elapsed < self.MAX_WALL_SECONDS, (
            f"Binary ran in {elapsed:.3f}s, ceiling is {self.MAX_WALL_SECONDS}s"
        )

    def test_compiles_and_runs_O3(self, tmp_path):
        exe = _compile_benchmark(self.SOURCE, opt_level=3, tmpdir=str(tmp_path))
        rc, stdout, elapsed = _run_binary(exe)
        assert rc == 0, f"Exit code {rc}"
        assert self.EXPECTED_VALUE in stdout, (
            f"Expected '{self.EXPECTED_VALUE}' in output, got:\n{stdout}"
        )
        assert elapsed < self.MAX_WALL_SECONDS, (
            f"Binary ran in {elapsed:.3f}s, ceiling is {self.MAX_WALL_SECONDS}s"
        )

    def test_O3_not_slower_than_O0_by_large_factor(self, tmp_path):
        """Compiled -O3 binary must not be more than 5x slower than -O0.

        Both optimisation levels run in ~0.75ms (OS startup dominated), so
        this guard catches a catastrophic regression where opt introduces
        expensive runtime behaviour.
        """
        tmp_o0 = tmp_path / "o0"
        tmp_o0.mkdir()
        tmp_o3 = tmp_path / "o3"
        tmp_o3.mkdir()

        exe_o0 = _compile_benchmark(self.SOURCE, opt_level=0, tmpdir=str(tmp_o0))
        exe_o3 = _compile_benchmark(self.SOURCE, opt_level=3, tmpdir=str(tmp_o3))

        _, _, t_o0 = _run_binary(exe_o0)
        _, _, t_o3 = _run_binary(exe_o3)

        assert t_o3 < t_o0 * 5, (
            f"O3 ({t_o3*1000:.2f}ms) more than 5x slower than O0 ({t_o0*1000:.2f}ms)"
        )


@skip_no_llvm
class TestMatrixSumBenchmark:
    """Matrix sum (200x200) benchmark compiled to native."""

    SOURCE = os.path.join(_BENCHMARKS_DIR, "bench_matrix.nxl")
    EXPECTED_VALUE = "799980000"
    MAX_WALL_SECONDS = 15.0

    def test_compiles_and_runs_O0(self, tmp_path):
        exe = _compile_benchmark(self.SOURCE, opt_level=0, tmpdir=str(tmp_path))
        rc, stdout, elapsed = _run_binary(exe)
        assert rc == 0, f"Exit code {rc}"
        assert self.EXPECTED_VALUE in stdout, (
            f"Expected '{self.EXPECTED_VALUE}' in output, got:\n{stdout}"
        )
        assert elapsed < self.MAX_WALL_SECONDS, (
            f"Binary ran in {elapsed:.3f}s, ceiling is {self.MAX_WALL_SECONDS}s"
        )

    def test_compiles_and_runs_O3(self, tmp_path):
        exe = _compile_benchmark(self.SOURCE, opt_level=3, tmpdir=str(tmp_path))
        rc, stdout, elapsed = _run_binary(exe)
        assert rc == 0, f"Exit code {rc}"
        assert self.EXPECTED_VALUE in stdout, (
            f"Expected '{self.EXPECTED_VALUE}' in output, got:\n{stdout}"
        )
        assert elapsed < self.MAX_WALL_SECONDS, (
            f"Binary ran in {elapsed:.3f}s, ceiling is {self.MAX_WALL_SECONDS}s"
        )


@skip_no_llvm
class TestSieveBenchmark:
    """Sieve of Eratosthenes (primes up to 1000) benchmark compiled to native."""

    SOURCE = os.path.join(_BENCHMARKS_DIR, "bench_sieve.nxl")
    EXPECTED_VALUE = "168"
    MAX_WALL_SECONDS = 15.0

    def test_compiles_and_runs_O0(self, tmp_path):
        exe = _compile_benchmark(self.SOURCE, opt_level=0, tmpdir=str(tmp_path))
        rc, stdout, elapsed = _run_binary(exe)
        assert rc == 0, f"Exit code {rc}"
        assert self.EXPECTED_VALUE in stdout, (
            f"Expected '{self.EXPECTED_VALUE}' in output, got:\n{stdout}"
        )
        assert elapsed < self.MAX_WALL_SECONDS, (
            f"Binary ran in {elapsed:.3f}s, ceiling is {self.MAX_WALL_SECONDS}s"
        )

    def test_compiles_and_runs_O3(self, tmp_path):
        exe = _compile_benchmark(self.SOURCE, opt_level=3, tmpdir=str(tmp_path))
        rc, stdout, elapsed = _run_binary(exe)
        assert rc == 0, f"Exit code {rc}"
        assert self.EXPECTED_VALUE in stdout, (
            f"Expected '{self.EXPECTED_VALUE}' in output, got:\n{stdout}"
        )
        assert elapsed < self.MAX_WALL_SECONDS, (
            f"Binary ran in {elapsed:.3f}s, ceiling is {self.MAX_WALL_SECONDS}s"
        )

    def test_output_is_not_stale_sieve_limit(self, tmp_path):
        """Guard against stale output: result must be '168', not '999' (the old sieve limit)."""
        exe = _compile_benchmark(self.SOURCE, opt_level=3, tmpdir=str(tmp_path))
        _, stdout, _ = _run_binary(exe)
        assert "999" not in stdout or self.EXPECTED_VALUE in stdout, (
            "Output contains stale sieve limit '999' without the correct prime count '168'"
        )
