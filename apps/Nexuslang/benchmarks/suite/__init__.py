"""
Benchmark suite package.

Aggregates all domain benchmark cases into ALL_CASES.
"""
from __future__ import annotations

from benchmarks.suite.algorithms import ALGORITHM_CASES
from benchmarks.suite.concurrency import CONCURRENCY_CASES
from benchmarks.suite.io_ops import IO_CASES
from benchmarks.suite.memory_ops import MEMORY_CASES
from benchmarks.suite.numerics import NUMERIC_CASES
from benchmarks.suite.strings import STRING_CASES

ALL_CASES = (
    ALGORITHM_CASES
    + NUMERIC_CASES
    + STRING_CASES
    + IO_CASES
    + MEMORY_CASES
    + CONCURRENCY_CASES
)

__all__ = [
    "ALL_CASES",
    "ALGORITHM_CASES",
    "NUMERIC_CASES",
    "STRING_CASES",
    "IO_CASES",
    "MEMORY_CASES",
    "CONCURRENCY_CASES",
]
