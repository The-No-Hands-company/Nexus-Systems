"""HKT parser hardening matrix.

This suite expands coverage with parameter matrices for valid and invalid
kind-annotation forms across function and class generic declarations.
"""

import os
import sys

import pytest


sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src"))

from nexuslang.parser.ast import ClassDefinition, FunctionDefinition
from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser


def _parse(source: str):
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens, source=source)
    return parser.parse()


KIND_CASES = [
    ("*", "*"),
    ("* -> *", "* -> *"),
    ("* -> * -> *", "* -> * -> *"),
    ("(* -> *) -> *", "(* -> *) -> *"),
    ("* -> (* -> *)", "* -> * -> *"),
    ("(* -> * -> *) -> *", "(* -> * -> *) -> *"),
    ("(* -> *) -> (* -> *)", "(* -> *) -> * -> *"),
    ("((* -> *) -> *) -> *", "((* -> *) -> *) -> *"),
]


@pytest.mark.parametrize("kind_source, expected_repr", KIND_CASES)
def test_function_generic_kind_matrix(kind_source: str, expected_repr: str):
    source = f"""
function transform<F :: {kind_source}, T: Comparable> with x
    return x
end
"""

    ast = _parse(source)
    func = ast.statements[0]

    assert isinstance(func, FunctionDefinition)
    assert func.type_parameters == ["F", "T"]
    assert func.type_constraints.get("T") == ["Comparable"]
    assert "F" in func.type_param_kinds
    assert repr(func.type_param_kinds["F"]) == expected_repr


@pytest.mark.parametrize("kind_source, expected_repr", KIND_CASES)
def test_class_generic_kind_matrix(kind_source: str, expected_repr: str):
    source = f"""
class Container<F :: {kind_source}, Item>
    property value
end
"""

    ast = _parse(source)
    cls = ast.statements[0]

    assert isinstance(cls, ClassDefinition)
    assert cls.generic_parameters == ["F", "Item"]
    assert "F" in cls.type_param_kinds
    assert "Item" not in cls.type_param_kinds
    assert repr(cls.type_param_kinds["F"]) == expected_repr


INVALID_KIND_CASES = [
    "function bad<F :: -> *> with x\n    return x\nend\n",
    "function bad<F :: * ->> with x\n    return x\nend\n",
    "function bad<F :: (* -> *> with x\n    return x\nend\n",
    "function bad<F :: * -> (* -> >) > with x\n    return x\nend\n",
    "class Bad<F :: (* -> *>\n    property value\nend\n",
    "class Bad<F :: *> -> *\n    property value\nend\n",
]


@pytest.mark.parametrize("source", INVALID_KIND_CASES)
def test_invalid_kind_annotation_matrix(source: str):
    with pytest.raises(Exception) as exc_info:
        _parse(source)

    message = str(exc_info.value)
    assert "kind annotation" in message.lower() or "expected" in message.lower()
