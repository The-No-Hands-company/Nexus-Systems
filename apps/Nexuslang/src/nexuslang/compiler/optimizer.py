"""
Compiler Optimizers for NexusLang

Includes:
- Bounds Check Optimizer: Eliminates redundant array bounds checks
- Constant Folder: Evaluates constant expressions at compile time
- Dead Code Eliminator: Removes unreachable code
"""

from typing import Any, Optional

class BoundsCheckOptimizer:
    """Analyzes array accesses to determine if bounds checks can be eliminated."""
    
    def __init__(self):
        self.safe_accesses = set()  # (array_name, index_identifier)
        self.loop_bounds = {}       # loop_var -> (start, end)
        self.array_sizes = {}       # array_name -> size (compile-time known)
    
    def set_array_size(self, array_name, size):
        """Register an array's compile-time known size."""
        self.array_sizes[array_name] = size
    
    def set_loop_bounds(self, loop_var, start, end):
        """Register loop induction variable bounds."""
        self.loop_bounds[loop_var] = (start, end)
    
    def clear_loop_bounds(self, loop_var):
        """Clear loop bounds when exiting loop scope."""
        if loop_var in self.loop_bounds:
            del self.loop_bounds[loop_var]
    
    def analyze_constant_index(self, array_name, index_value):
        """
        Check if constant index access is provably safe.
        
        Args:
            array_name: Name of the array
            index_value: Constant index value (int)
            
        Returns:
            True if access is provably safe, False otherwise
        """
        if array_name not in self.array_sizes:
            return False
        
        array_size = self.array_sizes[array_name]
        
        # Check: 0 <= index < size
        if isinstance(index_value, int) and isinstance(array_size, int):
            return 0 <= index_value < array_size
        
        return False
    
    def analyze_loop_variable(self, array_name, loop_var):
        """
        Check if loop induction variable access is provably safe.
        
        Args:
            array_name: Name of the array
            loop_var: Loop induction variable name
            
        Returns:
            True if all accesses in loop are provably safe, False otherwise
        """
        if array_name not in self.array_sizes:
            return False
        
        if loop_var not in self.loop_bounds:
            return False
        
        array_size = self.array_sizes[array_name]
        start, end = self.loop_bounds[loop_var]
        
        # Check: start >= 0 and end <= array_size
        if isinstance(start, int) and isinstance(end, int) and isinstance(array_size, int):
            return start >= 0 and end <= array_size
        
        return False
    
    def is_safe_access(self, array_name, index_info):
        """
        Determine if an array access is provably safe.
        
        Args:
            array_name: Name of the array being accessed
            index_info: Information about the index (constant value or variable name)
            
        Returns:
            True if the access is provably safe and bounds check can be eliminated
        """
        # Pattern 1: Constant index
        if isinstance(index_info, int):
            return self.analyze_constant_index(array_name, index_info)
        
        # Pattern 2: Loop induction variable
        if isinstance(index_info, str) and index_info in self.loop_bounds:
            return self.analyze_loop_variable(array_name, index_info)
        
        # Pattern 3: Explicitly marked safe (e.g., after guard check)
        if (array_name, index_info) in self.safe_accesses:
            return True
        
        return False
    
    def mark_safe_access(self, array_name, index_info):
        """Mark an access as safe (e.g., after explicit bounds check in user code)."""
        self.safe_accesses.add((array_name, index_info))
    
    def reset(self):
        """Reset optimizer state (e.g., between functions)."""
        self.safe_accesses.clear()
        self.loop_bounds.clear()
        # Keep array_sizes as they're global


class ConstantFolder:
    """Constant folding optimization - evaluates constant expressions at compile time.
    
    Optimizations:
    - Arithmetic: 2 + 3 → 5, 10 * 0 → 0
    - Logical: true and false → false, not true → false  
    - Comparisons: 5 > 3 → true, 'a' == 'b' → false
    - String concatenation: 'hello' + ' world' → 'hello world'
    - Bitwise: 5 & 3 → 1, 8 >> 2 → 2
    """
    
    def fold_expression(self, node):
        """Recursively fold constants in an expression tree."""
        from ..parser.ast import BinaryOperation, UnaryOperation, Literal
        
        if isinstance(node, Literal):
            return node  # Already a constant
        
        elif isinstance(node, BinaryOperation):
            return self._fold_binary_op(node)
        
        elif isinstance(node, UnaryOperation):
            return self._fold_unary_op(node)
        
        # Not a foldable expression
        return node
    
    def _fold_binary_op(self, node):
        """Fold binary operations with constant operands."""
        from ..parser.ast import BinaryOperation, Literal
        
        # Recursively fold operands
        left = self.fold_expression(node.left)
        right = self.fold_expression(node.right)
        
        # Both operands must be literals to fold
        if not (isinstance(left, Literal) and isinstance(right, Literal)):
            # Update with folded children
            node.left = left
            node.right = right
            return node
        
        # Extract values
        left_val = left.value
        right_val = right.value
        
        # Get operator string
        if hasattr(node.operator, 'lexeme'):
            op = node.operator.lexeme.lower()
        elif hasattr(node.operator, 'type'):
            op_type = str(node.operator.type)
            op = op_type.split('.')[-1].lower()
        else:
            op = str(node.operator).lower()
        
        # Evaluate the operation
        result = self._evaluate_binary(left_val, op, right_val)
        
        if result is not None:
            # Determine the result type
            result_type = type(result).__name__
            return Literal(result_type, result)
        
        # Couldn't fold, return with folded children
        node.left = left
        node.right = right
        return node
    
    def _evaluate_binary(self, left: Any, op: str, right: Any) -> Optional[Any]:
        """Evaluate a binary operation on constants."""
        try:
            # Arithmetic
            if op in ('plus', '+'):
                if isinstance(left, str) or isinstance(right, str):
                    return str(left) + str(right)
                return left + right
            elif op in ('minus', '-'):
                return left - right
            elif op in ('times', '*', 'multiply'):
                return left * right
            elif op in ('divided by', 'divided_by', '/', 'divide'):
                if right == 0:
                    return None  # Don't fold division by zero
                if isinstance(left, int) and isinstance(right, int):
                    return left // right
                return left / right
            elif op in ('modulo', 'mod', '%'):
                if right == 0:
                    return None
                return left % right
            elif op in ('power', '**'):
                return left ** right
            
            # Comparisons
            elif op in ('greater than', 'greater_than', '>'):
                return left > right
            elif op in ('less than', 'less_than', '<'):
                return left < right
            elif op in ('equal to', 'equal_to', '==', 'is'):
                return left == right
            elif op in ('not equal to', 'not_equal_to', '!='):
                return left != right
            elif op in ('greater than or equal to', 'greater_than_or_equal_to', '>='):
                return left >= right
            elif op in ('less than or equal to', 'less_than_or_equal_to', '<='):
                return left <= right
            
            # Logical
            elif op in ('and', '&&'):
                return bool(left) and bool(right)
            elif op in ('or', '||'):
                return bool(left) or bool(right)
            
            # Bitwise
            elif op in ('bitwise and', 'bitwise_and', '&'):
                return int(left) & int(right)
            elif op in ('bitwise or', 'bitwise_or', '|'):
                return int(left) | int(right)
            elif op in ('bitwise xor', 'bitwise_xor', '^'):
                return int(left) ^ int(right)
            elif op in ('left shift', 'left_shift', '<<'):
                return int(left) << int(right)
            elif op in ('right shift', 'right_shift', '>>'):
                return int(left) >> int(right)
        except:
            return None  # Evaluation failed
        
        return None  # Unknown operator
    
    def _fold_unary_op(self, node):
        """Fold unary operations with constant operands."""
        from ..parser.ast import UnaryOperation, Literal
        
        # Recursively fold operand
        operand = self.fold_expression(node.operand)
        
        # Operand must be literal to fold
        if not isinstance(operand, Literal):
            node.operand = operand
            return node
        
        value = operand.value
        
        # Get operator
        if hasattr(node.operator, 'lexeme'):
            op = node.operator.lexeme.lower()
        elif hasattr(node.operator, 'type'):
            op_type = str(node.operator.type)
            op = op_type.split('.')[-1].lower()
        else:
            op = str(node.operator).lower()
        
        # Evaluate
        result = self._evaluate_unary(op, value)
        
        if result is not None:
            result_type = type(result).__name__
            return Literal(result_type, result)
        
        node.operand = operand
        return node
    
    def _evaluate_unary(self, op: str, value: Any) -> Optional[Any]:
        """Evaluate a unary operation on a constant."""
        try:
            if op in ('not', '!'):
                return not bool(value)
            elif op in ('minus', '-', 'negate'):
                return -value
            elif op in ('bitwise not', 'bitwise_not', '~'):
                return ~int(value)
        except:
            return None
        
        return None


class DeadCodeEliminator:
    """Dead code elimination optimization - removes unreachable code.
    
    Removes:
    - Code after return statements
    - Code after break/continue statements  
    - Code in 'if false' blocks
    - Unreachable code after unconditional jumps
    """
    
    def eliminate_dead_code(self, statements):
        """
        Remove unreachable statements from a list.
        
        Returns a new list with dead code removed.
        """
        from ..parser.ast import (
            ReturnStatement, BreakStatement, ContinueStatement,
            IfStatement, WhileLoop, ForLoop, Block,
            Literal, FunctionDefinition
        )
        
        result = []
        reachable = True
        
        for stmt in statements:
            # If we've hit unreachable code, skip everything
            if not reachable:
                continue
            
            # Process function definitions (they have their own scope)
            if isinstance(stmt, FunctionDefinition):
                if stmt.body:
                    stmt.body = self.eliminate_dead_code(stmt.body)
                result.append(stmt)
                continue
            
            # Process control flow statements
            if isinstance(stmt, IfStatement):
                # Check if condition is constant
                if isinstance(stmt.condition, Literal):
                    if stmt.condition.value:
                        # Condition is always true - keep then block, drop else
                        stmt.then_block = self.eliminate_dead_code(stmt.then_block)
                        stmt.else_block = []
                    else:
                        # Condition is always false - keep else block, drop then
                        stmt.then_block = []
                        if stmt.else_block:
                            stmt.else_block = self.eliminate_dead_code(stmt.else_block)
                else:
                    # Dynamic condition - process both blocks
                    stmt.then_block = self.eliminate_dead_code(stmt.then_block)
                    if stmt.else_block:
                        stmt.else_block = self.eliminate_dead_code(stmt.else_block)
                
                result.append(stmt)
                continue
            
            elif isinstance(stmt, WhileLoop):
                # Process loop body
                if stmt.body:
                    stmt.body = self.eliminate_dead_code(stmt.body)
                result.append(stmt)
                continue
            
            elif isinstance(stmt, ForLoop):
                # Process loop body
                if stmt.body:
                    stmt.body = self.eliminate_dead_code(stmt.body)
                result.append(stmt)
                continue
            
            elif isinstance(stmt, Block):
                # Process block contents
                stmt.statements = self.eliminate_dead_code(stmt.statements)
                result.append(stmt)
                continue
            
            # Add current statement
            result.append(stmt)
            
            # Check if this statement makes subsequent code unreachable
            if isinstance(stmt, (ReturnStatement, BreakStatement, ContinueStatement)):
                reachable = False
        
        return result
    
    def optimize_function(self, func_def):
        """Optimize a function definition by removing dead code."""
        if func_def.body:
            func_def.body = self.eliminate_dead_code(func_def.body)
        return func_def
