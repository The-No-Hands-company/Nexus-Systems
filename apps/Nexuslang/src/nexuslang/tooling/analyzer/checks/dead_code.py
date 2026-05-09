"""
Dead Code Checker
=================

Detects unused and unreachable code:
- Unreachable statements
- Unused variables
- Unused functions
- Redundant conditions
- Dead branches
"""

from typing import Dict, Set, List, Optional, Tuple
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category


class DeadCodeChecker(BaseChecker):
    """
    Checks for dead code.

    Error codes:
    - D001: Unreachable code after return
    - D002: Unreachable code after break/continue
    - D003: Unused variable
    - D004: Unused function
    - D005: Redundant condition (always true/false)
    """

    CHECKER_NAME = "dead_code"

    def __init__(self):
        super().__init__()
        self.defined_vars: Set[Tuple[Tuple[str, ...], str]] = set()
        self.used_vars: Set[Tuple[Tuple[str, ...], str]] = set()
        self.defined_functions: Set[str] = set()
        self.called_functions: Set[str] = set()
        self.variable_locations: Dict[Tuple[Tuple[str, ...], str], object] = {}
        self.function_locations: Dict[str, object] = {}
    
    def check(self, ast: ASTNode, source: str = "", lines: List[str] = None) -> List[Issue]:
        """Check AST for dead code."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines or []
        self.defined_vars = set()
        self.used_vars = set()
        self.defined_functions = set()
        self.called_functions = set()
        self.variable_locations = {}
        self.function_locations = {}
        
        # Pass 1: Collect definitions and usage
        self.walk_ast_with_context(ast, self._collect_definitions_and_usage)
        
        # Pass 2: Check for unreachable code
        self.walk_ast_with_context(ast, self._check_reachability)
        
        # Pass 3: Report unused items
        self._report_unused()
        
        return self.issues
    
    def _collect_definitions_and_usage(
        self,
        node: ASTNode,
        scope_stack: Tuple[str, ...],
        ancestors: Tuple[str, ...],
    ):
        """Collect variable and function definitions and usage."""
        node_type = node.__class__.__name__
        
        if node_type == 'VariableDeclaration':
            var_name = getattr(node, 'name', None)
            if var_name:
                key = (scope_stack, var_name)
                self.defined_vars.add(key)
                self.variable_locations.setdefault(key, self.get_node_location(node))
        
        elif node_type == 'FunctionDefinition':
            func_name = getattr(node, 'name', None)
            if func_name and not self._is_nested_function(ancestors) and not self._is_type_member(ancestors):
                self.defined_functions.add(func_name)
                self.function_locations.setdefault(func_name, self.get_node_location(node))
        
        elif node_type in ['Identifier', 'Variable', 'VariableReference']:
            var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
            if var_name:
                resolved = self._resolve_variable_key(scope_stack, var_name)
                if resolved is not None:
                    self.used_vars.add(resolved)
        
        elif node_type == 'FunctionCall':
            func_name = getattr(node, 'name', None) or getattr(node, 'function_name', None)
            if func_name:
                self.called_functions.add(func_name)
    
    def _check_reachability(
        self,
        node: ASTNode,
        _scope_stack: Tuple[str, ...],
        _ancestors: Tuple[str, ...],
    ):
        """Check for unreachable code."""
        node_type = node.__class__.__name__
        
        if node_type in ['Program', 'Block', 'FunctionBody', 'FunctionDefinition', 'MethodDefinition',
                          'WhileLoop', 'ForLoop', 'ForEachLoop']:
            self._check_block_reachability(node)
        
        elif node_type in ['If', 'IfStatement']:
            self._check_redundant_condition(node)
    
    def _check_block_reachability(self, node: ASTNode):
        """Check statements in a block for reachability."""
        statements = getattr(node, 'statements', []) or getattr(node, 'body', [])
        
        terminator_code = None
        for stmt in statements:
            stmt_type = stmt.__class__.__name__
            
            # Check if code after return/break/continue
            if terminator_code:
                location = self.get_node_location(stmt)

                if terminator_code == "D001":
                    msg = "Unreachable code: statement after return"
                else:
                    msg = "Unreachable code: statement after break/continue"
                
                self.issues.append(Issue(
                    code=terminator_code,
                    severity=Severity.WARNING,
                    category=Category.DEAD_CODE,
                    message=msg,
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Remove unreachable code or fix control flow"
                ))
                # Only report once per block
                break
            
            # Mark if we found a terminator
            if stmt_type in ('Return', 'ReturnStatement',
                             'Break', 'BreakStatement',
                             'Continue', 'ContinueStatement'):
                terminator_code = (
                    "D001"
                    if stmt_type in ('Return', 'ReturnStatement')
                    else "D002"
                )
    
    def _check_redundant_condition(self, node: ASTNode):
        """Check for redundant conditions (always true/false)."""
        condition = getattr(node, 'condition', None)
        if not condition:
            return
        
        # Check for literal boolean conditions
        condition_type = condition.__class__.__name__
        
        if condition_type == 'BooleanLiteral':
            value = getattr(condition, 'value', None)
            location = self.get_node_location(node)
            
            if value is True:
                self.issues.append(Issue(
                    code="D005",
                    severity=Severity.WARNING,
                    category=Category.DEAD_CODE,
                    message="Redundant condition: always evaluates to true",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Remove the condition or fix the logic"
                ))
            elif value is False:
                self.issues.append(Issue(
                    code="D005",
                    severity=Severity.WARNING,
                    category=Category.DEAD_CODE,
                    message="Redundant condition: always evaluates to false",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Remove the dead branch or fix the logic"
                ))
        
        # Check for conditions like "x == x"
        elif condition_type == 'BinaryOperation':
            left = getattr(condition, 'left', None)
            right = getattr(condition, 'right', None)
            operator = getattr(condition, 'operator', None)
            
            if left and right and operator in ['equals', '==']:
                left_name = self._get_identifier_name(left)
                right_name = self._get_identifier_name(right)
                
                if left_name and left_name == right_name:
                    location = self.get_node_location(node)
                    self.issues.append(Issue(
                        code="D005",
                        severity=Severity.WARNING,
                        category=Category.DEAD_CODE,
                        message=f"Redundant condition: comparing '{left_name}' with itself",
                        location=location,
                        source_line=self.get_source_line(location.line),
                        suggestion="Remove redundant comparison"
                    ))
    
    def _report_unused(self):
        """Report unused variables and functions."""
        # Unused variables
        unused_vars = self.defined_vars - self.used_vars
        for scope_stack, var_name in sorted(unused_vars, key=lambda item: (self.variable_locations[item].line, self.variable_locations[item].column, item[1])):
            if var_name.startswith('_'):
                continue

            location = self.variable_locations[(scope_stack, var_name)]
            self.issues.append(Issue(
                code="D003",
                severity=Severity.WARNING,
                category=Category.DEAD_CODE,
                message=f"Unused variable '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Remove unused variable or prefix with '_' if intentional"
            ))
        
        # Unused functions (excluding entry points and special names)
        unused_functions = self.defined_functions - self.called_functions
        for func_name in sorted(unused_functions, key=lambda name: (self.function_locations[name].line, self.function_locations[name].column, name)):
            if func_name in ['main', 'init', 'setup'] or func_name.startswith('_'):
                continue

            location = self.function_locations[func_name]
            self.issues.append(Issue(
                code="D004",
                severity=Severity.WARNING,
                category=Category.DEAD_CODE,
                message=f"Unused function '{func_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Remove unused function or invoke it from reachable code"
            ))

    def _resolve_variable_key(self, scope_stack: Tuple[str, ...], var_name: str) -> Optional[Tuple[Tuple[str, ...], str]]:
        """Resolve a variable name to the nearest known declaration scope."""
        for depth in range(len(scope_stack), 0, -1):
            key = (scope_stack[:depth], var_name)
            if key in self.variable_locations:
                return key
        return None

    def _is_nested_function(self, ancestors: Tuple[str, ...]) -> bool:
        """Return True when a function is nested inside another callable scope."""
        return any(name in {'FunctionDefinition', 'MethodDefinition', 'LambdaExpression'} for name in ancestors)

    def _is_type_member(self, ancestors: Tuple[str, ...]) -> bool:
        """Return True for functions declared inside class-like type scopes."""
        return any(name in {'ClassDefinition', 'TraitDefinition', 'InterfaceDefinition'} for name in ancestors)
    
    def _get_identifier_name(self, node: ASTNode) -> Optional[str]:
        """Get identifier name from a node."""
        if not node:
            return None
        
        node_type = node.__class__.__name__
        
        if node_type in ['Identifier', 'Variable']:
            return getattr(node, 'name', None) or getattr(node, 'identifier', None)
        
        return None
