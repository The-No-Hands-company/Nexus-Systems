"""Focused borrow checker branch coverage."""

from nexuslang.parser.ast import (
    BorrowExpression,
    ClassDefinition,
    Identifier,
    Literal,
    MatchCase,
    MatchExpression,
    MethodDefinition,
    MoveExpression,
    Parameter,
    Program,
    ReturnStatement,
    TryCatchBlock,
    VariableDeclaration,
    WhileLoop,
    ForLoop,
)
from nexuslang.typesystem.borrow_checker import BorrowChecker


def _check(program):
    return BorrowChecker().check(program)


def _list_literal(elements):
    return type("ListLiteral", (), {"elements": elements})()


class TestBorrowCheckerAdditionalBranches:
    def test_try_catch_block_merges_moved_state(self):
        program = Program([
            VariableDeclaration("x", Literal("integer", 1)),
            TryCatchBlock(
                [VariableDeclaration("y", MoveExpression("x"))],
                [],
            ),
            VariableDeclaration("z", Identifier("x")),
        ])

        errors = _check(program)

        assert any("moved" in str(error).lower() for error in errors)

    def test_method_definition_tracks_parameters_in_own_scope(self):
        method = MethodDefinition(
            "mutate",
            [Parameter("x", "Integer")],
            [
                VariableDeclaration("b", BorrowExpression("x")),
                VariableDeclaration("x", Literal("integer", 2)),
            ],
        )
        program = Program([ClassDefinition("Box", methods=[method])])

        errors = _check(program)

        assert any("borrowed" in str(error).lower() for error in errors)

    def test_while_loop_body_is_checked(self):
        program = Program([
            VariableDeclaration("x", Literal("integer", 1)),
            WhileLoop(
                Identifier("x"),
                [
                    VariableDeclaration("b", BorrowExpression("x")),
                    VariableDeclaration("x", Literal("integer", 3)),
                ],
            ),
        ])

        errors = _check(program)

        assert any("borrowed" in str(error).lower() for error in errors)

    def test_for_loop_body_is_checked(self):
        program = Program([
            VariableDeclaration("items", _list_literal([Literal("integer", 1)])),
            ForLoop(
                "item",
                iterable=Identifier("items"),
                body=[
                    VariableDeclaration("b", BorrowExpression("items")),
                    VariableDeclaration("items", Literal("integer", 2)),
                ],
            ),
        ])

        errors = _check(program)

        assert any("borrowed" in str(error).lower() for error in errors)

    def test_match_expression_recurses_into_case_body(self):
        program = Program([
            VariableDeclaration("x", Literal("integer", 1)),
            MatchExpression(
                Identifier("x"),
                [
                    MatchCase(
                        Identifier("value"),
                        [
                            VariableDeclaration("b", BorrowExpression("x")),
                            VariableDeclaration("x", Literal("integer", 2)),
                        ],
                    )
                ],
            ),
        ])

        errors = _check(program)

        assert any("borrowed" in str(error).lower() for error in errors)

    def test_async_function_definition_is_treated_like_function_definition(self):
        async_fn = type(
            "AsyncFunctionDefinition",
            (),
            {
                "name": "async_transfer",
                "parameters": [Parameter("x", "Integer")],
                "body": [
                    VariableDeclaration("moved", MoveExpression("x")),
                    ReturnStatement(Identifier("x")),
                ],
            },
        )()

        errors = _check(Program([async_fn]))

        assert any("moved" in str(error).lower() for error in errors)
