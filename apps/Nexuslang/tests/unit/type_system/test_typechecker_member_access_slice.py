"""Focused coverage for typechecker.check_member_access fallback branches."""

from types import SimpleNamespace

from nexuslang.parser.ast import Identifier
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import ANY_TYPE, ClassType, FunctionType, INTEGER_TYPE, STRING_TYPE


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _member_expr(object_expr, member_name, is_method_call=False):
    return SimpleNamespace(
        object_expr=object_expr,
        member_name=member_name,
        is_method_call=is_method_call,
    )


class TestMemberAccessSlice:
    def test_inferred_function_method_call_returns_return_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_member_access_type = (
            lambda expr, variables: FunctionType([INTEGER_TYPE], STRING_TYPE)
        )

        result = checker.check_member_access(
            _member_expr(Identifier("obj"), "convert", is_method_call=True),
            env,
        )

        assert result == STRING_TYPE

    def test_fallback_returns_class_property_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_member_access_type = lambda expr, variables: ANY_TYPE

        widget_type = ClassType("Widget", {"size": INTEGER_TYPE}, {})
        env.define_variable("widget", widget_type)

        result = checker.check_member_access(
            _member_expr(Identifier("widget"), "size", is_method_call=False),
            env,
        )

        assert result == INTEGER_TYPE

    def test_fallback_returns_method_type_for_method_reference(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_member_access_type = lambda expr, variables: ANY_TYPE

        method_type = FunctionType([INTEGER_TYPE], STRING_TYPE)
        widget_type = ClassType("Widget", {}, {"format": method_type})
        env.define_variable("widget", widget_type)

        result = checker.check_member_access(
            _member_expr(Identifier("widget"), "format", is_method_call=False),
            env,
        )

        assert result is method_type

    def test_fallback_method_call_with_non_function_method_type_returns_raw_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_member_access_type = lambda expr, variables: ANY_TYPE

        widget_type = ClassType("Widget", {}, {"status": STRING_TYPE})
        env.define_variable("widget", widget_type)

        result = checker.check_member_access(
            _member_expr(Identifier("widget"), "status", is_method_call=True),
            env,
        )

        assert result == STRING_TYPE

    def test_fallback_missing_member_appends_error_and_returns_any(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_member_access_type = lambda expr, variables: ANY_TYPE

        widget_type = ClassType("Widget", {}, {})
        env.define_variable("widget", widget_type)

        result = checker.check_member_access(
            _member_expr(Identifier("widget"), "missing", is_method_call=False),
            env,
        )

        assert result == ANY_TYPE
        assert any("Member 'missing' not found in class 'Widget'" in error for error in checker.errors)

    def test_fallback_non_class_object_returns_any(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_member_access_type = lambda expr, variables: ANY_TYPE
        env.define_variable("plain", INTEGER_TYPE)

        result = checker.check_member_access(
            _member_expr(Identifier("plain"), "whatever", is_method_call=False),
            env,
        )

        assert result == ANY_TYPE