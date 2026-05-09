"""Type safety checker for static analysis.

Detects common static type issues in AST form, including:
- T001: Variable declaration type mismatch
- T002: Function argument type mismatch
- T007: Wrong function argument count
"""

from typing import Any, Dict, List, Set
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Category, Issue, Severity


class TypeSafetyChecker(BaseChecker):
    """Checks for type-safety violations."""

    CHECKER_NAME = "type_safety"

    def __init__(self):
        super().__init__()
        self.variables: Dict[str, str] = {}
        self.functions: Dict[str, Dict[str, Any]] = {}

    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        """Run type checks over the parsed AST."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.variables = {}
        self.functions = {}

        self._walk_nodes(ast, self._collect_types, set())
        self._walk_nodes(ast, self._check_node, set())
        return self.issues

    def _collect_types(self, node: ASTNode) -> None:
        node_type = type(node).__name__

        if node_type == 'VariableDeclaration':
            name = getattr(node, 'name', None)
            annotation = self._normalize_type(getattr(node, 'type_annotation', None))
            if name and annotation:
                self.variables[name] = annotation

        elif node_type == 'FunctionDefinition':
            name = getattr(node, 'name', None)
            if not name:
                return

            params = getattr(node, 'parameters', [])
            param_types: List[str] = []
            for param in params:
                param_type = self._normalize_type(getattr(param, 'type_annotation', None))
                param_types.append(param_type or 'Any')

            return_type = self._normalize_type(getattr(node, 'return_type', None)) or 'Any'
            self.functions[name] = {
                'params': param_types,
                'return_type': return_type,
            }

    def _check_node(self, node: ASTNode) -> None:
        node_type = type(node).__name__
        if node_type == 'VariableDeclaration':
            self._check_variable_declaration(node)
        elif node_type == 'FunctionCall':
            self._check_function_call(node)

    def _walk_nodes(self, node: Any, callback, seen: Set[int]) -> None:
        """Traverse parser objects even when they do not subclass ASTNode."""
        if node is None:
            return

        if isinstance(node, (str, bytes, int, float, bool)):
            return

        node_id = id(node)
        if node_id in seen:
            return
        seen.add(node_id)

        callback(node)

        if isinstance(node, dict):
            for value in node.values():
                self._walk_nodes(value, callback, seen)
            return

        if isinstance(node, (list, tuple, set)):
            for item in node:
                self._walk_nodes(item, callback, seen)
            return

        if hasattr(node, '__dict__'):
            for value in vars(node).values():
                self._walk_nodes(value, callback, seen)

    def _check_variable_declaration(self, node: ASTNode) -> None:
        declared_type = self._normalize_type(getattr(node, 'type_annotation', None))
        value = getattr(node, 'value', None)

        # The shared AST walker does not descend through VariableDeclaration.value,
        # so type checks for nested function calls are triggered explicitly here.
        if type(value).__name__ == 'FunctionCall':
            self._check_function_call(value)

        if not declared_type or value is None:
            return

        actual_type = self._infer_type(value)
        if self._is_compatible(actual_type, declared_type):
            return

        location = self.get_node_location(node)
        self.issues.append(Issue(
            code='T001',
            severity=Severity.ERROR,
            category=Category.TYPE_SAFETY,
            message=(
                f"Type mismatch: cannot assign '{actual_type}' "
                f"to variable of type '{declared_type}'"
            ),
            location=location,
            source_line=self.get_source_line(location.line),
            suggestion=f"Cast value to '{declared_type}' or update the declaration type",
        ))

    def _check_function_call(self, node: ASTNode) -> None:
        func_name = getattr(node, 'name', None)
        if func_name not in self.functions:
            return

        args = getattr(node, 'arguments', [])
        expected = self.functions[func_name]['params']

        if len(args) != len(expected):
            location = self.get_node_location(node)
            self.issues.append(Issue(
                code='T007',
                severity=Severity.ERROR,
                category=Category.TYPE_SAFETY,
                message=f"Wrong number of arguments: expected {len(expected)}, got {len(args)}",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Provide exactly {len(expected)} argument(s)",
            ))
            return

        for idx, (arg, expected_type) in enumerate(zip(args, expected), start=1):
            actual_type = self._infer_type(arg)
            if self._is_compatible(actual_type, expected_type):
                continue

            location = self.get_node_location(node)
            self.issues.append(Issue(
                code='T002',
                severity=Severity.ERROR,
                category=Category.TYPE_SAFETY,
                message=(
                    f"Type mismatch in argument {idx}: "
                    f"expected '{expected_type}', got '{actual_type}'"
                ),
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Cast argument {idx} to '{expected_type}'",
            ))

    def _infer_type(self, node: ASTNode) -> str:
        if node is None:
            return 'Any'

        node_type = type(node).__name__

        if node_type == 'Literal':
            literal_type = str(getattr(node, 'type', '')).lower()
            if literal_type == 'integer':
                return 'Integer'
            if literal_type == 'float':
                return 'Float'
            if literal_type == 'string':
                return 'String'
            if literal_type == 'boolean':
                return 'Boolean'
            if literal_type == 'null':
                return 'Null'
            return 'Any'

        if node_type == 'StringLiteral':
            return 'String'

        if node_type == 'Identifier':
            name = getattr(node, 'name', None)
            if name and name in self.variables:
                return self.variables[name]
            return 'Any'

        if node_type == 'FunctionCall':
            name = getattr(node, 'name', None)
            if name and name in self.functions:
                return self.functions[name]['return_type']
            return 'Any'

        if node_type == 'BinaryOperation':
            left = self._infer_type(getattr(node, 'left', None))
            right = self._infer_type(getattr(node, 'right', None))
            op = getattr(node, 'operator', None)
            if op in {'equals', 'not_equals', '==', '!=', 'greater', 'less', '>', '<'}:
                return 'Boolean'
            if op in {'plus', 'minus', 'multiply', 'divide', '+', '-', '*', '/'}:
                if 'Float' in {left, right}:
                    return 'Float'
                if 'Integer' in {left, right}:
                    return 'Integer'
                if left == 'String' and right == 'String' and op in {'plus', '+'}:
                    return 'String'
            return 'Any'

        return 'Any'

    def _is_compatible(self, actual: str, expected: str) -> bool:
        if actual == expected:
            return True
        if actual == 'Any' or expected == 'Any':
            return True
        if expected == 'Number' and actual in {'Integer', 'Float'}:
            return True
        if expected == 'Float' and actual == 'Integer':
            return True
        return False

    def _normalize_type(self, typ: Any) -> str:
        if typ is None:
            return ''
        value = str(typ).strip()
        if not value:
            return ''

        mapping = {
            'int': 'Integer',
            'integer': 'Integer',
            'float': 'Float',
            'str': 'String',
            'string': 'String',
            'bool': 'Boolean',
            'boolean': 'Boolean',
            'number': 'Number',
            'list': 'List',
            'dict': 'Dict',
            'any': 'Any',
            'null': 'Null',
            'none': 'Null',
        }
        return mapping.get(value.lower(), value)
