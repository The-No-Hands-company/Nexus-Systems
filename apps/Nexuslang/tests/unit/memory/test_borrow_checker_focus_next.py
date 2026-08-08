"""Focused branch coverage slice for borrow_checker internals and expression handlers."""

from types import SimpleNamespace

from nexuslang.parser.ast import (
    BorrowExpression,
    Identifier,
    Literal,
    MoveExpression,
    Program,
    VariableDeclaration,
)
from nexuslang.typesystem.borrow_checker import BorrowChecker, BorrowScope, VarBorrowState


class DropBorrowStatement:
    def __init__(self, var_name, mutable=False):
        self.var_name = var_name
        self.mutable = mutable
        self.line_number = 1


class VariableAssignment:
    def __init__(self, name, value):
        self.name = name
        self.value = value
        self.line_number = 1


class AssignmentStatement(VariableAssignment):
    pass


class Assignment(VariableAssignment):
    pass


class UnaryOperation:
    def __init__(self, operand=None, expression=None):
        self.operand = operand
        self.expression = expression


class MethodCall:
    def __init__(self, obj, arguments):
        self.object = obj
        self.arguments = arguments


class IndexExpression:
    def __init__(self, obj, index):
        self.object = obj
        self.index = index


class DictLiteral:
    def __init__(self, pairs):
        self.pairs = pairs


class TryCatchStatement:
    def __init__(self, try_block, catch_block, finally_block=None):
        self.try_block = try_block
        self.catch_block = catch_block
        self.finally_block = finally_block


class TestBorrowScopeInternalBranches:
    def test_pop_set_all_names_and_merge_moved_behavior(self):
        scope = BorrowScope()

        # pop() should be a no-op for the global scope
        scope.pop()

        scope.push()
        scope.define("x", VarBorrowState())
        scope.define("y", VarBorrowState())
        scope.set("x", VarBorrowState(is_moved=True))

        assert scope.get("x").is_moved is True
        assert scope.all_names() == {"x", "y"}

        snap = scope.snapshot()
        # add moved variable in another snapshot and merge back
        other = scope.snapshot()
        other[-1]["y"] = VarBorrowState(is_moved=True)
        scope.restore(snap)
        scope.merge_moved_from(other)

        assert scope.get("y").is_moved is True


class TestBorrowCheckerNextSlice:
    def _check(self, statements):
        return BorrowChecker().check(Program(statements))

    def test_borrow_drop_error_paths_and_success_paths(self):
        errors = self._check([
            VariableDeclaration("x", Literal("integer", 1)),
            DropBorrowStatement("x", mutable=True),
            DropBorrowStatement("x", mutable=False),
        ])
        assert any("mutable borrow" in str(err).lower() for err in errors)
        assert any("immutable borrow" in str(err).lower() for err in errors)

        # Successful mutable then immutable drop paths
        no_errors = self._check([
            VariableDeclaration("x", Literal("integer", 1)),
            VariableDeclaration("m", BorrowExpression("x", mutable=True)),
            DropBorrowStatement("x", mutable=True),
            VariableDeclaration("i", BorrowExpression("x", mutable=False)),
            DropBorrowStatement("x", mutable=False),
        ])
        assert no_errors == []

    def test_assignment_alias_handlers_and_rhs_recursion(self):
        errors = self._check([
            VariableDeclaration("x", Literal("integer", 1)),
            VariableDeclaration("b", BorrowExpression("x", mutable=False)),
            AssignmentStatement("x", MoveExpression("x")),
            Assignment("x", Literal("integer", 2)),
        ])

        # assignment while borrowed + move while borrowed
        assert any("cannot assign" in str(err).lower() for err in errors)
        assert any("cannot move" in str(err).lower() for err in errors)

    def test_expression_handler_recursion_paths(self):
        errors = self._check([
            VariableDeclaration("x", Literal("integer", 1)),
            VariableDeclaration("m", MoveExpression("x")),
            UnaryOperation(operand=Identifier("x")),
            MethodCall(Identifier("x"), [Identifier("x")]),
            IndexExpression(Identifier("x"), Identifier("x")),
            DictLiteral([(Identifier("x"), Identifier("x"))]),
        ])

        # use-after-move from different recursive handler paths
        moved_uses = [err for err in errors if "use of moved value" in str(err).lower()]
        assert len(moved_uses) >= 4

    def test_try_catch_statement_finally_branch_is_checked(self):
        errors = self._check([
            VariableDeclaration("x", Literal("integer", 1)),
            VariableDeclaration("b", BorrowExpression("x", mutable=False)),
            TryCatchStatement(
                try_block=[],
                catch_block=[],
                finally_block=[VariableAssignment("x", Literal("integer", 3))],
            ),
        ])

        assert any("cannot assign" in str(err).lower() for err in errors)

    def test_generic_fallback_skips_none_children_and_recurses_lists(self):
        checker = BorrowChecker()
        program = Program([
            VariableDeclaration("x", Literal("integer", 1)),
            VariableDeclaration("m", MoveExpression("x")),
            SimpleNamespace(
                statements=[Identifier("x")],
                body=None,
                then_block=None,
                else_block=None,
                condition=None,
                value=None,
                expression=None,
                left=None,
                right=None,
                arguments=None,
                iterable=None,
                iterator=None,
            ),
        ])

        errors = checker.check(program)
        assert any("use of moved value" in str(err).lower() for err in errors)
