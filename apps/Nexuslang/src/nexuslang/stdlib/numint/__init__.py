"""
NLPL Numerical Integration and Differentiation Module.

Provides numerical methods for calculus — integration, differentiation,
root finding, and single-variable optimization.  Everything is pure Python
with no external dependencies.

Registered NexusLang Functions
--------------------------
Integration
    integrate_midpoint(f, a, b, n)     -> float  Midpoint rectangle rule
    integrate_trapezoid(f, a, b, n)    -> float  Trapezoidal rule
    integrate_simpson(f, a, b, n)      -> float  Simpson's 1/3 rule (n even)
    integrate_simpson38(f, a, b, n)    -> float  Simpson's 3/8 rule (n mult of 3)
    integrate_romberg(f, a, b, tol)    -> float  Romberg / Richardson extrapolation
    integrate_gauss(f, a, b, n)        -> float  Gauss-Legendre quadrature (n = 1..5)
    integrate_adaptive(f, a, b, tol)   -> float  Adaptive Simpson
    integrate_data(xs, ys)             -> float  Trapezoid from discrete data

Differentiation
    diff_forward(f, x, h)              -> float  Forward-difference derivative
    diff_backward(f, x, h)            -> float  Backward-difference derivative
    diff_central(f, x, h)              -> float  Central-difference derivative (O(h^2))
    diff_second(f, x, h)              -> float  Second derivative (central, O(h^2))
    diff_nth(f, x, n, h)              -> float  n-th derivative via repeated central diff
    diff_partial(f, xs, i, h)         -> float  Partial derivative w.r.t. variable i
    diff_gradient(f, xs, h)           -> List[float]  Gradient of f at xs
    diff_jacobian(fs, xs, h)          -> List[List[float]]  Jacobian matrix
    diff_hessian(f, xs, h)            -> List[List[float]]  Hessian matrix

Root finding
    root_bisection(f, a, b, tol)      -> float  Bisection method
    root_newton(f, x0, tol)           -> float  Newton-Raphson
    root_secant(f, x0, x1, tol)       -> float  Secant method

Optimization
    minimize_golden(f, a, b, tol)     -> float  Golden-section search (minimum)
    minimize_brent(f, a, b, tol)      -> float  Brent's method (minimum)
"""

import math
from typing import Any, Callable, List

# ---------------------------------------------------------------------------
# Type aliases
# ---------------------------------------------------------------------------

_Func1 = Callable[[float], float]           # scalar -> scalar
_FuncN = Callable[[List[float]], float]     # vector -> scalar


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------

def _call(f: Any, x: float) -> float:
    """Call f(x), coercing the result to float."""
    result = f(x)
    if isinstance(result, (int, float)):
        return float(result)
    raise TypeError(f"Function must return a numeric value, got {type(result).__name__}")


def _call_vec(f: Any, xs: List[float]) -> float:
    """Call f(xs) where xs is a list, coercing result to float."""
    result = f(xs)
    if isinstance(result, (int, float)):
        return float(result)
    raise TypeError(f"Function must return a numeric value, got {type(result).__name__}")


# Precomputed Gauss-Legendre nodes (xi) and weights (wi) on [-1, 1] for n=1..5
_GL_NODES_WEIGHTS = {
    1: ([0.0], [2.0]),
    2: ([-0.5773502691896257, 0.5773502691896257],
        [1.0, 1.0]),
    3: ([-0.7745966692414834, 0.0, 0.7745966692414834],
        [0.5555555555555556, 0.8888888888888888, 0.5555555555555556]),
    4: ([-0.8611363115940526, -0.3399810435848563,
          0.3399810435848563,  0.8611363115940526],
        [0.3478548451374538, 0.6521451548625461,
         0.6521451548625461, 0.3478548451374538]),
    5: ([-0.9061798459386640, -0.5384693101056831, 0.0,
          0.5384693101056831,  0.9061798459386640],
        [0.2369268850561891, 0.4786286704993665, 0.5688888888888889,
         0.4786286704993665, 0.2369268850561891]),
}


# ---------------------------------------------------------------------------
# Integration
# ---------------------------------------------------------------------------

def integrate_midpoint(f: Any, a: float, b: float, n: int = 1000) -> float:
    """Approximate the definite integral of f over [a, b] using the midpoint rule.

    Divides [a, b] into n equal sub-intervals and samples f at each midpoint.
    O(h^2) accuracy.
    """
    a, b = float(a), float(b)
    n = int(n)
    if n < 1:
        raise ValueError("n must be >= 1")
    h = (b - a) / n
    total = sum(_call(f, a + (i + 0.5) * h) for i in range(n))
    return total * h


def integrate_trapezoid(f: Any, a: float, b: float, n: int = 1000) -> float:
    """Approximate the definite integral of f over [a, b] using the trapezoidal rule.

    O(h^2) accuracy.
    """
    a, b = float(a), float(b)
    n = int(n)
    if n < 1:
        raise ValueError("n must be >= 1")
    h = (b - a) / n
    total = _call(f, a) + _call(f, b)
    total += 2.0 * sum(_call(f, a + i * h) for i in range(1, n))
    return total * h / 2.0


def integrate_simpson(f: Any, a: float, b: float, n: int = 1000) -> float:
    """Approximate the definite integral of f over [a, b] using Simpson's 1/3 rule.

    n must be even; if odd, it is incremented by 1.  O(h^4) accuracy.
    """
    a, b = float(a), float(b)
    n = int(n)
    if n < 2:
        raise ValueError("n must be >= 2")
    if n % 2 != 0:
        n += 1
    h = (b - a) / n
    xs = [a + i * h for i in range(n + 1)]
    total = _call(f, xs[0]) + _call(f, xs[n])
    for i in range(1, n):
        coeff = 4.0 if i % 2 != 0 else 2.0
        total += coeff * _call(f, xs[i])
    return total * h / 3.0


def integrate_simpson38(f: Any, a: float, b: float, n: int = 999) -> float:
    """Approximate the definite integral of f over [a, b] using Simpson's 3/8 rule.

    n must be a multiple of 3; rounds up to the nearest multiple of 3.
    O(h^4) accuracy.
    """
    a, b = float(a), float(b)
    n = int(n)
    if n < 3:
        raise ValueError("n must be >= 3")
    remainder = n % 3
    if remainder != 0:
        n += 3 - remainder
    h = (b - a) / n
    xs = [a + i * h for i in range(n + 1)]
    total = _call(f, xs[0]) + _call(f, xs[n])
    for i in range(1, n):
        coeff = 2.0 if i % 3 == 0 else 3.0
        total += coeff * _call(f, xs[i])
    return total * 3.0 * h / 8.0


def integrate_romberg(f: Any, a: float, b: float,
                      tol: float = 1e-10, max_iter: int = 20) -> float:
    """Approximate the definite integral of f over [a, b] using Romberg integration.

    Combines the trapezoidal rule with Richardson extrapolation for O(h^(2k))
    accuracy.  Iterates until the result changes by less than *tol*.
    """
    a, b = float(a), float(b)
    tol = float(tol)
    R = [[0.0] * (max_iter + 1) for _ in range(max_iter + 1)]
    h = b - a
    R[0][0] = 0.5 * h * (_call(f, a) + _call(f, b))
    for i in range(1, max_iter + 1):
        h /= 2
        # Trapezoid contribution from new midpoints
        n_new = 2 ** (i - 1)
        s = sum(_call(f, a + (2 * k - 1) * h) for k in range(1, n_new + 1))
        R[i][0] = 0.5 * R[i - 1][0] + h * s
        # Richardson extrapolation
        for j in range(1, i + 1):
            factor = 4.0 ** j
            R[i][j] = (factor * R[i][j - 1] - R[i - 1][j - 1]) / (factor - 1.0)
        if i > 1 and abs(R[i][i] - R[i - 1][i - 1]) < tol:
            return R[i][i]
    return R[max_iter][max_iter]


def integrate_gauss(f: Any, a: float, b: float, n: int = 5) -> float:
    """Approximate the definite integral of f over [a, b] using Gauss-Legendre quadrature.

    *n* specifies the number of quadrature points (1 to 5).  Higher n is more
    accurate for smooth functions; n=5 integrates polynomials of degree <= 9 exactly.
    """
    a, b = float(a), float(b)
    n = int(n)
    if n not in _GL_NODES_WEIGHTS:
        raise ValueError(f"integrate_gauss: n must be 1..5, got {n}")
    nodes, weights = _GL_NODES_WEIGHTS[n]
    mid = (a + b) / 2.0
    half = (b - a) / 2.0
    total = sum(w * _call(f, mid + half * xi) for xi, w in zip(nodes, weights))
    return half * total


def _adaptive_simpson(f: Any, a: float, b: float, tol: float, depth: int) -> float:
    """Recursive adaptive Simpson helper."""
    c = (a + b) / 2.0
    fa, fc, fb = _call(f, a), _call(f, c), _call(f, b)
    h = b - a
    whole = h / 6.0 * (fa + 4.0 * fc + fb)
    d = (a + c) / 2.0
    e = (c + b) / 2.0
    fd, fe = _call(f, d), _call(f, e)
    left  = h / 12.0 * (fa + 4.0 * fd + fc)
    right = h / 12.0 * (fc + 4.0 * fe + fb)
    delta = left + right - whole
    if depth <= 0 or abs(delta) < 15.0 * tol:
        return left + right + delta / 15.0
    return (_adaptive_simpson(f, a, c, tol / 2.0, depth - 1) +
            _adaptive_simpson(f, c, b, tol / 2.0, depth - 1))


def integrate_adaptive(f: Any, a: float, b: float, tol: float = 1e-8) -> float:
    """Approximate the definite integral of f over [a, b] using adaptive Simpson's rule.

    Subdivides the interval recursively where error is large, achieving *tol*
    approximate absolute accuracy.
    """
    return _adaptive_simpson(f, float(a), float(b), float(tol), depth=50)


def integrate_data(xs: List[float], ys: List[float]) -> float:
    """Approximate the integral under a discrete curve using the trapezoidal rule.

    *xs* and *ys* are lists of equal length; xs need not be uniformly spaced.
    """
    xs = [float(x) for x in xs]
    ys = [float(y) for y in ys]
    if len(xs) != len(ys):
        raise ValueError("integrate_data: xs and ys must have the same length")
    if len(xs) < 2:
        raise ValueError("integrate_data: need at least 2 points")
    return sum(
        0.5 * (ys[i] + ys[i + 1]) * abs(xs[i + 1] - xs[i])
        for i in range(len(xs) - 1)
    )


# ---------------------------------------------------------------------------
# Differentiation
# ---------------------------------------------------------------------------

_DEFAULT_H = 1e-5


def diff_forward(f: Any, x: float, h: float = _DEFAULT_H) -> float:
    """First derivative using the forward-difference formula: (f(x+h) - f(x)) / h.

    O(h) accuracy.
    """
    x, h = float(x), float(h)
    return (_call(f, x + h) - _call(f, x)) / h


def diff_backward(f: Any, x: float, h: float = _DEFAULT_H) -> float:
    """First derivative using the backward-difference formula: (f(x) - f(x-h)) / h.

    O(h) accuracy.
    """
    x, h = float(x), float(h)
    return (_call(f, x) - _call(f, x - h)) / h


def diff_central(f: Any, x: float, h: float = _DEFAULT_H) -> float:
    """First derivative using the central-difference formula: (f(x+h) - f(x-h)) / 2h.

    O(h^2) accuracy.
    """
    x, h = float(x), float(h)
    return (_call(f, x + h) - _call(f, x - h)) / (2.0 * h)


def diff_second(f: Any, x: float, h: float = _DEFAULT_H) -> float:
    """Second derivative using the central-difference formula:
    (f(x+h) - 2*f(x) + f(x-h)) / h^2.  O(h^2) accuracy.
    """
    x, h = float(x), float(h)
    return (_call(f, x + h) - 2.0 * _call(f, x) + _call(f, x - h)) / (h * h)


def diff_nth(f: Any, x: float, n: int, h: float = _DEFAULT_H) -> float:
    """Approximate the n-th derivative of f at x using finite differences.

    Uses the central-difference scheme applied n times via binomial coefficients.
    Accurate for small n; for large n floating-point cancellation increases.
    """
    x, h = float(x), float(h)
    n = int(n)
    if n < 0:
        raise ValueError("diff_nth: order n must be >= 0")
    if n == 0:
        return _call(f, x)
    # Generalised central difference: sum_{k=0}^{n} (-1)^(n-k) * C(n,k) * f(x + (k - n/2)*h)
    total = 0.0
    for k in range(n + 1):
        binom = math.comb(n, k)
        sign = (-1) ** (n - k)
        total += sign * binom * _call(f, x + (k - n / 2.0) * h)
    return total / (h ** n)


def diff_partial(f: Any, xs: List[float], i: int, h: float = _DEFAULT_H) -> float:
    """Partial derivative of f with respect to variable i at point xs.

    Uses the central-difference formula.
    """
    xs = [float(v) for v in xs]
    i = int(i)
    h = float(h)
    if i < 0 or i >= len(xs):
        raise ValueError(f"diff_partial: index {i} out of range for xs of length {len(xs)}")
    xs_p = xs[:]
    xs_m = xs[:]
    xs_p[i] += h
    xs_m[i] -= h
    return (_call_vec(f, xs_p) - _call_vec(f, xs_m)) / (2.0 * h)


def diff_gradient(f: Any, xs: List[float], h: float = _DEFAULT_H) -> List[float]:
    """Gradient of scalar function f at point xs (list of partial derivatives)."""
    xs = [float(v) for v in xs]
    return [diff_partial(f, xs, i, h) for i in range(len(xs))]


def diff_jacobian(fs: Any, xs: List[float], h: float = _DEFAULT_H) -> List[List[float]]:
    """Jacobian matrix of a vector-valued function fs at point xs.

    *fs* must be a list of scalar functions (or a single function returning a list).
    Each row i of the Jacobian is the gradient of the i-th component of fs.
    """
    xs = [float(v) for v in xs]
    h = float(h)
    # If fs is a single function returning a list, call it to discover dimensions
    if callable(fs) and not isinstance(fs, list):
        sample = fs(xs)
        if not isinstance(sample, (list, tuple)):
            raise TypeError("diff_jacobian: fs must return a list/tuple for vector output")
        m = len(sample)
        rows = []
        for i in range(m):
            row = []
            for j in range(len(xs)):
                xp = xs[:]
                xm = xs[:]
                xp[j] += h
                xm[j] -= h
                fp = fs(xp)
                fm = fs(xm)
                row.append((float(fp[i]) - float(fm[i])) / (2.0 * h))
            rows.append(row)
        return rows
    # fs is a list of scalar functions
    if not isinstance(fs, (list, tuple)):
        raise TypeError("diff_jacobian: fs must be a callable or list of callables")
    return [diff_gradient(fi, xs, h) for fi in fs]


def diff_hessian(f: Any, xs: List[float], h: float = _DEFAULT_H) -> List[List[float]]:
    """Hessian matrix of scalar function f at point xs.

    H[i][j] = d^2f / (dxi dxj) via mixed central differences.
    """
    xs = [float(v) for v in xs]
    h = float(h)
    n = len(xs)
    H = [[0.0] * n for _ in range(n)]
    for i in range(n):
        for j in range(n):
            if i == j:
                # Pure second derivative
                xs_p = xs[:]
                xs_m = xs[:]
                xs_p[i] += h
                xs_m[i] -= h
                H[i][j] = (
                    _call_vec(f, xs_p) - 2.0 * _call_vec(f, xs) + _call_vec(f, xs_m)
                ) / (h * h)
            elif j > i:
                # Mixed: (f(x+hi+hj) - f(x+hi-hj) - f(x-hi+hj) + f(x-hi-hj)) / 4h^2
                xpp = xs[:]
                xpm = xs[:]
                xmp = xs[:]
                xmm = xs[:]
                xpp[i] += h; xpp[j] += h
                xpm[i] += h; xpm[j] -= h
                xmp[i] -= h; xmp[j] += h
                xmm[i] -= h; xmm[j] -= h
                val = (
                    _call_vec(f, xpp) - _call_vec(f, xpm) -
                    _call_vec(f, xmp) + _call_vec(f, xmm)
                ) / (4.0 * h * h)
                H[i][j] = val
                H[j][i] = val   # Hessian is symmetric
    return H


# ---------------------------------------------------------------------------
# Root finding
# ---------------------------------------------------------------------------

def root_bisection(f: Any, a: float, b: float,
                   tol: float = 1e-10, max_iter: int = 1000) -> float:
    """Find a root of f in [a, b] using the bisection method.

    Requires f(a) and f(b) to have opposite signs.
    Raises ValueError if the bracket condition is not met.
    """
    a, b, tol = float(a), float(b), float(tol)
    fa, fb = _call(f, a), _call(f, b)
    if fa * fb > 0:
        raise ValueError(
            f"root_bisection: f(a)={fa} and f(b)={fb} must have opposite signs"
        )
    for _ in range(int(max_iter)):
        c = (a + b) / 2.0
        fc = _call(f, c)
        if abs(fc) < tol or (b - a) / 2.0 < tol:
            return c
        if fa * fc < 0:
            b = c
            fb = fc
        else:
            a = c
            fa = fc
    return (a + b) / 2.0


def root_newton(f: Any, x0: float, tol: float = 1e-10,
                max_iter: int = 100, h: float = 1e-7) -> float:
    """Find a root of f near x0 using the Newton-Raphson method.

    Approximates f' using the central-difference formula with step *h*.
    Raises ValueError if convergence is not achieved.
    """
    x = float(x0)
    tol = float(tol)
    for _ in range(int(max_iter)):
        fx = _call(f, x)
        if abs(fx) < tol:
            return x
        fpx = diff_central(f, x, h)
        if abs(fpx) < 1e-14:
            raise ValueError(
                f"root_newton: derivative near zero at x={x}, cannot continue"
            )
        x_new = x - fx / fpx
        if abs(x_new - x) < tol:
            return x_new
        x = x_new
    raise ValueError(
        f"root_newton: did not converge within {max_iter} iterations (last x={x})"
    )


def root_secant(f: Any, x0: float, x1: float,
                tol: float = 1e-10, max_iter: int = 100) -> float:
    """Find a root of f using the secant method starting from x0 and x1.

    Raises ValueError if convergence is not achieved.
    """
    x0, x1, tol = float(x0), float(x1), float(tol)
    f0 = _call(f, x0)
    for _ in range(int(max_iter)):
        f1 = _call(f, x1)
        if abs(f1) < tol:
            return x1
        denom = f1 - f0
        if abs(denom) < 1e-14:
            raise ValueError(
                f"root_secant: nearly identical function values at x0={x0}, x1={x1}"
            )
        x2 = x1 - f1 * (x1 - x0) / denom
        if abs(x2 - x1) < tol:
            return x2
        x0, f0 = x1, f1
        x1 = x2
    raise ValueError(
        f"root_secant: did not converge within {max_iter} iterations (last x={x1})"
    )


# ---------------------------------------------------------------------------
# Optimization
# ---------------------------------------------------------------------------

_PHI = (math.sqrt(5.0) - 1.0) / 2.0          # golden ratio conjugate ≈ 0.618


def minimize_golden(f: Any, a: float, b: float, tol: float = 1e-10) -> float:
    """Find the minimum of f on [a, b] using golden-section search.

    Requires f to be unimodal on [a, b].
    Returns the x-value of the minimum.
    """
    a, b, tol = float(a), float(b), float(tol)
    c = b - _PHI * (b - a)
    d = a + _PHI * (b - a)
    fc, fd = _call(f, c), _call(f, d)
    while abs(b - a) > tol:
        if fc < fd:
            b = d
            d, fd = c, fc
            c = b - _PHI * (b - a)
            fc = _call(f, c)
        else:
            a = c
            c, fc = d, fd
            d = a + _PHI * (b - a)
            fd = _call(f, d)
    return (a + b) / 2.0


def minimize_brent(f: Any, a: float, b: float, tol: float = 1e-8) -> float:
    """Find the minimum of f on [a, b] using Brent's method.

    Combines golden-section search with parabolic interpolation.
    Requires f to be unimodal on [a, b].
    Returns the x-value of the minimum.
    """
    a, b, tol = float(a), float(b), float(tol)
    x = w = v = a + _PHI * (b - a)
    fx = fw = fv = _call(f, x)
    d = 0.0
    e = 0.0

    for _ in range(500):
        m = 0.5 * (a + b)
        tol1 = tol * abs(x) + 1e-10
        tol2 = 2.0 * tol1
        if abs(x - m) <= tol2 - 0.5 * (b - a):
            return x
        # Attempt parabolic interpolation
        if abs(e) > tol1:
            r = (x - w) * (fx - fv)
            q_val = (x - v) * (fx - fw)
            p = (x - v) * q_val - (x - w) * r
            q_val = 2.0 * (q_val - r)
            if q_val > 0:
                p = -p
            else:
                q_val = -q_val
            r = e
            e = d
            if abs(p) < abs(0.5 * q_val * r) and p > q_val * (a - x) and p < q_val * (b - x):
                d = p / q_val
                u = x + d
                if (u - a) < tol2 or (b - u) < tol2:
                    d = tol1 if x < m else -tol1
            else:
                e = (b if x < m else a) - x
                d = _PHI * e
        else:
            e = (b if x < m else a) - x
            d = _PHI * e

        u = x + (d if abs(d) >= tol1 else (tol1 if d > 0 else -tol1))
        fu = _call(f, u)

        if fu <= fx:
            if u < x:
                b = x
            else:
                a = x
            v, fv = w, fw
            w, fw = x, fx
            x, fx = u, fu
        else:
            if u < x:
                a = u
            else:
                b = u
            if fu <= fw or w == x:
                v, fv = w, fw
                w, fw = u, fu
            elif fu <= fv or v == x or v == w:
                v, fv = u, fu

    return x


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

def register_numint_functions(runtime: Any) -> None:
    """Register all numerical integration / differentiation functions with the runtime."""
    # Integration
    runtime.register_function("integrate_midpoint",  integrate_midpoint)
    runtime.register_function("integrate_trapezoid", integrate_trapezoid)
    runtime.register_function("integrate_simpson",   integrate_simpson)
    runtime.register_function("integrate_simpson38", integrate_simpson38)
    runtime.register_function("integrate_romberg",   integrate_romberg)
    runtime.register_function("integrate_gauss",     integrate_gauss)
    runtime.register_function("integrate_adaptive",  integrate_adaptive)
    runtime.register_function("integrate_data",      integrate_data)
    # Differentiation
    runtime.register_function("diff_forward",   diff_forward)
    runtime.register_function("diff_backward",  diff_backward)
    runtime.register_function("diff_central",   diff_central)
    runtime.register_function("diff_second",    diff_second)
    runtime.register_function("diff_nth",       diff_nth)
    runtime.register_function("diff_partial",   diff_partial)
    runtime.register_function("diff_gradient",  diff_gradient)
    runtime.register_function("diff_jacobian",  diff_jacobian)
    runtime.register_function("diff_hessian",   diff_hessian)
    # Root finding
    runtime.register_function("root_bisection", root_bisection)
    runtime.register_function("root_newton",    root_newton)
    runtime.register_function("root_secant",    root_secant)
    # Optimization
    runtime.register_function("minimize_golden", minimize_golden)
    runtime.register_function("minimize_brent",  minimize_brent)
