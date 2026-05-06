"""
Base Checker Class
==================

Abstract base class for all analysis checkers.
"""

from abc import ABC, abstractmethod
from typing import List
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
        Walk AST recursively, calling callback on each node.
        
        Args:
            node: Current AST node
            callback: Function to call on each node
        """
        if node is None:
            return
        
        # Call callback
        callback(node)
        
        # Walk children
        if hasattr(node, 'children'):
            for child in node.children:
                self.walk_ast(child, callback)
        
        # Walk common attributes
        for attr_name in ['body', 'statements', 'expressions', 'arguments', 
                         'condition', 'then_branch', 'else_branch',
                         'initializer', 'update', 'left', 'right']:
            if hasattr(node, attr_name):
                attr = getattr(node, attr_name)
                if isinstance(attr, list):
                    for item in attr:
                        if isinstance(item, ASTNode):
                            self.walk_ast(item, callback)
                elif isinstance(attr, ASTNode):
                    self.walk_ast(attr, callback)
    
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
