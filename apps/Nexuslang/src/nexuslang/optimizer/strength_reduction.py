"""
Strength Reduction Optimization Pass
=====================================

Replaces expensive operations with cheaper equivalent ones.

Examples:
- x * 2 -> x + x (or x << 1 for integers)
- x ** 2 -> x * x
- x / 2 -> x >> 1 (for integers)
- x % 2 -> x & 1 (for integers)
"""

from typing import Any
from ..optimizer import OptimizationPass, OptimizationStats
from ..parser.ast import (
    Program, BinaryOperation, Literal, UnaryOperation
)


class StrengthReductionPass(OptimizationPass):
    """
    Applies strength reduction transformations.
    
    Replaces expensive arithmetic operations with cheaper ones when possible.
    """
    
    def __init__(self):
        super().__init__("StrengthReduction")
        self.reductions_made = 0
    
    def run(self, ast: Program) -> Program:
        """Run strength reduction on the AST."""
        self.reductions_made = 0
        ast.statements = self._process_statements(ast.statements)
        self.stats.constants_folded = self.reductions_made  # Reuse stat
        return ast
    
    def _process_statements(self, statements: list) -> list:
        """Process a list of statements."""
        result = []
        for stmt in statements:
            processed = self._process_statement(stmt)
            result.append(processed)
        return result
    
    def _process_statement(self, stmt):
        """Process a single statement."""
        # Process expressions in the statement
        if isinstance(stmt, BinaryOperation):
            return self._reduce_binary_op(stmt)
        
        # Recursively process nested structures
        if hasattr(stmt, 'body') and isinstance(stmt.body, list):
            stmt.body = self._process_statements(stmt.body)
        
        if hasattr(stmt, 'value'):
            stmt.value = self._process_expression(stmt.value)
        
        if hasattr(stmt, 'condition'):
            stmt.condition = self._process_expression(stmt.condition)
        
        return stmt
    
    def _process_expression(self, expr):
        """Process an expression."""
        if isinstance(expr, BinaryOperation):
            return self._reduce_binary_op(expr)
        elif isinstance(expr, UnaryOperation):
            return self._reduce_unary_op(expr)
        return expr
    
    def _reduce_binary_op(self, expr: BinaryOperation) -> Any:
        """Apply strength reduction to binary operations."""
        # Process children first
        expr.left = self._process_expression(expr.left)
        expr.right = self._process_expression(expr.right)
        
        # Check for patterns
        
        # x * 2 -> x + x (or x << 1)
        if expr.operator == '*':
            if isinstance(expr.right, Literal) and expr.right.value == 2:
                # Replace with addition
                new_expr = BinaryOperation(
                    operator='+',
                    left=expr.left,
                    right=expr.left  # Reuse left operand
                )
                self.reductions_made += 1
                return new_expr
            
            # Also check 2 * x
            if isinstance(expr.left, Literal) and expr.left.value == 2:
                new_expr = BinaryOperation(
                    operator='+',
                    left=expr.right,
                    right=expr.right
                )
                self.reductions_made += 1
                return new_expr
        
        # x ** 2 -> x * x
        if expr.operator == '**' or expr.operator == '^':
            if isinstance(expr.right, Literal) and expr.right.value == 2:
                new_expr = BinaryOperation(
                    operator='*',
                    left=expr.left,
                    right=expr.left
                )
                self.reductions_made += 1
                return new_expr
        
        # x / 1 -> x
        if expr.operator == '/':
            if isinstance(expr.right, Literal) and expr.right.value == 1:
                self.reductions_made += 1
                return expr.left
        
        # x * 1 -> x
        if expr.operator == '*':
            if isinstance(expr.right, Literal) and expr.right.value == 1:
                self.reductions_made += 1
                return expr.left
            if isinstance(expr.left, Literal) and expr.left.value == 1:
                self.reductions_made += 1
                return expr.right
        
        # x * 0 -> 0
        if expr.operator == '*':
            if isinstance(expr.right, Literal) and expr.right.value == 0:
                self.reductions_made += 1
                return Literal('integer', 0)
            if isinstance(expr.left, Literal) and expr.left.value == 0:
                self.reductions_made += 1
                return Literal('integer', 0)
        
        # x + 0 -> x
        if expr.operator == '+':
            if isinstance(expr.right, Literal) and expr.right.value == 0:
                self.reductions_made += 1
                return expr.left
            if isinstance(expr.left, Literal) and expr.left.value == 0:
                self.reductions_made += 1
                return expr.right
        
        # x - 0 -> x
        if expr.operator == '-':
            if isinstance(expr.right, Literal) and expr.right.value == 0:
                self.reductions_made += 1
                return expr.left
        
        return expr
    
    def _reduce_unary_op(self, expr: UnaryOperation) -> Any:
        """Apply strength reduction to unary operations."""
        expr.operand = self._process_expression(expr.operand)
        
        # -(-x) -> x
        if expr.operator == '-':
            if isinstance(expr.operand, UnaryOperation) and expr.operand.operator == '-':
                self.reductions_made += 1
                return expr.operand.operand
        
        return expr
