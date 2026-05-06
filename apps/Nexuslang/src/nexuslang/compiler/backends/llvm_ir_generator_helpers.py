    def _evaluate_constant_expr(self, expr):
        """
        Try to evaluate expression to constant value at compile time.
        
        Args:
            expr: AST expression node
            
        Returns:
            Constant value (int) if expression is compile-time constant, None otherwise
        """
        if expr is None:
            return None
        
        # Literal values
        if type(expr).__name__ == 'Literal':
            if isinstance(expr.value, int):
                return expr.value
        
        # Unary operations (e.g., -5)
        elif type(expr).__name__ == 'UnaryOperation':
            if hasattr(expr, 'operator') and hasattr(expr, 'operand'):
                operand_val = self._evaluate_constant_expr(expr.operand)
                if operand_val is not None:
                    # Get operator string
                    op = expr.operator
                    if hasattr(op, 'lexeme'):
                        op_str = op.lexeme
                    elif hasattr(op, 'type'):
                        op_str = str(op.type).lower()
                    else:
                        op_str = str(op)
                    
                    if op_str in ('-', 'minus', 'negate'):
                        return -operand_val
                    elif op_str in ('+', 'plus'):
                        return operand_val
        
        # Binary operations (e.g., 5 + 3)
        elif type(expr).__name__ == 'BinaryOperation':
            if hasattr(expr, 'left') and hasattr(expr, 'right') and hasattr(expr, 'operator'):
                left_val = self._evaluate_constant_expr(expr.left)
                right_val = self._evaluate_constant_expr(expr.right)
                
                if left_val is not None and right_val is not None:
                    op = expr.operator
                    if hasattr(op, 'lexeme'):
                        op_str = op.lexeme
                    else:
                        op_str = str(op)
                    
                    if op_str in ('+', 'plus'):
                        return left_val + right_val
                    elif op_str in ('-', 'minus'):
                        return left_val - right_val
                    elif op_str in ('*', 'times', 'multiply'):
                        return left_val * right_val
                    elif op_str in ('/', 'divide'):
                        if right_val != 0:
                            return left_val // right_val  # Integer division
        
        return None
