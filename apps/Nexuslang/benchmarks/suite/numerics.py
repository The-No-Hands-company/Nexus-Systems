"""
Numerical computation benchmark suite.

Covers: matrix multiply, prime sieve, fibonacci (iterative/recursive),
Gaussian elimination, Newton's method, Monte Carlo.
"""
from __future__ import annotations

import math
import random
from typing import List

from benchmarks.benchmark_ci import BenchmarkCase

CATEGORY = "numerics"


# ---------------------------------------------------------------------------
# Matrix operations
# ---------------------------------------------------------------------------

def _matmul_naive(a: List[List[float]], b: List[List[float]]) -> List[List[float]]:
    n = len(a)
    c = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for k in range(n):
            for j in range(n):
                c[i][j] += a[i][k] * b[k][j]
    return c


def _make_matrix(n: int, seed: int = 42) -> List[List[float]]:
    rng = random.Random(seed)
    return [[rng.random() for _ in range(n)] for _ in range(n)]


_MAT32 = _make_matrix(32)
_MAT64 = _make_matrix(64)


def _bench_matmul_32() -> None:
    _matmul_naive(_MAT32, _MAT32)


def _bench_matmul_64() -> None:
    _matmul_naive(_MAT64, _MAT64)


def _bench_mat_transpose_100() -> None:
    m = _make_matrix(100)
    n = len(m)
    _ = [[m[j][i] for j in range(n)] for i in range(n)]


def _bench_dot_product_10k() -> None:
    rng = random.Random(1)
    a = [rng.random() for _ in range(10_000)]
    b = [rng.random() for _ in range(10_000)]
    _ = sum(x * y for x, y in zip(a, b))


# ---------------------------------------------------------------------------
# Prime sieve
# ---------------------------------------------------------------------------

def _sieve_of_eratosthenes(limit: int) -> List[int]:
    is_prime = bytearray([1]) * (limit + 1)
    is_prime[0] = is_prime[1] = 0
    for i in range(2, int(limit ** 0.5) + 1):
        if is_prime[i]:
            is_prime[i * i :: i] = bytearray(len(is_prime[i * i :: i]))
    return [i for i, v in enumerate(is_prime) if v]


def _bench_sieve_10k() -> None:
    _sieve_of_eratosthenes(10_000)


def _bench_sieve_100k() -> None:
    _sieve_of_eratosthenes(100_000)


def _bench_sieve_1m() -> None:
    _sieve_of_eratosthenes(1_000_000)


# ---------------------------------------------------------------------------
# Fibonacci
# ---------------------------------------------------------------------------

def _fib_iter(n: int) -> int:
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a


def _fib_matrix(n: int) -> int:
    """O(log n) matrix exponentiation."""
    def mat_mul(a: List[List[int]], b: List[List[int]]) -> List[List[int]]:
        return [
            [a[0][0] * b[0][0] + a[0][1] * b[1][0],
             a[0][0] * b[0][1] + a[0][1] * b[1][1]],
            [a[1][0] * b[0][0] + a[1][1] * b[1][0],
             a[1][0] * b[0][1] + a[1][1] * b[1][1]],
        ]

    def mat_pow(m: List[List[int]], p: int) -> List[List[int]]:
        result = [[1, 0], [0, 1]]
        while p > 0:
            if p & 1:
                result = mat_mul(result, m)
            m = mat_mul(m, m)
            p >>= 1
        return result

    if n == 0:
        return 0
    return mat_pow([[1, 1], [1, 0]], n)[0][1]


def _bench_fib_iter_1000() -> None:
    _fib_iter(1_000)


def _bench_fib_matrix_1000() -> None:
    _fib_matrix(1_000)


# ---------------------------------------------------------------------------
# Gaussian elimination
# ---------------------------------------------------------------------------

def _gauss_eliminate(matrix: List[List[float]]) -> List[float]:
    """Solve Ax=b via partial-pivot Gaussian elimination."""
    n = len(matrix)
    m = [row[:] for row in matrix]
    for col in range(n):
        pivot = max(range(col, n), key=lambda r: abs(m[r][col]))
        m[col], m[pivot] = m[pivot], m[col]
        if abs(m[col][col]) < 1e-12:
            continue
        for row in range(col + 1, n):
            factor = m[row][col] / m[col][col]
            for j in range(col, n + 1):
                m[row][j] -= factor * m[col][j]
    x = [0.0] * n
    for i in range(n - 1, -1, -1):
        x[i] = m[i][n]
        for j in range(i + 1, n):
            x[i] -= m[i][j] * x[j]
        x[i] /= m[i][i] if abs(m[i][i]) > 1e-12 else 1.0
    return x


def _make_augmented(n: int, seed: int = 99) -> List[List[float]]:
    rng = random.Random(seed)
    m = [[rng.random() + (10 if i == j else 0) for j in range(n + 1)]
         for i in range(n)]
    return m


_AUG_20 = _make_augmented(20)
_AUG_50 = _make_augmented(50)


def _bench_gauss_20() -> None:
    _gauss_eliminate(_AUG_20)


def _bench_gauss_50() -> None:
    _gauss_eliminate(_AUG_50)


# ---------------------------------------------------------------------------
# Newton's method
# ---------------------------------------------------------------------------

def _bench_newton_sqrt_1k() -> None:
    """Find sqrt of 1000 values via Newton–Raphson."""
    for val in range(1, 1001):
        x = float(val)
        for _ in range(20):
            x = 0.5 * (x + val / x)


# ---------------------------------------------------------------------------
# Monte Carlo
# ---------------------------------------------------------------------------

def _bench_monte_carlo_pi_100k() -> None:
    rng = random.Random(3)
    inside = sum(
        1 for _ in range(100_000)
        if rng.random() ** 2 + rng.random() ** 2 <= 1.0
    )
    _ = 4.0 * inside / 100_000


# ---------------------------------------------------------------------------
# Integer arithmetic
# ---------------------------------------------------------------------------

def _bench_integer_arithmetic_100k() -> None:
    acc = 0
    for i in range(1, 100_001):
        acc = (acc + i * i) % (2 ** 31)


def _bench_float_arithmetic_100k() -> None:
    acc = 0.0
    for i in range(1, 100_001):
        acc += math.sqrt(float(i))


def _bench_gcd_100k() -> None:
    import math as m
    rng = random.Random(17)
    for _ in range(100_000):
        m.gcd(rng.randint(1, 10 ** 9), rng.randint(1, 10 ** 9))


# ---------------------------------------------------------------------------
# Case list
# ---------------------------------------------------------------------------

NUMERIC_CASES = [
    # Matrix
    BenchmarkCase("matmul_32x32",      CATEGORY, _bench_matmul_32, bench_iters=50),
    BenchmarkCase("matmul_64x64",      CATEGORY, _bench_matmul_64, bench_iters=10),
    BenchmarkCase("mat_transpose_100", CATEGORY, _bench_mat_transpose_100),
    BenchmarkCase("dot_product_10k",   CATEGORY, _bench_dot_product_10k),
    # Primes
    BenchmarkCase("sieve_10k",         CATEGORY, _bench_sieve_10k),
    BenchmarkCase("sieve_100k",        CATEGORY, _bench_sieve_100k),
    BenchmarkCase("sieve_1m",          CATEGORY, _bench_sieve_1m, bench_iters=5),
    # Fibonacci
    BenchmarkCase("fib_iter_1000",     CATEGORY, _bench_fib_iter_1000),
    BenchmarkCase("fib_matrix_1000",   CATEGORY, _bench_fib_matrix_1000),
    # Linear algebra
    BenchmarkCase("gauss_20x20",       CATEGORY, _bench_gauss_20),
    BenchmarkCase("gauss_50x50",       CATEGORY, _bench_gauss_50),
    # Root finding / approximation
    BenchmarkCase("newton_sqrt_1k",    CATEGORY, _bench_newton_sqrt_1k),
    BenchmarkCase("monte_carlo_100k",  CATEGORY, _bench_monte_carlo_pi_100k, bench_iters=10),
    # Arithmetic
    BenchmarkCase("int_arith_100k",    CATEGORY, _bench_integer_arithmetic_100k),
    BenchmarkCase("float_arith_100k",  CATEGORY, _bench_float_arithmetic_100k),
    BenchmarkCase("gcd_100k",          CATEGORY, _bench_gcd_100k),
]
