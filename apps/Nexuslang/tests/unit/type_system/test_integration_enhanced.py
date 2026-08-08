"""Focused regression coverage for the enhanced type system integration facade."""

from nexuslang.parser.ast import (
    ClassDefinition,
    FunctionCall,
    FunctionDefinition,
    Literal,
    MethodDefinition,
    Parameter,
    PropertyDeclaration,
)
from nexuslang.typesystem.integration_enhanced import (
    IntegratedTypeSystem,
    get_type_system,
    reset_type_system,
)
from nexuslang.typesystem.types import (
    BOOLEAN_TYPE,
    ClassType,
    FLOAT_TYPE,
    FunctionType,
    INTEGER_TYPE,
)


class TestIntegratedTypeSystem:
    def test_infer_function_signature_builds_complete_function_type(self):
        system = IntegratedTypeSystem()
        func_def = FunctionDefinition(
            "add",
            [Parameter("left", "Integer"), Parameter("right", "Integer")],
            [Literal("integer", 0)],
            "Float",
        )

        signature = system.infer_function_signature(func_def)

        assert isinstance(signature, FunctionType)
        assert signature.param_types == [INTEGER_TYPE, INTEGER_TYPE]
        assert signature.return_type == FLOAT_TYPE

    def test_register_class_type_infers_method_signatures(self):
        system = IntegratedTypeSystem()
        system.type_registry.register_type(ClassType("BaseMetric", {}, {}, []))
        class_def = ClassDefinition(
            "Metric",
            properties=[PropertyDeclaration("value", "Integer")],
            methods=[
                MethodDefinition(
                    "is_positive",
                    [Parameter("threshold", "Integer")],
                    [],
                    "Boolean",
                )
            ],
            parent_classes=["BaseMetric"],
        )

        class_type = system.register_class_type(class_def)

        assert class_type.properties["value"] == INTEGER_TYPE
        assert class_type.methods["is_positive"] == FunctionType([INTEGER_TYPE], BOOLEAN_TYPE)
        assert system.is_subtype("Metric", "BaseMetric")

    def test_check_function_call_reports_arity_and_type_errors(self):
        system = IntegratedTypeSystem()
        func_type = FunctionType([INTEGER_TYPE], BOOLEAN_TYPE)

        wrong_arity = FunctionCall("check", [Literal("integer", 1), Literal("integer", 2)])
        wrong_type = FunctionCall("check", [Literal("string", "oops")])

        assert system.check_function_call(wrong_arity, func_type) == (
            False,
            "Function expects 1 arguments, got 2",
        )

        valid, error = system.check_function_call(wrong_type, func_type)
        assert valid is False
        assert "Argument 1" in error

    def test_create_generic_context_and_reset_clear_state(self):
        system = IntegratedTypeSystem()
        context = system.create_generic_context("map", ["T"], {"T": ["Any"]})
        system.type_environment["x"] = INTEGER_TYPE

        assert "map" in system.generic_contexts
        assert context is system.generic_contexts["map"]

        system.reset()

        assert system.type_environment == {}
        assert system.generic_contexts == {}


class TestIntegratedTypeSystemSingleton:
    def test_get_type_system_reuses_global_instance_until_reset(self):
        reset_type_system()
        first = get_type_system(enable_type_checking=False)
        second = get_type_system(enable_type_checking=True)

        assert first is second
        assert first.type_checker is None

        reset_type_system()
        third = get_type_system(enable_type_checking=True)

        assert third is not first
        assert third.type_checker is not None
