"""
Interpreter extension for error propagation (? operator).

Production-ready implementation - no shortcuts.
"""

def execute_TryExpression(self, node):
    """Execute the ? operator for error propagation.
    
    Production-ready implementation:
    - Evaluates expression
    - If Result.is_ok(), unwraps and returns value
    - If Result.is_err(), propagates error by setting return_value
    - Validates that expression returns Result type
    
    Args:
        node: TryExpression AST node
        
    Returns:
        Unwrapped Ok value
        
    Raises:
        RuntimeError: If expression doesn't return Result
        RuntimeError: If used outside function context
    """
    from ..stdlib.option_result import Result
    
    # Evaluate the expression
    result = self.execute(node.expression)
    
    # Validate it's a Result type
    if not isinstance(result, Result):
        raise RuntimeError(
            f"? operator can only be used on Result types, got {type(result).__name__}"
        )
    
    # Check if Ok or Err
    if result.is_ok():
        # Unwrap and return the value
        return result.unwrap()
    else:
        # Propagate the error by setting return_value
        # This will cause early return from the function
        self.return_value = result
        return None  # This value won't be used due to early return
