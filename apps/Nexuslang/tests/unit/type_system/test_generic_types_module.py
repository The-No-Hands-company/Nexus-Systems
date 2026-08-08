"""Focused regression tests for the legacy generic_types module."""

import pytest

from nexuslang.typesystem.generic_types import (
    GENERIC_TYPE_REGISTRY,
    GenericTypeConstraint,
    GenericTypeContext,
    GenericTypeRegistry,
)
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    INTEGER_TYPE,
    STRING_TYPE,
    ClassType,
    DictionaryType,
    FunctionType,
    GenericParameter,
    ListType,
    TraitType,
    UnionType,
    COMPARABLE_TRAIT,
)


class TestGenericTypeRegistry:
    def test_register_and_create_type_instance(self):
        registry = GenericTypeRegistry()
        registry.register_generic_type("Box", ["T"], ListType(GenericParameter("T")))

        instance = registry.create_type_instance("Box", [INTEGER_TYPE])

        assert isinstance(instance, ListType)
        assert instance.element_type is INTEGER_TYPE

    def test_create_type_instance_uses_cache(self):
        registry = GenericTypeRegistry()
        registry.register_generic_type("Box", ["T"], ListType(GenericParameter("T")))

        first = registry.create_type_instance("Box", [STRING_TYPE])
        second = registry.create_type_instance("Box", [STRING_TYPE])

        assert first is second

    def test_create_type_instance_rejects_unknown_generic(self):
        registry = GenericTypeRegistry()

        with pytest.raises(TypeError, match="Generic type 'Missing' not found"):
            registry.create_type_instance("Missing", [INTEGER_TYPE])

    def test_create_type_instance_rejects_wrong_arity(self):
        registry = GenericTypeRegistry()
        registry.register_generic_type("Pair", ["K", "V"], DictionaryType(GenericParameter("K"), GenericParameter("V")))

        with pytest.raises(
            TypeError,
            match="Generic type 'Pair' expects 2 type arguments, got 1",
        ):
            registry.create_type_instance("Pair", [INTEGER_TYPE])

    def test_substitute_types_handles_structured_types(self):
        registry = GenericTypeRegistry()
        substitutions = {"T": STRING_TYPE, "U": INTEGER_TYPE}

        result = registry._substitute_types(
            FunctionType(
                [
                    ListType(GenericParameter("T")),
                    DictionaryType(GenericParameter("T"), GenericParameter("U")),
                ],
                UnionType([GenericParameter("T"), GenericParameter("U")]),
            ),
            substitutions,
        )

        assert isinstance(result, FunctionType)
        assert isinstance(result.param_types[0], ListType)
        assert result.param_types[0].element_type is STRING_TYPE
        assert isinstance(result.param_types[1], DictionaryType)
        assert result.param_types[1].key_type is STRING_TYPE
        assert result.param_types[1].value_type is INTEGER_TYPE
        assert isinstance(result.return_type, UnionType)
        assert set(result.return_type.types) == {STRING_TYPE, INTEGER_TYPE}

    def test_substitute_types_handles_class_type_properties_methods_and_parents(self):
        registry = GenericTypeRegistry()
        substitutions = {"T": FLOAT_TYPE}

        base_class = ClassType(
            "Container",
            {"value": GenericParameter("T")},
            {"get": FunctionType([], GenericParameter("T"))},
            ["BaseContainer"],
        )

        result = registry._substitute_types(base_class, substitutions)

        assert isinstance(result, ClassType)
        assert result.name == "Container"
        assert result.properties["value"] is FLOAT_TYPE
        assert isinstance(result.methods["get"], FunctionType)
        assert result.methods["get"].return_type is FLOAT_TYPE
        assert result.parent_classes == ["BaseContainer"]

    def test_substitute_types_returns_primitive_and_any_unchanged(self):
        registry = GenericTypeRegistry()

        assert registry._substitute_types(INTEGER_TYPE, {"T": STRING_TYPE}) is INTEGER_TYPE
        assert registry._substitute_types(ANY_TYPE, {"T": STRING_TYPE}) is ANY_TYPE


class TestGenericTypeContext:
    def test_add_type_parameter_records_constraint(self):
        context = GenericTypeContext()
        context.add_type_parameter("T", COMPARABLE_TRAIT)

        assert "T" in context.type_parameters
        assert len(context.constraints) == 1
        assert str(context.constraints[0]) == "T: Comparable"

    def test_check_constraints_accepts_trait_and_non_trait_bounds(self):
        context = GenericTypeContext()
        context.add_type_parameter("T", [COMPARABLE_TRAIT, INTEGER_TYPE])

        assert context.check_constraints({"T": INTEGER_TYPE})
        assert not context.check_constraints({"T": FLOAT_TYPE})

    def test_get_substituted_type_handles_nested_types(self):
        context = GenericTypeContext()
        substitutions = {"T": BOOLEAN_TYPE, "U": STRING_TYPE}

        result = context.get_substituted_type(
            FunctionType(
                [
                    ListType(GenericParameter("T")),
                    DictionaryType(GenericParameter("T"), GenericParameter("U")),
                ],
                ClassType(
                    "Wrapper",
                    {"payload": GenericParameter("U")},
                    {"read": FunctionType([], GenericParameter("T"))},
                ),
            ),
            substitutions,
        )

        assert isinstance(result, FunctionType)
        assert result.param_types[0].element_type is BOOLEAN_TYPE
        assert result.param_types[1].key_type is BOOLEAN_TYPE
        assert result.param_types[1].value_type is STRING_TYPE
        assert result.return_type.properties["payload"] is STRING_TYPE
        assert result.return_type.methods["read"].return_type is BOOLEAN_TYPE

    def test_get_substituted_type_leaves_unknown_parameter_intact(self):
        context = GenericTypeContext()

        result = context.get_substituted_type(GenericParameter("Missing"), {})

        assert isinstance(result, GenericParameter)
        assert result.name == "Missing"


class TestGenericTypeConstraint:
    def test_constraint_rejects_non_implementing_type(self):
        trait = TraitType("Imaginary", {}, [])
        constraint = GenericTypeConstraint("T", [trait])

        assert not constraint.check(BOOLEAN_TYPE)


def test_global_registry_is_available():
    assert GENERIC_TYPE_REGISTRY is not None