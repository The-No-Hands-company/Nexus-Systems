"""Focused coverage for typechecker.check_type_cast and check_generic_constraints."""

from types import SimpleNamespace

from nexuslang.parser.ast import Identifier
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    ClassType,
    COMPARABLE_TRAIT,
    EQUATABLE_TRAIT,
    FLOAT_TYPE,
    FunctionType,
    INTEGER_TYPE,
    TraitType,
    Type,
    STRING_TYPE,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _type_cast_expr(expression, target_type):
    return SimpleNamespace(
        expression=expression,
        target_type=target_type,
    )


class TestTypeCastSlice:
    """Test check_type_cast handler for string type mappings."""

    def test_type_cast_string_integer_lower(self):
        """String 'integer' maps to INTEGER_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "integer"), env
        )

        assert result == INTEGER_TYPE

    def test_type_cast_string_int_lower(self):
        """String 'int' maps to INTEGER_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "int"), env
        )

        assert result == INTEGER_TYPE

    def test_type_cast_string_float(self):
        """String 'float' maps to FLOAT_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "float"), env
        )

        assert result == FLOAT_TYPE

    def test_type_cast_string_string(self):
        """String 'string' maps to STRING_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "string"), env
        )

        assert result == STRING_TYPE

    def test_type_cast_string_boolean_lower(self):
        """String 'boolean' maps to BOOLEAN_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "boolean"), env
        )

        assert result == BOOLEAN_TYPE

    def test_type_cast_string_bool_lower(self):
        """String 'bool' maps to BOOLEAN_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "bool"), env
        )

        assert result == BOOLEAN_TYPE

    def test_type_cast_string_unknown_returns_any(self):
        """Unknown string type maps to ANY_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "unknown_type"), env
        )

        assert result == ANY_TYPE

    def test_type_cast_string_mixed_case_normalized(self):
        """String type names are case-insensitive."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), "INTEGER"), env
        )

        assert result == INTEGER_TYPE

    def test_type_cast_type_object_returned_directly(self):
        """Type object passed as target_type is returned directly."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), INTEGER_TYPE), env
        )

        assert result == INTEGER_TYPE

    def test_type_cast_non_type_object_returns_any(self):
        """Non-Type, non-string target_type returns ANY_TYPE."""
        checker = _checker()
        env = _env()
        checker.check_statement = lambda expr, env: ANY_TYPE

        result = checker.check_type_cast(
            _type_cast_expr(Identifier("x"), 42), env
        )

        assert result == ANY_TYPE


class TestGenericConstraintsSlice:
    """Test check_generic_constraints handler for trait validation."""

    def test_generic_constraints_length_mismatch_more_args(self):
        """Length mismatch: more type_args than type_params appends error."""
        checker = _checker()
        type_params = ["T"]
        type_args = [INTEGER_TYPE, STRING_TYPE]  # More args
        constraints = {}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is False
        assert any("Expected 1 type arguments, got 2" in err for err in checker.errors)

    def test_generic_constraints_length_mismatch_fewer_args(self):
        """Length mismatch: fewer type_args than type_params appends error."""
        checker = _checker()
        type_params = ["T", "U"]
        type_args = [INTEGER_TYPE]  # Fewer args
        constraints = {}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is False
        assert any("Expected 2 type arguments, got 1" in err for err in checker.errors)

    def test_generic_constraints_no_constraints_satisfied(self):
        """No constraints specified for parameters means all satisfied."""
        checker = _checker()
        type_params = ["T"]
        type_args = [INTEGER_TYPE]
        constraints = {}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is True

    def test_generic_constraints_hkt_satisfied(self):
        """HKT constraint satisfied returns no error."""
        checker = _checker()
        type_params = ["T"]
        type_args = [INTEGER_TYPE]
        constraints = {"T": ["Functor"]}
        checker.hkt_registry = {"Functor": SimpleNamespace()}
        checker.check_hkt_constraint = lambda constructor, trait: True

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is True

    def test_generic_constraints_hkt_not_satisfied(self):
        """HKT constraint not satisfied appends error."""
        checker = _checker()
        type_params = ["T"]
        type_args = [INTEGER_TYPE]
        constraints = {"T": ["Functor"]}
        checker.hkt_registry = {"Functor": SimpleNamespace()}
        checker.check_hkt_constraint = lambda constructor, trait: False

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is False
        assert any("does not implement HKT trait 'Functor'" in err for err in checker.errors)

    def test_generic_constraints_comparable_trait_satisfied(self):
        """Known trait (COMPARABLE_TRAIT) satisfied returns no error."""
        checker = _checker()
        checker.hkt_registry = {}
        type_params = ["T"]
        type_args = [INTEGER_TYPE]  # INTEGER_TYPE implements Comparable
        constraints = {"T": ["Comparable"]}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        # INTEGER_TYPE should implement Comparable
        assert result is True or len(checker.errors) == 0

    def test_generic_constraints_unknown_trait(self):
        """Unknown trait name appends error."""
        checker = _checker()
        checker.hkt_registry = {}
        type_params = ["T"]
        type_args = [INTEGER_TYPE]
        constraints = {"T": ["UnknownTrait"]}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is False
        assert any("Unknown trait 'UnknownTrait'" in err for err in checker.errors)

    def test_generic_constraints_all_satisfied(self):
        """All constraints satisfied returns True."""
        checker = _checker()
        checker.hkt_registry = {}
        type_params = ["T"]
        type_args = [INTEGER_TYPE]
        constraints = {}  # No constraints

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is True

    def test_generic_constraints_multiple_params_all_satisfied(self):
        """Multiple type parameters, all satisfied."""
        checker = _checker()
        checker.hkt_registry = {}
        type_params = ["T", "U"]
        type_args = [INTEGER_TYPE, STRING_TYPE]
        constraints = {}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is True

    def test_generic_constraints_known_trait_not_implemented(self):
        """Known trait with non-conforming type appends the expected error."""
        checker = _checker()
        checker.hkt_registry = {}
        type_params = ["T"]
        type_args = [ClassType("PlainObject", {}, {})]
        constraints = {"T": ["Comparable"]}

        result = checker.check_generic_constraints(
            type_params, type_args, constraints, context="test"
        )

        assert result is False
        assert any("does not implement trait 'Comparable'" in err for err in checker.errors)


class TestValidateGenericFunctionCallSlice:
    def test_validate_generic_function_call_returns_true_for_non_generic_definition(self):
        checker = _checker()
        func_def = SimpleNamespace()

        result = checker.validate_generic_function_call("plain", func_def, [INTEGER_TYPE])

        assert result is True

    def test_validate_generic_function_call_uses_empty_constraints_when_not_dict(self):
        checker = _checker()

        captured = {}

        def _capture(type_parameters, type_args, constraints, context):
            captured["constraints"] = constraints
            captured["context"] = context
            return True

        checker.check_generic_constraints = _capture
        func_def = SimpleNamespace(
            type_parameters=["T"],
            type_constraints=["not-a-dict"],
        )

        result = checker.validate_generic_function_call("wrapped", func_def, [INTEGER_TYPE])

        assert result is True
        assert captured["constraints"] == {}
        assert captured["context"] == "function 'wrapped'"

    def test_validate_generic_function_call_passes_dict_constraints_through(self):
        checker = _checker()

        captured = {}

        def _capture(type_parameters, type_args, constraints, context):
            captured["constraints"] = constraints
            return True

        checker.check_generic_constraints = _capture
        func_def = SimpleNamespace(
            type_parameters=["T"],
            type_constraints={"T": ["Comparable"]},
        )

        result = checker.validate_generic_function_call("ordered", func_def, [INTEGER_TYPE])

        assert result is True
        assert captured["constraints"] == {"T": ["Comparable"]}
