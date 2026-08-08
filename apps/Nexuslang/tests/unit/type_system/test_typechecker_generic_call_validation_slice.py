"""Focused coverage for generic instantiation and function-call validation paths."""

from types import SimpleNamespace

from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    DictionaryType,
    FLOAT_TYPE,
    FunctionType,
    INTEGER_TYPE,
    ListType,
    SetType,
    STRING_TYPE,
    TupleType,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _generic_node(generic_name, type_args=None, initial_value=None):
    return SimpleNamespace(
        generic_name=generic_name,
        type_args=type_args or [],
        initial_value=initial_value,
    )


def _call(name, arguments=None, named_arguments=None):
    return SimpleNamespace(
        name=name,
        arguments=arguments or [],
        named_arguments={} if named_arguments is None else named_arguments,
    )


class TestGenericInstantiationEdges:
    def test_list_infers_single_element_type_from_non_list_initial_value(self):
        checker = _checker()
        checker.check_statement = lambda expr, env: FLOAT_TYPE

        result = checker.check_generic_type_instantiation(
            _generic_node("list", initial_value=object()),
            _env(),
        )

        assert isinstance(result, ListType)
        assert result.element_type == FLOAT_TYPE

    def test_dict_inference_falls_back_when_initial_value_not_dict(self):
        checker = _checker()
        checker.check_statement = lambda expr, env: STRING_TYPE

        result = checker.check_generic_type_instantiation(
            _generic_node("dict", initial_value=object()),
            _env(),
        )

        assert isinstance(result, DictionaryType)
        assert result.key_type == ANY_TYPE
        assert result.value_type == ANY_TYPE

    def test_set_inference_returns_existing_set_type(self):
        checker = _checker()
        inferred = SetType(INTEGER_TYPE)
        checker.check_statement = lambda expr, env: inferred

        result = checker.check_generic_type_instantiation(
            _generic_node("set", initial_value=object()),
            _env(),
        )

        assert result is inferred

    def test_set_inference_wraps_non_set_initial_value(self):
        checker = _checker()
        checker.check_statement = lambda expr, env: STRING_TYPE

        result = checker.check_generic_type_instantiation(
            _generic_node("set", initial_value=object()),
            _env(),
        )

        assert isinstance(result, SetType)
        assert result.element_type == STRING_TYPE

    def test_tuple_inference_returns_existing_tuple_type(self):
        checker = _checker()
        inferred = TupleType([INTEGER_TYPE, STRING_TYPE])
        checker.check_statement = lambda expr, env: inferred

        result = checker.check_generic_type_instantiation(
            _generic_node("tuple", initial_value=object()),
            _env(),
        )

        assert result is inferred

    def test_tuple_inference_falls_back_to_any_tuple_when_not_tuple(self):
        checker = _checker()
        checker.check_statement = lambda expr, env: ListType(INTEGER_TYPE)

        result = checker.check_generic_type_instantiation(
            _generic_node("tuple", initial_value=object()),
            _env(),
        )

        assert isinstance(result, TupleType)
        assert result.element_types == [ANY_TYPE]

    def test_list_without_type_args_or_initial_value_defaults_to_any(self):
        checker = _checker()

        result = checker.check_generic_type_instantiation(
            _generic_node("list"),
            _env(),
        )

        assert isinstance(result, ListType)
        assert result.element_type == ANY_TYPE

    def test_set_without_type_args_or_initial_value_defaults_to_any(self):
        checker = _checker()

        result = checker.check_generic_type_instantiation(
            _generic_node("set"),
            _env(),
        )

        assert isinstance(result, SetType)
        assert result.element_type == ANY_TYPE

    def test_tuple_without_type_args_or_initial_value_defaults_to_any(self):
        checker = _checker()

        result = checker.check_generic_type_instantiation(
            _generic_node("tuple"),
            _env(),
        )

        assert isinstance(result, TupleType)
        assert result.element_types == [ANY_TYPE]


class TestFunctionCallValidationEdges:
    def test_non_variadic_exact_arity_error_message(self):
        checker = _checker()
        env = _env()

        fn_type = FunctionType([INTEGER_TYPE, STRING_TYPE], BOOLEAN_TYPE)
        env.define_function("exact", fn_type)

        result = checker.check_function_call(_call("exact", [object()]), env)

        assert result == BOOLEAN_TYPE
        assert any("expects 2 arguments, got 1" in err for err in checker.errors)

    def test_non_variadic_default_range_error_message(self):
        checker = _checker()
        env = _env()

        fn_type = FunctionType([INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE], BOOLEAN_TYPE)
        fn_type.has_defaults = True
        fn_type.min_params = 1
        env.define_function("ranged", fn_type)

        result = checker.check_function_call(_call("ranged", []), env)

        assert result == BOOLEAN_TYPE
        assert any("expects 1-3 arguments, got 0" in err for err in checker.errors)

    def test_variadic_min_required_error_message(self):
        checker = _checker()
        env = _env()

        fn_type = FunctionType([INTEGER_TYPE, STRING_TYPE], STRING_TYPE)
        fn_type.variadic = True
        fn_type.min_params = 2
        fn_type.variadic_index = 1
        env.define_function("varfn", fn_type)

        result = checker.check_function_call(_call("varfn", [object()]), env)

        assert result == STRING_TYPE
        assert any("expects at least 2 arguments, got 1" in err for err in checker.errors)

    def test_named_arguments_are_checked_via_check_statement(self):
        checker = _checker()
        env = _env()

        fn_type = FunctionType([INTEGER_TYPE, STRING_TYPE], BOOLEAN_TYPE)
        fn_type.has_defaults = True
        fn_type.min_params = 0
        env.define_function("named_ok", fn_type)

        seen = []

        def _record_check(expr, _env):
            seen.append(expr)
            return ANY_TYPE

        checker.check_statement = _record_check
        checker.type_inference.infer_argument_types_from_function = (
            lambda function_type, arguments, variables: []
        )

        call = _call(
            "named_ok",
            arguments=[],
            named_arguments={"first": object(), "second": object()},
        )

        result = checker.check_function_call(call, env)

        assert result == BOOLEAN_TYPE
        assert len(seen) == 2

    def test_variadic_index_only_checks_non_variadic_prefix_arguments(self):
        checker = _checker()
        env = _env()

        fn_type = FunctionType([INTEGER_TYPE, STRING_TYPE], BOOLEAN_TYPE)
        fn_type.variadic = True
        fn_type.min_params = 1
        fn_type.variadic_index = 1
        env.define_function("var_slice", fn_type)

        checker.type_inference.infer_argument_types_from_function = (
            lambda function_type, arguments, variables: [STRING_TYPE, INTEGER_TYPE]
        )

        call = _call("var_slice", arguments=[object(), object()])
        result = checker.check_function_call(call, env)

        assert result == BOOLEAN_TYPE
        assert any("argument 1 expects type 'integer'" in err for err in checker.errors)
        assert all("argument 2" not in err for err in checker.errors)
