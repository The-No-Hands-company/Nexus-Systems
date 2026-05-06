"""Tests for Higher-Kinded Type (HKT) parser support.

Validates:
- Kind annotation parsing: *, * -> *, (* -> *) -> *
- Generic parameters with :: kind syntax in functions
- Generic parameters with :: kind syntax in classes
- Correct AST node construction (StarKindAnnotation, ArrowKindAnnotation)
- Integration with HKT type system objects
"""

import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../src"))

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.parser.ast import (
    FunctionDefinition,
    ClassDefinition,
    StarKindAnnotation,
    ArrowKindAnnotation,
)


def parse(source: str):
    """Parse source and return AST program node."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


# ---------------------------------------------------------------------------
# Lexer: DOUBLE_COLON token
# ---------------------------------------------------------------------------

class TestDoubleColonToken:
    """Verify :: is tokenized as DOUBLE_COLON."""

    def test_double_colon_token(self):
        from nexuslang.parser.lexer import TokenType
        lexer = Lexer("F :: *")
        tokens = lexer.tokenize()
        types = [t.type for t in tokens]
        assert TokenType.DOUBLE_COLON in types

    def test_single_colon_unchanged(self):
        from nexuslang.parser.lexer import TokenType
        lexer = Lexer("a : b")
        tokens = lexer.tokenize()
        types = [t.type for t in tokens]
        assert TokenType.COLON in types
        assert TokenType.DOUBLE_COLON not in types


# ---------------------------------------------------------------------------
# Kind Annotation Parsing
# ---------------------------------------------------------------------------

class TestKindAnnotationParsing:
    """Verify kind annotations are parsed into correct AST nodes."""

    def test_star_kind_in_function(self):
        """function map<F :: *> ..."""
        source = """
function identity<F :: *> with x
    return x
end
"""
        ast = parse(source)
        func = ast.statements[0]
        assert isinstance(func, FunctionDefinition)
        assert "F" in func.type_param_kinds
        assert isinstance(func.type_param_kinds["F"], StarKindAnnotation)

    def test_star_to_star_kind(self):
        """function map<F :: * -> *> ..."""
        source = """
function map<F :: * -> *> with x
    return x
end
"""
        ast = parse(source)
        func = ast.statements[0]
        assert "F" in func.type_param_kinds
        kind = func.type_param_kinds["F"]
        assert isinstance(kind, ArrowKindAnnotation)
        assert isinstance(kind.left, StarKindAnnotation)
        assert isinstance(kind.right, StarKindAnnotation)

    def test_star_to_star_to_star_kind(self):
        """function either<E :: * -> * -> *> ... (right-associative)"""
        source = """
function either<E :: * -> * -> *> with x
    return x
end
"""
        ast = parse(source)
        func = ast.statements[0]
        kind = func.type_param_kinds["E"]
        assert isinstance(kind, ArrowKindAnnotation)
        assert isinstance(kind.left, StarKindAnnotation)
        # Right is * -> * (right-associative)
        assert isinstance(kind.right, ArrowKindAnnotation)
        assert isinstance(kind.right.left, StarKindAnnotation)
        assert isinstance(kind.right.right, StarKindAnnotation)

    def test_parenthesized_kind(self):
        """function hof<G :: (* -> *) -> *> ..."""
        source = """
function hof<G :: (* -> *) -> *> with x
    return x
end
"""
        ast = parse(source)
        func = ast.statements[0]
        kind = func.type_param_kinds["G"]
        assert isinstance(kind, ArrowKindAnnotation)
        # Left is (* -> *)
        assert isinstance(kind.left, ArrowKindAnnotation)
        assert isinstance(kind.left.left, StarKindAnnotation)
        assert isinstance(kind.left.right, StarKindAnnotation)
        # Right is *
        assert isinstance(kind.right, StarKindAnnotation)

    def test_multiple_params_mixed_kinds(self):
        """function transform<F :: * -> *, T> ..."""
        source = """
function transform<F :: * -> *, T> with x
    return x
end
"""
        ast = parse(source)
        func = ast.statements[0]
        assert "F" in func.type_parameters
        assert "T" in func.type_parameters
        assert "F" in func.type_param_kinds
        assert isinstance(func.type_param_kinds["F"], ArrowKindAnnotation)
        # T has no kind annotation (defaults to *)
        assert "T" not in func.type_param_kinds

    def test_kind_with_trait_bounds(self):
        """function constrained<T: Comparable, F :: * -> *> ..."""
        source = """
function constrained<T: Comparable, F :: * -> *> with x
    return x
end
"""
        ast = parse(source)
        func = ast.statements[0]
        assert "T" in func.type_parameters
        assert "F" in func.type_parameters
        assert func.type_constraints.get("T") == ["Comparable"]
        assert isinstance(func.type_param_kinds["F"], ArrowKindAnnotation)


# ---------------------------------------------------------------------------
# HKT in Class Definitions
# ---------------------------------------------------------------------------

class TestHKTClassParsing:
    """Verify kind annotations work in class generic parameters."""

    def test_class_with_hkt_param(self):
        """class Functor<F :: * -> *>"""
        source = """
class Functor<F :: * -> *>
    property value
end
"""
        ast = parse(source)
        cls = ast.statements[0]
        assert isinstance(cls, ClassDefinition)
        assert "F" in cls.generic_parameters
        assert "F" in cls.type_param_kinds
        assert isinstance(cls.type_param_kinds["F"], ArrowKindAnnotation)

    def test_class_with_mixed_params(self):
        """class Transformer<F :: * -> *, A>"""
        source = """
class Transformer<F :: * -> *, A>
    property value
end
"""
        ast = parse(source)
        cls = ast.statements[0]
        assert cls.generic_parameters == ["F", "A"]
        assert "F" in cls.type_param_kinds
        assert "A" not in cls.type_param_kinds


# ---------------------------------------------------------------------------
# AST Kind Annotation repr
# ---------------------------------------------------------------------------

class TestKindAnnotationRepr:
    """Verify kind annotation string representations."""

    def test_star_repr(self):
        assert repr(StarKindAnnotation()) == "*"

    def test_arrow_repr(self):
        kind = ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation())
        assert repr(kind) == "* -> *"

    def test_nested_arrow_repr(self):
        inner = ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation())
        kind = ArrowKindAnnotation(inner, StarKindAnnotation())
        assert repr(kind) == "(* -> *) -> *"


# ---------------------------------------------------------------------------
# Integration: AST Kind -> HKT Type System Kind
# ---------------------------------------------------------------------------

class TestASTToHKTConversion:
    """Verify _ast_kind_to_hkt correctly maps AST nodes to HKT Kind objects."""

    def test_star_converts(self):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.typesystem.hkt import STAR
        result = Interpreter._ast_kind_to_hkt(StarKindAnnotation())
        assert result is STAR

    def test_arrow_converts(self):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.typesystem.hkt import STAR, ArrowKind
        ast_kind = ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation())
        result = Interpreter._ast_kind_to_hkt(ast_kind)
        assert isinstance(result, ArrowKind)
        assert result.param_kind is STAR
        assert result.result_kind is STAR

    def test_nested_converts(self):
        from nexuslang.interpreter.interpreter import Interpreter
        from nexuslang.typesystem.hkt import STAR, ArrowKind
        inner = ArrowKindAnnotation(StarKindAnnotation(), StarKindAnnotation())
        ast_kind = ArrowKindAnnotation(inner, StarKindAnnotation())
        result = Interpreter._ast_kind_to_hkt(ast_kind)
        assert isinstance(result, ArrowKind)
        assert isinstance(result.param_kind, ArrowKind)
        assert result.result_kind is STAR
