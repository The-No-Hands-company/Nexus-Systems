"""Focused branch coverage for TypeInferenceEngine helper paths."""

from nexuslang.parser.ast import FunctionCall, Identifier, IdentifierPattern, Literal
from nexuslang.typesystem.type_inference import TypeInferenceEngine
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    INTEGER_TYPE,
    STRING_TYPE,
    ClassType,
    DictionaryType,
    FunctionType,
    ListType,
    NULL_TYPE,
)


def _member_access(object_expr, member_name, is_method_call=False):
    return type(
        "MemberAccess",
        (),
        {
            "node_type": "member_access",
            "object_expr": object_expr,
            "member_name": member_name,
            "is_method_call": is_method_call,
        },
    )()


def _index_expression(array_expr, index_expr):
    return type(
        "IndexExpression",
        (),
        {
            "node_type": "index_expression",
            "array_expr": array_expr,
            "index_expr": index_expr,
        },
    )()


class TestTypeInferenceMemberAndIndexHelpers:
    def setup_method(self):
        self.engine = TypeInferenceEngine()

    def test_member_access_on_class_property_and_method(self):
        method_type = FunctionType([INTEGER_TYPE], STRING_TYPE)
        class_type = ClassType(
            "Widget",
            {"count": INTEGER_TYPE},
            {"format": method_type},
            [],
        )
        env = {"widget": class_type}

        widget = Identifier("widget")

        assert self.engine.infer_member_access_type(_member_access(widget, "count"), env) is INTEGER_TYPE
        assert self.engine.infer_member_access_type(_member_access(widget, "format"), env) is method_type
        assert self.engine.infer_member_access_type(_member_access(widget, "format", True), env) is STRING_TYPE

    def test_member_access_on_collections_and_string(self):
        env = {
            "items": ListType(BOOLEAN_TYPE),
            "mapping": DictionaryType(STRING_TYPE, FLOAT_TYPE),
            "label": STRING_TYPE,
        }

        items = Identifier("items")
        mapping = Identifier("mapping")
        label = Identifier("label")

        assert self.engine.infer_member_access_type(_member_access(items, "length"), env) is INTEGER_TYPE
        assert self.engine.infer_member_access_type(_member_access(items, "first"), env) is BOOLEAN_TYPE
        append_type = self.engine.infer_member_access_type(_member_access(items, "append"), env)
        assert isinstance(append_type, FunctionType)
        assert append_type.param_types[0] is BOOLEAN_TYPE
        assert append_type.return_type is NULL_TYPE
        assert self.engine.infer_member_access_type(_member_access(mapping, "keys"), env) == ListType(STRING_TYPE)
        assert self.engine.infer_member_access_type(_member_access(mapping, "values"), env) == ListType(FLOAT_TYPE)
        assert self.engine.infer_member_access_type(_member_access(label, "upper"), env) is STRING_TYPE
        assert self.engine.infer_member_access_type(_member_access(label, "split"), env) == FunctionType([STRING_TYPE], ListType(STRING_TYPE))

    def test_member_access_unknown_object_and_missing_member_fall_back_to_any(self):
        env = {"unknown": ANY_TYPE}
        assert self.engine.infer_member_access_type(_member_access(Identifier("unknown"), "whatever"), env) is ANY_TYPE
        class_type = ClassType("Widget", {}, {}, [])
        env = {"widget": class_type}
        assert self.engine.infer_member_access_type(_member_access(Identifier("widget"), "missing"), env) is ANY_TYPE

    def test_index_expression_for_list_dictionary_and_string(self):
        env = {
            "items": ListType(INTEGER_TYPE),
            "mapping": DictionaryType(STRING_TYPE, FLOAT_TYPE),
            "label": STRING_TYPE,
        }

        assert self.engine.infer_index_expression_type(_index_expression(Identifier("items"), Literal("integer", 0)), env) is INTEGER_TYPE
        assert self.engine.infer_index_expression_type(_index_expression(Identifier("mapping"), Literal("string", "key")), env) is FLOAT_TYPE
        assert self.engine.infer_index_expression_type(_index_expression(Identifier("label"), Literal("integer", 1)), env) is STRING_TYPE
        assert self.engine.infer_index_expression_type(_index_expression(Identifier("missing"), Literal("integer", 1)), env) is ANY_TYPE


class TestTypeInferenceNestedCallsAndArguments:
    def setup_method(self):
        self.engine = TypeInferenceEngine()

    def test_infer_nested_call_type_uses_function_signature(self):
        func_type = FunctionType([INTEGER_TYPE, STRING_TYPE], BOOLEAN_TYPE)
        env = {"combine": func_type}
        call = FunctionCall("combine", [Literal("integer", 1), Literal("string", "x")])

        inferred = self.engine.infer_nested_call_type(call, env)

        assert inferred is BOOLEAN_TYPE

    def test_infer_nested_call_type_uses_cached_return_type_for_unknown_environment(self):
        self.engine.function_return_types["cached"] = STRING_TYPE
        call = FunctionCall("cached", [])

        assert self.engine.infer_nested_call_type(call, {}) is STRING_TYPE

    def test_infer_argument_types_from_function_uses_expected_types(self):
        function_type = FunctionType([INTEGER_TYPE, ListType(STRING_TYPE)], BOOLEAN_TYPE)
        arguments = [Literal("integer", 4), type("ListLiteral", (), {"node_type": "list_literal", "elements": [Literal("string", "a")]} )()]

        inferred = self.engine.infer_argument_types_from_function(function_type, arguments, {})

        assert inferred[0] is INTEGER_TYPE
        assert inferred[1] == ListType(STRING_TYPE)

    def test_create_generic_context_preserves_parameter_names_and_constraints(self):
        context = self.engine.create_generic_context(
            "wrap",
            ["T", "U"],
            {"T": ["Comparable"], "U": ["Printable"]},
        )

        assert set(context.type_parameters) == {"T", "U"}
        assert len(context.constraints) == 2
        assert context.constraints[0].parameter in {"T", "U"}

    def test_infer_lambda_body_type_without_returns_defaults_to_null_or_expected(self):
        assert self.engine._infer_lambda_body_type([], None, {}) is NULL_TYPE
        assert self.engine._infer_lambda_body_type([], STRING_TYPE, {}) is STRING_TYPE


class TestTypeInferencePatternBindings:
    def setup_method(self):
        self.engine = TypeInferenceEngine()

    def test_option_result_and_list_pattern_bindings(self):
        option_type = type("OptionType", (), {"name": "Option", "type_parameters": [INTEGER_TYPE]})()
        result_type = type("ResultType", (), {"name": "Result", "type_parameters": [STRING_TYPE, FLOAT_TYPE]})()

        option_pattern = type("OptionPattern", (), {"variant": "Some", "binding": "value", "binding_type_annotation": None})()
        result_ok_pattern = type("ResultPattern", (), {"variant": "Ok", "binding": "ok", "binding_type_annotation": None})()
        result_err_pattern = type("ResultPattern", (), {"variant": "Err", "binding": "err", "binding_type_annotation": None})()
        list_pattern = type("ListPattern", (), {"elements": [IdentifierPattern("head")], "rest": "tail"})()

        option_bindings = self.engine._infer_option_pattern_bindings(option_pattern, option_type)
        result_ok_bindings = self.engine._infer_result_pattern_bindings(result_ok_pattern, result_type)
        result_err_bindings = self.engine._infer_result_pattern_bindings(result_err_pattern, result_type)
        list_bindings = self.engine._infer_list_pattern_bindings(list_pattern, ListType(STRING_TYPE))

        assert option_bindings["value"] is INTEGER_TYPE
        assert result_ok_bindings["ok"] is STRING_TYPE
        assert result_err_bindings["err"] is FLOAT_TYPE
        assert list_bindings["head"] is STRING_TYPE
        assert list_bindings["tail"] == ListType(STRING_TYPE)
