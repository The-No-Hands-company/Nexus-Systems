"""Focused coverage for typechecker.check_generic_type_instantiation."""

from types import SimpleNamespace

from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    DictionaryType,
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


def _node(generic_name, type_args=None, initial_value=None):
    return SimpleNamespace(
        generic_name=generic_name,
        type_args=type_args or [],
        initial_value=initial_value,
    )


class TestGenericInstantiationSlice:
    def test_list_with_explicit_element_type(self):
        checker = _checker()

        result = checker.check_generic_type_instantiation(_node("list", ["Integer"]), _env())

        assert isinstance(result, ListType)
        assert result.element_type == INTEGER_TYPE

    def test_list_infers_from_initial_value(self):
        checker = _checker()
        inferred = ListType(STRING_TYPE)
        checker.check_statement = lambda expr, env: inferred

        result = checker.check_generic_type_instantiation(_node("list", initial_value=object()), _env())

        assert result is inferred

    def test_dictionary_with_explicit_key_value_types(self):
        checker = _checker()

        result = checker.check_generic_type_instantiation(_node("dictionary", ["String", "Integer"]), _env())

        assert isinstance(result, DictionaryType)
        assert result.key_type == STRING_TYPE
        assert result.value_type == INTEGER_TYPE

    def test_dictionary_infers_from_initial_value(self):
        checker = _checker()
        inferred = DictionaryType(STRING_TYPE, INTEGER_TYPE)
        checker.check_statement = lambda expr, env: inferred

        result = checker.check_generic_type_instantiation(_node("dict", initial_value=object()), _env())

        assert result is inferred

    def test_set_and_tuple_instantiation_paths(self):
        checker = _checker()

        set_result = checker.check_generic_type_instantiation(_node("set", ["Integer"]), _env())
        tuple_result = checker.check_generic_type_instantiation(_node("tuple", ["Integer", "String"]), _env())

        assert isinstance(set_result, SetType)
        assert set_result.element_type == INTEGER_TYPE
        assert isinstance(tuple_result, TupleType)
        assert tuple_result.element_types[0] == INTEGER_TYPE
        assert tuple_result.element_types[1] == STRING_TYPE

    def test_queue_and_stack_are_list_aliases(self):
        checker = _checker()

        queue_result = checker.check_generic_type_instantiation(_node("queue", ["Integer"]), _env())
        stack_result = checker.check_generic_type_instantiation(_node("stack"), _env())

        assert isinstance(queue_result, ListType)
        assert queue_result.element_type == INTEGER_TYPE
        assert isinstance(stack_result, ListType)
        assert stack_result.element_type == ANY_TYPE

    def test_unknown_generic_type_appends_error_and_returns_any(self):
        checker = _checker()

        result = checker.check_generic_type_instantiation(_node("mystery"), _env())

        assert result == ANY_TYPE
        assert any("Unknown generic type: mystery" in error for error in checker.errors)