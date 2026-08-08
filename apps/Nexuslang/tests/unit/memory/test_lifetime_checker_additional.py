"""Focused branch coverage for the lifetime checker."""

from nexuslang.parser.ast import (
    AsyncFunctionDefinition,
    BorrowExpression,
    BorrowExpressionWithLifetime,
    FunctionDefinition,
    IdentifierPattern,
    LifetimeAnnotation,
    MatchCase,
    MatchExpression,
    Parameter,
    Program,
    ReturnStatement,
    ReturnTypeWithLifetime,
    TryCatch,
    Literal,
)
from nexuslang.typesystem.lifetime_checker import LifetimeChecker


def _check(program):
    return LifetimeChecker().check(program)


def _borrow_with_lifetime(var_name, label, mutable=False):
    return BorrowExpressionWithLifetime(var_name, mutable, LifetimeAnnotation(label))


def _borrow_param(name, label):
    return Parameter(name, ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation(label)))


class TestLifetimeCheckerAdditionalBranches:
    def test_empty_lifetime_label_is_rejected(self):
        program = Program([_borrow_with_lifetime("x", "")])

        errors = _check(program)

        assert any("cannot be empty" in str(error).lower() for error in errors)

    def test_async_function_definition_uses_same_lifetime_rules(self):
        async_fn = AsyncFunctionDefinition(
            "async_echo",
            [_borrow_param("x", "outer")],
            [ReturnStatement(_borrow_with_lifetime("x", "inner"))],
            ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation("outer")),
        )

        errors = _check(Program([async_fn]))

        assert any("inner" in str(error) for error in errors)

    def test_try_catch_recurses_into_nested_borrow_expressions(self):
        function_def = FunctionDefinition(
            "try_wrap",
            [_borrow_param("x", "outer")],
            [
                TryCatch(
                    [BorrowExpressionWithLifetime("x", False, LifetimeAnnotation("inner"))],
                    [],
                )
            ],
            ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation("outer")),
        )

        errors = _check(Program([function_def]))

        assert any("inner" in str(error) for error in errors)

    def test_match_expression_recurses_into_case_bodies(self):
        function_def = FunctionDefinition(
            "match_wrap",
            [_borrow_param("x", "outer")],
            [
                MatchExpression(
                    Literal("integer", 1),
                    [
                        MatchCase(
                            IdentifierPattern("value"),
                            [BorrowExpressionWithLifetime("x", False, LifetimeAnnotation("inner"))],
                        )
                    ],
                )
            ],
            ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation("outer")),
        )

        errors = _check(Program([function_def]))

        assert any("inner" in str(error) for error in errors)

    def test_plain_borrow_inside_return_statement_records_missing_lifetime(self):
        function_def = FunctionDefinition(
            "plain_return",
            [_borrow_param("x", "outer")],
            [ReturnStatement(BorrowExpression("x"))],
            ReturnTypeWithLifetime("String", lifetime=LifetimeAnnotation("outer")),
        )

        errors = _check(Program([function_def]))

        assert any("without a lifetime annotation" in str(error).lower() for error in errors)
