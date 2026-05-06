"""
Simple type inference for the NexusLang language.
This provides basic type inference for variables and expressions.
"""

from typing import Dict, List, Optional, Any, Type
from ..parser.ast import (
    Program, VariableDeclaration, FunctionDefinition, Parameter,
    BinaryOperation, UnaryOperation, Literal, Identifier, FunctionCall,
    Expression, ReturnStatement
)
from ..parser.lexer import Token, TokenType
from ..typesystem.types import (
    Type, ListType, DictionaryType, FunctionType,
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, NULL_TYPE, ANY_TYPE,
    get_type_by_name, infer_type
)


class SimpleTypeInference:
    """Simple type inference engine for NexusLang."""
    
    def __init__(self):
        self.variable_types: Dict[str, Type] = {}
        self.function_return_types: Dict[str, Type] = {}
    
    def reset(self):
        """Reset the inference engine state."""
        self.variable_types.clear()
        self.function_return_types.clear()
    
    def _get_operator_type(self, operator: Any) -> Optional[TokenType]:
        """Extract TokenType from operator (handles both Token objects and strings)."""
        if isinstance(operator, Token):
            return operator.type
        elif isinstance(operator, TokenType):
            return operator
        return None
    
    def infer_expression_type(self, expr: Any, env: Optional[Dict[str, Type]] = None) -> Type:
        """Infer the type of an expression."""
        if env is None:
            env = {}
        
        # Literal values
        if isinstance(expr, Literal):
            return infer_type(expr.value)
        
        # Variable references
        if isinstance(expr, Identifier):
            if expr.name in env:
                return env[expr.name]
            if expr.name in self.variable_types:
                return self.variable_types[expr.name]
            return ANY_TYPE
        
        # Binary operations
        if isinstance(expr, BinaryOperation):
            left_type = self.infer_expression_type(expr.left, env)
            right_type = self.infer_expression_type(expr.right, env)
            
            op_type = self._get_operator_type(expr.operator)
            
            # Arithmetic operators
            if op_type in (TokenType.PLUS, TokenType.MINUS, TokenType.TIMES, TokenType.MODULO):
                if left_type == INTEGER_TYPE and right_type == INTEGER_TYPE:
                    return INTEGER_TYPE
                elif left_type in (INTEGER_TYPE, FLOAT_TYPE) and right_type in (INTEGER_TYPE, FLOAT_TYPE):
                    return FLOAT_TYPE
                # String concatenation with PLUS
                elif op_type == TokenType.PLUS and (left_type == STRING_TYPE or right_type == STRING_TYPE):
                    return STRING_TYPE
            
            # Division always returns float
            elif op_type == TokenType.DIVIDED_BY:
                if left_type in (INTEGER_TYPE, FLOAT_TYPE) and right_type in (INTEGER_TYPE, FLOAT_TYPE):
                    return FLOAT_TYPE
            
            # Power operations return float  
            elif op_type == TokenType.POWER:
                if left_type in (INTEGER_TYPE, FLOAT_TYPE) and right_type in (INTEGER_TYPE, FLOAT_TYPE):
                    return FLOAT_TYPE
            
            # Floor division
            elif op_type == TokenType.FLOOR_DIVIDE:
                if left_type in (INTEGER_TYPE, FLOAT_TYPE) and right_type in (INTEGER_TYPE, FLOAT_TYPE):
                    return INTEGER_TYPE
            
            # Comparison operators
            elif op_type in (TokenType.EQUAL_TO, TokenType.NOT_EQUAL_TO,
                             TokenType.GREATER_THAN, TokenType.LESS_THAN,
                             TokenType.GREATER_THAN_OR_EQUAL_TO, TokenType.LESS_THAN_OR_EQUAL_TO,
                             TokenType.IN):
                return BOOLEAN_TYPE
            
            # Logical operators
            elif op_type in (TokenType.AND, TokenType.OR):
                return BOOLEAN_TYPE
            
            return ANY_TYPE
        
        # Unary operations
        if isinstance(expr, UnaryOperation):
            operand_type = self.infer_expression_type(expr.operand, env)
            
            op_type = self._get_operator_type(expr.operator)
            
            if op_type == TokenType.MINUS and operand_type in (INTEGER_TYPE, FLOAT_TYPE):
                return operand_type
            elif op_type == TokenType.NOT:
                return BOOLEAN_TYPE
            
            return ANY_TYPE
        
        # Function calls
        if isinstance(expr, FunctionCall):
            if expr.name in self.function_return_types:
                return self.function_return_types[expr.name]
            if expr.name in env:
                func_val = env[expr.name]
                if isinstance(func_val, FunctionType):
                    return func_val.return_type
            return ANY_TYPE
        
        # List literals/expressions
        if hasattr(expr, 'node_type') and expr.node_type in ('list_literal', 'list_expression'):
            if hasattr(expr, 'elements') and expr.elements:
                element_type = self.infer_expression_type(expr.elements[0], env)
                return ListType(element_type)
            return ListType(ANY_TYPE)
        
        # Dictionary literals/expressions
        if hasattr(expr, 'node_type') and expr.node_type in ('dictionary_literal', 'dictionary_expression', 'dict_expression'):
            if hasattr(expr, 'entries') and expr.entries:
                # Dictionary has entries as list of (key, value) tuples
                first_entry = expr.entries[0]
                key_type = self.infer_expression_type(first_entry[0], env)
                value_type = self.infer_expression_type(first_entry[1], env)
                return DictionaryType(key_type, value_type)
            return DictionaryType(ANY_TYPE, ANY_TYPE)
        
        return ANY_TYPE
    
    def infer_variable_type(self, declaration: VariableDeclaration, env: Optional[Dict[str, Type]] = None) -> Type:
        """Infer the type of a variable from its declaration."""
        if env is None:
            env = {}
        
        # If explicit type annotation exists, use it
        if hasattr(declaration, 'type_annotation') and declaration.type_annotation:
            return get_type_by_name(declaration.type_annotation)
        
        # Otherwise infer from the initial value
        if hasattr(declaration, 'value') and declaration.value:
            inferred_type = self.infer_expression_type(declaration.value, env)
            # Cache the inferred type
            self.variable_types[declaration.name] = inferred_type
            return inferred_type
        
        return ANY_TYPE
    
    def infer_function_return_type(self, function: FunctionDefinition, env: Optional[Dict[str, Type]] = None) -> Type:
        """Infer the return type of a function."""
        if env is None:
            env = {}
        
        # If explicit return type annotation exists, use it
        if hasattr(function, 'return_type') and function.return_type:
            return_type = get_type_by_name(function.return_type)
            self.function_return_types[function.name] = return_type
            return return_type
        
        # Create local environment with parameter types
        func_env = env.copy()
        for param in function.parameters:
            if hasattr(param, 'type_annotation') and param.type_annotation:
                param_type = get_type_by_name(param.type_annotation)
                func_env[param.name] = param_type
            else:
                func_env[param.name] = ANY_TYPE
        
        # Look for return statements and infer type
        return_types = []
        for stmt in function.body:
            if isinstance(stmt, ReturnStatement):
                if stmt.value:
                    return_type = self.infer_expression_type(stmt.value, func_env)
                    return_types.append(return_type)
                else:
                    return_types.append(NULL_TYPE)
        
        if not return_types:
            # No explicit return statements
            inferred_type = NULL_TYPE
        elif all(t == return_types[0] for t in return_types):
            # All return types are the same
            inferred_type = return_types[0]
        else:
            # Mixed return types - use ANY for now
            inferred_type = ANY_TYPE
        
        self.function_return_types[function.name] = inferred_type
        return inferred_type
    
    def infer_program_types(self, program: Program) -> Dict[str, Type]:
        """Infer types for all variables and functions in a program."""
        env = {}
        
        for stmt in program.statements:
            if isinstance(stmt, VariableDeclaration):
                var_type = self.infer_variable_type(stmt, env)
                env[stmt.name] = var_type
            elif isinstance(stmt, FunctionDefinition):
                func_return_type = self.infer_function_return_type(stmt, env)
                # Create function type
                param_types = []
                for param in stmt.parameters:
                    if hasattr(param, 'type_annotation') and param.type_annotation:
                        param_types.append(get_type_by_name(param.type_annotation))
                    else:
                        param_types.append(ANY_TYPE)
                func_type = FunctionType(param_types, func_return_type)
                env[stmt.name] = func_type
        
        return env


# Global instance for easy access
_type_inference = SimpleTypeInference()


def infer_variable_type(declaration: VariableDeclaration, env: Optional[Dict[str, Type]] = None) -> Type:
    """Convenience function to infer a variable's type."""
    return _type_inference.infer_variable_type(declaration, env)


def infer_expression_type(expr: Expression, env: Optional[Dict[str, Type]] = None) -> Type:
    """Convenience function to infer an expression's type."""
    return _type_inference.infer_expression_type(expr, env)


def infer_function_return_type(function: FunctionDefinition, env: Optional[Dict[str, Type]] = None) -> Type:
    """Convenience function to infer a function's return type."""
    return _type_inference.infer_function_return_type(function, env)


def infer_program_types(program: Program) -> Dict[str, Type]:
    """Convenience function to infer types for an entire program."""
    return _type_inference.infer_program_types(program)
