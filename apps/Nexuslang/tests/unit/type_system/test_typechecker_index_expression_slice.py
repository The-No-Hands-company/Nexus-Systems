"""Focused coverage for typechecker.check_index_expression branches."""

from nexuslang.parser.ast import Identifier, IndexExpression, Literal
from nexuslang.typesystem.typechecker import TypeChecker, TypeEnvironment
from nexuslang.typesystem.types import ANY_TYPE, DictionaryType, INTEGER_TYPE, ListType, STRING_TYPE


def _checker() -> TypeChecker:
    return TypeChecker(enable_ownership_passes=False)


def _env() -> TypeEnvironment:
    return TypeEnvironment()


class TestIndexExpressionSlice:
    def test_inference_fast_path_returns_inferred_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_index_expression_type = lambda expr, variables: STRING_TYPE

        expr = IndexExpression(Identifier("arr"), Literal("integer", 0))
        result = checker.check_index_expression(expr, env)

        assert result == STRING_TYPE

    def test_list_index_returns_element_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_index_expression_type = lambda expr, variables: ANY_TYPE
        env.define_variable("arr", ListType(INTEGER_TYPE))

        expr = IndexExpression(Identifier("arr"), Literal("integer", 0))
        result = checker.check_index_expression(expr, env)

        assert result == INTEGER_TYPE
        assert checker.errors == []

    def test_list_index_type_mismatch_reports_error(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_index_expression_type = lambda expr, variables: ANY_TYPE
        env.define_variable("arr", ListType(INTEGER_TYPE))

        expr = IndexExpression(Identifier("arr"), Literal("string", "bad"))
        result = checker.check_index_expression(expr, env)

        assert result == INTEGER_TYPE
        assert any("List index must be Integer" in error for error in checker.errors)

    def test_dictionary_key_mismatch_reports_error_and_returns_value_type(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_index_expression_type = lambda expr, variables: ANY_TYPE
        env.define_variable("d", DictionaryType(STRING_TYPE, INTEGER_TYPE))

        expr = IndexExpression(Identifier("d"), Literal("integer", 1))
        result = checker.check_index_expression(expr, env)

        assert result == INTEGER_TYPE
        assert any("Dictionary key must be" in error for error in checker.errors)

    def test_string_index_type_mismatch_reports_error(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_index_expression_type = lambda expr, variables: ANY_TYPE
        env.define_variable("s", STRING_TYPE)

        expr = IndexExpression(Identifier("s"), Literal("string", "x"))
        result = checker.check_index_expression(expr, env)

        assert result == STRING_TYPE
        assert any("String index must be Integer" in error for error in checker.errors)

    def test_non_indexable_type_reports_error_and_returns_inferred_any(self):
        checker = _checker()
        env = _env()
        checker.type_inference.infer_index_expression_type = lambda expr, variables: ANY_TYPE
        env.define_variable("n", INTEGER_TYPE)

        expr = IndexExpression(Identifier("n"), Literal("integer", 0))
        result = checker.check_index_expression(expr, env)

        assert result == ANY_TYPE
        assert any("Cannot index into type" in error for error in checker.errors)