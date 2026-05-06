"""
Style Checker
=============

Checks code style and best practices:
- Naming conventions
- Code organization
- Documentation
- Line length
- Whitespace
"""

from typing import Dict, Set, List, Optional
import sys
import os
import re

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category


class StyleChecker(BaseChecker):
    """
    Checks for style issues and best practices.
    
    Error codes:
    - S001: Function name should use snake_case
    - S002: Class name should use PascalCase
    - S003: Constant should use UPPER_CASE
    - S004: Variable name too short (< 2 chars, except i, j, k in loops)
    - S005: Line too long (> 100 characters)
    - S006: Missing function documentation
    - S007: Missing class documentation
    - S008: Trailing whitespace
    - S009: Multiple blank lines
    - S010: Missing blank line after function definition
    - S011: Function too long (> 50 lines)
    - S012: Too many parameters (> 5)
    - S013: Too deeply nested (> 4 levels)
    - S014: Magic number (use named constant)
    - S015: Inconsistent indentation
    - S016: Missing type annotation
    - S017: Redundant parentheses
    - S018: TODO/FIXME comment
    - S019: Commented-out code
    - S020: Inconsistent naming style
    """
    
    def __init__(self):
        super().__init__()
        self.nesting_level = 0
        self.max_line_length = 100
        self.max_function_lines = 50
        self.max_parameters = 5
        self.max_nesting = 4
    
    CHECKER_NAME = "style"

    def check(self, ast: ASTNode, source: str = "", lines: List[str] = None) -> List[Issue]:
        """Check AST for style issues."""
        self.issues = []
        self.nesting_level = 0
        self.current_source = source
        self.current_lines = lines or (source.splitlines() if source else [])

        self.walk_ast(ast, self._check_node)

        # Check line-based issues
        self._check_line_issues()

        return self.issues
    
    def _check_node(self, node: ASTNode):
        """Check a single node for style issues."""
        node_type = node.__class__.__name__
        
        if node_type == 'FunctionDefinition':
            self._check_function_style(node)
        elif node_type == 'ClassDefinition':
            self._check_class_style(node)
        elif node_type == 'VariableDeclaration':
            self._check_variable_style(node)
        elif node_type in ['If', 'While', 'For', 'ForEach']:
            self._check_nesting(node)
        elif node_type in ['IntegerLiteral', 'FloatLiteral']:
            self._check_magic_numbers(node)
    
    def _check_function_style(self, node: ASTNode):
        """Check function naming and structure."""
        func_name = getattr(node, 'name', None)
        params = getattr(node, 'parameters', [])
        body = getattr(node, 'body', None)
        docstring = getattr(node, 'docstring', None)
        
        if not func_name:
            return
        
        location = self.get_node_location(node)
        
        # Check naming convention (snake_case)
        if not re.match(r'^[a-z_][a-z0-9_]*$', func_name):
            self.issues.append(Issue(
                code="S001",
                severity=Severity.INFO,
                category=Category.STYLE,
                message=f"Function name '{func_name}' should use snake_case",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Rename to '{self._to_snake_case(func_name)}'"
            ))
        
        # Check parameter count
        if len(params) > self.max_parameters:
            self.issues.append(Issue(
                code="S012",
                severity=Severity.WARNING,
                category=Category.STYLE,
                message=f"Function has too many parameters ({len(params)} > {self.max_parameters})",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Consider using a parameter object or refactoring"
            ))
        
        # Check for missing documentation
        if not docstring and not func_name.startswith('_'):
            self.issues.append(Issue(
                code="S006",
                severity=Severity.INFO,
                category=Category.STYLE,
                message=f"Function '{func_name}' is missing documentation",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Add a docstring describing the function's purpose"
            ))
        
        # Check function length
        if body:
            statements = getattr(body, 'statements', [])
            if len(statements) > self.max_function_lines:
                self.issues.append(Issue(
                    code="S011",
                    severity=Severity.WARNING,
                    category=Category.STYLE,
                    message=f"Function is too long ({len(statements)} statements > {self.max_function_lines})",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Consider breaking into smaller functions"
                ))
    
    def _check_class_style(self, node: ASTNode):
        """Check class naming and documentation."""
        class_name = getattr(node, 'name', None)
        docstring = getattr(node, 'docstring', None)
        
        if not class_name:
            return
        
        location = self.get_node_location(node)
        
        # Check naming convention (PascalCase)
        if not re.match(r'^[A-Z][a-zA-Z0-9]*$', class_name):
            self.issues.append(Issue(
                code="S002",
                severity=Severity.INFO,
                category=Category.STYLE,
                message=f"Class name '{class_name}' should use PascalCase",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Rename to '{self._to_pascal_case(class_name)}'"
            ))
        
        # Check for missing documentation
        if not docstring:
            self.issues.append(Issue(
                code="S007",
                severity=Severity.INFO,
                category=Category.STYLE,
                message=f"Class '{class_name}' is missing documentation",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Add a docstring describing the class's purpose"
            ))
    
    def _check_variable_style(self, node: ASTNode):
        """Check variable naming."""
        var_name = getattr(node, 'name', None)
        is_const = getattr(node, 'is_const', False)
        var_type = getattr(node, 'type', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check constant naming (UPPER_CASE)
        if is_const and not re.match(r'^[A-Z_][A-Z0-9_]*$', var_name):
            self.issues.append(Issue(
                code="S003",
                severity=Severity.INFO,
                category=Category.STYLE,
                message=f"Constant '{var_name}' should use UPPER_CASE",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Rename to '{var_name.upper()}'"
            ))
        
        # Check variable name length
        if len(var_name) < 2 and var_name not in ['i', 'j', 'k', 'n', 'x', 'y', 'z']:
            self.issues.append(Issue(
                code="S004",
                severity=Severity.HINT,
                category=Category.STYLE,
                message=f"Variable name '{var_name}' is too short",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Use a more descriptive name"
            ))
    
    def _check_nesting(self, node: ASTNode):
        """Check nesting depth."""
        self.nesting_level += 1
        
        if self.nesting_level > self.max_nesting:
            location = self.get_node_location(node)
            self.issues.append(Issue(
                code="S013",
                severity=Severity.WARNING,
                category=Category.STYLE,
                message=f"Code is nested too deeply (level {self.nesting_level} > {self.max_nesting})",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Consider extracting nested logic into functions"
            ))
        
        # Process children
        for attr_name in dir(node):
            if attr_name.startswith('_'):
                continue
            attr = getattr(node, attr_name, None)
            if isinstance(attr, list):
                for item in attr:
                    if isinstance(item, ASTNode):
                        self.walk_ast(item, self._check_node)
            elif isinstance(attr, ASTNode):
                self.walk_ast(attr, self._check_node)
        
        self.nesting_level -= 1
    
    def _check_magic_numbers(self, node: ASTNode):
        """Check for magic numbers."""
        value = getattr(node, 'value', None)
        
        if value is None:
            return
        
        # Allow common constants
        allowed_numbers = [0, 1, 2, -1, 10, 100, 1000]
        
        if isinstance(value, (int, float)) and value not in allowed_numbers:
            location = self.get_node_location(node)
            self.issues.append(Issue(
                code="S014",
                severity=Severity.HINT,
                category=Category.STYLE,
                message=f"Magic number {value} should be a named constant",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Define as a constant: MAX_VALUE = {value}"
            ))
    
    def _check_line_issues(self):
        """Check line-based style issues."""
        for line_num, line in enumerate(self.current_lines, 1):
            location_file = self.current_file
            
            # Check line length
            if len(line) > self.max_line_length:
                from ..report import SourceLocation
                self.issues.append(Issue(
                    code="S005",
                    severity=Severity.INFO,
                    category=Category.STYLE,
                    message=f"Line too long ({len(line)} > {self.max_line_length} characters)",
                    location=SourceLocation(file=location_file, line=line_num, column=self.max_line_length),
                    source_line=line[:80] + "...",
                    suggestion="Break the line into multiple lines"
                ))
            
            # Check trailing whitespace
            if line.rstrip() != line.rstrip('\n').rstrip():
                from ..report import SourceLocation
                self.issues.append(Issue(
                    code="S008",
                    severity=Severity.HINT,
                    category=Category.STYLE,
                    message="Trailing whitespace",
                    location=SourceLocation(file=location_file, line=line_num, column=len(line.rstrip())),
                    source_line=line.rstrip('\n'),
                    suggestion="Remove trailing whitespace"
                ))
            
            # Check for TODO/FIXME comments
            if 'TODO' in line or 'FIXME' in line:
                from ..report import SourceLocation
                self.issues.append(Issue(
                    code="S018",
                    severity=Severity.HINT,
                    category=Category.STYLE,
                    message="TODO/FIXME comment found",
                    location=SourceLocation(file=location_file, line=line_num, column=line.find('TODO') if 'TODO' in line else line.find('FIXME')),
                    source_line=line.rstrip('\n'),
                    suggestion="Consider creating a task or issue tracker entry"
                ))
    
    def _to_snake_case(self, name: str) -> str:
        """Convert name to snake_case."""
        # Insert underscores before uppercase letters
        s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
        return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()
    
    def _to_pascal_case(self, name: str) -> str:
        """Convert name to PascalCase."""
        words = re.split(r'[_\s]+', name)
        return ''.join(word.capitalize() for word in words if word)

