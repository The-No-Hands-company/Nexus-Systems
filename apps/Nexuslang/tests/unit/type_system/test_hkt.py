"""
Tests for Higher-Kinded Types (8.3 Advanced Type Features).

Tests cover:
    - Kind hierarchy: StarKind, ArrowKind
    - Kind arithmetic: arity, application, equality
    - TypeConstructorParam: ground vs constructor params, kind annotations
    - TypeApplication: creating and comparing applied types
    - HigherKindedType: Functor/Monad/Foldable/Applicative/Traversable
    - HKTRegistry: registration, lookup, implementation tracking
"""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../../src"))

from nexuslang.typesystem.hkt import (
    Kind, StarKind, ArrowKind,
    STAR, STAR_TO_STAR, STAR_TO_STAR_TO_STAR, HO_FUNCTOR_KIND,
    TypeConstructorParam, TypeApplication, HigherKindedType,
    HKTRegistry, GLOBAL_HKT_REGISTRY,
    FUNCTOR_HKT, APPLICATIVE_HKT, MONAD_HKT, FOLDABLE_HKT, TRAVERSABLE_HKT,
    kind_arity, kinds_equal, kind_of_application,
)


# ===========================================================================
# Kind Hierarchy
# ===========================================================================

class TestStarKind:
    """Tests for StarKind (kind *)."""

    def test_star_kind_singleton(self):
        """StarKind should be a singleton."""
        assert StarKind() is StarKind()

    def test_star_is_singleton(self):
        """Module-level STAR constant is the same instance."""
        assert STAR is StarKind()

    def test_star_kind_repr(self):
        assert repr(STAR) == "*"

    def test_star_kind_arity_zero(self):
        """Ground types need no further application."""
        assert STAR.arity() == 0

    def test_star_kind_apply_returns_none(self):
        """Cannot apply a ground type to another type."""
        assert STAR.apply() is None

    def test_star_kind_equality(self):
        assert STAR == StarKind()

    def test_star_kind_hash_consistent(self):
        assert hash(STAR) == hash(StarKind())


class TestArrowKind:
    """Tests for ArrowKind (kind F -> G)."""

    def test_arrow_kind_repr_simple(self):
        k = ArrowKind(STAR, STAR)
        assert repr(k) == "* -> *"

    def test_arrow_kind_repr_nested(self):
        k = STAR_TO_STAR_TO_STAR
        assert repr(k) == "* -> * -> *"

    def test_arrow_kind_arity_one(self):
        assert STAR_TO_STAR.arity() == 1

    def test_arrow_kind_arity_two(self):
        assert STAR_TO_STAR_TO_STAR.arity() == 2

    def test_arrow_kind_apply(self):
        """Applying STAR to STAR_TO_STAR should yield STAR."""
        result = STAR_TO_STAR.apply()
        assert result == STAR

    def test_arrow_kind_equality(self):
        k1 = ArrowKind(STAR, STAR)
        k2 = ArrowKind(STAR, STAR)
        assert k1 == k2

    def test_arrow_kind_inequality_different_result(self):
        k1 = ArrowKind(STAR, STAR)
        k2 = ArrowKind(STAR, STAR_TO_STAR)
        assert k1 != k2

    def test_arrow_kind_hash_equal_objects(self):
        k1 = ArrowKind(STAR, STAR)
        k2 = ArrowKind(STAR, STAR)
        assert hash(k1) == hash(k2)

    def test_ho_functor_kind(self):
        """(* -> *) -> * should have arity 1."""
        assert HO_FUNCTOR_KIND.arity() == 1
        assert HO_FUNCTOR_KIND.param_kind == STAR_TO_STAR
        assert HO_FUNCTOR_KIND.result_kind == STAR


class TestKindUtilities:
    """Tests for kind_arity, kinds_equal, kind_of_application."""

    def test_kind_arity_star(self):
        assert kind_arity(STAR) == 0

    def test_kind_arity_star_to_star(self):
        assert kind_arity(STAR_TO_STAR) == 1

    def test_kind_arity_star_to_star_to_star(self):
        assert kind_arity(STAR_TO_STAR_TO_STAR) == 2

    def test_kinds_equal_true(self):
        assert kinds_equal(STAR, StarKind())

    def test_kinds_equal_false(self):
        assert not kinds_equal(STAR, STAR_TO_STAR)

    def test_kind_of_application_ground_to_ground(self):
        """* -> * applied to * yields *."""
        result = kind_of_application(STAR_TO_STAR, STAR)
        assert result == STAR

    def test_kind_of_application_kind_mismatch(self):
        """Applying * -> * to * -> * is ill-kinded here."""
        result = kind_of_application(STAR_TO_STAR, STAR_TO_STAR)
        assert result is None

    def test_kind_of_application_not_arrow_returns_none(self):
        result = kind_of_application(STAR, STAR)
        assert result is None


# ===========================================================================
# TypeConstructorParam
# ===========================================================================

class TestTypeConstructorParam:
    """Tests for TypeConstructorParam."""

    def test_default_kind_is_star(self):
        p = TypeConstructorParam("T")
        assert p.kind == STAR
        assert p.is_ground()
        assert not p.is_constructor()

    def test_constructor_kind(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        assert F.kind == STAR_TO_STAR
        assert F.is_constructor()
        assert not F.is_ground()

    def test_expected_arity_ground(self):
        T = TypeConstructorParam("T")
        assert T.expected_arity() == 0

    def test_expected_arity_constructor(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        assert F.expected_arity() == 1

    def test_equality(self):
        F1 = TypeConstructorParam("F", STAR_TO_STAR)
        F2 = TypeConstructorParam("F", STAR_TO_STAR)
        assert F1 == F2

    def test_inequality_different_name(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        G = TypeConstructorParam("G", STAR_TO_STAR)
        assert F != G

    def test_inequality_different_kind(self):
        T1 = TypeConstructorParam("T", STAR)
        T2 = TypeConstructorParam("T", STAR_TO_STAR)
        assert T1 != T2

    def test_repr_ground(self):
        T = TypeConstructorParam("T")
        assert repr(T) == "T"

    def test_repr_constructor(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        assert "::" in repr(F) and "F" in repr(F)

    def test_hash_consistent(self):
        F1 = TypeConstructorParam("F", STAR_TO_STAR)
        F2 = TypeConstructorParam("F", STAR_TO_STAR)
        assert hash(F1) == hash(F2)


# ===========================================================================
# TypeApplication
# ===========================================================================

class TestTypeApplication:
    """Tests for TypeApplication: F<A>."""

    def test_basic_application(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        A = TypeConstructorParam("A", STAR)
        fa = TypeApplication(F, A)
        assert fa.constructor == F
        assert fa.argument == A

    def test_result_kind_star_to_star_applied_to_star(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        A = TypeConstructorParam("A", STAR)
        fa = TypeApplication(F, A)
        assert fa.result_kind() == STAR

    def test_type_application_equality(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        A = TypeConstructorParam("A", STAR)
        fa1 = TypeApplication(F, A)
        fa2 = TypeApplication(F, A)
        assert fa1 == fa2

    def test_type_application_inequality(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        G = TypeConstructorParam("G", STAR_TO_STAR)
        A = TypeConstructorParam("A", STAR)
        assert TypeApplication(F, A) != TypeApplication(G, A)

    def test_type_application_repr(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        A = TypeConstructorParam("A", STAR)
        fa = TypeApplication(F, A)
        assert "F" in repr(fa) and "A" in repr(fa)

    def test_chained_application(self):
        """Chained: TypeApplication(TypeApplication(Dict, K), V)."""
        Dict = TypeConstructorParam("Dict", STAR_TO_STAR_TO_STAR)
        K = TypeConstructorParam("K", STAR)
        V = TypeConstructorParam("V", STAR)
        dict_k = TypeApplication(Dict, K)
        dict_kv = TypeApplication(dict_k, V)
        assert dict_kv.argument == V

    def test_substitute_constructor_param(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        A = TypeConstructorParam("A", STAR)
        G = TypeConstructorParam("G", STAR_TO_STAR)
        fa = TypeApplication(F, A)
        substituted = fa.substitute({"F": G})
        assert substituted.constructor == G
        assert substituted.argument == A


# ===========================================================================
# HigherKindedType
# ===========================================================================

class TestHigherKindedType:
    """Tests for HigherKindedType class."""

    def test_create_hkt(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        hkt = HigherKindedType("MyF", [F])
        assert hkt.name == "MyF"
        assert len(hkt.constructor_params) == 1

    def test_requires_constructor_of_kind(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        hkt = HigherKindedType("Functor", [F])
        assert hkt.requires_constructor_of_kind(STAR_TO_STAR)
        assert not hkt.requires_constructor_of_kind(STAR)

    def test_hkt_is_implemented_by_constructor_matching_kind(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        hkt = HigherKindedType("Functor", [F])
        assert hkt.is_implemented_by_constructor(STAR_TO_STAR)
        assert not hkt.is_implemented_by_constructor(STAR)

    def test_hkt_repr(self):
        F = TypeConstructorParam("F", STAR_TO_STAR)
        hkt = HigherKindedType("Functor", [F])
        assert "Functor" in repr(hkt)


# ===========================================================================
# Built-in HKT Traits
# ===========================================================================

class TestBuiltinHKTTraits:
    """Tests for FUNCTOR_HKT, APPLICATIVE_HKT, MONAD_HKT, etc."""

    def test_functor_hkt_exists(self):
        assert FUNCTOR_HKT is not None
        assert FUNCTOR_HKT.name == "Functor"

    def test_functor_has_map_method(self):
        assert "map" in FUNCTOR_HKT.methods

    def test_applicative_inherits_functor(self):
        assert FUNCTOR_HKT in APPLICATIVE_HKT.parent_hkt

    def test_monad_inherits_applicative(self):
        assert APPLICATIVE_HKT in MONAD_HKT.parent_hkt

    def test_monad_has_bind(self):
        assert "bind" in MONAD_HKT.methods

    def test_monad_has_unit(self):
        assert "unit" in MONAD_HKT.methods

    def test_foldable_has_fold(self):
        assert "fold" in FOLDABLE_HKT.methods

    def test_traversable_has_traverse(self):
        assert "traverse" in TRAVERSABLE_HKT.methods

    def test_all_builtin_hkts_have_constructor_params(self):
        for trait in [FUNCTOR_HKT, APPLICATIVE_HKT, MONAD_HKT, FOLDABLE_HKT, TRAVERSABLE_HKT]:
            assert len(trait.constructor_params) >= 1
            assert all(isinstance(p, TypeConstructorParam) for p in trait.constructor_params)

    def test_all_builtin_constructors_have_star_to_star_kind(self):
        for trait in [FUNCTOR_HKT, MONAD_HKT, FOLDABLE_HKT]:
            for p in trait.constructor_params:
                assert p.kind == STAR_TO_STAR


# ===========================================================================
# HKTRegistry
# ===========================================================================

class TestHKTRegistry:
    """Tests for HKTRegistry."""

    def test_register_and_get(self):
        reg = HKTRegistry()
        F = TypeConstructorParam("F", STAR_TO_STAR)
        hkt = HigherKindedType("Custom", [F], methods={})
        reg.register(hkt)
        assert reg.get("Custom") is hkt

    def test_get_nonexistent_returns_none(self):
        reg = HKTRegistry()
        assert reg.get("Nonexistent") is None

    def test_register_implementation(self):
        reg = HKTRegistry()
        reg.register_implementation("List", "Functor")
        assert reg.implements("List", "Functor")
        assert not reg.implements("List", "Monad")

    def test_get_implementations(self):
        reg = HKTRegistry()
        reg.register_implementation("List", "Functor")
        reg.register_implementation("List", "Foldable")
        impls = reg.get_implementations("List")
        assert "Functor" in impls
        assert "Foldable" in impls

    def test_all_names(self):
        reg = HKTRegistry()
        F = TypeConstructorParam("F", STAR_TO_STAR)
        reg.register(HigherKindedType("A", [F]))
        reg.register(HigherKindedType("B", [F]))
        names = reg.all_names()
        assert "A" in names and "B" in names

    def test_global_registry_contains_builtins(self):
        for name in ["Functor", "Applicative", "Monad", "Foldable", "Traversable"]:
            assert GLOBAL_HKT_REGISTRY.get(name) is not None

    def test_global_registry_list_implements_functor(self):
        assert GLOBAL_HKT_REGISTRY.implements("List", "Functor")

    def test_global_registry_list_implements_monad(self):
        assert GLOBAL_HKT_REGISTRY.implements("List", "Monad")

    def test_global_registry_dict_implements_foldable(self):
        assert GLOBAL_HKT_REGISTRY.implements("Dictionary", "Foldable")
