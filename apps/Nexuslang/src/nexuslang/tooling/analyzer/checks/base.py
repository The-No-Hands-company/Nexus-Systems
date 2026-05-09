"""
Base Checker Class
==================

Abstract base class for all analysis checkers.
"""

from abc import ABC, abstractmethod
from typing import Any, List, Optional, Set, Tuple
import sys
import os

# Add parent directories to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from ..report import Issue


class BaseChecker(ABC):
    """Base class for all static analysis checkers."""
    
    def __init__(self):
        self.issues: List[Issue] = []
        self.current_file = ""
        self.current_source = ""
        self.current_lines = []
    
    @abstractmethod
    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        """
        Run analysis on AST.
        
        Args:
            ast: Parsed AST root node
            source: Full source code
            lines: Source split into lines
            
        Returns:
            List of issues found
        """
        pass
    
    def walk_ast(self, node: ASTNode, callback):
        """
        Walk AST recursively, calling callback(node) on each node.

        This walker now supports legacy parser nodes that do not subclass
        ``ASTNode`` but still expose child nodes via ``__dict__``.
        
        Args:
            node: Current AST node
            callback: Function to call on each node
        """
        def _adapter(current_node, _scope_stack, _ancestors):
            callback(current_node)

        self.walk_ast_with_context(node, _adapter)

    def walk_ast_with_context(
        self,
        node: Any,
        callback,
        scope_stack: Optional[Tuple[str, ...]] = None,
        ancestors: Optional[Tuple[str, ...]] = None,
        visited: Optional[Set[int]] = None,
    ) -> None:
        """
        Walk AST-like objects recursively with context.

        Args:
            node: Current node/value to walk
            callback: Function called as callback(node, scope_stack, ancestors)
            scope_stack: Lexical scope breadcrumbs
            ancestors: Parent node-type chain
            visited: Internal cycle guard
        """
        if node is None or isinstance(node, (str, bytes, int, float, bool)):
            return

        if visited is None:
            visited = set()
        if scope_stack is None:
            scope_stack = ("<module>",)
        if ancestors is None:
            ancestors = tuple()

        if isinstance(node, list):
            for item in node:
                self.walk_ast_with_context(item, callback, scope_stack, ancestors, visited)
            return

        if isinstance(node, tuple):
            for item in node:
                self.walk_ast_with_context(item, callback, scope_stack, ancestors, visited)
            return

        if isinstance(node, set):
            for item in node:
                self.walk_ast_with_context(item, callback, scope_stack, ancestors, visited)
            return

        if isinstance(node, dict):
            for value in node.values():
                self.walk_ast_with_context(value, callback, scope_stack, ancestors, visited)
            return

        if not hasattr(node, '__dict__'):
            return

        node_id = id(node)
        if node_id in visited:
            return
        visited.add(node_id)

        callback(node, scope_stack, ancestors)

        node_type = node.__class__.__name__
        next_scope = scope_stack
        if node_type == 'FunctionDefinition':
            next_scope = scope_stack + (f"function:{getattr(node, 'name', '<anonymous>')}",)
        elif node_type == 'MethodDefinition':
            next_scope = scope_stack + (f"method:{getattr(node, 'name', '<anonymous>')}",)
        elif node_type == 'LambdaExpression':
            next_scope = scope_stack + ('lambda',)

        next_ancestors = ancestors + (node_type,)
        for attr in vars(node).values():
            self.walk_ast_with_context(attr, callback, next_scope, next_ancestors, visited)

    def iter_child_nodes(self, node: Any):
        """Yield direct child nodes for generic object-backed AST shapes."""
        if not hasattr(node, '__dict__'):
            return

        for key, value in vars(node).items():
            if key.startswith('_'):
                continue

            if isinstance(value, list):
                for item in value:
                    if item is not None and hasattr(item, '__dict__'):
                        yield item
            elif isinstance(value, tuple):
                for item in value:
                    if item is not None and hasattr(item, '__dict__'):
                        yield item
            elif isinstance(value, set):
                for item in value:
                    if item is not None and hasattr(item, '__dict__'):
                        yield item
            elif isinstance(value, dict):
                for item in value.values():
                    if item is not None and hasattr(item, '__dict__'):
                        yield item
            elif hasattr(value, '__dict__'):
                yield value
    
    def get_source_line(self, line_num: int) -> str:
        """Get source line by number (1-indexed)."""
        if 1 <= line_num <= len(self.current_lines):
            return self.current_lines[line_num - 1]
        return ""
    
    def get_node_location(self, node: ASTNode):
        """Extract location from AST node."""
        from ..report import SourceLocation
        
        line = getattr(node, 'line', None) or getattr(node, 'line_number', None)
        if line is None:
            line = 1  # Default to line 1 if no line information
        
        column = getattr(node, 'column', None)
        if column is None:
            column = 1  # Default to column 1 if no column information
        
        return SourceLocation(
            file=self.current_file,
            line=int(line),
            column=int(column)
        )
