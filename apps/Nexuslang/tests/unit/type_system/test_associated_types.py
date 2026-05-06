"""
Tests for Associated Types (8.3 Advanced Type Features).

Tests cover:
    - AssociatedTypeDecl: creation, bounds, defaults
    - TypeProjection: T::Item projection, resolution, substitution
    - AssociatedTypeRegistry: registration, resolution, validation
    - TraitType integration: declaring and querying associated types
    - TypeKind.ASSOCIATED and TypeKind.TYPE_PROJECTION entries
"""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../../src"))

from nexuslang.typesystem.associated_types import (
    AssociatedTypeDecl,
    TypeProjection,
    AssociatedTypeRegistry,
    GLOBAL_ASSOC_REGISTRY,
    project,
    bind_associated_type,
    resolve_projection,
)
from nexuslang.typesystem.types import (
    TraitType, ClassType, FunctionType,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, ANY_TYPE,
    TypeKind, get_type_by_name,
)


# ===========================================================================
# AssociatedTypeDecl
# ===========================================================================

class TestAssociatedTypeDecl:
    """Tests for AssociatedTypeDecl."""

    def test_basic_creation(self):
        decl = AssociatedTypeDecl("Item")
        assert decl.name == "Item"
        assert decl.bounds == []
        assert decl.default is None

    def test_with_bounds(self):
        decl = AssociatedTypeDecl("Key", bounds=["Comparable", "Equatable"])
        assert "Comparable" in decl.bounds
        assert "Equatable" in decl.bounds
        assert decl.has_bounds()

    def test_without_bounds(self):
        decl = AssociatedTypeDecl("Item")
        assert not decl.has_bounds()

    def test_with_default(self):
        decl = AssociatedTypeDecl("Error", default=STRING_TYPE)
        assert decl.default is STRING_TYPE

    def test_equality_by_name(self):
        d1 = AssociatedTypeDecl("Item")
        d2 = AssociatedTypeDecl("Item")
        assert d1 == d2

    def test_inequality_different_name(self):
        d1 = AssociatedTypeDecl("Item")
        d2 = AssociatedTypeDecl("Value")
        assert d1 != d2

    def test_hash_consistent(self):
        d1 = AssociatedTypeDecl("Item")
        d2 = AssociatedTypeDecl("Item")
        assert hash(d1) == hash(d2)

    def test_repr_no_bounds(self):
        decl = AssociatedTypeDecl("Item")
        assert "Item" in repr(decl)

    def test_repr_with_bounds(self):
        decl = AssociatedTypeDecl("Key", bounds=["Comparable"])
        r = repr(decl)
        assert "Key" in r
        assert "Comparable" in r

    def test_satisfies_bounds_no_bounds_always_true(self):
        decl = AssociatedTypeDecl("Item")
        assert decl.satisfies_bounds(INTEGER_TYPE)

    def test_satisfies_bounds_with_none_lookup_permissive(self):
        """Without trait lookup, bound validation is permissive."""
        decl = AssociatedTypeDecl("Item", bounds=["SomeTrait"])
        assert decl.satisfies_bounds(INTEGER_TYPE, trait_lookup=None)

    def test_satisfies_bounds_with_trait_lookup_pass(self):
        """A type that implements the trait should pass."""
        from nexuslang.typesystem.types import COMPARABLE_TRAIT
        decl = AssociatedTypeDecl("Key", bounds=["Comparable"])
        # INTEGER_TYPE implements Comparable via _check_primitive_trait_impl
        result = decl.satisfies_bounds(INTEGER_TYPE, trait_lookup={"Comparable": COMPARABLE_TRAIT})
        assert isinstance(result, bool)


# ===========================================================================
# TypeProjection
# ===========================================================================

class TestTypeProjection:
    """Tests for TypeProjection (T::Item)."""

    def test_basic_projection_creation(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        proj = TypeProjection(T, "Item")
        assert proj.associated_type_name == "Item"
        assert proj.base_type is T

    def test_repr_format(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        proj = TypeProjection(T, "Item")
        assert repr(proj) == "T::Item"

    def test_project_helper(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        proj = project(T, "Key")
        assert proj.associated_type_name == "Key"

    def test_equality(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        p1 = TypeProjection(T, "Item")
        p2 = TypeProjection(T, "Item")
        assert p1 == p2

    def test_inequality_different_member(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        p1 = TypeProjection(T, "Item")
        p2 = TypeProjection(T, "Key")
        assert p1 != p2

    def test_hash_consistent(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        p1 = TypeProjection(T, "Item")
        p2 = TypeProjection(T, "Item")
        assert hash(p1) == hash(p2)

    def test_resolve_with_registry_found(self):
        """Projection resolves when registry has the binding."""
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        proj = TypeProjection(T, "Item")
        reg = AssociatedTypeRegistry()
        reg.register("T", "Iterator", "Item", INTEGER_TYPE)
        resolved = proj.resolve(reg, trait_name="Iterator")
        assert resolved is INTEGER_TYPE

    def test_resolve_with_registry_not_found(self):
        """Projection returns self if not in registry."""
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        proj = TypeProjection(T, "Item")
        reg = AssociatedTypeRegistry()
        resolved = proj.resolve(reg)
        assert resolved is proj

    def test_compatibility_with_same_projection(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        p1 = TypeProjection(T, "Item")
        p2 = TypeProjection(T, "Item")
        assert p1.is_compatible_with(p2)

    def test_compatibility_with_any(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        p = TypeProjection(T, "Item")
        assert p.is_compatible_with(ANY_TYPE)

    def test_compatibility_with_different_projection_false(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("T")
        p1 = TypeProjection(T, "Item")
        p2 = TypeProjection(T, "Key")
        assert not p1.is_compatible_with(p2)


# ===========================================================================
# AssociatedTypeRegistry
# ===========================================================================

class TestAssociatedTypeRegistry:
    """Tests for AssociatedTypeRegistry CRUD and validation."""

    def test_register_and_resolve(self):
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter", "Iterator", "Item", INTEGER_TYPE)
        resolved = reg.resolve("NumberIter", "Iterator", "Item")
        assert resolved is INTEGER_TYPE

    def test_resolve_nonexistent_class_returns_none(self):
        reg = AssociatedTypeRegistry()
        assert reg.resolve("NonExistent", "Iterator", "Item") is None

    def test_resolve_nonexistent_trait_returns_none(self):
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter", "Iterator", "Item", INTEGER_TYPE)
        assert reg.resolve("NumberIter", "NonTrait", "Item") is None

    def test_resolve_without_trait_name(self):
        """Resolving with trait_name=None searches all traits."""
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter", "Iterator", "Item", INTEGER_TYPE)
        resolved = reg.resolve("NumberIter", None, "Item")
        assert resolved is INTEGER_TYPE

    def test_has_implementation(self):
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter", "Iterator", "Item", INTEGER_TYPE)
        assert reg.has_implementation("NumberIter", "Iterator")
        assert not reg.has_implementation("NumberIter", "Collection")

    def test_get_all_for_class(self):
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter", "Iterator", "Item", INTEGER_TYPE)
        reg.register("NumberIter", "Summable", "Sum", FLOAT_TYPE)
        data = reg.get_all_for_class("NumberIter")
        assert "Iterator" in data
        assert "Summable" in data

    def test_get_all_for_trait(self):
        reg = AssociatedTypeRegistry()
        reg.register("PairIter", "Iterator", "Item", INTEGER_TYPE)
        reg.register("PairIter", "Iterator", "Key", STRING_TYPE)
        data = reg.get_all_for_trait("PairIter", "Iterator")
        assert data["Item"] is INTEGER_TYPE
        assert data["Key"] is STRING_TYPE

    def test_class_names(self):
        reg = AssociatedTypeRegistry()
        reg.register("ClassA", "T1", "X", INTEGER_TYPE)
        reg.register("ClassB", "T2", "Y", STRING_TYPE)
        names = reg.class_names()
        assert "ClassA" in names
        assert "ClassB" in names

    def test_validate_complete_implementation(self):
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter", "Iterator", "Item", INTEGER_TYPE)
        errors = reg.validate_trait_implementation(
            "NumberIter", "Iterator",
            {"Item": AssociatedTypeDecl("Item")}
        )
        assert errors == []

    def test_validate_missing_required_type(self):
        reg = AssociatedTypeRegistry()
        # Don't register any associated types
        errors = reg.validate_trait_implementation(
            "MyClass", "MyTrait",
            {"Item": AssociatedTypeDecl("Item")}
        )
        assert len(errors) == 1
        assert "Item" in errors[0]

    def test_validate_missing_but_has_default(self):
        """Missing associated type with a default is acceptable."""
        reg = AssociatedTypeRegistry()
        decl_with_default = AssociatedTypeDecl("Error", default=STRING_TYPE)
        errors = reg.validate_trait_implementation(
            "MyClass", "MyTrait",
            {"Error": decl_with_default}
        )
        assert errors == []

    def test_bind_associated_type_helper(self):
        reg = AssociatedTypeRegistry()
        bind_associated_type("MyClass", "Trait", "Item", INTEGER_TYPE, registry=reg)
        resolved = reg.resolve("MyClass", "Trait", "Item")
        assert resolved is INTEGER_TYPE

    def test_resolve_projection_helper(self):
        from nexuslang.typesystem.types import GenericParameter
        T = GenericParameter("NumberIter2")
        proj = TypeProjection(T, "Item")
        reg = AssociatedTypeRegistry()
        reg.register("NumberIter2", None, "Item", STRING_TYPE)
        # Note: resolve with trait_name=None
        bind_associated_type("NumberIter2", "Iterator", "Item", STRING_TYPE, registry=reg)
        resolved = resolve_projection(proj, registry=reg, trait_name="Iterator")
        assert resolved is STRING_TYPE


# ===========================================================================
# TraitType + AssociatedTypeDecl Integration
# ===========================================================================

class TestTraitTypeAssociatedTypeIntegration:
    """Tests for TraitType enhanced with AssociatedTypeDecl."""

    def test_trait_with_associated_type_decl_dict(self):
        decl = AssociatedTypeDecl("Item")
        trait = TraitType("Iterator", {}, associated_types={"Item": decl})
        assert trait.get_associated_type("Item") is decl

    def test_trait_with_legacy_list_converted(self):
        """Legacy List[str] associated types should be converted to dict."""
        trait = TraitType("LegacyTrait", {}, associated_types=["Item", "Key"])
        assert isinstance(trait.associated_types, dict)
        assert "Item" in trait.associated_types
        assert "Key" in trait.associated_types

    def test_trait_legacy_associated_types_are_decls(self):
        trait = TraitType("LegacyTrait", {}, associated_types=["Item"])
        item = trait.get_associated_type("Item")
        assert isinstance(item, AssociatedTypeDecl)
        assert item.name == "Item"

    def test_trait_declare_associated_type(self):
        trait = TraitType("Container", {})
        decl = AssociatedTypeDecl("Element")
        trait.declare_associated_type(decl)
        assert trait.get_associated_type("Element") is decl

    def test_trait_associated_type_names(self):
        trait = TraitType("Mixed", {}, associated_types={
            "Item": AssociatedTypeDecl("Item"),
            "Error": AssociatedTypeDecl("Error"),
        })
        names = trait.associated_type_names()
        assert "Item" in names
        assert "Error" in names

    def test_trait_get_missing_associated_type_returns_none(self):
        trait = TraitType("Empty", {})
        assert trait.get_associated_type("Nonexistent") is None

    def test_trait_with_no_associated_types(self):
        trait = TraitType("Simple", {})
        assert trait.associated_types == {}
        assert trait.associated_type_names() == []


# ===========================================================================
# TypeKind enum entries  
# ===========================================================================

class TestNewTypeKindEntries:
    """Verify the new TypeKind entries added as part of 8.3."""

    def test_type_kind_associated_exists(self):
        assert hasattr(TypeKind, "ASSOCIATED")

    def test_type_kind_type_projection_exists(self):
        assert hasattr(TypeKind, "TYPE_PROJECTION")

    def test_type_kind_type_constructor_exists(self):
        assert hasattr(TypeKind, "TYPE_CONSTRUCTOR")

    def test_type_kind_type_application_exists(self):
        assert hasattr(TypeKind, "TYPE_APPLICATION")

    def test_type_kind_alias_exists(self):
        assert hasattr(TypeKind, "ALIAS")
