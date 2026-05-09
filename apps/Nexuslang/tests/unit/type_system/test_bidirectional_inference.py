"""
Test bidirectional type inference and generic constraint propagation.
"""

import pytest

from nexuslang.typesystem.types import (
    ListType, DictionaryType, FunctionType, ClassType,
    INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, BOOLEAN_TYPE, ANY_TYPE,
    get_type_by_name,
)
from nexuslang.typesystem.type_inference import TypeInferenceEngine
from nexuslang.typesystem.typechecker import TypeChecker
from nexuslang.parser.ast import (
    Literal, VariableDeclaration, FunctionDefinition, FunctionCall, Parameter, Program,
)


def test_bidirectional_inference():
    """Test bidirectional type inference with expected types."""
    print("Testing Bidirectional Type Inference...")

    engine = TypeInferenceEngine()

    # Test 1: List literal with expected type
    print("\n1. Testing list literal with expected type:")

    list_expr = type('ListLiteral', (), {
        'node_type': 'list_literal',
        'elements': [
            Literal('integer', 1),
            Literal('integer', 2),
            Literal('integer', 3)
        ]
    })()

    expected_type = ListType(INTEGER_TYPE)
    inferred = engine.infer_with_expected_type(list_expr, expected_type, {})

    assert inferred == expected_type, f"Expected {expected_type}, got {inferred}"
    print(f"    List literal inferred as {inferred}")

    # Test 2: Dictionary literal with expected type
    print("\n2. Testing dictionary literal with expected type:")

    dict_expr = type('DictLiteral', (), {
        'node_type': 'dictionary_literal',
        'keys': [Literal('string', "a"), Literal('string', "b")],
        'values': [Literal('integer', 1), Literal('integer', 2)]
    })()

    expected_dict_type = DictionaryType(STRING_TYPE, INTEGER_TYPE)
    inferred_dict = engine.infer_with_expected_type(dict_expr, expected_dict_type, {})

    assert inferred_dict == expected_dict_type
    print(f"    Dictionary literal inferred as {inferred_dict}")

    # Test 3: No expected type (fallback to regular inference)
    print("\n3. Testing fallback to regular inference:")

    inferred_no_expected = engine.infer_with_expected_type(list_expr, None, {})
    assert isinstance(inferred_no_expected, ListType)
    print(f"    Falls back to regular inference: {inferred_no_expected}")

    # Test 4: Type mismatch (should fall back)
    print("\n4. Testing type mismatch fallback:")

    float_list = type('ListLiteral', (), {
        'node_type': 'list_literal',
        'elements': [Literal('float', 1.5), Literal('float', 2.5)]
    })()

    inferred_mismatch = engine.infer_with_expected_type(float_list, ListType(INTEGER_TYPE), {})
    print(f"    Type mismatch handled: {inferred_mismatch}")

    print("\n All bidirectional inference tests passed!")


# ---------------------------------------------------------------------------
# Helper builders
# ---------------------------------------------------------------------------

def _make_generic_func(
    name: str,
    type_params: list,
    param_name_type_pairs: list,
    return_type: str = "T",
    constraints: dict = None,
) -> FunctionDefinition:
    """Build a FunctionDefinition with generic type parameters."""
    params = [Parameter(pname, ptype) for pname, ptype in param_name_type_pairs]
    return FunctionDefinition(
        name=name,
        parameters=params,
        body=[],
        return_type=return_type,
        type_parameters=type_params,
        type_constraints=constraints or {},
    )


def _make_call(func_name: str, *literal_args) -> FunctionCall:
    """Build a FunctionCall AST node with literal arguments."""
    literals = list(literal_args)
    call = FunctionCall(func_name, literals)
    call.named_arguments = {}
    return call


# ---------------------------------------------------------------------------
# Tests for _infer_generic_substitutions
# ---------------------------------------------------------------------------

class TestInferGenericSubstitutions:
    """Unit tests for TypeChecker._infer_generic_substitutions."""

    def setup_method(self):
        self.tc = TypeChecker(enable_ownership_passes=False)

    def test_single_type_param_inferred_as_integer(self):
        func_def = _make_generic_func("identity", ["T"], [("x", "T")])
        subs = self.tc._infer_generic_substitutions(func_def, [INTEGER_TYPE])
        assert subs["T"] == INTEGER_TYPE

    def test_single_type_param_inferred_as_string(self):
        func_def = _make_generic_func("wrap", ["T"], [("val", "T")])
        subs = self.tc._infer_generic_substitutions(func_def, [STRING_TYPE])
        assert subs["T"] == STRING_TYPE

    def test_two_distinct_type_params_inferred(self):
        func_def = _make_generic_func(
            "pair", ["T", "U"], [("first", "T"), ("second", "U")]
        )
        subs = self.tc._infer_generic_substitutions(func_def, [INTEGER_TYPE, STRING_TYPE])
        assert subs["T"] == INTEGER_TYPE
        assert subs["U"] == STRING_TYPE

    def test_repeated_type_param_consistent(self):
        func_def = _make_generic_func("max_val", ["T"], [("a", "T"), ("b", "T")])
        subs = self.tc._infer_generic_substitutions(func_def, [INTEGER_TYPE, INTEGER_TYPE])
        assert subs["T"] == INTEGER_TYPE

    def test_repeated_type_param_conflicting_emits_error(self):
        func_def = _make_generic_func("max_val", ["T"], [("a", "T"), ("b", "T")])
        self.tc._infer_generic_substitutions(func_def, [INTEGER_TYPE, STRING_TYPE])
        assert any("Conflicting types" in e for e in self.tc.errors)

    def test_non_generic_param_not_added_to_substitutions(self):
        func_def = _make_generic_func("fixed", ["T"], [("x", "T"), ("y", "Integer")])
        subs = self.tc._infer_generic_substitutions(func_def, [FLOAT_TYPE, INTEGER_TYPE])
        assert "T" in subs
        assert subs["T"] == FLOAT_TYPE
        # "Integer" is not a type-param name — should not appear
        assert "Integer" not in subs

    def test_empty_type_params_returns_empty(self):
        func_def = FunctionDefinition(
            "no_generics", [Parameter("x", "Integer")], body=[], type_parameters=[]
        )
        subs = self.tc._infer_generic_substitutions(func_def, [INTEGER_TYPE])
        assert subs == {}


# ---------------------------------------------------------------------------
# Tests for _type_satisfies_constraint
# ---------------------------------------------------------------------------

class TestTypeSatisfiesConstraint:
    """Unit tests for TypeChecker._type_satisfies_constraint."""

    def setup_method(self):
        self.tc = TypeChecker(enable_ownership_passes=False)

    def test_integer_satisfies_comparable(self):
        assert self.tc._type_satisfies_constraint(INTEGER_TYPE, "Comparable")

    def test_float_satisfies_comparable(self):
        assert self.tc._type_satisfies_constraint(FLOAT_TYPE, "Comparable")

    def test_string_satisfies_comparable(self):
        assert self.tc._type_satisfies_constraint(STRING_TYPE, "Comparable")

    def test_integer_satisfies_equatable(self):
        assert self.tc._type_satisfies_constraint(INTEGER_TYPE, "Equatable")

    def test_integer_satisfies_numeric(self):
        assert self.tc._type_satisfies_constraint(INTEGER_TYPE, "Numeric")

    def test_float_satisfies_numeric(self):
        assert self.tc._type_satisfies_constraint(FLOAT_TYPE, "Numeric")

    def test_string_does_not_satisfy_numeric(self):
        assert not self.tc._type_satisfies_constraint(STRING_TYPE, "Numeric")

    def test_any_type_satisfies_everything(self):
        assert self.tc._type_satisfies_constraint(ANY_TYPE, "Comparable")
        assert self.tc._type_satisfies_constraint(ANY_TYPE, "Numeric")
        assert self.tc._type_satisfies_constraint(ANY_TYPE, "SomeUnknownTrait")

    def test_class_type_satisfies_registered_trait(self):
        self.tc.trait_methods["Printable"] = {"to_string"}
        cls = ClassType("MyClass", {}, {"to_string": None})
        assert self.tc._type_satisfies_constraint(cls, "Printable")

    def test_class_type_fails_registered_trait_missing_method(self):
        self.tc.trait_methods["Serializable"] = {"serialize", "deserialize"}
        cls = ClassType("Partial", {}, {"serialize": None})
        assert not self.tc._type_satisfies_constraint(cls, "Serializable")

    def test_class_type_satisfies_via_parent_class(self):
        cls = ClassType("Dog", {}, {}, parent_classes=["Comparable"])
        assert self.tc._type_satisfies_constraint(cls, "Comparable")

    def test_list_type_satisfies_iterable(self):
        lst = ListType(INTEGER_TYPE)
        assert self.tc._type_satisfies_constraint(lst, "Iterable")


# ---------------------------------------------------------------------------
# Tests for _check_generic_constraints (full constraint propagation)
# ---------------------------------------------------------------------------

class TestCheckGenericConstraints:
    """Integration tests for generic constraint propagation at call sites."""

    def setup_method(self):
        self.tc = TypeChecker(enable_ownership_passes=False)

    def test_no_constraints_emits_no_errors(self):
        func_def = _make_generic_func("wrap", ["T"], [("x", "T")], constraints={})
        self.tc._check_generic_constraints("wrap", {"T": INTEGER_TYPE}, func_def)
        assert self.tc.errors == []

    def test_satisfied_comparable_constraint_no_error(self):
        func_def = _make_generic_func(
            "max_val", ["T"], [("a", "T"), ("b", "T")],
            constraints={"T": ["Comparable"]}
        )
        self.tc._check_generic_constraints("max_val", {"T": INTEGER_TYPE}, func_def)
        assert self.tc.errors == []

    def test_satisfied_comparable_constraint_string(self):
        func_def = _make_generic_func(
            "sort_key", ["T"], [("x", "T")], constraints={"T": ["Comparable"]}
        )
        self.tc._check_generic_constraints("sort_key", {"T": STRING_TYPE}, func_def)
        assert self.tc.errors == []

    def test_unsatisfied_numeric_constraint_emits_error(self):
        func_def = _make_generic_func(
            "sum_vals", ["T"], [("x", "T")], constraints={"T": ["Numeric"]}
        )
        self.tc._check_generic_constraints("sum_vals", {"T": STRING_TYPE}, func_def)
        assert any("Numeric" in e and "sum_vals" in e for e in self.tc.errors)

    def test_unsatisfied_constraint_names_type_and_param(self):
        func_def = _make_generic_func(
            "calc", ["T"], [("x", "T")], constraints={"T": ["Numeric"]}
        )
        self.tc._check_generic_constraints("calc", {"T": BOOLEAN_TYPE}, func_def)
        errors = self.tc.errors
        assert any(
            "Boolean" in e or "boolean" in e.lower()
            for e in errors
        )
        assert any("T" in e for e in errors)

    def test_multiple_constraints_all_satisfied(self):
        func_def = _make_generic_func(
            "process", ["T"], [("x", "T")],
            constraints={"T": ["Comparable", "Equatable"]}
        )
        self.tc._check_generic_constraints("process", {"T": INTEGER_TYPE}, func_def)
        assert self.tc.errors == []

    def test_multiple_constraints_one_fails(self):
        func_def = _make_generic_func(
            "process", ["T"], [("x", "T")],
            constraints={"T": ["Numeric", "Comparable"]}
        )
        # BOOLEAN_TYPE fails Numeric but passes Comparable
        self.tc._check_generic_constraints("process", {"T": BOOLEAN_TYPE}, func_def)
        assert any("Numeric" in e for e in self.tc.errors)

    def test_class_constraint_via_trait_methods(self):
        self.tc.trait_methods["Printable"] = {"to_string"}
        cls = ClassType("Doc", {}, {"to_string": None})
        func_def = _make_generic_func(
            "render", ["T"], [("x", "T")], constraints={"T": ["Printable"]}
        )
        self.tc._check_generic_constraints("render", {"T": cls}, func_def)
        assert self.tc.errors == []

    def test_class_constraint_fails_missing_method(self):
        self.tc.trait_methods["Printable"] = {"to_string"}
        cls = ClassType("Opaque", {}, {})
        func_def = _make_generic_func(
            "render", ["T"], [("x", "T")], constraints={"T": ["Printable"]}
        )
        self.tc._check_generic_constraints("render", {"T": cls}, func_def)
        assert any("Printable" in e for e in self.tc.errors)

    def test_unresolved_param_skipped(self):
        func_def = _make_generic_func(
            "unresolved", ["T"], [("x", "T")], constraints={"T": ["Numeric"]}
        )
        # T is not in substitutions — should not raise, just skip
        self.tc._check_generic_constraints("unresolved", {}, func_def)
        assert self.tc.errors == []


# ---------------------------------------------------------------------------
# End-to-end: constraint propagation wired into check_function_call
# ---------------------------------------------------------------------------

class TestGenericConstraintEndToEnd:
    """Verify that check_function_call runs constraint checks for generic calls."""

    def _build_program_with_generic_fn(self, constraints: dict) -> tuple:
        """Return (TypeChecker, Program, call_node) for a generic function."""
        tc = TypeChecker(enable_ownership_passes=False)

        func_def = _make_generic_func(
            "max_val",
            ["T"],
            [("a", "T"), ("b", "T")],
            return_type="T",
            constraints=constraints,
        )

        # Register the function definition the same way the typechecker does
        from nexuslang.typesystem.types import GenericParameter, FunctionType as FT
        generic_param = GenericParameter("T")
        fn_type = FT([generic_param, generic_param], generic_param)
        fn_type.has_defaults = False
        fn_type.min_params = 2
        fn_type.variadic = False
        fn_type.variadic_index = -1
        tc.env.define_function("max_val", fn_type)
        # Register as generic function template
        tc.generic_functions["max_val"] = func_def

        return tc

    def test_integer_passes_comparable_constraint(self):
        tc = self._build_program_with_generic_fn({"T": ["Comparable"]})
        call = _make_call("max_val", Literal("integer", 1), Literal("integer", 2))
        tc.check_function_call(call, tc.env)
        assert tc.errors == []

    def test_string_fails_numeric_constraint(self):
        tc = self._build_program_with_generic_fn({"T": ["Numeric"]})
        call = _make_call("max_val", Literal("string", "a"), Literal("string", "b"))
        tc.check_function_call(call, tc.env)
        assert any("Numeric" in e for e in tc.errors)

    def test_float_passes_numeric_constraint(self):
        tc = self._build_program_with_generic_fn({"T": ["Numeric"]})
        call = _make_call("max_val", Literal("float", 1.5), Literal("float", 2.5))
        tc.check_function_call(call, tc.env)
        assert tc.errors == []

    def test_unconstrained_generic_always_passes(self):
        tc = self._build_program_with_generic_fn({})
        call = _make_call("max_val", Literal("string", "x"), Literal("string", "y"))
        tc.check_function_call(call, tc.env)
        assert tc.errors == []


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

