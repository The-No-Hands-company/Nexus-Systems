"""
Type Safety Checker
===================

Detects type safety violations:
- Type mismatches
- Unsafe casts
- Implicit conversions
- Incompatible operations
- Wrong number of arguments
"""

from typing import Dict, Set, List, Optional, Any, Type
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category


class TypeSafetyChecker(BaseChecker):
    """
    Checks for type safety violations.
    
    Error codes:
    - T001: Type mismatch in assignment
    - T002: Type mismatch in function call
    - T003: Type mismatch in return statement
    - T004: Type mismatch in operation
    - T005: Incompatible types in comparison
    - T006: Unsafe cast
    - T007: Wrong number of arguments
    - T008: Unknown type
    - T009: Incompatible operand types
    - T010: Type annotation missing where required
    """
    
    def __init__(self):
        super().__init__()
        self.variables: Dict[str, str] = {}  # var_name -> type
        self.functions: Dict[str, Dict[str, Any]] = {}  # func_name -> {params, return_type}
        self.type_hierarchy = {
            'Integer': ['Number'],
            'Float': ['Number'],
            'String': [],
            'Boolean': [],
            'Pointer': [],
            'List': [],
            'Dict': [],
        }
    
    def check(self, ast: ASTNode) -> List[Issue]:
        """Check AST for type safety issues."""
        self.issues = []
        self.variables = {}
        self.functions = {}
        
        # First pass: collect type information
        self.walk_ast(ast, self._collect_types)
        
        # Second pass: check type usage
        self.walk_ast(ast, self._check_node)
        
        return self.issues
    
    def _collect_types(self, node: ASTNode):
        """Collect type information from declarations."""
        node_type = node.__class__.__name__
        
        if node_type == 'VariableDeclaration':
            var_name = getattr(node, 'name', None)
            var_type = getattr(node, 'type', None)
            if var_name:
                self.variables[var_name] = var_type or 'Any'
        
        elif node_type == 'FunctionDefinition':
            func_name = getattr(node, 'name', None)
            params = getattr(node, 'parameters', [])
            return_type = getattr(node, 'return_type', None)
            
            if func_name:
                param_types = []
                for param in params:
                    param_type = getattr(param, 'type', 'Any')
                    param_types.append(param_type)
                
                self.functions[func_name] = {
                    'params': param_types,
                    'return_type': return_type or 'Any'
                }
    
    def _check_node(self, node: ASTNode):
        """Check a node for type issues."""
        node_type = node.__class__.__name__
        
        if node_type == 'VariableDeclaration':
            self._check_variable_declaration(node)
        elif node_type == 'Assignment':
            self._check_assignment(node)
        elif node_type == 'FunctionCall':
            self._check_function_call(node)
        elif node_type == 'Return':
            self._check_return(node)
        elif node_type == 'BinaryOperation':
            self._check_binary_operation(node)
        elif node_type == 'TypeCast':
            self._check_type_cast(node)
    
    def _check_variable_declaration(self, node: ASTNode):
        """Check variable declaration for type issues."""
        var_name = getattr(node, 'name', None)
        declared_type = getattr(node, 'type', None)
        value = getattr(node, 'value', None)
        
        if declared_type and value:
            value_type = self._infer_type(value)
            if not self._is_compatible(value_type, declared_type):
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="T001",
                    severity=Severity.ERROR,
                    category=Category.TYPE_SAFETY,
                    message=f"Type mismatch: cannot assign '{value_type}' to variable of type '{declared_type}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Change the type of '{var_name}' to '{value_type}' or cast the value"
                ))
    
    def _check_assignment(self, node: ASTNode):
        """Check assignment for type compatibility."""
        target = getattr(node, 'target', None)
        value = getattr(node, 'value', None)
        
        if not target or not value:
            return
        
        # Get target variable name
        target_name = None
        if hasattr(target, 'name'):
            target_name = target.name
        elif hasattr(target, 'identifier'):
            target_name = target.identifier
        
        if target_name and target_name in self.variables:
            expected_type = self.variables[target_name]
            actual_type = self._infer_type(value)
            
            if not self._is_compatible(actual_type, expected_type):
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="T001",
                    severity=Severity.ERROR,
                    category=Category.TYPE_SAFETY,
                    message=f"Type mismatch: cannot assign '{actual_type}' to '{target_name}' of type '{expected_type}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Cast the value to '{expected_type}' or change the variable type"
                ))
    
    def _check_function_call(self, node: ASTNode):
        """Check function call arguments."""
        func_name = getattr(node, 'name', None) or getattr(node, 'function_name', None)
        args = getattr(node, 'arguments', [])
        
        if func_name and func_name in self.functions:
            func_info = self.functions[func_name]
            expected_params = func_info['params']
            
            # Check argument count
            if len(args) != len(expected_params):
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="T007",
                    severity=Severity.ERROR,
                    category=Category.TYPE_SAFETY,
                    message=f"Wrong number of arguments: expected {len(expected_params)}, got {len(args)}",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Provide exactly {len(expected_params)} argument(s)"
                ))
                return
            
            # Check argument types
            for i, (arg, expected_type) in enumerate(zip(args, expected_params)):
                actual_type = self._infer_type(arg)
                if not self._is_compatible(actual_type, expected_type):
                    location = self.get_node_location(node)
                    self.issues.append(Issue(
                        code="T002",
                        severity=Severity.ERROR,
                        category=Category.TYPE_SAFETY,
                        message=f"Type mismatch in argument {i+1}: expected '{expected_type}', got '{actual_type}'",
                        location=location,
                        source_line=self.get_source_line(location.line),
                        suggestion=f"Cast argument to '{expected_type}'"
                    ))
    
    def _check_return(self, node: ASTNode):
        """Check return statement type against function return type.
        
        Validates that return expressions match the declared return type
        of the containing function. Tracks function context during analysis.
        """
        # Get return expression and value
        return_expr = getattr(node, 'value', None)
        if not return_expr:
            # Empty return - check if function expects void/None
            if hasattr(self, '_current_function_return_type'):
                expected = self._current_function_return_type
                if expected and expected.lower() not in ('void', 'none', 'nothing'):
                    location = self._get_location(node)
                    self.issues.append(Issue(
                        code='T008',
                        severity='error',
                        message=f"Function expects return type '{expected}', but return statement has no value",
                        location=location,
                        source_line=self.get_source_line(location.line),
                        suggestion=f"Return a value of type '{expected}' or change function return type to void"
                    ))
            return
        
        # Infer type of return expression
        actual_type = self._infer_type(return_expr)
        if not actual_type:
            return  # Cannot determine type
        
        # Check against function's declared return type
        if hasattr(self, '_current_function_return_type'):
            expected_type = self._current_function_return_type
            if expected_type and not self._types_compatible(actual_type, expected_type):
                location = self._get_location(node)
                self.issues.append(Issue(
                    code='T009',
                    severity='error',
                    message=f"Return type mismatch: expected '{expected_type}', got '{actual_type}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Return value of type '{expected_type}' or update function return type"
                ))
    
    def visit_function(self, node: ASTNode):
        """Track function context for return type checking."""
        # Save previous function context
        prev_return_type = getattr(self, '_current_function_return_type', None)
        
        # Set new function return type
        return_type = getattr(node, 'return_type', None)
        self._current_function_return_type = return_type
        
        # Visit function body
        if hasattr(node, 'body'):
            for stmt in node.body:
                self.visit(stmt)
        
        # Restore previous context
        self._current_function_return_type = prev_return_type
    
    def _check_binary_operation(self, node: ASTNode):
        """Check binary operation for type compatibility."""
        left = getattr(node, 'left', None)
        right = getattr(node, 'right', None)
        operator = getattr(node, 'operator', None)
        
        if not left or not right:
            return
        
        left_type = self._infer_type(left)
        right_type = self._infer_type(right)
        
        # Check for incompatible operations
        if operator in ['plus', 'minus', 'multiply', 'divide', '+', '-', '*', '/']:
            if left_type not in ['Integer', 'Float', 'Number', 'String'] or \
               right_type not in ['Integer', 'Float', 'Number', 'String']:
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="T009",
                    severity=Severity.ERROR,
                    category=Category.TYPE_SAFETY,
                    message=f"Incompatible operand types: '{left_type}' and '{right_type}' for operator '{operator}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Ensure both operands are numeric types"
                ))
        
        # Check for type mismatches in comparisons
        elif operator in ['equals', 'not_equals', '==', '!=']:
            if left_type != right_type and not self._is_compatible(left_type, right_type):
                location = self.get_node_location(node)
                self.issues.append(Issue(
                    code="T005",
                    severity=Severity.WARNING,
                    category=Category.TYPE_SAFETY,
                    message=f"Comparing incompatible types: '{left_type}' and '{right_type}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion="Ensure both operands have compatible types"
                ))
    
    def _check_type_cast(self, node: ASTNode):
        """Check type cast for safety."""
        from_type = self._infer_type(getattr(node, 'expression', None))
        to_type = getattr(node, 'target_type', None)
        
        # Warn about potentially unsafe casts
        unsafe_casts = [
            ('Pointer', 'Integer'),
            ('Integer', 'Pointer'),
            ('String', 'Integer'),
            ('Integer', 'String'),
        ]
        
        if (from_type, to_type) in unsafe_casts:
            location = self.get_node_location(node)
            self.issues.append(Issue(
                code="T006",
                severity=Severity.WARNING,
                category=Category.TYPE_SAFETY,
                message=f"Unsafe cast from '{from_type}' to '{to_type}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion="Ensure the cast is intentional and safe"
            ))
    
    def _infer_type(self, node: ASTNode) -> str:
        """Infer type from an expression node."""
        if node is None:
            return 'Any'
        
        node_type = node.__class__.__name__
        
        if node_type == 'IntegerLiteral':
            return 'Integer'
        elif node_type == 'FloatLiteral':
            return 'Float'
        elif node_type == 'StringLiteral':
            return 'String'
        elif node_type == 'BooleanLiteral':
            return 'Boolean'
        elif node_type == 'ListLiteral':
            return 'List'
        elif node_type == 'DictLiteral':
            return 'Dict'
        elif node_type == 'NullLiteral':
            return 'Null'
        elif node_type in ['Identifier', 'Variable']:
            var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
            if var_name and var_name in self.variables:
                return self.variables[var_name]
            return 'Any'
        elif node_type == 'FunctionCall':
            func_name = getattr(node, 'name', None) or getattr(node, 'function_name', None)
            if func_name and func_name in self.functions:
                return self.functions[func_name]['return_type']
            return 'Any'
        elif node_type == 'BinaryOperation':
            # Infer type from operation
            left_type = self._infer_type(getattr(node, 'left', None))
            right_type = self._infer_type(getattr(node, 'right', None))
            operator = getattr(node, 'operator', None)
            
            if operator in ['equals', 'not_equals', 'greater', 'less', '==', '!=', '>', '<']:
                return 'Boolean'
            elif operator in ['plus', 'minus', 'multiply', 'divide', '+', '-', '*', '/']:
                if 'Float' in [left_type, right_type]:
                    return 'Float'
                elif 'Integer' in [left_type, right_type]:
                    return 'Integer'
                return 'Number'
            return 'Any'
        
        return 'Any'
    
    def _is_compatible(self, actual_type: str, expected_type: str) -> bool:
        """Check if actual type is compatible with expected type."""
        if actual_type == expected_type:
            return True
        if expected_type == 'Any' or actual_type == 'Any':
            return True
        
        # Check type hierarchy
        if actual_type in self.type_hierarchy:
            if expected_type in self.type_hierarchy[actual_type]:
                return True
        
        return False
from .base import BaseChecker
from ..report import Issue
from nexuslang.parser.ast import ASTNode

class TypeSafetyChecker(BaseChecker):

    CHECKER_NAME = "type_safety"

    def check(self, ast: ASTNode, source: str, lines: list[str]) -> list[Issue]:
        return []
