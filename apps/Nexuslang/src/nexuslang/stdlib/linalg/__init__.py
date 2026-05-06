"""
NLPL Linear Algebra Module.

Provides matrix and vector operations.  Everything is pure Python — no
numpy required — so the module is always available without extra installs.

All matrices are represented as List[List[float]] in row-major order.
All vectors are List[float].

Registered NexusLang Functions
--------------------------
Matrix creation
    mat_zeros(rows, cols)          -> List[List[float]]
    mat_ones(rows, cols)           -> List[List[float]]
    mat_identity(n)                -> List[List[float]]
    mat_from_list(data)            -> List[List[float]]  (validates shape)
    mat_random(rows, cols)         -> List[List[float]]  (uniform 0..1)
    mat_diagonal(values)           -> List[List[float]]  (diag from vector)

Matrix shape / info
    mat_shape(matrix)              -> [rows, cols]
    mat_rows(matrix)               -> Integer
    mat_cols(matrix)               -> Integer
    mat_is_square(matrix)          -> Boolean

Matrix element-wise operations
    mat_add(a, b)                  -> List[List[float]]
    mat_sub(a, b)                  -> List[List[float]]
    mat_scale(matrix, scalar)      -> List[List[float]]
    mat_negate(matrix)             -> List[List[float]]
    mat_element_mul(a, b)          -> element-wise product (Hadamard)

Matrix structural operations
    mat_transpose(matrix)          -> List[List[float]]
    mat_mul(a, b)                  -> matrix multiplication
    mat_pow(matrix, n)             -> repeated matrix multiplication (n >= 0)

Matrix properties / decomposition
    mat_trace(matrix)              -> Float   (sum of main diagonal)
    mat_determinant(matrix)        -> Float   (via LU decomposition)
    mat_inverse(matrix)            -> List[List[float]]   (Gauss-Jordan)
    mat_rank(matrix)               -> Integer (via row echelon)
    mat_is_symmetric(matrix)       -> Boolean
    mat_frobenius_norm(matrix)     -> Float   (sqrt of sum of squares)

Linear systems
    mat_solve(a, b)                -> List[float]  (solve Ax = b, Gaussian elim)
    mat_least_squares(a, b)        -> List[float]  (min ||Ax - b||, normal eqns)

Vector operations
    vec_dot(a, b)                  -> Float
    vec_cross(a, b)                -> List[float]  (3-D only)
    vec_norm(v)                    -> Float   (Euclidean / L2)
    vec_norm_l1(v)                 -> Float   (L1 / Manhattan)
    vec_normalize(v)               -> List[float]  (unit vector)
    vec_add(a, b)                  -> List[float]
    vec_sub(a, b)                  -> List[float]
    vec_scale(v, scalar)           -> List[float]
    vec_angle(a, b)                -> Float   (radians, in [0, pi])
    vec_project(a, b)              -> List[float]  (projection of a onto b)
    vec_outer(a, b)                -> List[List[float]]  (outer product matrix)
"""

from __future__ import annotations

import math
import random as _random
import copy
from typing import Any, List, Optional


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

_Matrix = List[List[float]]
_Vector = List[float]


def _coerce_matrix(m: Any) -> _Matrix:
    """Accept nested lists; coerce all values to float."""
    result = []
    for row in m:
        result.append([float(x) for x in row])
    return result


def _check_matrix(m: _Matrix, name: str = "matrix") -> None:
    if not m or not m[0]:
        raise ValueError(f"{name} must be non-empty")
    ncols = len(m[0])
    for i, row in enumerate(m):
        if len(row) != ncols:
            raise ValueError(
                f"{name} row {i} has {len(row)} columns, expected {ncols}"
            )


def _check_same_shape(a: _Matrix, b: _Matrix) -> None:
    if len(a) != len(b) or len(a[0]) != len(b[0]):
        raise ValueError(
            f"Shape mismatch: {_shape(a)} vs {_shape(b)}"
        )


def _shape(m: _Matrix) -> tuple:
    return (len(m), len(m[0]) if m else 0)


def _copy(m: _Matrix) -> _Matrix:
    return [row[:] for row in m]


# ---------------------------------------------------------------------------
# Matrix creation
# ---------------------------------------------------------------------------

def mat_zeros(rows: int, cols: int) -> _Matrix:
    """Return a *rows* x *cols* matrix of zeros."""
    rows, cols = int(rows), int(cols)
    if rows <= 0 or cols <= 0:
        raise ValueError("rows and cols must be positive integers")
    return [[0.0] * cols for _ in range(rows)]


def mat_ones(rows: int, cols: int) -> _Matrix:
    """Return a *rows* x *cols* matrix of ones."""
    rows, cols = int(rows), int(cols)
    if rows <= 0 or cols <= 0:
        raise ValueError("rows and cols must be positive integers")
    return [[1.0] * cols for _ in range(rows)]


def mat_identity(n: int) -> _Matrix:
    """Return the *n* x *n* identity matrix."""
    n = int(n)
    if n <= 0:
        raise ValueError("n must be a positive integer")
    result = [[0.0] * n for _ in range(n)]
    for i in range(n):
        result[i][i] = 1.0
    return result


def mat_diagonal(values: _Vector) -> _Matrix:
    """Create a square diagonal matrix from *values*."""
    n = len(values)
    if n == 0:
        raise ValueError("values must not be empty")
    result = [[0.0] * n for _ in range(n)]
    for i, v in enumerate(values):
        result[i][i] = float(v)
    return result


def mat_from_list(data: Any) -> _Matrix:
    """Validate and normalise nested list *data* into a matrix.

    Raises ValueError if rows have different lengths or data is empty.
    """
    m = _coerce_matrix(data)
    _check_matrix(m)
    return m


def mat_random(rows: int, cols: int) -> _Matrix:
    """Return a *rows* x *cols* matrix with random values in [0, 1)."""
    rows, cols = int(rows), int(cols)
    if rows <= 0 or cols <= 0:
        raise ValueError("rows and cols must be positive integers")
    return [[_random.random() for _ in range(cols)] for _ in range(rows)]


# ---------------------------------------------------------------------------
# Matrix shape / info
# ---------------------------------------------------------------------------

def mat_shape(matrix: _Matrix) -> List[int]:
    """Return [rows, cols] of *matrix*."""
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    return [len(m), len(m[0])]


def mat_rows(matrix: _Matrix) -> int:
    """Return number of rows."""
    return int(len(matrix))


def mat_cols(matrix: _Matrix) -> int:
    """Return number of columns (from the first row)."""
    if not matrix:
        return 0
    return int(len(matrix[0]))


def mat_is_square(matrix: _Matrix) -> bool:
    """Return True if *matrix* has equal rows and columns."""
    m = _coerce_matrix(matrix)
    if not m or not m[0]:
        return False
    return len(m) == len(m[0])


# ---------------------------------------------------------------------------
# Element-wise operations
# ---------------------------------------------------------------------------

def mat_add(a: _Matrix, b: _Matrix) -> _Matrix:
    """Element-wise addition of matrices *a* and *b*."""
    a, b = _coerce_matrix(a), _coerce_matrix(b)
    _check_same_shape(a, b)
    return [[a[i][j] + b[i][j] for j in range(len(a[0]))] for i in range(len(a))]


def mat_sub(a: _Matrix, b: _Matrix) -> _Matrix:
    """Element-wise subtraction *a* - *b*."""
    a, b = _coerce_matrix(a), _coerce_matrix(b)
    _check_same_shape(a, b)
    return [[a[i][j] - b[i][j] for j in range(len(a[0]))] for i in range(len(a))]


def mat_scale(matrix: _Matrix, scalar: float) -> _Matrix:
    """Multiply every element of *matrix* by *scalar*."""
    m = _coerce_matrix(matrix)
    s = float(scalar)
    return [[m[i][j] * s for j in range(len(m[0]))] for i in range(len(m))]


def mat_negate(matrix: _Matrix) -> _Matrix:
    """Return *matrix* with all elements negated."""
    return mat_scale(matrix, -1.0)


def mat_element_mul(a: _Matrix, b: _Matrix) -> _Matrix:
    """Element-wise (Hadamard) product of *a* and *b*."""
    a, b = _coerce_matrix(a), _coerce_matrix(b)
    _check_same_shape(a, b)
    return [[a[i][j] * b[i][j] for j in range(len(a[0]))] for i in range(len(a))]


# ---------------------------------------------------------------------------
# Structural operations
# ---------------------------------------------------------------------------

def mat_transpose(matrix: _Matrix) -> _Matrix:
    """Return the transpose of *matrix*."""
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    rows, cols = len(m), len(m[0])
    return [[m[r][c] for r in range(rows)] for c in range(cols)]


def mat_mul(a: _Matrix, b: _Matrix) -> _Matrix:
    """Matrix multiply *a* (m x k) by *b* (k x n) → m x n result."""
    a, b = _coerce_matrix(a), _coerce_matrix(b)
    _check_matrix(a, "a")
    _check_matrix(b, "b")
    m, k1 = len(a), len(a[0])
    k2, n = len(b), len(b[0])
    if k1 != k2:
        raise ValueError(
            f"Cannot multiply {m}x{k1} matrix by {k2}x{n} matrix"
        )
    result = [[0.0] * n for _ in range(m)]
    for i in range(m):
        for j in range(n):
            s = 0.0
            for k in range(k1):
                s += a[i][k] * b[k][j]
            result[i][j] = s
    return result


def mat_pow(matrix: _Matrix, n: int) -> _Matrix:
    """Raise square *matrix* to the integer power *n* (n >= 0).

    mat_pow(A, 0) returns the identity matrix.
    mat_pow(A, 1) returns A unchanged.
    """
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    if not mat_is_square(m):
        raise ValueError("mat_pow requires a square matrix")
    n = int(n)
    if n < 0:
        raise ValueError("mat_pow exponent must be >= 0")
    size = len(m)
    result = mat_identity(size)
    base = _copy(m)
    while n > 0:
        if n % 2 == 1:
            result = mat_mul(result, base)
        base = mat_mul(base, base)
        n //= 2
    return result


# ---------------------------------------------------------------------------
# Matrix properties / decomposition
# ---------------------------------------------------------------------------

def mat_trace(matrix: _Matrix) -> float:
    """Return the sum of the main diagonal elements."""
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    if not mat_is_square(m):
        raise ValueError("mat_trace requires a square matrix")
    return sum(m[i][i] for i in range(len(m)))


def _lu_decompose(m: _Matrix):
    """LU decomposition with partial pivoting.

    Returns (L, U, P, sign) where P is the permutation vector,
    sign is +1/-1 (parity of row swaps), used for determinant.
    Raises ValueError if the matrix is singular.
    """
    n = len(m)
    a = [row[:] for row in m]
    perm = list(range(n))
    sign = 1

    for col in range(n):
        # Find pivot
        max_val = abs(a[col][col])
        max_row = col
        for row in range(col + 1, n):
            if abs(a[row][col]) > max_val:
                max_val = abs(a[row][col])
                max_row = row

        if max_row != col:
            a[col], a[max_row] = a[max_row], a[col]
            perm[col], perm[max_row] = perm[max_row], perm[col]
            sign *= -1

        pivot = a[col][col]
        if abs(pivot) < 1e-12:
            raise ValueError("Matrix is singular (no unique solution)")

        for row in range(col + 1, n):
            factor = a[row][col] / pivot
            a[row][col] = factor
            for k in range(col + 1, n):
                a[row][k] -= factor * a[col][k]

    return a, perm, sign


def mat_determinant(matrix: _Matrix) -> float:
    """Compute the determinant of a square matrix via LU decomposition."""
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    if not mat_is_square(m):
        raise ValueError("mat_determinant requires a square matrix")
    n = len(m)
    if n == 1:
        return m[0][0]
    if n == 2:
        return m[0][0] * m[1][1] - m[0][1] * m[1][0]
    try:
        lu, _, sign = _lu_decompose(m)
        det = float(sign)
        for i in range(n):
            det *= lu[i][i]
        return det
    except ValueError:
        return 0.0


def mat_inverse(matrix: _Matrix) -> _Matrix:
    """Return the inverse of a square matrix using Gauss-Jordan elimination.

    Raises ValueError if the matrix is singular.
    """
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    if not mat_is_square(m):
        raise ValueError("mat_inverse requires a square matrix")
    n = len(m)
    # Augment with identity
    aug = [m[i][:] + [1.0 if i == j else 0.0 for j in range(n)] for i in range(n)]

    for col in range(n):
        # Partial pivot
        max_row = max(range(col, n), key=lambda r: abs(aug[r][col]))
        aug[col], aug[max_row] = aug[max_row], aug[col]

        pivot = aug[col][col]
        if abs(pivot) < 1e-12:
            raise ValueError("Matrix is singular — no inverse exists")

        # Normalise pivot row
        aug[col] = [x / pivot for x in aug[col]]

        # Eliminate column
        for row in range(n):
            if row != col:
                factor = aug[row][col]
                aug[row] = [aug[row][k] - factor * aug[col][k] for k in range(2 * n)]

    return [row[n:] for row in aug]


def mat_rank(matrix: _Matrix) -> int:
    """Return the rank of *matrix* (number of linearly independent rows).

    Uses reduced row echelon form, iterating over columns so that
    all-zero columns are skipped correctly.
    """
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    rows, cols = len(m), len(m[0])
    ref = [row[:] for row in m]
    pivot_row = 0
    for col in range(cols):
        if pivot_row >= rows:
            break
        # Find a non-zero pivot in this column at or below pivot_row
        swap = next(
            (r for r in range(pivot_row, rows) if abs(ref[r][col]) > 1e-12),
            None,
        )
        if swap is None:
            # Entire column below pivot_row is zero; skip to next column
            continue
        ref[pivot_row], ref[swap] = ref[swap], ref[pivot_row]
        pivot = ref[pivot_row][col]
        ref[pivot_row] = [x / pivot for x in ref[pivot_row]]
        for r in range(rows):
            if r != pivot_row:
                factor = ref[r][col]
                ref[r] = [ref[r][k] - factor * ref[pivot_row][k] for k in range(cols)]
        pivot_row += 1
    return pivot_row


def mat_is_symmetric(matrix: _Matrix) -> bool:
    """Return True if *matrix* equals its transpose (within floating-point tolerance)."""
    m = _coerce_matrix(matrix)
    if not mat_is_square(m):
        return False
    n = len(m)
    for i in range(n):
        for j in range(i + 1, n):
            if abs(m[i][j] - m[j][i]) > 1e-9:
                return False
    return True


def mat_frobenius_norm(matrix: _Matrix) -> float:
    """Return the Frobenius norm: sqrt(sum of squares of all elements)."""
    m = _coerce_matrix(matrix)
    _check_matrix(m)
    return math.sqrt(sum(m[i][j] ** 2 for i in range(len(m)) for j in range(len(m[0]))))


# ---------------------------------------------------------------------------
# Linear systems
# ---------------------------------------------------------------------------

def mat_solve(a: _Matrix, b: Any) -> _Vector:
    """Solve the linear system Ax = b for x using Gaussian elimination.

    *a* must be an n x n square matrix; *b* may be a flat list of n values
    or a column matrix (n x 1).

    Raises ValueError if the system has no unique solution.
    """
    a = _coerce_matrix(a)
    _check_matrix(a, "a")
    if not mat_is_square(a):
        raise ValueError("mat_solve requires a square coefficient matrix")
    n = len(a)

    # Accept b as flat vector or n x 1 matrix
    if isinstance(b[0], (list, tuple)):
        bvec = [float(b[i][0]) for i in range(len(b))]
    else:
        bvec = [float(x) for x in b]

    if len(bvec) != n:
        raise ValueError(f"b must have {n} elements, got {len(bvec)}")

    # Build augmented matrix [A | b]
    aug = [a[i][:] + [bvec[i]] for i in range(n)]

    for col in range(n):
        max_row = max(range(col, n), key=lambda r: abs(aug[r][col]))
        aug[col], aug[max_row] = aug[max_row], aug[col]
        pivot = aug[col][col]
        if abs(pivot) < 1e-12:
            raise ValueError("Matrix is singular — system has no unique solution")
        aug[col] = [x / pivot for x in aug[col]]
        for row in range(n):
            if row != col:
                factor = aug[row][col]
                aug[row] = [aug[row][k] - factor * aug[col][k] for k in range(n + 1)]

    return [aug[i][n] for i in range(n)]


def mat_least_squares(a: _Matrix, b: Any) -> _Vector:
    """Return the least-squares solution to Ax ≈ b via the normal equations.

    Solves (A^T A) x = A^T b.
    Useful when A is overdetermined (more rows than columns).
    """
    a = _coerce_matrix(a)
    _check_matrix(a, "a")

    if isinstance(b[0], (list, tuple)):
        bvec = [float(b[i][0]) for i in range(len(b))]
    else:
        bvec = [float(x) for x in b]

    at = mat_transpose(a)
    ata = mat_mul(at, a)
    atb_flat = [
        sum(at[i][k] * bvec[k] for k in range(len(bvec)))
        for i in range(len(at))
    ]
    return mat_solve(ata, atb_flat)


# ---------------------------------------------------------------------------
# Vector operations
# ---------------------------------------------------------------------------

def vec_dot(a: _Vector, b: _Vector) -> float:
    """Return the dot product of vectors *a* and *b*."""
    a = [float(x) for x in a]
    b = [float(x) for x in b]
    if len(a) != len(b):
        raise ValueError(f"Vectors must have equal length, got {len(a)} and {len(b)}")
    return sum(x * y for x, y in zip(a, b))


def vec_cross(a: _Vector, b: _Vector) -> _Vector:
    """Return the cross product of two 3-D vectors."""
    a = [float(x) for x in a]
    b = [float(x) for x in b]
    if len(a) != 3 or len(b) != 3:
        raise ValueError("vec_cross requires 3-dimensional vectors")
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def vec_norm(v: _Vector) -> float:
    """Return the Euclidean (L2) norm of vector *v*."""
    return math.sqrt(sum(float(x) ** 2 for x in v))


def vec_norm_l1(v: _Vector) -> float:
    """Return the L1 (Manhattan) norm of vector *v*."""
    return sum(abs(float(x)) for x in v)


def vec_normalize(v: _Vector) -> _Vector:
    """Return the unit vector in the direction of *v*.

    Raises ValueError if *v* is the zero vector.
    """
    n = vec_norm(v)
    if n < 1e-15:
        raise ValueError("Cannot normalise the zero vector")
    return [float(x) / n for x in v]


def vec_add(a: _Vector, b: _Vector) -> _Vector:
    """Element-wise vector addition."""
    a, b = [float(x) for x in a], [float(x) for x in b]
    if len(a) != len(b):
        raise ValueError("Vectors must have equal length")
    return [x + y for x, y in zip(a, b)]


def vec_sub(a: _Vector, b: _Vector) -> _Vector:
    """Element-wise vector subtraction."""
    a, b = [float(x) for x in a], [float(x) for x in b]
    if len(a) != len(b):
        raise ValueError("Vectors must have equal length")
    return [x - y for x, y in zip(a, b)]


def vec_scale(v: _Vector, scalar: float) -> _Vector:
    """Scale all elements of *v* by *scalar*."""
    s = float(scalar)
    return [float(x) * s for x in v]


def vec_angle(a: _Vector, b: _Vector) -> float:
    """Return the angle in radians between vectors *a* and *b* (0 to pi)."""
    na, nb = vec_norm(a), vec_norm(b)
    if na < 1e-15 or nb < 1e-15:
        raise ValueError("Cannot compute angle with the zero vector")
    cos_theta = vec_dot(a, b) / (na * nb)
    # Clamp for floating-point safety
    cos_theta = max(-1.0, min(1.0, cos_theta))
    return math.acos(cos_theta)


def vec_project(a: _Vector, b: _Vector) -> _Vector:
    """Return the projection of vector *a* onto vector *b*.

    Result = (dot(a, b) / dot(b, b)) * b
    """
    a = [float(x) for x in a]
    b = [float(x) for x in b]
    bb = vec_dot(b, b)
    if abs(bb) < 1e-15:
        raise ValueError("Cannot project onto the zero vector")
    scale = vec_dot(a, b) / bb
    return [x * scale for x in b]


def vec_outer(a: _Vector, b: _Vector) -> _Matrix:
    """Return the outer product of *a* (m,) and *b* (n,) as an m x n matrix."""
    a = [float(x) for x in a]
    b = [float(x) for x in b]
    return [[ai * bj for bj in b] for ai in a]


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_linalg_functions(runtime: Any) -> None:
    """Register all linear algebra functions with the NexusLang runtime."""
    # Matrix creation
    runtime.register_function("mat_zeros",       mat_zeros)
    runtime.register_function("mat_ones",        mat_ones)
    runtime.register_function("mat_identity",    mat_identity)
    runtime.register_function("mat_diagonal",    mat_diagonal)
    runtime.register_function("mat_from_list",   mat_from_list)
    runtime.register_function("mat_random",      mat_random)

    # Matrix info
    runtime.register_function("mat_shape",       mat_shape)
    runtime.register_function("mat_rows",        mat_rows)
    runtime.register_function("mat_cols",        mat_cols)
    runtime.register_function("mat_is_square",   mat_is_square)

    # Element-wise
    runtime.register_function("mat_add",         mat_add)
    runtime.register_function("mat_sub",         mat_sub)
    runtime.register_function("mat_scale",       mat_scale)
    runtime.register_function("mat_negate",      mat_negate)
    runtime.register_function("mat_element_mul", mat_element_mul)

    # Structural
    runtime.register_function("mat_transpose",   mat_transpose)
    runtime.register_function("mat_mul",         mat_mul)
    runtime.register_function("mat_pow",         mat_pow)

    # Properties / decomposition
    runtime.register_function("mat_trace",           mat_trace)
    runtime.register_function("mat_determinant",     mat_determinant)
    runtime.register_function("mat_inverse",         mat_inverse)
    runtime.register_function("mat_rank",            mat_rank)
    runtime.register_function("mat_is_symmetric",    mat_is_symmetric)
    runtime.register_function("mat_frobenius_norm",  mat_frobenius_norm)

    # Linear systems
    runtime.register_function("mat_solve",           mat_solve)
    runtime.register_function("mat_least_squares",   mat_least_squares)

    # Vector operations
    runtime.register_function("vec_dot",         vec_dot)
    runtime.register_function("vec_cross",       vec_cross)
    runtime.register_function("vec_norm",        vec_norm)
    runtime.register_function("vec_norm_l1",     vec_norm_l1)
    runtime.register_function("vec_normalize",   vec_normalize)
    runtime.register_function("vec_add",         vec_add)
    runtime.register_function("vec_sub",         vec_sub)
    runtime.register_function("vec_scale",       vec_scale)
    runtime.register_function("vec_angle",       vec_angle)
    runtime.register_function("vec_project",     vec_project)
    runtime.register_function("vec_outer",       vec_outer)


__all__ = [
    "mat_zeros", "mat_ones", "mat_identity", "mat_diagonal",
    "mat_from_list", "mat_random",
    "mat_shape", "mat_rows", "mat_cols", "mat_is_square",
    "mat_add", "mat_sub", "mat_scale", "mat_negate", "mat_element_mul",
    "mat_transpose", "mat_mul", "mat_pow",
    "mat_trace", "mat_determinant", "mat_inverse", "mat_rank",
    "mat_is_symmetric", "mat_frobenius_norm",
    "mat_solve", "mat_least_squares",
    "vec_dot", "vec_cross", "vec_norm", "vec_norm_l1", "vec_normalize",
    "vec_add", "vec_sub", "vec_scale", "vec_angle", "vec_project", "vec_outer",
    "register_linalg_functions",
]
