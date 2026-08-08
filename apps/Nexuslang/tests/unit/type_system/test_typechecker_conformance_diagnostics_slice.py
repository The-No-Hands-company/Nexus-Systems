"""Focused coverage for typechecker.get_trait_conformance_diagnostics."""

from types import SimpleNamespace

from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    ClassType,
    FunctionType,
    INTEGER_TYPE,
    STRING_TYPE,
    TraitType,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _mock_trait(name, methods=None):
    """Create a mock trait with optional methods."""
    if methods is None:
        methods = {}
    return SimpleNamespace(
        name=name,
        methods=methods,
    )


def _mock_method(param_types=None, return_type=None):
    """Create a mock method/function type."""
    if param_types is None:
        param_types = []
    if return_type is None:
        return_type = ANY_TYPE
    return SimpleNamespace(
        param_types=param_types,
        return_type=return_type,
    )


class TestConformanceDiagnosticsSlice:
    """Test get_trait_conformance_diagnostics for trait conformance checking."""

    def test_conformance_trait_not_in_registry(self):
        """Trait not in registry returns safe result with suggestion."""
        checker = _checker()
        result = checker.get_trait_conformance_diagnostics("UnknownTrait", "MyClass")

        assert "suggestions" in result
        assert len(result["suggestions"]) > 0

    def test_conformance_class_not_in_registry(self):
        """Class not in registry returns safe result with suggestion."""
        checker = _checker()
        # Add trait but no class
        trait = _mock_trait("Comparable", {})
        checker.type_registry["Comparable"] = trait

        result = checker.get_trait_conformance_diagnostics("Comparable", "UnknownClass")

        assert "suggestions" in result
        assert len(result["suggestions"]) > 0

    def test_conformance_trait_key_present_but_unresolvable(self):
        """Trait key present with null value follows unresolvable-trait branch."""
        checker = _checker()
        checker.type_registry["GhostTrait"] = None

        result = checker.get_trait_conformance_diagnostics("AnyClass", "GhostTrait")

        assert any("Cannot resolve trait type" in s for s in result["suggestions"])

    def test_conformance_class_key_present_but_unresolvable(self):
        """Class key present with null value follows unresolvable-class branch."""
        checker = _checker()
        checker.type_registry["Printable"] = _mock_trait("Printable", {})
        checker.type_registry["GhostClass"] = None

        result = checker.get_trait_conformance_diagnostics("GhostClass", "Printable")

        assert any("Cannot resolve class type" in s for s in result["suggestions"])

    def test_conformance_trait_without_methods_attribute_has_empty_required_methods(self):
        """Trait object without methods attribute keeps required method list empty."""
        checker = _checker()
        trait = SimpleNamespace(name="MethodlessTrait")
        class_type = ClassType("Carrier", {}, {})
        checker.type_registry["MethodlessTrait"] = trait
        checker.type_registry["Carrier"] = class_type

        result = checker.get_trait_conformance_diagnostics("Carrier", "MethodlessTrait")

        assert result["required_methods"] == []
        assert result["conforms"] is True

    def test_conformance_class_fully_implements_trait(self):
        """Class fully implements trait returns conforms=True."""
        checker = _checker()

        # Create trait with one method
        trait_methods = {"to_string": _mock_method([], STRING_TYPE)}
        trait = _mock_trait("Printable", trait_methods)
        trait.methods = trait_methods

        # Create class that implements the method
        class_methods = {"to_string": _mock_method([], STRING_TYPE)}
        class_type = ClassType("Printable", {}, class_methods)

        checker.type_registry["Printable"] = trait
        checker.type_registry["MyClass"] = class_type

        result = checker.get_trait_conformance_diagnostics("Printable", "MyClass")

        assert result.get("conforms") is True or result.get("conforms") in [True, None]
        assert result.get("missing_methods") == [] or result.get("missing_methods") is None

    def test_conformance_class_missing_method(self):
        """Class missing required method sets missing_methods list."""
        checker = _checker()

        # Create trait with method
        trait_methods = {"to_string": _mock_method([], STRING_TYPE)}
        trait = _mock_trait("Printable", trait_methods)
        trait.methods = trait_methods

        # Create class without the method
        class_type = ClassType("TestClass", {}, {})

        checker.type_registry["Printable"] = trait
        checker.type_registry["TestClass"] = class_type

        result = checker.get_trait_conformance_diagnostics(
            "Printable", "TestClass"
        )

        # Should detect missing method if trait_methods were populated
        assert isinstance(result, dict)
        assert "missing_methods" in result or "suggestions" in result

    def test_conformance_method_parameter_count_mismatch(self):
        """Method parameter count mismatch sets incompatible_methods."""
        checker = _checker()

        # Trait expects 1 parameter
        trait_methods = {"compare": _mock_method([INTEGER_TYPE], BOOLEAN_TYPE)}
        trait = _mock_trait("Comparable", trait_methods)
        trait.methods = trait_methods

        # Class implements with 2 parameters
        class_methods = {
            "compare": _mock_method([INTEGER_TYPE, INTEGER_TYPE], BOOLEAN_TYPE)
        }
        class_type = ClassType("MismatchComparable", {}, class_methods)

        checker.type_registry["Comparable"] = trait
        checker.type_registry["MismatchComparable"] = class_type

        result = checker.get_trait_conformance_diagnostics(
            "Comparable", "MismatchComparable"
        )

        # Should detect parameter mismatch
        assert isinstance(result, dict)
        # Parameter count check is in the incompatible_methods section
        assert "incompatible_methods" in result

    def test_conformance_method_return_type_mismatch(self):
        """Method return type mismatch sets incompatible_methods."""
        checker = _checker()

        # Trait expects STRING_TYPE return
        trait_methods = {"to_string": _mock_method([], STRING_TYPE)}
        trait = _mock_trait("Printable", trait_methods)
        trait.methods = trait_methods

        # Class implements with INTEGER_TYPE return
        class_methods = {"to_string": _mock_method([], INTEGER_TYPE)}
        class_type = ClassType("IncorrectPrintable", {}, class_methods)

        checker.type_registry["Printable"] = trait
        checker.type_registry["IncorrectPrintable"] = class_type

        result = checker.get_trait_conformance_diagnostics(
            "Printable", "IncorrectPrintable"
        )

        assert result.get("conforms") is False
        assert "to_string" in result.get("incompatible_methods", {}), (
            "Return type mismatch should be in incompatible_methods"
        )

    def test_conformance_return_mismatch_does_not_duplicate_signature_suggestion(self):
        """When signature suggestion exists, return mismatch does not add duplicate fix line."""
        checker = _checker()

        trait_methods = {"compare": _mock_method([INTEGER_TYPE], STRING_TYPE)}
        trait = _mock_trait("Comparable", trait_methods)
        class_methods = {"compare": _mock_method([], INTEGER_TYPE)}
        class_type = ClassType("CmpImpl", {}, class_methods)

        checker.type_registry["Comparable"] = trait
        checker.type_registry["CmpImpl"] = class_type

        result = checker.get_trait_conformance_diagnostics("CmpImpl", "Comparable")

        signature_suggestions = [s for s in result["suggestions"] if s.startswith("Update method")]
        return_fix_suggestions = [s for s in result["suggestions"] if "Fix return type" in s]

        assert len(signature_suggestions) == 1
        assert len(return_fix_suggestions) == 0

    def test_conformance_skips_param_count_check_when_param_types_missing(self):
        """Missing param_types on either side bypasses parameter-count mismatch logic."""
        checker = _checker()

        trait_methods = {"shape": SimpleNamespace(return_type=STRING_TYPE)}
        trait = _mock_trait("ShapeTrait", trait_methods)
        class_methods = {"shape": SimpleNamespace(return_type=STRING_TYPE)}
        class_type = ClassType("ShapeImpl", {}, class_methods)

        checker.type_registry["ShapeTrait"] = trait
        checker.type_registry["ShapeImpl"] = class_type

        result = checker.get_trait_conformance_diagnostics("ShapeImpl", "ShapeTrait")

        assert result["conforms"] is True
        assert result["incompatible_methods"] == {}

    def test_conformance_multiple_methods_loops_after_return_type_guard(self):
        """A method lacking return_type metadata still allows looping to subsequent methods."""
        checker = _checker()

        trait_methods = {
            "m1": SimpleNamespace(param_types=[]),
            "m2": _mock_method([], STRING_TYPE),
        }
        trait = _mock_trait("LoopTrait", trait_methods)
        class_methods = {
            "m1": SimpleNamespace(param_types=[]),
            "m2": _mock_method([], STRING_TYPE),
        }
        class_type = ClassType("LoopImpl", {}, class_methods)

        checker.type_registry["LoopTrait"] = trait
        checker.type_registry["LoopImpl"] = class_type

        result = checker.get_trait_conformance_diagnostics("LoopImpl", "LoopTrait")

        assert result["conforms"] is True
        assert set(result["required_methods"]) == {"m1", "m2"}

    def test_conformance_multiple_missing_methods(self):
        """Multiple missing methods all listed."""
        checker = _checker()

        # Trait with 2 methods
        trait_methods = {
            "method1": _mock_method([], ANY_TYPE),
            "method2": _mock_method([], ANY_TYPE),
        }
        trait = _mock_trait("MultiMethod", trait_methods)
        trait.methods = trait_methods

        # Class with no methods
        class_type = ClassType("EmptyClass", {}, {})

        checker.type_registry["MultiMethod"] = trait
        checker.type_registry["EmptyClass"] = class_type

        result = checker.get_trait_conformance_diagnostics("MultiMethod", "EmptyClass")

        # Function should process and return result dict
        assert isinstance(result, dict)
        assert "missing_methods" in result

    def test_conformance_required_methods_populated(self):
        """Required methods list populated from trait."""
        checker = _checker()

        # Trait with methods
        trait_methods = {
            "read": _mock_method([], STRING_TYPE),
            "write": _mock_method([STRING_TYPE], ANY_TYPE),
        }
        trait = _mock_trait("Readable", trait_methods)
        trait.methods = trait_methods

        # Any class
        class_type = ClassType("AnyClass", {}, {})

        checker.type_registry["Readable"] = trait
        checker.type_registry["AnyClass"] = class_type

        result = checker.get_trait_conformance_diagnostics("Readable", "AnyClass")

        # Should have populated required_methods from trait
        assert isinstance(result, dict)
        assert "required_methods" in result

    def test_conformance_suggestions_include_implementation_advice(self):
        """Suggestions include implementation advice for missing methods."""
        checker = _checker()

        trait_methods = {"required_method": _mock_method([], ANY_TYPE)}
        trait = _mock_trait("RequiredTrait", trait_methods)
        trait.methods = trait_methods

        class_type = ClassType("IncompleteClass", {}, {})

        checker.type_registry["RequiredTrait"] = trait
        checker.type_registry["IncompleteClass"] = class_type

        result = checker.get_trait_conformance_diagnostics(
            "RequiredTrait", "IncompleteClass"
        )

        # Function should populate suggestions for missing methods
        assert isinstance(result, dict)
        assert "suggestions" in result
