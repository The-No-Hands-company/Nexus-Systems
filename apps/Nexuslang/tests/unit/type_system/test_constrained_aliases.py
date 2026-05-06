"""
Tests for Type Alias Registry and Constrained Type Aliases (8.3 Advanced Type Features).

Tests cover:
    - TypeAliasType: parameterized aliases, constraints, instantiation
    - TypeAliasRegistry: registration, lookup, expansion, recursive expansion
    - GLOBAL_ALIAS_REGISTRY: global singleton
    - Constraint violations raise TypeError
    - get_type_by_name integration: resolves registered aliases by name
    - expand_type: recursive expansion of aliased types
"""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../../src"))

from nexuslang.typesystem.type_alias_registry import (
    TypeAliasRegistry,
    GLOBAL_ALIAS_REGISTRY,
    register_alias,
    resolve_alias,
    expand_type,
)
from nexuslang.typesystem.types import (
    TypeAliasType, GenericParameter, ListType, DictionaryType,
    FunctionType, UnionType, TraitType,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, BOOLEAN_TYPE, ANY_TYPE,
    COMPARABLE_TRAIT,
    get_type_by_name,
)


# ===========================================================================
# TypeAliasType (existing class — verify contract)
# ===========================================================================

class TestTypeAliasType:
    """Verify TypeAliasType core behaviour relied on by the registry."""

    def test_simple_alias_no_params(self):
        alias = TypeAliasType("StringList", [], ListType(STRING_TYPE))
        assert alias.name == "StringList"
        assert alias.type_parameters == []
        assert isinstance(alias.target_type, ListType)

    def test_parameterized_alias_instantiate(self):
        T = GenericParameter("T")
        alias = TypeAliasType("MyList", ["T"], ListType(T))
        result = alias.instantiate([INTEGER_TYPE])
        assert isinstance(result, ListType)
        assert result.element_type is INTEGER_TYPE

    def test_instantiate_wrong_arity_raises(self):
        T = GenericParameter("T")
        alias = TypeAliasType("MyList", ["T"], ListType(T))
        with pytest.raises(TypeError):
            alias.instantiate([INTEGER_TYPE, STRING_TYPE])

    def test_alias_is_transparent_for_compatibility(self):
        alias = TypeAliasType("IntAlias", [], INTEGER_TYPE)
        assert alias.is_compatible_with(INTEGER_TYPE)

    def test_constrained_alias_valid_arg(self):
        T = GenericParameter("T")
        alias = TypeAliasType(
            "ComparableList", ["T"],
            ListType(T),
            constraints={"T": [COMPARABLE_TRAIT]}
        )
        # Integer implements Comparable
        result = alias.instantiate([INTEGER_TYPE])
        assert isinstance(result, ListType)

    def test_constrained_alias_invalid_arg_raises(self):
        T = GenericParameter("T")
        # A custom trait with no implementation on Boolean
        strict_trait = TraitType("StrictTrait", {
            "strict_method": FunctionType([INTEGER_TYPE], BOOLEAN_TYPE)
        })
        alias = TypeAliasType(
            "StrictList", ["T"],
            ListType(T),
            constraints={"T": [strict_trait]}
        )
        with pytest.raises(TypeError):
            alias.instantiate([BOOLEAN_TYPE])


# ===========================================================================
# TypeAliasRegistry — basic operations
# ===========================================================================

class TestTypeAliasRegistryBasicOps:
    """Tests for TypeAliasRegistry CRUD operations."""

    def test_register_and_has(self):
        reg = TypeAliasRegistry()
        alias = TypeAliasType("StringList", [], ListType(STRING_TYPE))
        reg.register(alias)
        assert reg.has("StringList")

    def test_has_missing_returns_false(self):
        reg = TypeAliasRegistry()
        assert not reg.has("Nonexistent")

    def test_get_returns_registered_alias(self):
        reg = TypeAliasRegistry()
        alias = TypeAliasType("Test", [], INTEGER_TYPE)
        reg.register(alias)
        assert reg.get("Test") is alias

    def test_get_missing_returns_none(self):
        reg = TypeAliasRegistry()
        assert reg.get("Missing") is None

    def test_all_names_sorted(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("Zebra", [], INTEGER_TYPE))
        reg.register(TypeAliasType("Alpha", [], STRING_TYPE))
        names = reg.all_names()
        assert names == sorted(names)
        assert "Zebra" in names and "Alpha" in names

    def test_register_duplicate_raises_by_default(self):
        reg = TypeAliasRegistry()
        alias = TypeAliasType("Dup", [], INTEGER_TYPE)
        reg.register(alias)
        with pytest.raises(ValueError):
            reg.register(TypeAliasType("Dup", [], STRING_TYPE))

    def test_register_duplicate_overwrite_ok(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("MyAlias", [], INTEGER_TYPE))
        reg.register(TypeAliasType("MyAlias", [], STRING_TYPE), overwrite=True)
        assert reg.get("MyAlias").target_type is STRING_TYPE

    def test_unregister_existing(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("Tmp", [], INTEGER_TYPE))
        assert reg.unregister("Tmp") is True
        assert not reg.has("Tmp")

    def test_unregister_missing_returns_false(self):
        reg = TypeAliasRegistry()
        assert reg.unregister("Nonexistent") is False

    def test_len(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("A", [], INTEGER_TYPE))
        reg.register(TypeAliasType("B", [], STRING_TYPE))
        assert len(reg) == 2

    def test_contains(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("X", [], INTEGER_TYPE))
        assert "X" in reg
        assert "Y" not in reg

    def test_clear(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("A", [], INTEGER_TYPE))
        reg.clear()
        assert len(reg) == 0

    def test_clone_is_independent(self):
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("Base", [], INTEGER_TYPE))
        copy = reg.clone()
        copy.register(TypeAliasType("Extra", [], STRING_TYPE))
        assert not reg.has("Extra")
        assert reg.has("Base") and copy.has("Base")

    def test_register_many(self):
        reg = TypeAliasRegistry()
        aliases = [
            TypeAliasType("A", [], INTEGER_TYPE),
            TypeAliasType("B", [], STRING_TYPE),
        ]
        reg.register_many(aliases)
        assert reg.has("A") and reg.has("B")


# ===========================================================================
# TypeAliasRegistry — expansion
# ===========================================================================

class TestTypeAliasRegistryExpansion:
    """Tests for expand() and expand_recursive()."""

    def test_expand_simple_alias(self):
        reg = TypeAliasRegistry()
        alias = TypeAliasType("IntList", [], ListType(INTEGER_TYPE))
        reg.register(alias)
        result = reg.expand("IntList")
        assert isinstance(result, ListType)
        assert result.element_type is INTEGER_TYPE

    def test_expand_parameterized_alias(self):
        T = GenericParameter("T")
        reg = TypeAliasRegistry()
        alias = TypeAliasType("Pair", ["T"], ListType(T))
        reg.register(alias)
        result = reg.expand("Pair", [STRING_TYPE])
        assert isinstance(result, ListType)
        assert result.element_type is STRING_TYPE

    def test_expand_missing_returns_none(self):
        reg = TypeAliasRegistry()
        assert reg.expand("NonExistent") is None

    def test_expand_parameterized_without_args_raises(self):
        T = GenericParameter("T")
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("MyPair", ["T"], ListType(T)))
        with pytest.raises(TypeError):
            reg.expand("MyPair")

    def test_expand_wrong_arg_count_raises(self):
        T = GenericParameter("T")
        reg = TypeAliasRegistry()
        reg.register(TypeAliasType("MyPair", ["T"], ListType(T)))
        with pytest.raises(TypeError):
            reg.expand("MyPair", [INTEGER_TYPE, STRING_TYPE])

    def test_expand_recursive_list_alias(self):
        """Expand a ListType that wraps an alias name."""
        reg = TypeAliasRegistry()
        inner = TypeAliasType("IntAlias", [], INTEGER_TYPE)
        reg.register(inner)
        outer = TypeAliasType("IntAlias", [], INTEGER_TYPE)
        result = reg.expand_recursive(outer)
        assert result is INTEGER_TYPE

    def test_expand_recursive_no_change_for_primitive(self):
        reg = TypeAliasRegistry()
        result = reg.expand_recursive(INTEGER_TYPE)
        assert result is INTEGER_TYPE


# ===========================================================================
# Global alias registry helpers
# ===========================================================================

class TestGlobalAliasRegistry:
    """Tests via the module-level helper functions."""

    def setup_method(self):
        """Use a fresh snapshot to avoid test pollution."""
        self._orig_aliases = dict(GLOBAL_ALIAS_REGISTRY._aliases)

    def teardown_method(self):
        """Restore the global registry after each test."""
        GLOBAL_ALIAS_REGISTRY._aliases = self._orig_aliases

    def test_register_alias_and_resolve(self):
        register_alias(TypeAliasType("GlobalIntList", [], ListType(INTEGER_TYPE)), overwrite=True)
        result = resolve_alias("GlobalIntList")
        assert isinstance(result, ListType)
        assert result.element_type is INTEGER_TYPE

    def test_resolve_alias_missing_returns_none(self):
        assert resolve_alias("_definitely_not_registered_12345") is None

    def test_get_type_by_name_resolves_registered_alias(self):
        register_alias(TypeAliasType("StrList", [], ListType(STRING_TYPE)), overwrite=True)
        resolved = get_type_by_name("StrList")
        assert isinstance(resolved, ListType)
        assert resolved.element_type is STRING_TYPE


# ===========================================================================
# expand_type utility
# ===========================================================================

class TestExpandTypeUtility:
    """Tests for the expand_type() convenience function."""

    def test_expand_type_no_alias_returns_same(self):
        reg = TypeAliasRegistry()
        result = expand_type(INTEGER_TYPE, registry=reg)
        assert result is INTEGER_TYPE

    def test_expand_type_uses_global_by_default(self):
        result = expand_type(STRING_TYPE)
        assert result is STRING_TYPE
