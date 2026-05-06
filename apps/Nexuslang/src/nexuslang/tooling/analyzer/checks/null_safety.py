"""
Null Safety Checker
===================

Detects null pointer issues:
- Null pointer dereferences
- Uninitialized variable use
- Unsafe Optional unwrapping
- Missing null checks
"""

from typing import Dict, Set, List, Optional
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


class VariableState:
    """Track variable nullability state."""
    def __init__(self, name: str, is_nullable: bool = False):
        self.name = name
        self.is_nullable = is_nullable
        self.is_initialized = False
        self.has_null_check = False


class NullSafetyAnalyzer(BaseChecker):

    CHECKER_NAME = "null_safety"
    """
    Checks for null safety violations.
    
    Detects:
    - Use of uninitialized variables
    - Null pointer dereferences
    - Missing null checks before dereference
    - Unsafe Optional<T> unwrapping
    """
    
    def __init__(self):
        super().__init__()
        self.variables: Dict[str, VariableState] = {}
        self.checked_vars: Set[str] = set()  # Vars checked for null in current block
    
    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        """Run null safety analysis."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.variables = {}
        self.checked_vars = set()
        
        # Walk AST
        self.walk_ast(ast, self._check_node)
        
        return self.issues
    
    def _check_node(self, node: ASTNode):
        """Check individual AST node."""
        node_type = type(node).__name__
        
        # Variable declaration
        if node_type == 'VariableDeclaration':
            self._handle_declaration(node)
        
        # Variable assignment
        elif node_type == 'Assignment':
            self._handle_assignment(node)
        
        # Variable use
        elif node_type in ['Identifier', 'VariableReference']:
            self._check_variable_use(node)
        
        # Pointer dereference
        elif node_type == 'PointerDereference':
            self._check_dereference(node)
        
        # Null check (if x is null / if x is not null)
        elif node_type == 'IfStatement':
            self._handle_null_check(node)
        
        # Member access (obj.member)
        elif node_type == 'MemberAccess':
            self._check_member_access(node)
    
    def _handle_declaration(self, node: ASTNode):
        """Handle variable declaration."""
        var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
        if not var_name:
            return
        
        # Check type annotation for nullable
        type_annotation = getattr(node, 'type_annotation', None) or getattr(node, 'var_type', None)
        is_nullable = False
        
        if type_annotation:
            type_str = str(type_annotation)
            is_nullable = 'Optional' in type_str or 'Pointer' in type_str
        
        # Create variable state
        var_state = VariableState(var_name, is_nullable)
        
        # Check if initialized
        has_value = hasattr(node, 'value') and node.value is not None
        var_state.is_initialized = has_value
        
        self.variables[var_name] = var_state
        
        # Warn if nullable without initialization
        if is_nullable and not has_value:
            location = self.get_node_location(node)
            self.issues.append(Issue(
                code="N001",
                severity=Severity.WARNING,
                category=Category.NULL_SAFETY,
                message=f"Nullable variable '{var_name}' declared without initialization",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Initialize '{var_name}' or check for null before use"
            ))
    
    def _handle_assignment(self, node: ASTNode):
        """Handle variable assignment."""
        var_name = None
        
        # Get target variable name
        if hasattr(node, 'target'):
            target = node.target
            if type(target).__name__ in ['Identifier', 'VariableReference']:
                var_name = getattr(target, 'name', None)
        elif hasattr(node, 'name'):
            var_name = node.name
        
        if not var_name:
            return
        
        # Mark as initialized
        if var_name in self.variables:
            self.variables[var_name].is_initialized = True
        else:
            # Undeclared variable - create state
            self.variables[var_name] = VariableState(var_name, is_nullable=False)
            self.variables[var_name].is_initialized = True
    
    def _check_variable_use(self, node: ASTNode):
        """Check variable use for initialization."""
        var_name = getattr(node, 'name', None)
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if declared
        if var_name not in self.variables:
            self.issues.append(Issue(
                code="N002",
                severity=Severity.ERROR,
                category=Category.NULL_SAFETY,
                message=f"Use of undeclared variable '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Declare '{var_name}' before use"
            ))
            return
        
        var_state = self.variables[var_name]
        
        # Check if initialized
        if not var_state.is_initialized:
            self.issues.append(Issue(
                code="N003",
                severity=Severity.ERROR,
                category=Category.NULL_SAFETY,
                message=f"Use of uninitialized variable '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Initialize '{var_name}' before use",
                fix=f"set {var_name} to <default_value>"
            ))
    
    def _check_dereference(self, node: ASTNode):
        """Check pointer dereference for null safety."""
        # Get pointer being dereferenced
        ptr_expr = getattr(node, 'expression', None) or getattr(node, 'pointer', None)
        if not ptr_expr:
            return
        
        var_name = None
        if type(ptr_expr).__name__ in ['Identifier', 'VariableReference']:
            var_name = getattr(ptr_expr, 'name', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if variable exists
        if var_name not in self.variables:
            return
        
        var_state = self.variables[var_name]
        
        # Check if nullable
        if var_state.is_nullable:
            # Check if we've seen a null check
            if var_name not in self.checked_vars:
                self.issues.append(Issue(
                    code="N004",
                    severity=Severity.WARNING,
                    category=Category.NULL_SAFETY,
                    message=f"Dereferencing potentially null pointer '{var_name}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Add null check before dereferencing",
                    fix=f"if {var_name} is not null\n    # dereference here\nend"
                ))
    
    def _handle_null_check(self, node: ASTNode):
        """Handle null check in if statement."""
        # Check if condition is a null check
        condition = getattr(node, 'condition', None)
        if not condition:
            return
        
        # Look for patterns like:
        # - x is null
        # - x is not null
        # - x equals null
        
        cond_type = type(condition).__name__
        
        if cond_type in ['BinaryOperation', 'Comparison']:
            left = getattr(condition, 'left', None)
            right = getattr(condition, 'right', None)
            operator = getattr(condition, 'operator', None)
            
            if not (left and right and operator):
                return
            
            # Check if comparing with null
            var_name = None
            is_null_check = False
            
            # Pattern: x is null / x is not null
            if operator in ['is', 'is_not', 'equals', 'not_equals']:
                if type(left).__name__ in ['Identifier', 'VariableReference']:
                    var_name = getattr(left, 'name', None)
                    if type(right).__name__ == 'NullLiteral':
                        is_null_check = True
                elif type(right).__name__ in ['Identifier', 'VariableReference']:
                    var_name = getattr(right, 'name', None)
                    if type(left).__name__ == 'NullLiteral':
                        is_null_check = True
            
            if is_null_check and var_name:
                # This is a null check - track it
                if operator in ['is_not', 'not_equals']:
                    # Positive check: x is not null
                    self.checked_vars.add(var_name)
    
    def _check_member_access(self, node: ASTNode):
        """Check member access on potentially null object."""
        obj_expr = getattr(node, 'object', None)
        if not obj_expr:
            return
        
        var_name = None
        if type(obj_expr).__name__ in ['Identifier', 'VariableReference']:
            var_name = getattr(obj_expr, 'name', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if variable exists and is nullable
        if var_name in self.variables:
            var_state = self.variables[var_name]
            
            if var_state.is_nullable and var_name not in self.checked_vars:
                member = getattr(node, 'member', None) or getattr(node, 'attribute', None)
                self.issues.append(Issue(
                    code="N005",
                    severity=Severity.WARNING,
                    category=Category.NULL_SAFETY,
                    message=f"Accessing member of potentially null object '{var_name}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Add null check before accessing '{var_name}.{member}'",
                    fix=f"if {var_name} is not null\n    # access {var_name}.{member}\nend"
                ))
