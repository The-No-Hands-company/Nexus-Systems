"""
Initialization Checker
======================

Detects uninitialized variable issues:
- Use before assignment
- Conditional initialization issues
- Path-dependent initialization
"""

from typing import Dict, Set, List
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


class InitializationChecker(BaseChecker):

    CHECKER_NAME = "initialization"
    """Checks for uninitialized variable use."""
    
    def __init__(self):
        super().__init__()
        self.declared: Set[str] = set()
        self.initialized: Set[str] = set()
        self.used: Set[str] = set()
    
    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        """Run initialization analysis."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.declared = set()
        self.initialized = set()
        self.used = set()
        
        self.walk_ast(ast, self._check_node)
        
        return self.issues
    
    def _check_node(self, node: ASTNode):
        """Check individual node."""
        node_type = type(node).__name__
        
        if node_type == 'VariableDeclaration':
            var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
            if var_name:
                self.declared.add(var_name)
                if hasattr(node, 'value') and getattr(node, 'value', None):
                    self.initialized.add(var_name)
        
        elif node_type in ['Identifier', 'VariableReference']:
            var_name = getattr(node, 'name', None)
            if var_name and var_name not in self.initialized and var_name not in self.used:
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="I001",
                    severity=Severity.ERROR,
                    category=Category.BEST_PRACTICE,
                    message=f"Variable '{var_name}' used before initialization",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Initialize '{var_name}' before use"
                ))
            self.used.add(var_name)
