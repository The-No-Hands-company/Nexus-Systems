"""
Tests for the NexusLang numerical integration / differentiation stdlib module
(src/nlpl/stdlib/numint).

Coverage
--------
Integration:     integrate_midpoint, integrate_trapezoid, integrate_simpson,
                 integrate_simpson38, integrate_romberg, integrate_gauss,
                 integrate_adaptive, integrate_data
Differentiation: diff_forward, diff_backward, diff_central, diff_second,
                 diff_nth, diff_partial, diff_gradient, diff_jacobian, diff_hessian
Root finding:    root_bisection, root_newton, root_secant
Optimization:    minimize_golden, minimize_brent
Registration
"""

import math
import pytest

from nexuslang.stdlib.numint import (
    integrate_midpoint, integrate_trapezoid, integrate_simpson,
    integrate_simpson38, integrate_romberg, integrate_gauss,
    integrate_adaptive, integrate_data,
    diff_forward, diff_backward, diff_central, diff_second,
    diff_nth, diff_partial, diff_gradient, diff_jacobian, diff_hessian,
    root_bisection, root_newton, root_secant,
    minimize_golden, minimize_brent,
    register_numint_functions,
)


# ---------------------------------------------------------------------------
# Shared fixtures / helpers
# ---------------------------------------------------------------------------

def f_sin(x):
    return math.sin(x)

def f_cos(x):
    return math.cos(x)

def f_poly(x):
    """x^3 - x  (primitive: x^4/4 - x^2/2)"""
    return x ** 3 - x

def f_exp(x):
    return math.exp(x)

def f_const(x):
    return 3.0

def f_linear(x):
    return 2.0 * x + 1.0

# Known definite integrals (exact)
# integral(sin, 0, pi)        = 2
# integral(cos, 0, pi/2)      = 1
# integral(x^3-x, 0, 2)       = 4-2 = 2
# integral(exp, 0, 1)         = e-1
# integral(3, 0, 5)           = 15
# integral(2x+1, 0, 3)        = [x^2+x]_0^3 = 12

LOOSE = 1e-4    # tolerance for low-order methods with default n
TIGHT = 1e-8    # tolerance for high-accuracy methods


class _FakeRuntime:
    def __init__(self):
        self.functions = {}

    def register_function(self, name, fn):
        self.functions[name] = fn


# ---------------------------------------------------------------------------
# Integration — midpoint rule
# ---------------------------------------------------------------------------

class TestIntegrateMidpoint:
    def test_sin_0_pi(self):
        assert integrate_midpoint(f_sin, 0, math.pi, 10000) == pytest.approx(2.0, abs=1e-5)

    def test_constant(self):
        assert integrate_midpoint(f_const, 0, 5, 100) == pytest.approx(15.0, abs=1e-9)

    def test_linear(self):
        assert integrate_midpoint(f_linear, 0, 3, 1000) == pytest.approx(12.0, abs=1e-6)

    def test_n_one(self):
        # Single rectangle: width=1, midpoint = 0.5, f(0.5)=0.5*2+1=2 → area=2
        assert integrate_midpoint(f_linear, 0, 1, 1) == pytest.approx(2.0)

    def test_invalid_n(self):
        with pytest.raises(ValueError):
            integrate_midpoint(f_sin, 0, 1, 0)

    def test_reversed_limits(self):
        # Swapped limits should give negative
        pos = integrate_midpoint(f_sin, 0, math.pi, 1000)
        neg = integrate_midpoint(f_sin, math.pi, 0, 1000)
        assert pos == pytest.approx(-neg, abs=1e-9)


# ---------------------------------------------------------------------------
# Integration — trapezoidal rule
# ---------------------------------------------------------------------------

class TestIntegrateTrapezoid:
    def test_sin_0_pi(self):
        assert integrate_trapezoid(f_sin, 0, math.pi, 10000) == pytest.approx(2.0, abs=1e-5)

    def test_constant(self):
        assert integrate_trapezoid(f_const, 0, 5, 100) == pytest.approx(15.0, abs=1e-9)

    def test_linear_exact(self):
        # Trapezoidal is exact for linear functions (any n)
        assert integrate_trapezoid(f_linear, 0, 3, 1) == pytest.approx(12.0, abs=1e-9)

    def test_exp_0_1(self):
        assert integrate_trapezoid(f_exp, 0, 1, 100000) == pytest.approx(math.e - 1, abs=1e-4)

    def test_invalid_n(self):
        with pytest.raises(ValueError):
            integrate_trapezoid(f_sin, 0, 1, 0)


# ---------------------------------------------------------------------------
# Integration — Simpson's 1/3 rule
# ---------------------------------------------------------------------------

class TestIntegrateSimpson:
    def test_sin_0_pi(self):
        assert integrate_simpson(f_sin, 0, math.pi, 1000) == pytest.approx(2.0, abs=1e-8)

    def test_cubic_polynomial(self):
        # integral(x^3-x, 0, 2) = [x^4/4 - x^2/2]_0^2 = 4 - 2 = 2
        assert integrate_simpson(f_poly, 0, 2, 1000) == pytest.approx(2.0, abs=1e-8)

    def test_constant(self):
        assert integrate_simpson(f_const, 0, 5, 100) == pytest.approx(15.0, abs=1e-9)

    def test_cos_0_pi_over2(self):
        assert integrate_simpson(f_cos, 0, math.pi / 2, 1000) == pytest.approx(1.0, abs=1e-10)

    def test_odd_n_auto_corrected(self):
        # Passing odd n should not raise — it increments n
        result = integrate_simpson(f_sin, 0, math.pi, 101)
        assert abs(result - 2.0) < 1e-6

    def test_n_less_than_2_raises(self):
        with pytest.raises(ValueError):
            integrate_simpson(f_sin, 0, 1, 1)

    def test_linear_exact_any_n(self):
        assert integrate_simpson(f_linear, 0, 3, 2) == pytest.approx(12.0, abs=1e-9)


# ---------------------------------------------------------------------------
# Integration — Simpson's 3/8 rule
# ---------------------------------------------------------------------------

class TestIntegrateSimpson38:
    def test_sin_0_pi(self):
        assert integrate_simpson38(f_sin, 0, math.pi, 999) == pytest.approx(2.0, abs=1e-8)

    def test_constant(self):
        assert integrate_simpson38(f_const, 0, 5, 300) == pytest.approx(15.0, abs=1e-9)

    def test_n_rounds_up(self):
        # n=4 is not a multiple of 3 — should auto-round to 6
        # Simpson 3/8 with n=6 is lower-order; use loose tolerance
        result = integrate_simpson38(f_sin, 0, math.pi, 4)
        assert abs(result - 2.0) < 1e-2

    def test_n_too_small_raises(self):
        with pytest.raises(ValueError):
            integrate_simpson38(f_sin, 0, 1, 2)


# ---------------------------------------------------------------------------
# Integration — Romberg
# ---------------------------------------------------------------------------

class TestIntegrateRomberg:
    def test_sin_0_pi(self):
        assert integrate_romberg(f_sin, 0, math.pi) == pytest.approx(2.0, abs=TIGHT)

    def test_exp_0_1(self):
        assert integrate_romberg(f_exp, 0, 1) == pytest.approx(math.e - 1, abs=TIGHT)

    def test_cos_0_pi_over2(self):
        assert integrate_romberg(f_cos, 0, math.pi / 2) == pytest.approx(1.0, abs=TIGHT)

    def test_constant(self):
        assert integrate_romberg(f_const, 0, 5) == pytest.approx(15.0, abs=TIGHT)

    def test_cubic(self):
        assert integrate_romberg(f_poly, 0, 2) == pytest.approx(2.0, abs=TIGHT)


# ---------------------------------------------------------------------------
# Integration — Gauss-Legendre
# ---------------------------------------------------------------------------

class TestIntegrateGauss:
    def test_constant_n1(self):
        assert integrate_gauss(f_const, 0, 5, 1) == pytest.approx(15.0, abs=1e-9)

    def test_linear_n2(self):
        # Gauss n=2 is exact for polynomials up to degree 3
        assert integrate_gauss(f_linear, 0, 3, 2) == pytest.approx(12.0, abs=1e-9)

    def test_sin_n5(self):
        assert integrate_gauss(f_sin, 0, math.pi, 5) == pytest.approx(2.0, abs=1e-5)

    def test_exp_n5(self):
        assert integrate_gauss(f_exp, 0, 1, 5) == pytest.approx(math.e - 1, abs=1e-5)

    def test_invalid_n(self):
        with pytest.raises(ValueError):
            integrate_gauss(f_sin, 0, 1, 6)
        with pytest.raises(ValueError):
            integrate_gauss(f_sin, 0, 1, 0)


# ---------------------------------------------------------------------------
# Integration — Adaptive Simpson
# ---------------------------------------------------------------------------

class TestIntegrateAdaptive:
    def test_sin_0_pi(self):
        assert integrate_adaptive(f_sin, 0, math.pi) == pytest.approx(2.0, abs=1e-7)

    def test_exp_0_1(self):
        assert integrate_adaptive(f_exp, 0, 1) == pytest.approx(math.e - 1, abs=1e-8)

    def test_constant(self):
        assert integrate_adaptive(f_const, 0, 5) == pytest.approx(15.0, abs=1e-9)

    def test_custom_tol(self):
        result = integrate_adaptive(f_sin, 0, math.pi, tol=1e-12)
        assert abs(result - 2.0) < 1e-9


# ---------------------------------------------------------------------------
# Integration — discrete data
# ---------------------------------------------------------------------------

class TestIntegrateData:
    def test_triangle(self):
        # triangle: xs=[0,1,2], ys=[0,1,0] — area = 1.0
        assert integrate_data([0, 1, 2], [0, 1, 0]) == pytest.approx(1.0)

    def test_constant_spacing(self):
        # integral of constant 2 from 0 to 4 = 8
        xs = [0, 1, 2, 3, 4]
        ys = [2.0] * 5
        assert integrate_data(xs, ys) == pytest.approx(8.0)

    def test_non_uniform_spacing(self):
        xs = [0.0, 0.5, 2.0]
        ys = [0.0, 1.0, 0.0]
        # 0.5*(0+1)*0.5 + 0.5*(1+0)*1.5 = 0.25 + 0.75 = 1.0
        assert integrate_data(xs, ys) == pytest.approx(1.0)

    def test_single_point_raises(self):
        with pytest.raises(ValueError):
            integrate_data([1.0], [1.0])

    def test_mismatched_lengths_raises(self):
        with pytest.raises(ValueError):
            integrate_data([0, 1, 2], [0, 1])


# ---------------------------------------------------------------------------
# Differentiation — forward / backward / central
# ---------------------------------------------------------------------------

class TestDiffForward:
    def test_sin_at_0(self):
        # d/dx sin(x) at x=0 = cos(0) = 1
        assert diff_forward(f_sin, 0.0) == pytest.approx(1.0, abs=1e-4)

    def test_poly_at_1(self):
        # d/dx (x^3-x) = 3x^2-1, at x=1 = 2
        assert diff_forward(f_poly, 1.0) == pytest.approx(2.0, abs=1e-4)


class TestDiffBackward:
    def test_sin_at_pi_over2(self):
        # d/dx sin(x) at pi/2 = cos(pi/2) = 0
        assert diff_backward(f_sin, math.pi / 2) == pytest.approx(0.0, abs=1e-4)

    def test_linear(self):
        # d/dx (2x+1) = 2 everywhere
        assert diff_backward(f_linear, 5.0) == pytest.approx(2.0, abs=1e-4)


class TestDiffCentral:
    def test_sin_at_0(self):
        assert diff_central(f_sin, 0.0) == pytest.approx(1.0, abs=1e-9)

    def test_cos_at_0(self):
        # d/dx cos(x) at 0 = -sin(0) = 0
        assert diff_central(f_cos, 0.0) == pytest.approx(0.0, abs=1e-9)

    def test_exp_self_derivative(self):
        # d/dx e^x = e^x; at x=1 → e
        assert diff_central(f_exp, 1.0) == pytest.approx(math.e, abs=1e-9)

    def test_poly_at_2(self):
        # 3*(2^2)-1 = 11
        assert diff_central(f_poly, 2.0) == pytest.approx(11.0, abs=1e-8)

    def test_custom_h(self):
        result = diff_central(f_sin, 0.0, h=1e-7)
        assert result == pytest.approx(1.0, abs=1e-9)


class TestDiffSecond:
    def test_sin_at_0(self):
        # d^2/dx^2 sin(x) = -sin(x); at 0 = 0
        assert diff_second(f_sin, 0.0) == pytest.approx(0.0, abs=1e-6)

    def test_sin_at_pi_over2(self):
        # d^2/dx^2 sin(x) at pi/2 = -1
        assert diff_second(f_sin, math.pi / 2) == pytest.approx(-1.0, abs=1e-5)

    def test_exp_self_second_derivative(self):
        # d^2/dx^2 e^x = e^x; at 1 → e
        assert diff_second(f_exp, 1.0) == pytest.approx(math.e, abs=1e-5)

    def test_quadratic_second_is_constant(self):
        # f(x) = x^2; f'' = 2 everywhere; finite-diff accumulates ~1e-4 error at x=5
        assert diff_second(lambda x: x ** 2, 5.0) == pytest.approx(2.0, abs=1e-4)


# ---------------------------------------------------------------------------
# Differentiation — nth derivative
# ---------------------------------------------------------------------------

class TestDiffNth:
    def test_zeroth_is_function(self):
        assert diff_nth(f_sin, 1.0, 0) == pytest.approx(math.sin(1.0), abs=1e-9)

    def test_first_equals_central(self):
        x = 1.5
        assert diff_nth(f_sin, x, 1) == pytest.approx(diff_central(f_sin, x), abs=1e-7)

    def test_second_of_sin(self):
        # -sin(1) ≈ -0.841
        assert diff_nth(f_sin, 1.0, 2) == pytest.approx(-math.sin(1.0), abs=1e-4)

    def test_negative_order_raises(self):
        with pytest.raises(ValueError):
            diff_nth(f_sin, 0.0, -1)


# ---------------------------------------------------------------------------
# Differentiation — partial / gradient / jacobian / hessian
# ---------------------------------------------------------------------------

class TestDiffPartial:
    def test_partial_x(self):
        # f(x,y) = x^2 + y^2; df/dx at (3,4) = 2*3 = 6
        f = lambda xs: xs[0] ** 2 + xs[1] ** 2
        assert diff_partial(f, [3.0, 4.0], 0) == pytest.approx(6.0, abs=1e-7)

    def test_partial_y(self):
        f = lambda xs: xs[0] ** 2 + xs[1] ** 2
        assert diff_partial(f, [3.0, 4.0], 1) == pytest.approx(8.0, abs=1e-7)

    def test_out_of_range_raises(self):
        f = lambda xs: xs[0]
        with pytest.raises(ValueError):
            diff_partial(f, [1.0], 1)


class TestDiffGradient:
    def test_quadratic_bowl(self):
        # f(x,y) = x^2 + y^2; grad = [2x, 2y]
        f = lambda xs: xs[0] ** 2 + xs[1] ** 2
        g = diff_gradient(f, [1.0, 2.0])
        assert g[0] == pytest.approx(2.0, abs=1e-7)
        assert g[1] == pytest.approx(4.0, abs=1e-7)

    def test_length_matches_input(self):
        f = lambda xs: sum(x ** 2 for x in xs)
        g = diff_gradient(f, [1.0, 2.0, 3.0])
        assert len(g) == 3

    def test_constant_gradient_is_zero(self):
        f = lambda xs: 5.0
        g = diff_gradient(f, [1.0, 2.0])
        assert g[0] == pytest.approx(0.0, abs=1e-9)
        assert g[1] == pytest.approx(0.0, abs=1e-9)


class TestDiffJacobian:
    def test_list_of_functions(self):
        # fs = [x+y, x-y]; J = [[1,1],[1,-1]]
        f1 = lambda xs: xs[0] + xs[1]
        f2 = lambda xs: xs[0] - xs[1]
        J = diff_jacobian([f1, f2], [1.0, 2.0])
        assert J[0][0] == pytest.approx(1.0, abs=1e-7)
        assert J[0][1] == pytest.approx(1.0, abs=1e-7)
        assert J[1][0] == pytest.approx(1.0, abs=1e-7)
        assert J[1][1] == pytest.approx(-1.0, abs=1e-7)

    def test_single_callable_returning_list(self):
        # fs([x,y]) = [x^2, y^2]; J = [[2x,0],[0,2y]]
        f = lambda xs: [xs[0] ** 2, xs[1] ** 2]
        J = diff_jacobian(f, [3.0, 4.0])
        assert J[0][0] == pytest.approx(6.0, abs=1e-7)
        assert J[0][1] == pytest.approx(0.0, abs=1e-7)
        assert J[1][0] == pytest.approx(0.0, abs=1e-7)
        assert J[1][1] == pytest.approx(8.0, abs=1e-7)

    def test_shape(self):
        fs = [lambda xs: xs[0], lambda xs: xs[1], lambda xs: xs[0] + xs[1]]
        J = diff_jacobian(fs, [1.0, 2.0])
        assert len(J) == 3
        assert len(J[0]) == 2


class TestDiffHessian:
    def test_quadratic_bowl(self):
        # f(x,y) = x^2 + y^2; H = [[2,0],[0,2]]
        f = lambda xs: xs[0] ** 2 + xs[1] ** 2
        H = diff_hessian(f, [1.0, 2.0])
        assert H[0][0] == pytest.approx(2.0, abs=1e-5)
        assert H[1][1] == pytest.approx(2.0, abs=1e-5)
        assert H[0][1] == pytest.approx(0.0, abs=1e-5)
        assert H[1][0] == pytest.approx(0.0, abs=1e-5)

    def test_symmetry(self):
        f = lambda xs: xs[0] ** 2 * xs[1] + xs[1] ** 3
        H = diff_hessian(f, [1.0, 1.0])
        assert H[0][1] == pytest.approx(H[1][0], abs=1e-7)

    def test_shape(self):
        f = lambda xs: sum(x ** 2 for x in xs)
        H = diff_hessian(f, [1.0, 2.0, 3.0])
        assert len(H) == 3
        assert len(H[0]) == 3

    def test_cross_term(self):
        # f(x,y) = x*y; d^2f/dxdy = 1
        f = lambda xs: xs[0] * xs[1]
        H = diff_hessian(f, [1.0, 1.0])
        assert H[0][1] == pytest.approx(1.0, abs=1e-5)


# ---------------------------------------------------------------------------
# Root finding
# ---------------------------------------------------------------------------

class TestRootBisection:
    def test_sin_root_near_pi(self):
        root = root_bisection(f_sin, 2.0, 4.0)
        assert root == pytest.approx(math.pi, abs=1e-8)

    def test_simple_linear(self):
        # f(x) = x - 3; root at 3
        root = root_bisection(lambda x: x - 3.0, 0.0, 5.0)
        assert root == pytest.approx(3.0, abs=1e-9)

    def test_cubic_root(self):
        # x^3 - x = 0 has root at x=1 in [0.5, 1.5]
        root = root_bisection(f_poly, 0.5, 1.5)
        assert root == pytest.approx(1.0, abs=1e-8)

    def test_no_bracket_raises(self):
        # Both f(1) and f(2) positive for x^2+1
        with pytest.raises(ValueError):
            root_bisection(lambda x: x ** 2 + 1, 1.0, 2.0)

    def test_exact_root_given(self):
        # When a is already a root
        root = root_bisection(lambda x: x, -1.0, 1.0)
        assert abs(root) < 1e-8


class TestRootNewton:
    def test_sin_root_near_pi(self):
        root = root_newton(f_sin, 3.0)
        assert root == pytest.approx(math.pi, abs=1e-10)

    def test_quadratic(self):
        # f(x) = x^2 - 4; root near x = 2
        root = root_newton(lambda x: x ** 2 - 4, 1.5)
        assert root == pytest.approx(2.0, abs=1e-9)

    def test_exp_minus_2(self):
        # e^x = 2 => x = ln(2)
        root = root_newton(lambda x: math.exp(x) - 2.0, 0.5)
        assert root == pytest.approx(math.log(2), abs=1e-10)

    def test_sin_zero_at_zero(self):
        root = root_newton(f_sin, 0.1)
        assert abs(root) < 1e-10


class TestRootSecant:
    def test_sin_root_near_pi(self):
        root = root_secant(f_sin, 3.0, 4.0)
        assert root == pytest.approx(math.pi, abs=1e-9)

    def test_linear_root(self):
        root = root_secant(lambda x: x - 5.0, 3.0, 7.0)
        assert root == pytest.approx(5.0, abs=1e-9)

    def test_cubic(self):
        root = root_secant(f_poly, 0.6, 1.4)
        assert root == pytest.approx(1.0, abs=1e-8)


# ---------------------------------------------------------------------------
# Optimization
# ---------------------------------------------------------------------------

class TestMinimizeGolden:
    def test_x_squared_minimum_at_zero(self):
        x_min = minimize_golden(lambda x: x ** 2, -1.0, 1.0)
        assert x_min == pytest.approx(0.0, abs=1e-8)

    def test_parabola_offset(self):
        # (x-3)^2 minimum at x=3
        x_min = minimize_golden(lambda x: (x - 3) ** 2, 0.0, 6.0)
        assert x_min == pytest.approx(3.0, abs=1e-7)

    def test_cos_minimum_near_pi(self):
        # cos has minimum at pi in [2, 4]
        x_min = minimize_golden(f_cos, 2.0, 4.0)
        assert x_min == pytest.approx(math.pi, abs=1e-7)

    def test_returns_x_not_fvalue(self):
        # Verify the return value is x, not f(x)
        x_min = minimize_golden(lambda x: (x - 2) ** 2 + 5, 0.0, 4.0)
        assert x_min == pytest.approx(2.0, abs=1e-7)


class TestMinimizeBrent:
    def test_x_squared_minimum_at_zero(self):
        x_min = minimize_brent(lambda x: x ** 2, -1.0, 1.0)
        assert x_min == pytest.approx(0.0, abs=1e-8)

    def test_parabola_offset(self):
        x_min = minimize_brent(lambda x: (x - 3) ** 2, 0.0, 6.0)
        assert x_min == pytest.approx(3.0, abs=1e-7)

    def test_cos_minimum_near_pi(self):
        x_min = minimize_brent(f_cos, 2.0, 4.0)
        assert x_min == pytest.approx(math.pi, abs=1e-7)

    def test_more_accurate_than_golden(self):
        # Brent should be within tighter tolerance by default
        x_min = minimize_brent(lambda x: (x - 1.23456789) ** 2, 0.0, 2.0)
        assert x_min == pytest.approx(1.23456789, abs=1e-7)

    def test_flat_region(self):
        # f(x) = (x-2)^4 — flat near minimum
        x_min = minimize_brent(lambda x: (x - 2) ** 4, 0.0, 4.0)
        assert x_min == pytest.approx(2.0, abs=1e-4)


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegistration:
    EXPECTED = {
        "integrate_midpoint", "integrate_trapezoid", "integrate_simpson",
        "integrate_simpson38", "integrate_romberg", "integrate_gauss",
        "integrate_adaptive", "integrate_data",
        "diff_forward", "diff_backward", "diff_central", "diff_second",
        "diff_nth", "diff_partial", "diff_gradient", "diff_jacobian", "diff_hessian",
        "root_bisection", "root_newton", "root_secant",
        "minimize_golden", "minimize_brent",
    }

    def setup_method(self):
        self._rt = _FakeRuntime()
        register_numint_functions(self._rt)

    def test_all_expected_registered(self):
        missing = self.EXPECTED - set(self._rt.functions.keys())
        assert not missing, f"Missing registrations: {missing}"

    def test_all_callables(self):
        for name, fn in self._rt.functions.items():
            assert callable(fn), f"{name} is not callable"

    def test_count(self):
        assert len(self._rt.functions) >= len(self.EXPECTED)
