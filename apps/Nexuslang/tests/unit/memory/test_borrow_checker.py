"""Tests for the compile-time borrow checker (BorrowChecker).

These tests verify that the static analysis pass correctly detects ownership
and borrow violations *before* the interpreter runs.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.borrow_checker import BorrowChecker


def _parse(source: str):
    """Parse NexusLang source and return the root AST node."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


def _check(source: str):
    """Return the list of borrow errors for the given source snippet."""
    ast = _parse(source)
    return BorrowChecker().check(ast)


class TestBorrowCheckerClean(unittest.TestCase):
    """Programs that should produce zero borrow errors."""

    def test_simple_move(self):
        """Moving a plain variable produces no errors."""
        errors = _check("""
set x to 42
set y to move x
""")
        self.assertEqual(errors, [])

    def test_immutable_borrow_then_drop(self):
        """Borrow, use, then drop -- clean."""
        errors = _check("""
set x to 10
set b to borrow x
drop borrow x
""")
        self.assertEqual(errors, [])

    def test_multiple_immutable_borrows(self):
        """Multiple simultaneous immutable borrows are allowed."""
        errors = _check("""
set x to "hello"
set b1 to borrow x
set b2 to borrow x
drop borrow x
drop borrow x
""")
        self.assertEqual(errors, [])

    def test_mutable_borrow_then_drop(self):
        """Mutable borrow then drop is clean."""
        errors = _check("""
set x to 0
set b to borrow mutable x
drop borrow mutable x
""")
        self.assertEqual(errors, [])

    def test_move_after_drop(self):
        """Moving a variable after all borrows are dropped is clean."""
        errors = _check("""
set x to 99
set b to borrow x
drop borrow x
set y to move x
""")
        self.assertEqual(errors, [])

    def test_move_inside_function(self):
        """Move inside a function scope is clean."""
        errors = _check("""
function transfer with val as Integer returns Integer
    set moved_val to move val
    return moved_val
end
""")
        self.assertEqual(errors, [])

    def test_borrow_inside_if_branches(self):
        """Borrows inside if/else branches without conflicts are clean."""
        errors = _check("""
set x to 1
if x is greater than 0
    set b to borrow x
    drop borrow x
end
""")
        self.assertEqual(errors, [])


class TestBorrowCheckerErrors(unittest.TestCase):
    """Programs that should produce borrow errors."""

    def test_use_after_move(self):
        """Reading a moved variable is an error."""
        errors = _check("""
set x to 42
set y to move x
set z to x
""")
        self.assertTrue(
            any("moved" in str(e).lower() or "use" in str(e).lower() for e in errors),
            f"Expected use-after-move error; got: {errors}",
        )

    def test_double_move(self):
        """Moving a variable twice must be caught."""
        errors = _check("""
set x to 10
set a to move x
set b to move x
""")
        self.assertTrue(
            any("moved" in str(e).lower() for e in errors),
            f"Expected double-move error; got: {errors}",
        )

    def test_move_while_borrowed(self):
        """Moving a borrowed variable is an error."""
        errors = _check("""
set x to 5
set b to borrow x
set y to move x
""")
        self.assertTrue(
            any("borrowed" in str(e).lower() or "borrow" in str(e).lower()
                for e in errors),
            f"Expected move-while-borrowed error; got: {errors}",
        )

    def test_double_mutable_borrow(self):
        """Taking two mutable borrows simultaneously is an error."""
        errors = _check("""
set x to 0
set b1 to borrow mutable x
set b2 to borrow mutable x
""")
        self.assertTrue(
            any("mutable" in str(e).lower() for e in errors),
            f"Expected double-mutable-borrow error; got: {errors}",
        )

    def test_immutable_borrow_while_mutably_borrowed(self):
        """Taking an immutable borrow while a mutable borrow is active is an error."""
        errors = _check("""
set x to 0
set m to borrow mutable x
set b to borrow x
""")
        self.assertTrue(
            any("mutable" in str(e).lower() or "borrowed" in str(e).lower()
                for e in errors),
            f"Expected borrow-conflict error; got: {errors}",
        )

    def test_mutable_borrow_while_immutably_borrowed(self):
        """Taking a mutable borrow while immutable borrows are active is an error."""
        errors = _check("""
set x to 0
set b to borrow x
set m to borrow mutable x
""")
        self.assertTrue(
            any("immutably" in str(e).lower() or "borrow" in str(e).lower()
                for e in errors),
            f"Expected borrow-conflict error; got: {errors}",
        )

    def test_drop_without_matching_borrow(self):
        """Dropping a borrow that was never taken is an error."""
        errors = _check("""
set x to 3
drop borrow x
""")
        self.assertTrue(
            any("no active" in str(e).lower() or "drop" in str(e).lower()
                for e in errors),
            f"Expected drop-without-borrow error; got: {errors}",
        )

    def test_assign_to_borrowed_variable(self):
        """Assigning to a borrowed variable (via VariableDeclaration re-set) is an error.

        Note: NexusLang uses 'set x to ...' which creates a new binding in the
        current scope.  The checker intercepts this at the VariableDeclaration
        handler and validates the borrow state.
        """
        # The checker should catch mutation of an immutably borrowed var.
        errors = _check("""
set x to 10
set b to borrow x
set x to 20
""")
        self.assertTrue(
            any("borrowed" in str(e).lower() or "assign" in str(e).lower()
                for e in errors),
            f"Expected assign-while-borrowed error; got: {errors}",
        )


class TestBorrowCheckerControlFlow(unittest.TestCase):
    """Control-flow conservatism: if a move happens in any branch, it's merged."""

    def test_move_in_then_branch_is_reported_after(self):
        """Variable moved in if-then branch is considered moved after the if."""
        errors = _check("""
set x to 1
if x is greater than 0
    set y to move x
end
set z to x
""")
        # After the if, x might be moved -> access is an error
        self.assertTrue(
            any("moved" in str(e).lower() for e in errors),
            f"Expected use-after-maybe-move error; got: {errors}",
        )

    def test_move_in_both_branches_clean_inside(self):
        """Moving x inside both branches should not produce errors inside them."""
        errors = _check("""
set x to 1
if x is greater than 0
    set a to move x
else
    set b to move x
end
""")
        self.assertEqual(errors, [])


if __name__ == '__main__':
    unittest.main()
