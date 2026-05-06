"""
Tests for the NexusLang linear algebra stdlib module (src/nlpl/stdlib/linalg).

Coverage
--------
- Matrix creation: mat_zeros, mat_ones, mat_identity, mat_diagonal,
                   mat_from_list, mat_random
- Matrix info:     mat_shape, mat_rows, mat_cols, mat_is_square
- Element-wise:    mat_add, mat_sub, mat_scale, mat_negate, mat_element_mul
- Structural:      mat_transpose, mat_mul, mat_pow
- Properties:      mat_trace, mat_determinant, mat_inverse, mat_rank,
                   mat_is_symmetric, mat_frobenius_norm
- Linear systems:  mat_solve, mat_least_squares
- Vectors:         vec_dot, vec_cross, vec_norm, vec_norm_l1, vec_normalize,
                   vec_add, vec_sub, vec_scale, vec_angle, vec_project,
                   vec_outer
- Registration
"""

import math
import pytest

from nexuslang.stdlib.linalg import (
    mat_zeros, mat_ones, mat_identity, mat_diagonal, mat_from_list, mat_random,
    mat_shape, mat_rows, mat_cols, mat_is_square,
    mat_add, mat_sub, mat_scale, mat_negate, mat_element_mul,
    mat_transpose, mat_mul, mat_pow,
    mat_trace, mat_determinant, mat_inverse, mat_rank,
    mat_is_symmetric, mat_frobenius_norm,
    mat_solve, mat_least_squares,
    vec_dot, vec_cross, vec_norm, vec_norm_l1, vec_normalize,
    vec_add, vec_sub, vec_scale, vec_angle, vec_project, vec_outer,
    register_linalg_functions,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def approx(a, b, tol=1e-9):
    """Element-wise approximate equality for scalars, vectors, or matrices."""
    if isinstance(a, (int, float)):
        return abs(a - b) < tol
    if isinstance(a[0], (int, float)):
        return all(abs(x - y) < tol for x, y in zip(a, b))
    return all(approx(ra, rb, tol) for ra, rb in zip(a, b))


class _FakeRuntime:
    def __init__(self):
        self.functions = {}

    def register_function(self, name, fn):
        self.functions[name] = fn


I2 = [[1.0, 0.0], [0.0, 1.0]]
I3 = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]
A23 = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]          # 2x3
M22 = [[1.0, 2.0], [3.0, 4.0]]                     # 2x2, det=-2
M33 = [[1.0, 2.0, 3.0], [0.0, 4.0, 5.0], [1.0, 0.0, 6.0]]   # det=22


# ---------------------------------------------------------------------------
# Matrix creation
# ---------------------------------------------------------------------------

class TestMatZeros:
    def test_shape(self):
        m = mat_zeros(3, 4)
        assert len(m) == 3 and len(m[0]) == 4

    def test_all_zero(self):
        m = mat_zeros(2, 3)
        assert all(x == 0.0 for row in m for x in row)

    def test_square(self):
        m = mat_zeros(5, 5)
        assert mat_is_square(m)

    def test_invalid_dims(self):
        with pytest.raises(ValueError):
            mat_zeros(0, 3)
        with pytest.raises(ValueError):
            mat_zeros(3, 0)


class TestMatOnes:
    def test_all_one(self):
        m = mat_ones(2, 2)
        assert all(x == 1.0 for row in m for x in row)

    def test_shape(self):
        m = mat_ones(4, 2)
        assert mat_shape(m) == [4, 2]


class TestMatIdentity:
    def test_2x2(self):
        assert approx(mat_identity(2), I2)

    def test_3x3(self):
        assert approx(mat_identity(3), I3)

    def test_diagonal_is_one(self):
        m = mat_identity(4)
        for i in range(4):
            assert m[i][i] == 1.0

    def test_off_diagonal_is_zero(self):
        m = mat_identity(4)
        for i in range(4):
            for j in range(4):
                if i != j:
                    assert m[i][j] == 0.0

    def test_invalid(self):
        with pytest.raises(ValueError):
            mat_identity(0)


class TestMatDiagonal:
    def test_values_on_diagonal(self):
        m = mat_diagonal([1.0, 2.0, 3.0])
        assert m[0][0] == 1.0 and m[1][1] == 2.0 and m[2][2] == 3.0

    def test_zeros_off_diagonal(self):
        m = mat_diagonal([1.0, 2.0])
        assert m[0][1] == 0.0 and m[1][0] == 0.0

    def test_shape_is_square(self):
        m = mat_diagonal([1, 2, 3, 4])
        assert mat_is_square(m) and len(m) == 4


class TestMatFromList:
    def test_valid_input(self):
        m = mat_from_list([[1, 2], [3, 4]])
        assert m[0][0] == 1.0 and m[1][1] == 4.0

    def test_coerces_to_float(self):
        m = mat_from_list([[1, 2]])
        assert isinstance(m[0][0], float)

    def test_ragged_raises(self):
        with pytest.raises(ValueError):
            mat_from_list([[1, 2], [3]])


class TestMatRandom:
    def test_shape(self):
        m = mat_random(3, 4)
        assert len(m) == 3 and len(m[0]) == 4

    def test_values_in_range(self):
        m = mat_random(5, 5)
        for row in m:
            for v in row:
                assert 0.0 <= v < 1.0

    def test_different_on_each_call(self):
        a, b = mat_random(3, 3), mat_random(3, 3)
        assert a != b  # extremely unlikely to collide


# ---------------------------------------------------------------------------
# Matrix info
# ---------------------------------------------------------------------------

class TestMatInfo:
    def test_shape_2x3(self):
        assert mat_shape(A23) == [2, 3]

    def test_rows(self):
        assert mat_rows(A23) == 2

    def test_cols(self):
        assert mat_cols(A23) == 3

    def test_is_square_true(self):
        assert mat_is_square(M22) is True

    def test_is_square_false(self):
        assert mat_is_square(A23) is False

    def test_is_square_non_square(self):
        assert mat_is_square([[1, 2, 3]]) is False


# ---------------------------------------------------------------------------
# Element-wise operations
# ---------------------------------------------------------------------------

class TestMatAdd:
    def test_basic(self):
        result = mat_add([[1, 2], [3, 4]], [[5, 6], [7, 8]])
        assert approx(result, [[6, 8], [10, 12]])

    def test_shape_mismatch(self):
        with pytest.raises(ValueError):
            mat_add([[1, 2]], [[1, 2, 3]])

    def test_add_zero_matrix(self):
        m = mat_zeros(3, 3)
        assert approx(mat_add(M22, [[0, 0], [0, 0]]), M22)


class TestMatSub:
    def test_basic(self):
        result = mat_sub([[5, 6], [7, 8]], [[1, 2], [3, 4]])
        assert approx(result, [[4, 4], [4, 4]])

    def test_self_is_zero(self):
        result = mat_sub(M22, M22)
        assert all(x == 0.0 for row in result for x in row)


class TestMatScale:
    def test_double(self):
        result = mat_scale([[1, 2], [3, 4]], 2.0)
        assert approx(result, [[2, 4], [6, 8]])

    def test_zero_scalar(self):
        result = mat_scale(M22, 0)
        assert all(x == 0.0 for row in result for x in row)


class TestMatNegate:
    def test_negate(self):
        result = mat_negate([[1, -2], [3, -4]])
        assert approx(result, [[-1, 2], [-3, 4]])


class TestMatElementMul:
    def test_hadamard(self):
        result = mat_element_mul([[1, 2], [3, 4]], [[5, 6], [7, 8]])
        assert approx(result, [[5, 12], [21, 32]])

    def test_with_zeros(self):
        result = mat_element_mul([[1, 2], [3, 4]], [[0, 0], [0, 0]])
        assert all(x == 0.0 for row in result for x in row)


# ---------------------------------------------------------------------------
# Structural operations
# ---------------------------------------------------------------------------

class TestMatTranspose:
    def test_2x3(self):
        t = mat_transpose(A23)
        assert mat_shape(t) == [3, 2]
        assert t[0][0] == 1.0 and t[2][1] == 6.0

    def test_symmetric_unchanged(self):
        sym = [[1, 2], [2, 3]]
        assert approx(mat_transpose(sym), sym)

    def test_double_transpose_is_identity(self):
        assert approx(mat_transpose(mat_transpose(A23)), A23)


class TestMatMul:
    def test_identity_preserves_matrix(self):
        assert approx(mat_mul(M22, mat_identity(2)), M22)

    def test_2x2(self):
        a = [[1, 2], [3, 4]]
        b = [[5, 6], [7, 8]]
        result = mat_mul(a, b)
        assert approx(result, [[19, 22], [43, 50]])

    def test_2x3_by_3x2(self):
        a = [[1, 2, 3], [4, 5, 6]]
        b = [[7, 8], [9, 10], [11, 12]]
        result = mat_mul(a, b)
        assert mat_shape(result) == [2, 2]
        assert approx(result, [[58, 64], [139, 154]])

    def test_incompatible_shapes(self):
        with pytest.raises(ValueError):
            mat_mul([[1, 2]], [[1, 2]])         # 1x2 * 1x2


class TestMatPow:
    def test_power_zero_is_identity(self):
        assert approx(mat_pow(M22, 0), mat_identity(2))

    def test_power_one_is_self(self):
        assert approx(mat_pow(M22, 1), M22)

    def test_power_two(self):
        expected = mat_mul(M22, M22)
        assert approx(mat_pow(M22, 2), expected)

    def test_negative_exponent_raises(self):
        with pytest.raises(ValueError):
            mat_pow(M22, -1)

    def test_non_square_raises(self):
        with pytest.raises(ValueError):
            mat_pow(A23, 2)


# ---------------------------------------------------------------------------
# Properties / decomposition
# ---------------------------------------------------------------------------

class TestMatTrace:
    def test_identity_trace(self):
        assert mat_trace(mat_identity(4)) == pytest.approx(4.0)

    def test_custom_matrix(self):
        assert mat_trace(M22) == pytest.approx(5.0)  # 1+4

    def test_non_square_raises(self):
        with pytest.raises(ValueError):
            mat_trace(A23)


class TestMatDeterminant:
    def test_1x1(self):
        assert mat_determinant([[7.0]]) == pytest.approx(7.0)

    def test_2x2(self):
        assert mat_determinant(M22) == pytest.approx(-2.0)   # 1*4 - 2*3

    def test_3x3(self):
        assert mat_determinant(M33) == pytest.approx(22.0)

    def test_identity(self):
        assert mat_determinant(mat_identity(5)) == pytest.approx(1.0)

    def test_singular(self):
        singular = [[1, 2], [2, 4]]
        assert mat_determinant(singular) == pytest.approx(0.0, abs=1e-9)

    def test_non_square_raises(self):
        with pytest.raises(ValueError):
            mat_determinant(A23)


class TestMatInverse:
    def test_2x2_inverse(self):
        inv = mat_inverse(M22)
        product = mat_mul(M22, inv)
        assert approx(product, mat_identity(2), tol=1e-9)

    def test_identity_inverse_is_identity(self):
        assert approx(mat_inverse(mat_identity(3)), mat_identity(3))

    def test_3x3_inverse(self):
        inv = mat_inverse(M33)
        product = mat_mul(M33, inv)
        assert approx(product, mat_identity(3), tol=1e-8)

    def test_singular_raises(self):
        singular = [[1, 2], [2, 4]]
        with pytest.raises(ValueError):
            mat_inverse(singular)

    def test_non_square_raises(self):
        with pytest.raises(ValueError):
            mat_inverse(A23)


class TestMatRank:
    def test_identity_full_rank(self):
        assert mat_rank(mat_identity(3)) == 3

    def test_zero_matrix_rank_zero(self):
        assert mat_rank(mat_zeros(3, 3)) == 0

    def test_rank_2x3(self):
        assert mat_rank(A23) == 2

    def test_rank_of_singular_matrix(self):
        singular = [[1, 2], [2, 4]]
        assert mat_rank(singular) == 1

    def test_full_rank_3x3(self):
        assert mat_rank(M33) == 3


class TestMatIsSymmetric:
    def test_identity_is_symmetric(self):
        assert mat_is_symmetric(mat_identity(3)) is True

    def test_symmetric_matrix(self):
        sym = [[1, 2, 3], [2, 5, 6], [3, 6, 9]]
        assert mat_is_symmetric(sym) is True

    def test_non_symmetric(self):
        assert mat_is_symmetric(M22) is False

    def test_non_square_is_not_symmetric(self):
        assert mat_is_symmetric(A23) is False


class TestMatFrobeniusNorm:
    def test_identity_2x2(self):
        assert mat_frobenius_norm(mat_identity(2)) == pytest.approx(math.sqrt(2))

    def test_all_ones_2x2(self):
        assert mat_frobenius_norm(mat_ones(2, 2)) == pytest.approx(2.0)

    def test_known_matrix(self):
        m = [[3.0, 4.0]]
        assert mat_frobenius_norm(m) == pytest.approx(5.0)


# ---------------------------------------------------------------------------
# Linear systems
# ---------------------------------------------------------------------------

class TestMatSolve:
    def test_simple_2x2(self):
        # x + y = 3, 2x - y = 0  =>  x=1, y=2
        a = [[1, 1], [2, -1]]
        b = [3, 0]
        x = mat_solve(a, b)
        assert x == pytest.approx([1.0, 2.0], abs=1e-9)

    def test_identity_solution_is_b(self):
        b = [4.0, 5.0, 6.0]
        x = mat_solve(mat_identity(3), b)
        assert x == pytest.approx(b, abs=1e-9)

    def test_3x3_system(self):
        # M33 x = [1, 2, 3]
        a = [[2.0, 1.0, -1.0], [-3.0, -1.0, 2.0], [-2.0, 1.0, 2.0]]
        b = [8, -11, -3]
        x = mat_solve(a, b)
        # Verify: A x ≈ b
        for i in range(3):
            row_sum = sum(a[i][j] * x[j] for j in range(3))
            assert row_sum == pytest.approx(b[i], abs=1e-9)

    def test_singular_system_raises(self):
        singular = [[1, 2], [2, 4]]
        with pytest.raises(ValueError):
            mat_solve(singular, [1, 2])

    def test_b_as_column_matrix(self):
        a = [[1.0, 0.0], [0.0, 2.0]]
        b = [[3.0], [8.0]]
        x = mat_solve(a, b)
        assert x == pytest.approx([3.0, 4.0], abs=1e-9)


class TestMatLeastSquares:
    def test_exact_solution_is_recovered(self):
        # When A is square and non-singular, least squares == solve
        a = [[1.0, 1.0], [2.0, -1.0]]
        b = [3.0, 0.0]
        x = mat_least_squares(a, b)
        assert x == pytest.approx([1.0, 2.0], abs=1e-8)

    def test_overdetermined_system(self):
        # 3 equations, 2 unknowns: fits y = x (slope=1, intercept=0)
        a = [[1.0, 1.0], [2.0, 1.0], [3.0, 1.0]]
        b = [1.0, 2.0, 3.0]
        x = mat_least_squares(a, b)
        # Slope should be 1, intercept 0
        assert x[0] == pytest.approx(1.0, abs=1e-8)
        assert x[1] == pytest.approx(0.0, abs=1e-8)


# ---------------------------------------------------------------------------
# Vector operations
# ---------------------------------------------------------------------------

class TestVecDot:
    def test_orthogonal(self):
        assert vec_dot([1, 0, 0], [0, 1, 0]) == pytest.approx(0.0)

    def test_parallel(self):
        assert vec_dot([1, 2, 3], [1, 2, 3]) == pytest.approx(14.0)

    def test_negative(self):
        assert vec_dot([1, 0], [-1, 0]) == pytest.approx(-1.0)

    def test_length_mismatch(self):
        with pytest.raises(ValueError):
            vec_dot([1, 2], [1, 2, 3])


class TestVecCross:
    def test_x_cross_y_is_z(self):
        result = vec_cross([1, 0, 0], [0, 1, 0])
        assert approx(result, [0, 0, 1])

    def test_anticommutative(self):
        a, b = [1, 2, 3], [4, 5, 6]
        cross_ab = vec_cross(a, b)
        cross_ba = vec_cross(b, a)
        assert approx(cross_ab, [-x for x in cross_ba])

    def test_parallel_vectors_give_zero(self):
        result = vec_cross([1, 2, 3], [2, 4, 6])
        assert approx(result, [0, 0, 0], tol=1e-9)

    def test_non_3d_raises(self):
        with pytest.raises(ValueError):
            vec_cross([1, 2], [3, 4])


class TestVecNorm:
    def test_unit_vector(self):
        assert vec_norm([1, 0, 0]) == pytest.approx(1.0)

    def test_3_4_5(self):
        assert vec_norm([3, 4]) == pytest.approx(5.0)

    def test_zero_vector(self):
        assert vec_norm([0, 0, 0]) == pytest.approx(0.0)


class TestVecNormL1:
    def test_basic(self):
        assert vec_norm_l1([1, -2, 3]) == pytest.approx(6.0)

    def test_zero(self):
        assert vec_norm_l1([0, 0]) == pytest.approx(0.0)


class TestVecNormalize:
    def test_result_is_unit(self):
        normalized = vec_normalize([3, 4])
        assert vec_norm(normalized) == pytest.approx(1.0)

    def test_direction_preserved(self):
        v = [1, 1, 1]
        n = vec_normalize(v)
        expected = 1.0 / math.sqrt(3)
        assert all(abs(x - expected) < 1e-9 for x in n)

    def test_zero_vector_raises(self):
        with pytest.raises(ValueError):
            vec_normalize([0, 0, 0])


class TestVecAdd:
    def test_basic(self):
        assert approx(vec_add([1, 2, 3], [4, 5, 6]), [5, 7, 9])

    def test_length_mismatch(self):
        with pytest.raises(ValueError):
            vec_add([1, 2], [1, 2, 3])


class TestVecSub:
    def test_basic(self):
        assert approx(vec_sub([5, 7, 9], [4, 5, 6]), [1, 2, 3])

    def test_self_is_zero(self):
        v = [1.0, 2.0, 3.0]
        assert approx(vec_sub(v, v), [0, 0, 0])


class TestVecScale:
    def test_double(self):
        assert approx(vec_scale([1, 2, 3], 2), [2, 4, 6])

    def test_zero_scale(self):
        assert approx(vec_scale([1, 2, 3], 0), [0, 0, 0])

    def test_negative(self):
        assert approx(vec_scale([1, -2, 3], -1), [-1, 2, -3])


class TestVecAngle:
    def test_same_direction_is_zero(self):
        angle = vec_angle([1, 0], [2, 0])
        assert angle == pytest.approx(0.0, abs=1e-9)

    def test_perpendicular_is_90(self):
        angle = vec_angle([1, 0], [0, 1])
        assert angle == pytest.approx(math.pi / 2, abs=1e-9)

    def test_opposite_is_180(self):
        angle = vec_angle([1, 0], [-1, 0])
        assert angle == pytest.approx(math.pi, abs=1e-9)

    def test_zero_vector_raises(self):
        with pytest.raises(ValueError):
            vec_angle([0, 0], [1, 0])


class TestVecProject:
    def test_project_onto_x_axis(self):
        result = vec_project([3, 4], [1, 0])
        assert approx(result, [3, 0])

    def test_project_onto_itself(self):
        result = vec_project([2, 3], [2, 3])
        assert approx(result, [2, 3], tol=1e-9)

    def test_zero_target_raises(self):
        with pytest.raises(ValueError):
            vec_project([1, 2], [0, 0])


class TestVecOuter:
    def test_shape(self):
        result = vec_outer([1, 2, 3], [4, 5])
        assert len(result) == 3 and len(result[0]) == 2

    def test_values(self):
        result = vec_outer([1, 2], [3, 4])
        assert approx(result, [[3, 4], [6, 8]])

    def test_rank_one_matrix(self):
        # outer product of unit vectors is a rank-1 matrix
        a = vec_outer([1, 0], [0, 1])
        assert mat_rank(a) == 1


# ---------------------------------------------------------------------------
# Registration
# ---------------------------------------------------------------------------

class TestRegistration:
    EXPECTED = {
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
    }

    def setup_method(self):
        self._rt = _FakeRuntime()
        register_linalg_functions(self._rt)

    def test_all_expected_names_registered(self):
        missing = self.EXPECTED - set(self._rt.functions.keys())
        assert not missing, f"Missing: {missing}"

    def test_all_registered_callables(self):
        for name, fn in self._rt.functions.items():
            assert callable(fn), f"{name} is not callable"

    def test_count(self):
        assert len(self._rt.functions) >= len(self.EXPECTED)
