"""
Constant Folding and Propagation Pass
=====================================

Evaluates constant expressions at compile time:
- Arithmetic: 2 + 3 → 5
- Boolean: true and false → false  
- String: "hello" + " world" → "hello world"
- Comparisons: 5 > 3 → true
"""

from typing import Any, Optional
import copy
from nexuslang.optimizer import OptimizationPass


class ConstantFoldingPass(OptimizationPass):
    """
    Constant folding optimization pass.
    
    Evaluates constant expressions at compile time to reduce
    runtime computation.
    """
    
    def __init__(self):
        super().__init__("Constant Folding")
    
    def run(self, ast: Any) -> Any:
        """Run constant folding on the AST."""
        optimized_ast = copy.deepcopy(ast)
        self._fold_constants(optimized_ast)
        return optimized_ast
    
    def _fold_constants(self, node: Any):
        """Recursively fold constants in the AST."""
        # First, recursively fold children
        if hasattr(node, 'statements'):
            for stmt in node.statements:
                self._fold_constants(stmt)
        
        if hasattr(node, 'body') and isinstance(node.body, list):
            for stmt in node.body:
                self._fold_constants(stmt)
        
        if hasattr(node, 'then_block') and isinstance(node.then_block, list):
            for stmt in node.then_block:
                self._fold_constants(stmt)
        
        if hasattr(node, 'else_block') and isinstance(node.else_block, list):
            for stmt in node.else_block:
                self._fold_constants(stmt)
        
        # Now fold this node if it's a binary operation
        if type(node).__name__ == 'BinaryOperation':
            self._fold_binary_operation(node)
        
        elif type(node).__name__ == 'UnaryOperation':
            self._fold_unary_operation(node)
    
    def _fold_binary_operation(self, node: Any):
        """Fold binary operations with constant operands."""
        # Check if both operands are literals
        left_type = type(node.left).__name__
        right_type = type(node.right).__name__
        
        if left_type != 'Literal' or right_type != 'Literal':
            return
        
        left_val = node.left.value
        right_val = node.right.value
        operator = node.operator
        
        result = None
        
        # Arithmetic operations
        if operator in ('+', 'plus'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val + right_val
            elif isinstance(left_val, str) and isinstance(right_val, str):
                result = left_val + right_val
        
        elif operator in ('-', 'minus'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val - right_val
        
        elif operator in ('*', 'times'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val * right_val
        
        elif operator in ('/', 'divided'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                if right_val != 0:
                    result = left_val / right_val
        
        elif operator in ('%', 'mod', 'modulo'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                if right_val != 0:
                    result = left_val % right_val
        
        # Comparison operations
        elif operator in ('==', 'equals', 'is equal to'):
            result = left_val == right_val
        
        elif operator in ('!=', 'not equals', 'is not equal to'):
            result = left_val != right_val
        
        elif operator in ('<', 'less than', 'is less than'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val < right_val
        
        elif operator in ('>', 'greater than', 'is greater than'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val > right_val
        
        elif operator in ('<=', 'less than or equal to', 'is less than or equal to'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val <= right_val
        
        elif operator in ('>=', 'greater than or equal to', 'is greater than or equal to'):
            if isinstance(left_val, (int, float)) and isinstance(right_val, (int, float)):
                result = left_val >= right_val
        
        # Boolean operations
        elif operator in ('and', 'AND'):
            if isinstance(left_val, bool) and isinstance(right_val, bool):
                result = left_val and right_val
        
        elif operator in ('or', 'OR'):
            if isinstance(left_val, bool) and isinstance(right_val, bool):
                result = left_val or right_val
        
        # If we computed a result, replace the operation with a literal
        if result is not None:
            # Import Literal here to avoid circular dependency
            from ..parser.ast import Literal
            
            # Determine literal type
            if isinstance(result, bool):
                lit_type = 'boolean'
            elif isinstance(result, int):
                lit_type = 'integer'
            elif isinstance(result, float):
                lit_type = 'float'
            elif isinstance(result, str):
                lit_type = 'string'
            else:
                return
            
            # Replace the binary operation with a literal
            node.__class__ = Literal
            node.type = lit_type
            node.value = result
            
            # Remove binary operation attributes
            if hasattr(node, 'left'):
                delattr(node, 'left')
            if hasattr(node, 'right'):
                delattr(node, 'right')
            if hasattr(node, 'operator'):
                delattr(node, 'operator')
            
            self.stats.constants_folded += 1
    
    def _fold_unary_operation(self, node: Any):
        """Fold unary operations with constant operands."""
        # Check if operand is a literal
        if type(node.operand).__name__ != 'Literal':
            return
        
        operand_val = node.operand.value
        operator = node.operator
        
        result = None
        
        # Numeric negation
        if operator in ('-', 'negative'):
            if isinstance(operand_val, (int, float)):
                result = -operand_val
        
        # Boolean negation
        elif operator in ('not', 'NOT', '!'):
            if isinstance(operand_val, bool):
                result = not operand_val
        
        # If we computed a result, replace the operation with a literal
        if result is not None:
            from ..parser.ast import Literal
            
            # Determine literal type
            if isinstance(result, bool):
                lit_type = 'boolean'
            elif isinstance(result, int):
                lit_type = 'integer'
            elif isinstance(result, float):
                lit_type = 'float'
            else:
                return
            
            # Replace the unary operation with a literal
            node.__class__ = Literal
            node.type = lit_type
            node.value = result
            
            # Remove unary operation attributes
            if hasattr(node, 'operand'):
                delattr(node, 'operand')
            if hasattr(node, 'operator'):
                delattr(node, 'operator')
            
            self.stats.constants_folded += 1
