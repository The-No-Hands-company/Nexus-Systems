"""Focused coverage for binary/unary operator error branches, loop error paths,
and FFI ABI type validation branches (opaque extern, struct-by-value, function type)."""

from types import SimpleNamespace

from nexuslang.parser.ast import (
    BinaryOperation,
    ForLoop,
    Identifier,
    Literal,
    UnaryOperation,
)
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    INTEGER_TYPE,
    ListType,
    NULL_TYPE,
    STRING_TYPE,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _int_lit(value: int = 1) -> Literal:
    return Literal("integer", value)


def _str_lit(value: str = "x") -> Literal:
    return Literal("string", value)


def _bool_lit(value: bool = True) -> Literal:
    return Literal("boolean", value)


# ---------------------------------------------------------------------------
# BinaryOperation: comparison with non-numeric operands (lines 2386-2394)
# ---------------------------------------------------------------------------

class TestBinaryOperationComparisonErrors:
    def test_less_than_non_numeric_left_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_str_lit("a"), "less than", _int_lit(1), line_number=5)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert any("must be numbers" in e for e in checker.errors)

    def test_greater_than_non_numeric_right_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(1), "greater than", _str_lit("b"), line_number=7)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert any("must be numbers" in e for e in checker.errors)

    def test_less_than_or_equal_both_non_numeric_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_str_lit("x"), "less than or equal to", _str_lit("y"), line_number=9)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert any("must be numbers" in e for e in checker.errors)

    def test_is_greater_than_valid_integers_no_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(3), "is greater than", _int_lit(1), line_number=11)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert not checker.errors


# ---------------------------------------------------------------------------
# BinaryOperation: logical operators with non-boolean operands (lines 2399-2409)
# ---------------------------------------------------------------------------

class TestBinaryOperationLogicalErrors:
    def test_and_non_boolean_left_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(1), "and", _bool_lit(True), line_number=3)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert any("Left operand of 'and' must be a boolean" in e for e in checker.errors)

    def test_or_non_boolean_right_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_bool_lit(True), "or", _int_lit(0), line_number=4)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert any("Right operand of 'or' must be a boolean" in e for e in checker.errors)

    def test_and_both_boolean_no_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_bool_lit(True), "and", _bool_lit(False), line_number=5)
        result = checker.check_binary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert not checker.errors


# ---------------------------------------------------------------------------
# BinaryOperation: bitwise operators (lines 2412-2428)
# ---------------------------------------------------------------------------

class TestBinaryOperationBitwiseErrors:
    def test_bitwise_and_valid_integers_returns_integer(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(5), "bitwise and", _int_lit(3), line_number=10)
        result = checker.check_binary_operation(op, env)
        assert result == INTEGER_TYPE
        assert not checker.errors

    def test_bitwise_or_symbol_valid(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(5), "|", _int_lit(3), line_number=11)
        result = checker.check_binary_operation(op, env)
        assert result == INTEGER_TYPE
        assert not checker.errors

    def test_shift_left_valid(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(2), "<<", _int_lit(3), line_number=12)
        result = checker.check_binary_operation(op, env)
        assert result == INTEGER_TYPE
        assert not checker.errors

    def test_bitwise_xor_non_integer_left_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_str_lit("x"), "bitwise xor", _int_lit(1), line_number=13)
        result = checker.check_binary_operation(op, env)
        assert result == INTEGER_TYPE
        assert any("Left operand of 'bitwise xor' must be an integer" in e for e in checker.errors)

    def test_shift_right_non_integer_right_appends_error(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(8), ">>", _str_lit("y"), line_number=14)
        result = checker.check_binary_operation(op, env)
        assert result == INTEGER_TYPE
        assert any("Right operand of '>>' must be an integer" in e for e in checker.errors)

    def test_bitwise_and_symbol_non_integer_both_appends_two_errors(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_str_lit("a"), "&", _str_lit("b"), line_number=15)
        result = checker.check_binary_operation(op, env)
        assert result == INTEGER_TYPE
        left_err = any("Left operand of '&' must be an integer" in e for e in checker.errors)
        right_err = any("Right operand of '&' must be an integer" in e for e in checker.errors)
        assert left_err and right_err

    def test_unsupported_binary_operator_appends_error_and_returns_any(self):
        checker = _checker()
        env = _env()
        op = BinaryOperation(_int_lit(1), "frobulate", _int_lit(2), line_number=20)
        result = checker.check_binary_operation(op, env)
        from nexuslang.typesystem.types import AnyType
        assert isinstance(result, AnyType)
        assert any("Unsupported binary operator" in e for e in checker.errors)


# ---------------------------------------------------------------------------
# UnaryOperation: error branches (lines 2435-2470)
# ---------------------------------------------------------------------------

class TestUnaryOperationErrors:
    def test_minus_non_numeric_appends_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("-", _str_lit("x"), line_number=1)
        result = checker.check_unary_operation(op, env)
        assert any("must be a number" in e for e in checker.errors)

    def test_minus_integer_no_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("-", _int_lit(5), line_number=1)
        result = checker.check_unary_operation(op, env)
        assert result == INTEGER_TYPE
        assert not checker.errors

    def test_not_non_boolean_appends_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("not", _int_lit(1), line_number=2)
        result = checker.check_unary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert any("must be a boolean" in e for e in checker.errors)

    def test_not_boolean_no_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("not", _bool_lit(True), line_number=2)
        result = checker.check_unary_operation(op, env)
        assert result == BOOLEAN_TYPE
        assert not checker.errors

    def test_bitwise_not_tilde_non_integer_appends_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("~", _str_lit("x"), line_number=3)
        result = checker.check_unary_operation(op, env)
        assert result == INTEGER_TYPE
        assert any("must be an integer" in e for e in checker.errors)

    def test_bitwise_not_word_non_integer_appends_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("bitwise not", _bool_lit(True), line_number=4)
        result = checker.check_unary_operation(op, env)
        assert result == INTEGER_TYPE
        assert any("must be an integer" in e for e in checker.errors)

    def test_bitwise_not_integer_no_error(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("~", _int_lit(7), line_number=5)
        result = checker.check_unary_operation(op, env)
        assert result == INTEGER_TYPE
        assert not checker.errors

    def test_unsupported_unary_operator_appends_error_and_returns_any(self):
        checker = _checker()
        env = _env()
        op = UnaryOperation("splort", _int_lit(1), line_number=6)
        from nexuslang.typesystem.types import AnyType
        result = checker.check_unary_operation(op, env)
        assert isinstance(result, AnyType)
        assert any("Unsupported unary operator" in e for e in checker.errors)


# ---------------------------------------------------------------------------
# check_for_loop: non-list iterable appends error (lines 1383-1387)
# ---------------------------------------------------------------------------

class TestForLoopNonListIterable:
    def test_non_list_iterable_appends_error_and_uses_any_type(self):
        checker = _checker()
        env = _env()
        loop = ForLoop(
            iterator="item",
            iterable=_int_lit(42),
            body=[],
            line_number=10,
        )
        result = checker.check_for_loop(loop, env)
        assert result == NULL_TYPE
        assert any("For loop iterable must be a list" in e for e in checker.errors)

    def test_list_iterable_no_error(self):
        checker = _checker()
        env = _env()
        env.define_variable("nums", ListType(INTEGER_TYPE))
        ident = Identifier("nums")
        loop = ForLoop(
            iterator="n",
            iterable=ident,
            body=[],
            line_number=11,
        )
        checker.check_for_loop(loop, env)
        assert not checker.errors


# ---------------------------------------------------------------------------
# _validate_ffi_abi_type: opaque extern, struct-by-value, function type (lines 861-876)
# ---------------------------------------------------------------------------

class TestValidateFfiAbiTypeAdvancedBranches:
    def test_opaque_extern_type_without_pointer_appends_error(self):
        checker = _checker()
        opaque_decl = SimpleNamespace(is_opaque=True)
        checker.ffi_extern_types["MyOpaque"] = opaque_decl
        checker._validate_ffi_abi_type("MyOpaque", 10, "param 1")
        assert any("has unknown layout" in e for e in checker.errors)
        assert any("pass it as a pointer" in e for e in checker.errors)

    def test_opaque_extern_type_with_pointer_annotation_no_error(self):
        checker = _checker()
        opaque_decl = SimpleNamespace(is_opaque=True)
        checker.ffi_extern_types["MyOpaque"] = opaque_decl
        checker._validate_ffi_abi_type("MyOpaque*", 11, "param 1")
        assert not checker.errors

    def test_non_opaque_extern_type_no_error(self):
        checker = _checker()
        concrete_decl = SimpleNamespace(is_opaque=False)
        checker.ffi_extern_types["Concrete"] = concrete_decl
        checker._validate_ffi_abi_type("Concrete", 12, "param 1")
        assert not checker.errors

    def test_struct_by_value_without_pointer_appends_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("MyStruct", 20, "param 2")
        assert any("requires explicit ABI layout" in e for e in checker.errors)

    def test_struct_pointer_annotation_no_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("MyStruct*", 21, "param 2")
        assert not checker.errors

    def test_function_type_without_pointer_appends_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("FunctionCallback", 30, "param 3")
        assert any("must be passed as a function pointer" in e for e in checker.errors)

    def test_function_pointer_annotation_no_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("FunctionCallback*", 31, "param 3")
        assert not checker.errors

    def test_aggregate_type_list_appends_abi_unstable_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("List<Integer>", 40, "return")
        assert any("ABI-unstable" in e for e in checker.errors)

    def test_primitive_integer_no_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("integer", 50, "return")
        assert not checker.errors

    def test_void_no_error(self):
        checker = _checker()
        checker._validate_ffi_abi_type("void", 51, "return")
        assert not checker.errors
