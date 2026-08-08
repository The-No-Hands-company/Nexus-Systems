"""Coverage slice for generator, yield, await, and yield-discovery helper branches."""

from types import SimpleNamespace

import pytest

from nexuslang.parser.ast import FunctionCall, Identifier, Literal
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import (
    ANY_TYPE,
    BOOLEAN_TYPE,
    FLOAT_TYPE,
    INTEGER_TYPE,
    NULL_TYPE,
    STRING_TYPE,
    AnyType,
    AwaitableType,
    ListType,
)


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


def _node(class_name: str, **attrs):
    cls = type(class_name, (), {})
    obj = cls()
    for key, value in attrs.items():
        setattr(obj, key, value)
    return obj


class TestGeneratorExpressionBranches:
    def test_generator_iterable_any_type_uses_any_element(self):
        checker = _checker()
        env = _env()

        gen = _node(
            "GeneratorExpression",
            iterable=Identifier("dynamic_iter"),
            target=Identifier("x"),
            condition=None,
            expr=Identifier("x"),
        )

        def _check_statement(expr, _):
            if expr is gen.iterable:
                return AnyType()
            return INTEGER_TYPE

        checker.check_statement = _check_statement

        result = checker.check_generator_expression(gen, env)

        assert isinstance(result, ListType)
        assert result.element_type == INTEGER_TYPE
        assert not checker.errors

    def test_generator_iterable_non_list_reports_error(self):
        checker = _checker()
        env = _env()

        gen = _node(
            "GeneratorExpression",
            iterable=Identifier("not_list"),
            target=Identifier("x"),
            condition=None,
            expr=Identifier("x"),
        )

        checker.check_statement = lambda expr, _: FLOAT_TYPE if expr is gen.iterable else INTEGER_TYPE

        result = checker.check_generator_expression(gen, env)

        assert isinstance(result, ListType)
        assert result.element_type == INTEGER_TYPE
        assert any("Generator iterable must be a list" in err for err in checker.errors)

    def test_generator_condition_non_boolean_reports_error(self):
        checker = _checker()
        env = _env()

        gen = _node(
            "GeneratorExpression",
            iterable=Identifier("items"),
            target=Identifier("x"),
            condition=Identifier("cond"),
            expr=Identifier("x"),
        )

        def _check_statement(expr, _):
            if expr is gen.iterable:
                return ListType(INTEGER_TYPE)
            if expr is gen.condition:
                return STRING_TYPE
            return INTEGER_TYPE

        checker.check_statement = _check_statement

        result = checker.check_generator_expression(gen, env)

        assert isinstance(result, ListType)
        assert result.element_type == INTEGER_TYPE
        assert any("Generator condition must be boolean" in err for err in checker.errors)

    def test_generator_without_identifier_target_skips_binding(self):
        checker = _checker()
        env = _env()

        gen = _node(
            "GeneratorExpression",
            iterable=Identifier("items"),
            target="not-identifier",
            condition=None,
            expr=Literal("integer", 1),
        )

        checker.check_statement = lambda expr, _: ListType(INTEGER_TYPE) if expr is gen.iterable else INTEGER_TYPE

        result = checker.check_generator_expression(gen, env)

        assert isinstance(result, ListType)
        assert result.element_type == INTEGER_TYPE
        with pytest.raises(Exception):
            env.get_variable_type("x")


class TestYieldExpressionBranches:
    def test_yield_outside_generator_reports_error_and_returns_value_type(self):
        checker = _checker()
        env = _env()
        node = _node("YieldExpression", value=Literal("integer", 7))

        result = checker.check_yield_expression(node, env)

        assert result == INTEGER_TYPE
        assert any("can only be used inside a function" in err for err in checker.errors)

    def test_yield_compatible_with_expected_type(self):
        checker = _checker()
        root = _env()
        root.is_generator_function = True
        root.expected_yield_type = INTEGER_TYPE

        inner = TypeEnvironment(parent=root)
        node = _node("YieldExpression", value=Literal("integer", 1))

        result = checker.check_yield_expression(node, inner)

        assert result == INTEGER_TYPE
        assert root.yielded_types == [INTEGER_TYPE]
        assert not checker.errors

    def test_yield_incompatible_with_expected_type_reports_error(self):
        checker = _checker()
        root = _env()
        root.is_generator_function = True
        root.expected_yield_type = INTEGER_TYPE

        inner = TypeEnvironment(parent=root)
        node = _node("YieldExpression", value=Literal("string", "oops"))

        result = checker.check_yield_expression(node, inner)

        assert result == STRING_TYPE
        assert root.yielded_types == [STRING_TYPE]
        assert any("is not compatible with generator element type" in err for err in checker.errors)

    def test_yield_with_none_value_returns_null_type(self):
        checker = _checker()
        root = _env()
        root.is_generator_function = True

        inner = TypeEnvironment(parent=root)
        node = _node("YieldExpression", value=None)

        result = checker.check_yield_expression(node, inner)

        assert result == NULL_TYPE
        assert root.yielded_types == [NULL_TYPE]


class TestAwaitExpressionBranches:
    def test_await_on_awaitable_returns_payload(self):
        checker = _checker()
        env = _env()
        node = _node("AwaitExpression", expression=Identifier("task"))

        checker.check_statement = lambda expr, _: AwaitableType(STRING_TYPE)

        result = checker.check_await_expression(node, env)

        assert result == STRING_TYPE

    def test_await_on_any_returns_any(self):
        checker = _checker()
        env = _env()
        node = _node("AwaitExpression", expression=Identifier("task"))

        checker.check_statement = lambda expr, _: AnyType()

        result = checker.check_await_expression(node, env)

        assert result == ANY_TYPE

    def test_await_direct_function_call_returns_operand_type(self):
        checker = _checker()
        env = _env()
        call = FunctionCall("compute", arguments=[])
        node = _node("AwaitExpression", expression=call)

        checker.check_statement = lambda expr, _: FLOAT_TYPE

        result = checker.check_await_expression(node, env)

        assert result == FLOAT_TYPE

    def test_await_non_awaitable_non_call_reports_error(self):
        checker = _checker()
        env = _env()
        node = _node("AwaitExpression", expression=Identifier("value"))

        checker.check_statement = lambda expr, _: INTEGER_TYPE

        result = checker.check_await_expression(node, env)

        assert result == ANY_TYPE
        assert any("await expects a task or awaitable value" in err for err in checker.errors)

    def test_await_uses_expr_attribute_fallback(self):
        checker = _checker()
        env = _env()
        node = _node("AwaitExpression", expr=Identifier("task"))

        checker.check_statement = lambda expr, _: AwaitableType(INTEGER_TYPE)

        result = checker.check_await_expression(node, env)

        assert result == INTEGER_TYPE


class TestYieldDiscoveryHelpers:
    def test_statement_contains_yield_detects_nested_dict_values(self):
        checker = _checker()
        payload = {"k": _node("YieldExpression")}

        assert checker._statement_contains_yield(payload) is True

    def test_statement_contains_yield_ignores_nested_function_scope(self):
        checker = _checker()
        nested = _node("FunctionDefinition", body=[_node("YieldExpression")])

        assert checker._statement_contains_yield(nested) is False

    def test_statement_contains_yield_walks_object_attrs(self):
        checker = _checker()
        wrapper = SimpleNamespace(inner=[SimpleNamespace(x=1), _node("YieldExpression")])

        assert checker._statement_contains_yield(wrapper) is True


class TestInferGeneratorYieldTypeBranches:
    def test_infer_generator_yield_type_with_any_short_circuits_to_any(self):
        checker = _checker()

        inferred = checker._infer_generator_yield_type([INTEGER_TYPE, ANY_TYPE, STRING_TYPE], "g")

        assert inferred == ANY_TYPE
        assert not checker.errors

    def test_infer_generator_yield_type_incompatible_types_reports_error(self):
        checker = _checker()

        inferred = checker._infer_generator_yield_type([INTEGER_TYPE, STRING_TYPE], "g")

        assert inferred == ANY_TYPE
        assert any("incompatible yield types" in err for err in checker.errors)

    def test_infer_generator_yield_type_compatible_numeric_types(self):
        checker = _checker()

        inferred = checker._infer_generator_yield_type([INTEGER_TYPE, FLOAT_TYPE], "g")

        assert inferred in (INTEGER_TYPE, FLOAT_TYPE, ANY_TYPE)
