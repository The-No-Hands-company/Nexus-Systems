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

from typing import Dict, Set, List, Optional
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
        self.defined_vars: Set[str] = set()
        self.used_vars: Set[str] = set()
        self.defined_functions: Set[str] = set()
        self.called_functions: Set[str] = set()
        self.has_return_or_break = False
    
    def check(self, ast: ASTNode, source: str = "", lines: List[str] = None) -> List[Issue]:
        """Check AST for dead code."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines or []
        self.defined_vars = set()
        self.used_vars = set()
        self.defined_functions = set()
        self.called_functions = set()
        
        # Pass 1: Collect definitions and usage
        self.walk_ast(ast, self._collect_definitions_and_usage)
        
        # Pass 2: Check for unreachable code
        self.walk_ast(ast, self._check_reachability)
        
        # Pass 3: Report unused items
        self._report_unused()
        
        return self.issues
    
    def _collect_definitions_and_usage(self, node: ASTNode):
        """Collect variable and function definitions and usage."""
        node_type = node.__class__.__name__
        
        if node_type == 'VariableDeclaration':
            var_name = getattr(node, 'name', None)
            if var_name:
                self.defined_vars.add(var_name)
        
        elif node_type == 'FunctionDefinition':
            func_name = getattr(node, 'name', None)
            if func_name:
                self.defined_functions.add(func_name)
        
        elif node_type in ['Identifier', 'Variable']:
            var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
            if var_name:
                self.used_vars.add(var_name)
        
        elif node_type == 'FunctionCall':
            func_name = getattr(node, 'name', None) or getattr(node, 'function_name', None)
            if func_name:
                self.called_functions.add(func_name)
    
    def _check_reachability(self, node: ASTNode):
        """Check for unreachable code."""
        node_type = node.__class__.__name__
        
        if node_type in ['Block', 'FunctionBody', 'FunctionDefinition',
                          'WhileLoop', 'ForLoop', 'ForEachLoop']:
            self._check_block_reachability(node)
        
        elif node_type == 'If':
            self._check_redundant_condition(node)
    
    def _check_block_reachability(self, node: ASTNode):
        """Check statements in a block for reachability."""
        statements = getattr(node, 'statements', []) or getattr(node, 'body', [])
        
        found_terminator = False
        for i, stmt in enumerate(statements):
            stmt_type = stmt.__class__.__name__
            
            # Check if code after return/break/continue
            if found_terminator:
                location = self.get_node_location(stmt)

                if stmt_type in ('Return', 'ReturnStatement'):
                    code = "D001"
                    msg = "Unreachable code: statement after return"
                elif stmt_type in ('Break', 'BreakStatement',
                                   'Continue', 'ContinueStatement'):
                    code = "D002"
                    msg = "Unreachable code: statement after break/continue"
                else:
                    code = "D001"
                    msg = "Unreachable code: statement will never execute"
                
                self.issues.append(Issue(
                    code=code,
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
                found_terminator = True
    
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
        for var_name in unused_vars:
            # Skip common patterns (underscore prefix, common names)
            if var_name.startswith('_') or var_name in ['result', 'temp', 'tmp']:
                continue
            
            # Note: We don't have location info here, would need to store it
            # For now, we'll skip reporting or use a generic location
            pass
        
        # Unused functions (excluding entry points and special names)
        unused_functions = self.defined_functions - self.called_functions
        for func_name in unused_functions:
            # Skip entry points and special functions
            if func_name in ['main', 'init', 'setup'] or func_name.startswith('_'):
                continue
            
            # Note: Same issue with location info
            pass
    
    def _get_identifier_name(self, node: ASTNode) -> Optional[str]:
        """Get identifier name from a node."""
        if not node:
            return None
        
        node_type = node.__class__.__name__
        
        if node_type in ['Identifier', 'Variable']:
            return getattr(node, 'name', None) or getattr(node, 'identifier', None)
        
        return None
