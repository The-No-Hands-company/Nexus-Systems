"""Tests for the compile-time lifetime checker (LifetimeChecker).

These tests verify that lifetime annotations are parsed correctly and that the
LifetimeChecker pass detects inconsistencies in lifetime annotations.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

import unittest

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.typesystem.lifetime_checker import LifetimeChecker
from nexuslang.parser.ast import (
    BorrowExpressionWithLifetime, LifetimeAnnotation,
    ReturnTypeWithLifetime,
)


def _parse(source: str):
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


def _check_lifetime(source: str):
    """Return (hard_errors, warnings) for the given source."""
    ast = _parse(source)
    errors = LifetimeChecker().check(ast)
    hard = [e for e in errors if not e.is_warning]
    warnings = [e for e in errors if e.is_warning]
    return hard, warnings


class TestLifetimeLexerAndParser(unittest.TestCase):
    """Verify the lexer / parser produce the correct AST nodes for lifetime syntax."""

    def test_borrow_expression_with_lifetime_node(self):
        """'borrow x with lifetime outer' produces BorrowExpressionWithLifetime."""
        ast = _parse("""
set x to 42
set b to borrow x with lifetime outer
""")
        # Walk AST to find the BorrowExpressionWithLifetime
        stmts = ast.statements
        # Third statement is `set b to ...`; its value should be the borrow node
        borrow_node = None
        for stmt in stmts:
            val = getattr(stmt, 'value', None)
            if isinstance(val, BorrowExpressionWithLifetime):
                borrow_node = val
                break
        self.assertIsNotNone(borrow_node, "Expected BorrowExpressionWithLifetime")
        self.assertEqual(borrow_node.var_name, "x")
        self.assertFalse(borrow_node.mutable)
        self.assertIsNotNone(borrow_node.lifetime)
        self.assertEqual(borrow_node.lifetime.label, "outer")

    def test_mutable_borrow_expression_with_lifetime_node(self):
        """'borrow mutable x with lifetime inner' is MutableBorrow with lifetime."""
        ast = _parse("""
set arr to 1
set m to borrow mutable arr with lifetime inner
""")
        borrow_node = None
        for stmt in ast.statements:
            val = getattr(stmt, 'value', None)
            if isinstance(val, BorrowExpressionWithLifetime):
                borrow_node = val
                break
        self.assertIsNotNone(borrow_node, "Expected BorrowExpressionWithLifetime")
        self.assertTrue(borrow_node.mutable)
        self.assertEqual(borrow_node.lifetime.label, "inner")

    def test_function_return_type_with_lifetime(self):
        """Function return type 'returns borrow String with lifetime outer' parses."""
        ast = _parse("""
function get_str with x as borrow String with lifetime outer returns borrow String with lifetime outer
    return borrow x with lifetime outer
end
""")
        func = ast.statements[0]
        self.assertEqual(func.name, "get_str")
        rt = func.return_type
        self.assertIsInstance(rt, ReturnTypeWithLifetime,
                              f"Expected ReturnTypeWithLifetime, got {type(rt)}")
        self.assertEqual(rt.lifetime.label, "outer")

    def test_parameter_lifetime_annotation(self):
        """Parameter 'x as borrow String with lifetime a' stores ReturnTypeWithLifetime."""
        ast = _parse("""
function echo with x as borrow String with lifetime a returns borrow String with lifetime a
    return borrow x with lifetime a
end
""")
        func = ast.statements[0]
        self.assertEqual(len(func.parameters), 1)
        param = func.parameters[0]
        ann = param.type_annotation
        self.assertIsInstance(ann, ReturnTypeWithLifetime,
                              f"Expected ReturnTypeWithLifetime as type_annotation, got {type(ann)}")
        self.assertEqual(ann.lifetime.label, "a")


class TestLifetimeCheckerClean(unittest.TestCase):
    """Programs whose lifetime annotations are internally consistent."""

    def test_declared_and_used_lifetime(self):
        """Declared parameter lifetime that is used in the body -- no errors."""
        errors, warnings = _check_lifetime("""
function get_ref with x as borrow String with lifetime outer returns borrow String with lifetime outer
    return borrow x with lifetime outer
end
""")
        self.assertEqual(errors, [], f"Unexpected errors: {errors}")

    def test_multiple_parameters_same_lifetime(self):
        """Two parameters sharing the same lifetime -- used correctly, no errors."""
        errors, warnings = _check_lifetime("""
function longest with x as borrow String with lifetime a and y as borrow String with lifetime a returns borrow String with lifetime a
    return borrow x with lifetime a
end
""")
        self.assertEqual(errors, [], f"Unexpected errors: {errors}")

    def test_no_lifetime_annotations(self):
        """A function with no lifetime annotations at all -- no errors."""
        errors, warnings = _check_lifetime("""
function add with a as Integer and b as Integer returns Integer
    return a plus b
end
""")
        self.assertEqual(errors, [])
        self.assertEqual(warnings, [])

    def test_top_level_borrow_with_lifetime_no_errors(self):
        """Top-level borrow with lifetime is allowed (no function context required)."""
        errors, _ = _check_lifetime("""
set x to 10
set b to borrow x with lifetime doc
drop borrow x
""")
        self.assertEqual(errors, [])


class TestLifetimeCheckerErrors(unittest.TestCase):
    """Programs with lifetime annotation inconsistencies."""

    def test_undeclared_lifetime_label(self):
        """Using a lifetime label not declared on any parameter is an error."""
        errors, _ = _check_lifetime("""
function leak with x as Integer returns borrow Integer with lifetime outer
    return borrow x with lifetime outer
end
""")
        self.assertTrue(
            any("outer" in str(e) for e in errors),
            f"Expected undeclared-lifetime error for 'outer'; got: {errors}",
        )

    def test_return_lifetime_mismatch(self):
        """Return borrow with wrong lifetime -- must be caught."""
        errors, _ = _check_lifetime("""
function wrong with x as borrow String with lifetime a returns borrow String with lifetime a
    return borrow x with lifetime b
end
""")
        # 'b' is undeclared, and/or the return borrow uses wrong label
        self.assertTrue(
            len(errors) > 0,
            f"Expected lifetime mismatch error; got no errors",
        )

    def test_return_borrow_missing_lifetime(self):
        """Function declares return lifetime but returns plain borrow -- error."""
        errors, _ = _check_lifetime("""
function miss with x as borrow String with lifetime a returns borrow String with lifetime a
    return borrow x
end
""")
        self.assertTrue(
            any("lifetime" in str(e).lower() for e in errors),
            f"Expected missing-return-lifetime error; got: {errors}",
        )


class TestLifetimeCheckerWarnings(unittest.TestCase):
    """Situations that produce warnings (not hard errors)."""

    def test_unused_lifetime_declaration_produces_warning(self):
        """Declaring a parameter lifetime but never using it -> warning."""
        _, warnings = _check_lifetime("""
function unused_lt with x as borrow String with lifetime a returns String
    return x
end
""")
        self.assertTrue(
            any("never used" in str(w).lower() or "unused" in str(w).lower()
                or "declared" in str(w).lower() for w in warnings),
            f"Expected unused-lifetime warning; got warnings: {warnings}",
        )


if __name__ == '__main__':
    unittest.main()
