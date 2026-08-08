"""Focused coverage for typechecker.py function-definition handling."""

from nexuslang.parser.ast import FunctionDefinition, Literal, Parameter
from nexuslang.typesystem.generics_system import TypeParameterInfo
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    INTEGER_TYPE,
    STRING_TYPE,
    FunctionType,
    GenericParameter,
    ListType,
)


def _checker() -> TypeChecker:
    return TypeChecker()


def _env() -> TypeEnvironment:
    return TypeEnvironment()


class _LegacyConstraint:
    def __init__(self, type_parameter: str, constraint_type: str):
        self.type_parameter = type_parameter
        self.constraint_type = constraint_type


class TestFunctionDefinitionGenerics:
    def test_generic_function_registers_template_and_generic_types(self):
        checker = _checker()
        env = _env()
        env.enter_generic_scope([TypeParameterInfo("T", [], None)])

        definition = FunctionDefinition(
            name="identity",
            parameters=[Parameter("value", "T")],
            body=[],
            return_type="T",
            type_parameters=["T"],
            type_constraints={"T": ["Comparable", "Printable"]},
        )

        function_type = checker.check_function_definition(definition, env)

        assert checker.generic_functions["identity"] is definition
        assert isinstance(function_type, FunctionType)
        assert isinstance(function_type.param_types[0], GenericParameter)
        assert function_type.param_types[0].name == "T"
        assert isinstance(function_type.return_type, GenericParameter)
        assert function_type.return_type.name == "T"
        assert env.generic_context is None

    def test_generic_function_supports_legacy_constraint_objects(self):
        checker = _checker()
        env = _env()
        env.enter_generic_scope([TypeParameterInfo("T", [], None)])

        definition = FunctionDefinition(
            name="legacy_generic",
            parameters=[Parameter("value", "T")],
            body=[],
            return_type="T",
            type_parameters=["T"],
            type_constraints=[_LegacyConstraint("T", "Integer")],
        )

        function_type = checker.check_function_definition(definition, env)

        assert checker.generic_functions["legacy_generic"] is definition
        assert isinstance(function_type.param_types[0], GenericParameter)
        assert function_type.param_types[0].name == "T"
        assert isinstance(function_type.return_type, GenericParameter)
        assert function_type.return_type.name == "T"


class TestFunctionDefinitionSignatureMetadata:
    def test_defaults_and_variadic_metadata_are_recorded(self):
        checker = _checker()
        env = _env()

        definition = FunctionDefinition(
            name="format_values",
            parameters=[
                Parameter("count", "Integer"),
                Parameter("label", "String", default_value=Literal("string", "items")),
                Parameter("values", "Float", is_variadic=True),
            ],
            body=[],
            return_type="Boolean",
        )

        function_type = checker.check_function_definition(definition, env)

        assert function_type.has_defaults is True
        assert function_type.min_params == 1
        assert function_type.variadic is True
        assert function_type.variadic_index == 2
        assert function_type.param_types[0] == INTEGER_TYPE
        assert function_type.param_types[1] == STRING_TYPE
        assert isinstance(function_type.param_types[2], ListType)
        assert function_type.param_types[2].element_type == FLOAT_TYPE
        assert function_type.return_type == BOOLEAN_TYPE
        assert env.get_function_type("format_values") is function_type

    def test_untyped_variadic_parameter_defaults_to_list_of_any(self):
        checker = _checker()
        env = _env()

        definition = FunctionDefinition(
            name="collect",
            parameters=[
                Parameter("head", "Integer"),
                Parameter("rest", None, is_variadic=True),
            ],
            body=[],
            return_type="Integer",
        )

        function_type = checker.check_function_definition(definition, env)

        assert function_type.min_params == 1
        assert function_type.variadic is True
        assert function_type.variadic_index == 1
        assert isinstance(function_type.param_types[1], ListType)
        assert function_type.param_types[1].element_type == ANY_TYPE


class TestFunctionDefinitionReturnInference:
    def test_missing_return_annotation_uses_inferred_return_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_function_return_type = lambda definition, variables: STRING_TYPE

        definition = FunctionDefinition(
            name="infer_me",
            parameters=[],
            body=[],
            return_type=None,
        )

        function_type = checker.check_function_definition(definition, env)

        assert function_type.return_type == STRING_TYPE

    def test_missing_return_annotation_keeps_any_when_inference_is_any(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_function_return_type = lambda definition, variables: ANY_TYPE

        definition = FunctionDefinition(
            name="infer_any",
            parameters=[],
            body=[],
            return_type=None,
        )

        function_type = checker.check_function_definition(definition, env)

        assert function_type.return_type == ANY_TYPE