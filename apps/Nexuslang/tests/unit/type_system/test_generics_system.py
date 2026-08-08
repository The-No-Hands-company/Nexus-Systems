import pytest

from nexuslang.typesystem.generics_system import (
    GenericContext,
    GenericTypeInference,
    Monomorphizer,
    TypeConstraint,
    TypeParameterInfo,
    create_dict_type,
    create_list_type,
)
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    ClassType,
    DictionaryType,
    FunctionType,
    GenericParameter,
    INTEGER_TYPE,
    ListType,
    PrimitiveType,
    STRING_TYPE,
)


class TestTypeParameterInfoAndConstraint:
    def test_constraint_and_parameter_string_forms(self):
        constraint = TypeConstraint("T", INTEGER_TYPE, "subtype")
        param_without_constraints = TypeParameterInfo("T", [], "invariant")
        param_with_constraints = TypeParameterInfo("U", [constraint], "covariant")

        assert str(constraint).startswith("T is subtype")
        assert str(param_without_constraints) == "T"
        assert str(param_with_constraints).startswith("U where T is subtype")


class TestGenericContext:
    def test_add_substitute_and_resolve_type_paths(self):
        parent = GenericContext()
        child = GenericContext(parent)

        comparable_constraint = TypeConstraint("T", INTEGER_TYPE, "subtype")
        interface_constraint = TypeConstraint("I", ClassType("Renderable", {}, {}), "interface")
        parent.add_type_parameter(TypeParameterInfo("T", [comparable_constraint], "invariant"))
        child.add_type_parameter(TypeParameterInfo("I", [interface_constraint], "invariant"))

        widget = ClassType("Widget", {}, {}, parent_classes=["Renderable"])
        parent.substitute_type("T", INTEGER_TYPE)
        child.substitute_type("I", widget)

        assert child.resolve_type(GenericParameter("T")) is INTEGER_TYPE
        assert child.resolve_type(ListType(GenericParameter("T"))).element_type is INTEGER_TYPE
        resolved_dict = child.resolve_type(DictionaryType(GenericParameter("T"), GenericParameter("I")))
        assert resolved_dict.key_type is INTEGER_TYPE
        assert resolved_dict.value_type is widget
        resolved_fn = child.resolve_type(FunctionType([GenericParameter("T")], GenericParameter("I")))
        assert resolved_fn.param_types[0] is INTEGER_TYPE
        assert resolved_fn.return_type is widget

    def test_constraint_validation_and_parent_delegation(self):
        parent = GenericContext()
        child = GenericContext(parent)
        parent.add_type_parameter(TypeParameterInfo("T", [], "invariant"))
        child.add_type_parameter(TypeParameterInfo("U", [], "invariant"))

        parent.substitute_type("T", STRING_TYPE)
        child.substitute_type("U", BOOLEAN_TYPE)

        assert child.resolve_type(GenericParameter("T")) is STRING_TYPE
        assert child.resolve_type(GenericParameter("U")) is BOOLEAN_TYPE

        with pytest.raises(ValueError, match="Unknown type parameter: X"):
            child.substitute_type("X", INTEGER_TYPE)

    def test_constraint_failure_for_non_matching_subtype_and_interface(self):
        subtype_context = GenericContext()
        subtype_context.add_type_parameter(
            TypeParameterInfo("T", [TypeConstraint("T", INTEGER_TYPE, "subtype")], "invariant")
        )
        with pytest.raises(TypeError, match="does not satisfy constraint"):
            subtype_context.substitute_type("T", STRING_TYPE)

        interface_context = GenericContext()
        interface_context.add_type_parameter(
            TypeParameterInfo(
                "I",
                [TypeConstraint("I", ClassType("Renderable", {}, {}), "interface")],
                "invariant",
            )
        )
        with pytest.raises(TypeError, match="does not satisfy constraint"):
            interface_context.substitute_type("I", ClassType("Widget", {}, {}))

    def test_comparable_and_equatable_constraints(self):
        context = GenericContext()
        comparable = TypeParameterInfo(
            "C",
            [TypeConstraint("C", INTEGER_TYPE, "comparable")],
            "invariant",
        )
        equatable = TypeParameterInfo(
            "E",
            [TypeConstraint("E", INTEGER_TYPE, "equatable")],
            "invariant",
        )
        context.add_type_parameter(comparable)
        context.add_type_parameter(equatable)

        context.substitute_type("C", ClassType("Widget", {}, {}))
        context.substitute_type("E", ANY_TYPE)

        assert context.type_substitutions["C"].name == "Widget"
        assert context.type_substitutions["E"] is ANY_TYPE


class TestGenericTypeInference:
    def test_infer_type_arguments_with_structure_and_defaults(self):
        inferrer = GenericTypeInference()
        params = [
            TypeParameterInfo("T", [], "invariant"),
            TypeParameterInfo("R", [], "invariant", default_type=BOOLEAN_TYPE),
        ]

        substitutions = inferrer.infer_type_arguments(
            params,
            [ListType(INTEGER_TYPE), DictionaryType(STRING_TYPE, INTEGER_TYPE)],
            [ListType(GenericParameter("T")), DictionaryType(STRING_TYPE, GenericParameter("R"))],
        )

        assert substitutions["T"] is INTEGER_TYPE
        assert substitutions["R"] is INTEGER_TYPE

    def test_infer_type_arguments_uses_default_and_raises_when_missing(self):
        inferrer = GenericTypeInference()
        default_only = [TypeParameterInfo("T", [], "invariant", default_type=BOOLEAN_TYPE)]

        substitutions = inferrer.infer_type_arguments(default_only, [], [])
        assert substitutions["T"] is BOOLEAN_TYPE

        params = [
            TypeParameterInfo("T", [], "invariant", default_type=BOOLEAN_TYPE),
            TypeParameterInfo("U", [], "invariant"),
        ]

        with pytest.raises(TypeError, match="Cannot infer type for parameter U"):
            inferrer.infer_type_arguments(params, [INTEGER_TYPE], [GenericParameter("T")])

    def test_unify_types_rejects_conflicting_bindings(self):
        inferrer = GenericTypeInference()
        substitutions = {"T": INTEGER_TYPE}

        with pytest.raises(TypeError, match="Type mismatch"):
            inferrer._unify_types(STRING_TYPE, GenericParameter("T"), substitutions)

        # Structured recursion reaches list/dictionary/function branches.
        substitutions = {}
        inferrer._unify_types(ListType(INTEGER_TYPE), ListType(GenericParameter("T")), substitutions)
        inferrer._unify_types(
            DictionaryType(STRING_TYPE, BOOLEAN_TYPE),
            DictionaryType(STRING_TYPE, GenericParameter("U")),
            substitutions,
        )
        inferrer._unify_types(
            FunctionType([INTEGER_TYPE], BOOLEAN_TYPE),
            FunctionType([GenericParameter("A")], GenericParameter("B")),
            substitutions,
        )
        assert substitutions["T"] is INTEGER_TYPE
        assert substitutions["U"] is BOOLEAN_TYPE
        assert substitutions["A"] is INTEGER_TYPE
        assert substitutions["B"] is BOOLEAN_TYPE


class TestMonomorphizer:
    def test_specialized_names_and_caching(self):
        monomorphizer = Monomorphizer()
        point = ClassType("Point", {}, {})

        list_name = monomorphizer.get_specialized_name(
            "Box",
            [INTEGER_TYPE, point, ListType(STRING_TYPE), DictionaryType(STRING_TYPE, INTEGER_TYPE)],
        )
        assert list_name == "Box_Integer_Point_List_String_Dict_String_Integer"
        assert monomorphizer.get_specialized_name("Box", [INTEGER_TYPE, point, ListType(STRING_TYPE), DictionaryType(STRING_TYPE, INTEGER_TYPE)]) == list_name
        assert monomorphizer.needs_specialization("Box", [INTEGER_TYPE, point]) is True
        monomorphizer.get_specialized_name("Box", [INTEGER_TYPE, point])
        assert monomorphizer.needs_specialization("Box", [INTEGER_TYPE, point]) is False

    def test_get_type_name_nested_structures(self):
        monomorphizer = Monomorphizer()
        nested = DictionaryType(ListType(STRING_TYPE), ListType(INTEGER_TYPE))

        assert monomorphizer._get_type_name(STRING_TYPE) == "String"
        assert monomorphizer._get_type_name(ListType(BOOLEAN_TYPE)) == "List_Boolean"
        assert monomorphizer._get_type_name(nested) == "Dict_List_String_List_Integer"


class TestGenericHelpers:
    def test_create_list_and_dict_helpers(self):
        list_type = create_list_type(INTEGER_TYPE)
        dict_type = create_dict_type(STRING_TYPE, INTEGER_TYPE)

        assert isinstance(list_type, ListType)
        assert list_type.element_type is INTEGER_TYPE
        assert isinstance(dict_type, DictionaryType)
        assert dict_type.key_type is STRING_TYPE
        assert dict_type.value_type is INTEGER_TYPE
