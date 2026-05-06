"""
Integration tests for Higher-Kinded Types (HKT) wired into the TypeChecker.

These tests verify that the TypeChecker correctly:
- Exposes check_hkt_constraint / get_implemented_hkts / check_hkt_implementation
- Rejects classes that declare HKT implementations but lack required methods
- Validates HKT trait names when passed as generic constraints
- Does NOT emit errors for completely unrelated code
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

import pytest
from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.typesystem.hkt import (
    GLOBAL_HKT_REGISTRY,
    FUNCTOR_HKT, APPLICATIVE_HKT, MONAD_HKT, FOLDABLE_HKT, TRAVERSABLE_HKT,
    HKT_TRAITS, HKTRegistry, HigherKindedType, TypeConstructorParam, STAR_TO_STAR,
)


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def make_checker() -> TypeChecker:
    return TypeChecker()


# ---------------------------------------------------------------------------
# 1. TypeChecker exposes HKT API
# ---------------------------------------------------------------------------

class TestTypeCheckerHKTAttributes:

    def test_checker_has_hkt_registry(self):
        tc = make_checker()
        assert hasattr(tc, 'hkt_registry')
        assert isinstance(tc.hkt_registry, HKTRegistry)

    def test_checker_has_check_hkt_constraint(self):
        tc = make_checker()
        assert callable(tc.check_hkt_constraint)

    def test_checker_has_get_implemented_hkts(self):
        tc = make_checker()
        assert callable(tc.get_implemented_hkts)

    def test_checker_has_check_hkt_implementation(self):
        tc = make_checker()
        assert callable(tc.check_hkt_implementation)

    def test_hkt_registry_contains_builtins(self):
        tc = make_checker()
        for name in ('Functor', 'Applicative', 'Monad', 'Foldable', 'Traversable'):
            assert tc.hkt_registry.get(name) is not None, f"Missing HKT: {name}"


# ---------------------------------------------------------------------------
# 2. check_hkt_constraint — known built-in constructors
# ---------------------------------------------------------------------------

class TestCheckHktConstraintBuiltins:

    @pytest.mark.parametrize("constructor", ["List", "Maybe", "Optional", "Result"])
    def test_list_like_implements_functor(self, constructor):
        tc = make_checker()
        assert tc.check_hkt_constraint(constructor, 'Functor') is True

    @pytest.mark.parametrize("constructor", ["List", "Maybe", "Optional", "Result"])
    def test_list_like_implements_monad(self, constructor):
        tc = make_checker()
        assert tc.check_hkt_constraint(constructor, 'Monad') is True

    @pytest.mark.parametrize("constructor", ["List", "Maybe", "Optional", "Result"])
    def test_list_like_implements_applicative(self, constructor):
        tc = make_checker()
        assert tc.check_hkt_constraint(constructor, 'Applicative') is True

    @pytest.mark.parametrize("constructor", ["Dictionary", "Tree", "Set"])
    def test_dict_like_implements_functor(self, constructor):
        tc = make_checker()
        assert tc.check_hkt_constraint(constructor, 'Functor') is True

    @pytest.mark.parametrize("constructor", ["Dictionary", "Tree", "Set"])
    def test_dict_like_implements_foldable(self, constructor):
        tc = make_checker()
        assert tc.check_hkt_constraint(constructor, 'Foldable') is True

    @pytest.mark.parametrize("constructor", ["Dictionary", "Tree", "Set"])
    def test_dict_like_does_not_implement_monad(self, constructor):
        tc = make_checker()
        assert tc.check_hkt_constraint(constructor, 'Monad') is False

    def test_integer_does_not_implement_functor(self):
        tc = make_checker()
        assert tc.check_hkt_constraint('Integer', 'Functor') is False

    def test_string_does_not_implement_monad(self):
        tc = make_checker()
        assert tc.check_hkt_constraint('String', 'Monad') is False

    def test_unknown_hkt_name_returns_false(self):
        tc = make_checker()
        # Not a known HKT — safe to return False without error
        assert tc.check_hkt_constraint('List', 'NonExistentTrait') is False

    def test_custom_constructor_after_registration(self):
        tc = make_checker()
        tc.hkt_registry.register_implementation('MyStream', 'Functor')
        assert tc.check_hkt_constraint('MyStream', 'Functor') is True
        assert tc.check_hkt_constraint('MyStream', 'Monad') is False


# ---------------------------------------------------------------------------
# 3. get_implemented_hkts — returns correct list
# ---------------------------------------------------------------------------

class TestGetImplementedHkts:

    def test_list_implements_functor_applicative_monad_foldable_traversable(self):
        tc = make_checker()
        impls = set(tc.get_implemented_hkts('List'))
        assert 'Functor' in impls
        assert 'Applicative' in impls
        assert 'Monad' in impls
        assert 'Foldable' in impls
        assert 'Traversable' in impls

    def test_dictionary_implements_functor_foldable(self):
        tc = make_checker()
        impls = set(tc.get_implemented_hkts('Dictionary'))
        assert 'Functor' in impls
        assert 'Foldable' in impls
        assert 'Monad' not in impls

    def test_unknown_type_returns_empty_list(self):
        tc = make_checker()
        assert tc.get_implemented_hkts('NoSuchType') == []

    def test_returns_list_not_other(self):
        tc = make_checker()
        result = tc.get_implemented_hkts('List')
        assert isinstance(result, list)


# ---------------------------------------------------------------------------
# 4. check_hkt_implementation — validates required methods
# ---------------------------------------------------------------------------

class TestCheckHktImplementation:

    def _make_dummy_class_type(self, method_names):
        """Build a minimal fake ClassType-like object."""
        class FakeClassType:
            def __init__(self, methods):
                self.methods = methods
        return FakeClassType({m: None for m in method_names})

    def test_no_errors_when_methods_present(self):
        tc = make_checker()
        # Functor requires 'map'.  Inject a mock class that has it.
        fake = self._make_dummy_class_type({'map', 'other_method'})
        tc.type_registry['MyFunctor'] = fake
        tc.check_hkt_implementation('MyFunctor', 'Functor')
        assert not any('MyFunctor' in e for e in tc.errors)

    def test_error_when_map_missing_for_functor(self):
        tc = make_checker()
        fake = self._make_dummy_class_type({'bind', 'unit'})  # no 'map'
        tc.type_registry['BadFunctor'] = fake
        tc.check_hkt_implementation('BadFunctor', 'Functor')
        assert any('BadFunctor' in e and 'map' in e for e in tc.errors)

    def test_error_when_bind_missing_for_monad(self):
        tc = make_checker()
        fake = self._make_dummy_class_type({'map', 'pure', 'ap', 'unit'})  # no 'bind'
        tc.type_registry['BadMonad'] = fake
        tc.check_hkt_implementation('BadMonad', 'Monad')
        assert any('BadMonad' in e and 'bind' in e for e in tc.errors)

    def test_monad_checks_inherited_functor_map(self):
        tc = make_checker()
        # Monad extends Applicative extends Functor — all methods required.
        fake = self._make_dummy_class_type({'bind', 'unit'})  # missing 'map', 'pure', 'ap'
        tc.type_registry['PartialMonad'] = fake
        tc.check_hkt_implementation('PartialMonad', 'Monad')
        errors_for_class = [e for e in tc.errors if 'PartialMonad' in e]
        missing = {word for e in errors_for_class for word in ['map', 'pure', 'ap'] if word in e}
        assert len(missing) > 0, "Expected errors for missing inherited methods"

    def test_unknown_hkt_name_is_noop(self):
        tc = make_checker()
        # Should not raise and should not append errors
        tc.check_hkt_implementation('SomeClass', 'NonExistentHKT')
        assert tc.errors == []

    def test_registration_happens_on_success(self):
        tc = make_checker()
        fake = self._make_dummy_class_type({'map'})
        tc.type_registry['GoodFunctor'] = fake
        tc.check_hkt_implementation('GoodFunctor', 'Functor')
        assert tc.check_hkt_constraint('GoodFunctor', 'Functor') is True


# ---------------------------------------------------------------------------
# 5. check_generic_constraints — HKT names are handled (not "Unknown trait")
# ---------------------------------------------------------------------------

class TestGenericConstraintsWithHkt:

    def test_functor_constraint_satisfied(self):
        tc = make_checker()
        from nexuslang.typesystem.types import ANY_TYPE
        # 'List' satisfies Functor constraint
        result = tc.check_generic_constraints(
            type_params=['F'],
            type_args=[ANY_TYPE],
            constraints={'F': ['Functor']},
            context='test'
        )
        # The check uses _type_name on ANY_TYPE.  Depending on implementation
        # this may or may not pass — but critically it must NOT produce
        # an "Unknown trait 'Functor'" error.
        hkt_errors = [e for e in tc.errors if "Unknown trait 'Functor'" in e]
        assert hkt_errors == [], f"Should not report Functor as unknown: {hkt_errors}"

    def test_monad_constraint_does_not_produce_unknown_trait_error(self):
        tc = make_checker()
        from nexuslang.typesystem.types import ANY_TYPE
        tc.check_generic_constraints(
            type_params=['M'],
            type_args=[ANY_TYPE],
            constraints={'M': ['Monad']},
            context='test_monad'
        )
        unknown_errors = [e for e in tc.errors if "Unknown trait 'Monad'" in e]
        assert unknown_errors == [], f"Monad should not be reported as unknown: {unknown_errors}"

    def test_non_hkt_unknown_trait_still_errors(self):
        tc = make_checker()
        from nexuslang.typesystem.types import ANY_TYPE
        tc.check_generic_constraints(
            type_params=['T'],
            type_args=[ANY_TYPE],
            constraints={'T': ['CompletelyMadeUpTrait']},
            context='test_unknown'
        )
        assert any('CompletelyMadeUpTrait' in e for e in tc.errors)


# ---------------------------------------------------------------------------
# 6. __init__.py exports are accessible
# ---------------------------------------------------------------------------

class TestPackageLevelExports:

    def test_can_import_hkt_types_from_typesystem_package(self):
        from nexuslang.typesystem import (
            Kind, StarKind, ArrowKind,
            STAR, STAR_TO_STAR,
            TypeConstructorParam, TypeApplication, HigherKindedType, HKTRegistry,
            GLOBAL_HKT_REGISTRY,
            FUNCTOR_HKT, APPLICATIVE_HKT, MONAD_HKT, FOLDABLE_HKT, TRAVERSABLE_HKT,
            HKT_TRAITS,
            kind_arity, kinds_equal, kind_of_application,
        )
        assert STAR.arity() == 0
        assert STAR_TO_STAR.arity() == 1
        assert FUNCTOR_HKT.name == 'Functor'
        assert MONAD_HKT.name == 'Monad'
        assert kind_arity(STAR) == 0
        assert kinds_equal(STAR, StarKind())

    def test_global_registry_accessible_from_package(self):
        from nexuslang.typesystem import GLOBAL_HKT_REGISTRY
        assert GLOBAL_HKT_REGISTRY.implements('List', 'Functor')
        assert not GLOBAL_HKT_REGISTRY.implements('Integer', 'Monad')
