"""
Type inference for the NexusLang language.
This module provides comprehensive type inference functionality.
"""

from typing import Dict, List, Optional, Set, Union, Any, Tuple, Pattern, Type
from ..parser.ast import (
    Program, VariableDeclaration, FunctionDefinition, Parameter,
    BinaryOperation, UnaryOperation, Literal, Identifier, FunctionCall,
    Expression, ClassDefinition, MethodDefinition, ReturnStatement,
    AllocatorHint,
)
from ..typesystem.types import (
    Type, PrimitiveType, ListType, DictionaryType, ClassType, 
    FunctionType, UnionType, AnyType, GenericType, GenericParameter,
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, NULL_TYPE, ANY_TYPE,
    get_type_by_name, infer_type
)


class TypeInferenceEngine:
    """Engine for inferring types in NexusLang programs."""
    
    def __init__(self):
        self.type_constraints: Dict[str, Type] = {}
        self.type_variables: Dict[str, Type] = {}
        self.next_type_var = 0
        self.function_return_types: Dict[str, Type] = {}
        self.variable_types: Dict[str, Type] = {}
    
    def fresh_type_variable(self) -> str:
        """Generate a fresh type variable name."""
        var_name = f"T{self.next_type_var}"
        self.next_type_var += 1
        return var_name
    
    def reset(self):
        """Reset the inference engine state."""
        self.type_constraints.clear()
        self.type_variables.clear()
        self.function_return_types.clear()
        self.variable_types.clear()
        self.next_type_var = 0
    
    def infer_expression_type(self, expr: Expression, env: Dict[str, Type]) -> Type:
        """Infer the type of an expression based on context."""
        if isinstance(expr, Literal):
            return infer_type(expr.value)
        
        if isinstance(expr, Identifier):
            if expr.name in env:
                return env[expr.name]
            if expr.name in self.variable_types:
                return self.variable_types[expr.name]
            return ANY_TYPE  # Unresolved identifier
        
        # Handle MemberAccess (object.property or object.method())
        if hasattr(expr, 'node_type') and expr.node_type == 'member_access':
            return self.infer_member_access_type(expr, env)
        
        # Handle IndexExpression (array[index])
        if hasattr(expr, 'node_type') and expr.node_type == 'index_expression':
            return self.infer_index_expression_type(expr, env)
        
        if isinstance(expr, BinaryOperation):
            left_type = self.infer_expression_type(expr.left, env)
            right_type = self.infer_expression_type(expr.right, env)
            
            # Infer type based on operation
            if expr.operator in ('plus', 'minus', 'times', 'divided by', 'modulo', '+', '-', '*', '/', '%'):
                # If both are integers, result is integer
                if left_type == INTEGER_TYPE and right_type == INTEGER_TYPE:
                    if expr.operator in ('divided by', '/'):
                        return FLOAT_TYPE  # Division always returns float
                    return INTEGER_TYPE
                # If either is float, result is float
                elif left_type in (INTEGER_TYPE, FLOAT_TYPE) and right_type in (INTEGER_TYPE, FLOAT_TYPE):
                    return FLOAT_TYPE
                # String concatenation
                elif expr.operator in ('plus', '+') and (left_type == STRING_TYPE or right_type == STRING_TYPE):
                    return STRING_TYPE
                
            elif expr.operator in ('to the power of', 'power', '**'):
                # Power operations
                if left_type in (INTEGER_TYPE, FLOAT_TYPE) and right_type in (INTEGER_TYPE, FLOAT_TYPE):
                    return FLOAT_TYPE  # Power usually returns float
                
            elif expr.operator in ('equals', 'is equal to', 'not equal to', 'is not equal to', 
                                    'greater than', 'is greater than', 'less than', 'is less than',
                                    'greater than or equal to', 'is greater than or equal to',
                                    'less than or equal to', 'is less than or equal to',
                                    '==', '!=', '<', '>', '<=', '>='):
                return BOOLEAN_TYPE
                
            elif expr.operator in ('and', 'or'):
                if left_type == BOOLEAN_TYPE and right_type == BOOLEAN_TYPE:
                    return BOOLEAN_TYPE
            
            return ANY_TYPE  # Default for unresolved operations
        
        if isinstance(expr, UnaryOperation):
            operand_type = self.infer_expression_type(expr.operand, env)
            
            if expr.operator == '-' and operand_type in (INTEGER_TYPE, FLOAT_TYPE):
                return operand_type
                
            elif expr.operator == 'not' and operand_type == BOOLEAN_TYPE:
                return BOOLEAN_TYPE
                
            return ANY_TYPE  # Default for unresolved operations
        
        if isinstance(expr, FunctionCall):
            # For function calls, check if we know the return type
            if expr.name in env and isinstance(env[expr.name], FunctionType):
                return env[expr.name].return_type
            if expr.name in self.function_return_types:
                return self.function_return_types[expr.name]
            return ANY_TYPE  # Unresolved function
        
        # Handle list literals
        if hasattr(expr, 'node_type') and expr.node_type == 'list_literal':
            if hasattr(expr, 'elements') and expr.elements:
                # Infer element type from first element
                element_type = self.infer_expression_type(expr.elements[0], env)
                return ListType(element_type)
            return ListType(ANY_TYPE)
        
        # Handle dictionary literals
        if hasattr(expr, 'node_type') and expr.node_type == 'dictionary_literal':
            if hasattr(expr, 'keys') and expr.keys:
                key_type = self.infer_expression_type(expr.keys[0], env)
                value_type = self.infer_expression_type(expr.values[0], env)
                return DictionaryType(key_type, value_type)
            return DictionaryType(ANY_TYPE, ANY_TYPE)
        
        return ANY_TYPE  # Default for unresolved expressions
    
    def infer_with_expected_type(self, expr: Expression, expected: Optional[Type], env: Dict[str, Type]) -> Type:
        """
        Infer expression type with expected type hint (bidirectional inference).
        
        This enables context-sensitive type inference where the expected type
        influences how we infer the expression's type.
        
        Args:
            expr: Expression to infer type for
            expected: Expected type from context (e.g., from variable annotation)
            env: Type environment
            
        Returns:
            Inferred type
        """
        if expected is None:
            # No expected type, fall back to regular inference
            return self.infer_expression_type(expr, env)
        
        # Handle list literals with expected element type
        if hasattr(expr, 'node_type') and expr.node_type == 'list_literal':
            if isinstance(expected, ListType):
                element_type = expected.element_type
                # Check all elements are compatible with expected element type
                if hasattr(expr, 'elements'):
                    for elem in expr.elements:
                        elem_type = self.infer_with_expected_type(elem, element_type, env)
                        if not elem_type.is_compatible_with(element_type):
                            # Type mismatch - fall back to regular inference
                            return self.infer_expression_type(expr, env)
                return expected
        
        # Handle dictionary literals with expected key/value types
        if hasattr(expr, 'node_type') and expr.node_type == 'dictionary_literal':
            if isinstance(expected, DictionaryType):
                key_type = expected.key_type
                value_type = expected.value_type
                # Check all entries are compatible
                if hasattr(expr, 'keys') and hasattr(expr, 'values'):
                    for k, v in zip(expr.keys, expr.values):
                        k_type = self.infer_with_expected_type(k, key_type, env)
                        v_type = self.infer_with_expected_type(v, value_type, env)
                        if not (k_type.is_compatible_with(key_type) and v_type.is_compatible_with(value_type)):
                            return self.infer_expression_type(expr, env)
                return expected
        
        # Handle lambda expressions with expected function type
        if hasattr(expr, 'node_type') and expr.node_type == 'lambda_expression':
            if isinstance(expected, FunctionType):
                # Infer lambda types from expected function type
                return self.infer_lambda_types(expr, expected, env)
            else:
                # No expected function type, try to infer from lambda itself
                return self.infer_lambda_types(expr, None, env)
        
        # Handle LambdaExpression class directly
        if expr.__class__.__name__ == 'LambdaExpression':
            if isinstance(expected, FunctionType):
                return self.infer_lambda_types(expr, expected, env)
            else:
                return self.infer_lambda_types(expr, None, env)
        
        # Handle function calls with expected return type
        if isinstance(expr, FunctionCall):
            # Even if we expect a specific return type, we still need to infer the actual return type
            # But we can use the expected type to guide argument type inference
            inferred = self.infer_expression_type(expr, env)
            if inferred.is_compatible_with(expected):
                return expected
            return inferred
        
        # For other expressions, check if inferred type matches expected
        inferred = self.infer_expression_type(expr, env)
        if inferred.is_compatible_with(expected):
            return expected  # Use expected type for better precision
        
        return inferred  # Fall back to inferred type
    
    def infer_variable_declaration(self, declaration: VariableDeclaration, env: Dict[str, Type], generic_context: Optional['GenericTypeContext'] = None) -> Type:
        """Infer the type of a variable declaration."""
        # If there's an explicit type annotation, use it
        if declaration.type_annotation:
            # AllocatorHint carries a base_type plus allocator metadata.
            # For inference purposes use only the base_type.
            raw_annotation = declaration.type_annotation
            effective_annotation = (
                raw_annotation.base_type
                if isinstance(raw_annotation, AllocatorHint)
                else raw_annotation
            )
            type_ = get_type_by_name(effective_annotation)
            
            # If we have a generic context, substitute any generic parameters
            if generic_context:
                type_ = generic_context.get_substituted_type(type_, {})
            
            # Use bidirectional inference with expected type
            if hasattr(declaration, 'value') and declaration.value:
                inferred = self.infer_with_expected_type(declaration.value, type_, env)
                return inferred
            
            return type_
        
        # Otherwise infer from the initial value
        if hasattr(declaration, 'value') and declaration.value:
            return self.infer_expression_type(declaration.value, env)
        
        return ANY_TYPE
    
    def infer_function_return_type(self, function: FunctionDefinition, env: Dict[str, Type]) -> Type:
        """Infer the return type of a function based on its body."""
        # If there's an explicit return type annotation, use it
        if function.return_type:
            return get_type_by_name(function.return_type)
        
        # Create a new environment with parameter types
        func_env = env.copy()
        
        # Create generic context if the function has type parameters
        generic_context = None
        if hasattr(function, 'type_parameters') and function.type_parameters:
            generic_context = self.create_generic_context(
                function.name,
                function.type_parameters,
                getattr(function, 'type_constraints', None)
            )
        
        # Process parameters
        for param in function.parameters:
            if param.type_annotation:
                param_type = get_type_by_name(param.type_annotation)
                if generic_context:
                    param_type = generic_context.get_substituted_type(param_type, {})
                func_env[param.name] = param_type
            else:
                # If parameter has no type annotation, use a type variable
                func_env[param.name] = ANY_TYPE
        
        # Infer types for variables in the function body
        self.infer_types_in_block(function.body, func_env, generic_context)
        
        # Analyze return statements to infer return type
        return_types = []
        for stmt in function.body:
            if hasattr(stmt, 'node_type') and stmt.node_type == 'return_statement':
                if stmt.value:
                    return_type = self.infer_expression_type(stmt.value, func_env)
                    return_types.append(return_type)
                else:
                    return_types.append(NULL_TYPE)
        
        if not return_types:
            # Function has no return statements, assume it returns null
            return NULL_TYPE
            
        # If there are multiple return statements, unify their types
        result_type = return_types[0]
        for rt in return_types[1:]:
            unified = self.unify_types(result_type, rt)
            if unified:
                result_type = unified
            else:
                # If types can't be unified, fallback to ANY
                return ANY_TYPE
                
        return result_type
    
    def infer_class_type(self, class_def: ClassDefinition, env: Dict[str, Type]) -> Type:
        """Infer the type of a class definition."""
        # Create generic context if the class has type parameters
        generic_context = None
        if hasattr(class_def, 'type_parameters') and class_def.type_parameters:
            generic_context = self.create_generic_context(
                class_def.name,
                class_def.type_parameters,
                getattr(class_def, 'type_constraints', None)
            )
        
        # Process properties
        properties = {}
        for prop in class_def.properties:
            if isinstance(prop, PropertyDeclaration):
                prop_type = ANY_TYPE
                if prop.type_annotation:
                    prop_type = get_type_by_name(prop.type_annotation)
                    if generic_context:
                        prop_type = generic_context.get_substituted_type(prop_type, {})
                properties[prop.name] = prop_type
        
        # Process methods
        methods = {}
        for method in class_def.methods:
            if isinstance(method, MethodDefinition):
                # Create a new environment for method scope
                method_env = env.copy()
                
                # Add 'this' parameter
                method_env['this'] = ClassType(class_def.name, properties, {}, None)
                
                # Process method parameters
                param_types = []
                for param in method.parameters:
                    param_type = ANY_TYPE
                    if param.type_annotation:
                        param_type = get_type_by_name(param.type_annotation)
                        if generic_context:
                            param_type = generic_context.get_substituted_type(param_type, {})
                    param_types.append(param_type)
                    method_env[param.name] = param_type
                
                # Infer return type
                return_type = ANY_TYPE
                if method.return_type:
                    return_type = get_type_by_name(method.return_type)
                    if generic_context:
                        return_type = generic_context.get_substituted_type(return_type, {})
                else:
                    return_type = self.infer_function_return_type(method, method_env)
                
                # Create method type
                method_type = FunctionType(param_types, return_type)
                methods[method.name] = method_type
        
        # Create class type
        class_type = ClassType(class_def.name, properties, methods, None)
        
        # If this is a generic class, wrap it in a GenericType
        if generic_context:
            class_type = GenericType(
                class_def.name,
                class_def.type_parameters,
                class_type
            )
        
        return class_type
    
    def unify_types(self, type1: Type, type2: Type) -> Optional[Type]:
        """Unify two types, returning a common supertype if possible."""
        if type1 == type2:
            return type1
        
        if type1 == ANY_TYPE or type2 == ANY_TYPE:
            return ANY_TYPE
        
        if isinstance(type1, UnionType) and isinstance(type2, UnionType):
            # Unify union types by taking the union of their types
            unified_types = set(type1.types + type2.types)
            return UnionType(list(unified_types))
        
        if isinstance(type1, UnionType):
            # If type2 is compatible with any of type1's types, return type1
            if any(type2.is_compatible_with(t) for t in type1.types):
                return type1
        
        if isinstance(type2, UnionType):
            # If type1 is compatible with any of type2's types, return type2
            if any(type1.is_compatible_with(t) for t in type2.types):
                return type2
        
        # If one type is compatible with the other, return the more general type
        if type1.is_compatible_with(type2):
            return type2
        if type2.is_compatible_with(type1):
            return type1
        
        return None  # Types cannot be unified
    
    def infer_types_in_block(self, statements: List[Any], initial_env: Dict[str, Type] = None, generic_context: Optional['GenericTypeContext'] = None) -> Dict[str, Type]:
        """Infer types for variables in a block of statements."""
        env = initial_env or {}
        
        for stmt in statements:
            if isinstance(stmt, VariableDeclaration):
                inferred_type = self.infer_variable_declaration(stmt, env, generic_context)
                env[stmt.name] = inferred_type
        
        return env
    
    def infer_argument_types_from_function(self, function_type: FunctionType, arguments: List[Expression], env: Dict[str, Type]) -> List[Type]:
        """
        Infer argument types using bidirectional inference from function signature.
        
        This uses the function's parameter types to guide inference of argument expressions.
        Especially useful for lambdas and literals where type can be ambiguous.
        
        Args:
            function_type: The function type being called
            arguments: Argument expressions
            env: Type environment
            
        Returns:
            List of inferred argument types
        """
        inferred_types = []
        
        for i, arg in enumerate(arguments):
            # Get expected type from function signature
            expected_type = None
            if i < len(function_type.param_types):
                expected_type = function_type.param_types[i]
            
            # Use bidirectional inference with expected type
            arg_type = self.infer_with_expected_type(arg, expected_type, env)
            inferred_types.append(arg_type)
        
        return inferred_types
    
    def infer_from_return_context(self, expr: Expression, expected_return_type: Type, env: Dict[str, Type]) -> Type:
        """
        Infer expression type with expected return type context.
        
        This helps when the expression is in a return statement and we know
        what type the function should return.
        
        Args:
            expr: Expression being returned
            expected_return_type: Expected return type of the function
            env: Type environment
            
        Returns:
            Inferred type
        """
        return self.infer_with_expected_type(expr, expected_return_type, env)
    
    def infer_lambda_types(self, lambda_expr, expected_func_type: Optional[FunctionType], env: Dict[str, Type]) -> FunctionType:
        """
        Infer parameter and return types for a lambda expression.
        
        This is the core of lambda type inference, using bidirectional inference:
        - If expected_func_type is provided, use it to guide parameter type inference
        - If lambda has explicit types, use those
        - Infer return type from body with expected return type context
        
        Args:
            lambda_expr: LambdaExpression AST node
            expected_func_type: Expected function type from context (or None)
            env: Type environment
            
        Returns:
            Inferred FunctionType for the lambda
        """
        from ..parser.ast import Parameter
        
        # Extract lambda parameters
        params = lambda_expr.parameters if hasattr(lambda_expr, 'parameters') else []
        body = lambda_expr.body if hasattr(lambda_expr, 'body') else None
        explicit_return_type = lambda_expr.return_type if hasattr(lambda_expr, 'return_type') else None
        
        # Infer parameter types
        param_types = []
        lambda_env = env.copy()
        
        for i, param in enumerate(params):
            param_name = param.name if hasattr(param, 'name') else str(param)
            
            # Check if parameter has explicit type annotation
            if hasattr(param, 'type_annotation') and param.type_annotation:
                # Use explicit type
                param_type = get_type_by_name(param.type_annotation)
            elif expected_func_type and i < len(expected_func_type.param_types):
                # Use expected type from context (bidirectional inference!)
                param_type = expected_func_type.param_types[i]
            else:
                # No type information available - use ANY
                param_type = ANY_TYPE
            
            param_types.append(param_type)
            lambda_env[param_name] = param_type
        
        # Infer return type
        if explicit_return_type:
            # Use explicit return type annotation
            return_type = get_type_by_name(explicit_return_type)
        elif expected_func_type:
            # Use expected return type and infer body with it
            if body:
                if isinstance(body, list):
                    # Multi-statement body - infer from return statements
                    return_type = self._infer_lambda_body_type(body, expected_func_type.return_type, lambda_env)
                else:
                    # Single expression body
                    return_type = self.infer_with_expected_type(body, expected_func_type.return_type, lambda_env)
            else:
                return_type = expected_func_type.return_type
        else:
            # No expected return type - infer from body without context
            if body:
                if isinstance(body, list):
                    return_type = self._infer_lambda_body_type(body, None, lambda_env)
                else:
                    return_type = self.infer_expression_type(body, lambda_env)
            else:
                return_type = ANY_TYPE
        
        return FunctionType(param_types, return_type)
    
    def _infer_lambda_body_type(self, statements: List[Any], expected_return_type: Optional[Type], env: Dict[str, Type]) -> Type:
        """
        Infer return type from lambda body statements.
        
        Looks for return statements and infers their types.
        
        Args:
            statements: List of statements in lambda body
            expected_return_type: Expected return type (or None)
            env: Type environment
            
        Returns:
            Inferred return type
        """
        return_types = []
        
        for stmt in statements:
            # Check if this is a return statement
            if hasattr(stmt, 'node_type') and stmt.node_type == 'return_statement':
                if hasattr(stmt, 'value') and stmt.value:
                    if expected_return_type:
                        # Use expected type to guide inference
                        ret_type = self.infer_with_expected_type(stmt.value, expected_return_type, env)
                    else:
                        # Infer without context
                        ret_type = self.infer_expression_type(stmt.value, env)
                    return_types.append(ret_type)
                else:
                    return_types.append(NULL_TYPE)
        
        if not return_types:
            # No return statements - lambda returns NULL
            return NULL_TYPE if not expected_return_type else expected_return_type
        
        # Unify all return types
        result_type = return_types[0]
        for rt in return_types[1:]:
            unified = self.unify_types(result_type, rt)
            if unified:
                result_type = unified
            else:
                # Can't unify - use ANY
                return ANY_TYPE
        
        return result_type
    
    def infer_member_access_type(self, member_access, env: Dict[str, Type]) -> Type:
        """
        Infer type of member access expression: object.member
        
        This handles:
        - Property access: obj.property
        - Method calls: obj.method()
        - Chained access: obj.method().property.another_method()
        
        Args:
            member_access: MemberAccess AST node
            env: Type environment
            
        Returns:
            Type of the accessed member
        """
        # First, infer the type of the object being accessed
        object_type = self.infer_expression_type(member_access.object_expr, env)
        
        if object_type == ANY_TYPE:
            return ANY_TYPE
        
        member_name = member_access.member_name
        
        # Handle class types
        if isinstance(object_type, ClassType):
            # Check if it's a property access
            if member_name in object_type.properties:
                return object_type.properties[member_name]
            
            # Check if it's a method access
            if member_name in object_type.methods:
                method_type = object_type.methods[member_name]
                
                # If this is a method call (has arguments), return the return type
                if hasattr(member_access, 'is_method_call') and member_access.is_method_call:
                    if isinstance(method_type, FunctionType):
                        return method_type.return_type
                    return ANY_TYPE
                else:
                    # Return the method type itself (for function references)
                    return method_type
            
            # Member not found in class
            return ANY_TYPE
        
        # Handle list types - support common list operations
        if isinstance(object_type, ListType):
            if member_name in ('length', 'size', 'count'):
                return INTEGER_TYPE
            elif member_name in ('first', 'last'):
                return object_type.element_type
            elif member_name in ('append', 'add', 'push'):
                # Return function type for method reference
                return FunctionType([object_type.element_type], NULL_TYPE)
            elif member_name in ('pop', 'remove'):
                return FunctionType([INTEGER_TYPE], object_type.element_type)
        
        # Handle dictionary types
        if isinstance(object_type, DictionaryType):
            if member_name in ('keys',):
                return ListType(object_type.key_type)
            elif member_name in ('values',):
                return ListType(object_type.value_type)
            elif member_name in ('items',):
                # Return list of tuples (simplified as ANY for now)
                return ListType(ANY_TYPE)
            elif member_name in ('get', 'has', 'contains'):
                return FunctionType([object_type.key_type], object_type.value_type)
        
        # Handle string types
        if object_type == STRING_TYPE:
            if member_name in ('length', 'size'):
                return INTEGER_TYPE
            elif member_name in ('upper', 'lower', 'trim', 'strip'):
                return STRING_TYPE
            elif member_name in ('split',):
                return FunctionType([STRING_TYPE], ListType(STRING_TYPE))
            elif member_name in ('contains', 'starts_with', 'ends_with'):
                return FunctionType([STRING_TYPE], BOOLEAN_TYPE)
        
        return ANY_TYPE
    
    def infer_index_expression_type(self, index_expr, env: Dict[str, Type]) -> Type:
        """
        Infer type of index expression: array[index]
        
        This handles:
        - List indexing: list[0]
        - Dictionary access: dict["key"]
        - Nested indexing: matrix[i][j]
        
        Args:
            index_expr: IndexExpression AST node
            env: Type environment
            
        Returns:
            Type of the indexed element
        """
        # Infer the type of the array/collection being indexed
        array_type = self.infer_expression_type(index_expr.array_expr, env)
        
        if array_type == ANY_TYPE:
            return ANY_TYPE
        
        # Handle list types
        if isinstance(array_type, ListType):
            # Index should be an integer
            index_type = self.infer_expression_type(index_expr.index_expr, env)
            if index_type not in (INTEGER_TYPE, ANY_TYPE):
                # Type error - but return element type anyway for error recovery
                pass
            return array_type.element_type
        
        # Handle dictionary types
        if isinstance(array_type, DictionaryType):
            # Index should match key type
            index_type = self.infer_expression_type(index_expr.index_expr, env)
            if not index_type.is_compatible_with(array_type.key_type) and index_type != ANY_TYPE:
                # Type error - but return value type anyway for error recovery
                pass
            return array_type.value_type
        
        # Handle string types (character access)
        if array_type == STRING_TYPE:
            # Index should be an integer
            return STRING_TYPE  # Return single character as string
        
        return ANY_TYPE
    
    def infer_nested_call_type(self, call: FunctionCall, env: Dict[str, Type]) -> Type:
        """
        Infer type of nested function calls with proper argument type propagation.
        
        This handles:
        - Simple nested calls: func1(func2(x))
        - Generic instantiation: get_list<Integer>()
        - Method chains as arguments: obj.method1().method2()
        
        Args:
            call: FunctionCall AST node
            env: Type environment
            
        Returns:
            Return type of the outermost call
        """
        # First, check if this function has a known type
        func_name = call.name
        
        if func_name in env:
            func_type = env[func_name]
        elif func_name in self.function_return_types:
            # Return the cached return type
            return self.function_return_types[func_name]
        else:
            # Unknown function - try to infer from arguments
            func_type = ANY_TYPE
        
        # If we have a function type, use bidirectional inference for arguments
        if isinstance(func_type, FunctionType):
            # Infer argument types with expected types from function signature
            arg_types = self.infer_argument_types_from_function(func_type, call.arguments, env)
            
            # Return the function's return type
            return func_type.return_type
        
        return ANY_TYPE
    def create_generic_context(self, func_name: str, type_parameters: list, 
                               type_constraints: dict = None):
        """
        Create a generic context for a function with type parameters.
        
        Args:
            func_name: Name of the function
            type_parameters: List of type parameter names (e.g., ['T', 'R'])
            type_constraints: Dict mapping parameter names to trait constraints
            
        Returns:
            A simple object to track generic context (simplified for now)
        """
        from nexuslang.typesystem.generic_types import GenericTypeContext
        
        context = GenericTypeContext()
        
        # Add type parameters to context
        for param_name in type_parameters:
            constraints = []
            if type_constraints and param_name in type_constraints:
                # Get trait constraints for this parameter
                trait_names = type_constraints[param_name]
                # Convert trait names to actual trait types would happen here
                constraints = trait_names  # Simplified for now
            
            context.add_type_parameter(param_name, constraints if constraints else None)
        
        return context
    def _infer_option_pattern_bindings(self, pattern, match_value_type: "Type") -> dict:
        """Return bindings for an OptionPattern (Some/None)."""
        bindings = {}
        if pattern.variant == "Some" and pattern.binding:
            is_option = (
                (hasattr(match_value_type, 'name') and match_value_type.name == 'Option')
                or (isinstance(match_value_type, GenericType) and match_value_type.name == 'Option')
            )
            if is_option and hasattr(match_value_type, 'type_parameters') and match_value_type.type_parameters:
                bindings[pattern.binding] = match_value_type.type_parameters[0]
            else:
                bindings[pattern.binding] = ANY_TYPE
        # None variant has no bindings
        return bindings

    def _infer_result_pattern_bindings(self, pattern, match_value_type: "Type") -> dict:
        """Return bindings for a ResultPattern (Ok/Err)."""
        bindings = {}
        is_result = (
            (hasattr(match_value_type, 'name') and match_value_type.name == 'Result')
            or (isinstance(match_value_type, GenericType) and match_value_type.name == 'Result')
        )
        has_params = is_result and hasattr(match_value_type, 'type_parameters') and match_value_type.type_parameters

        if pattern.variant == "Ok" and pattern.binding:
            if has_params and len(match_value_type.type_parameters) >= 1:
                bindings[pattern.binding] = match_value_type.type_parameters[0]
            else:
                bindings[pattern.binding] = ANY_TYPE
        elif pattern.variant == "Err" and pattern.binding:
            if has_params and len(match_value_type.type_parameters) >= 2:
                bindings[pattern.binding] = match_value_type.type_parameters[1]
            else:
                bindings[pattern.binding] = ANY_TYPE
        return bindings

    def _infer_list_pattern_bindings(self, pattern, match_value_type: "Type") -> dict:
        """Return bindings for a ListPattern ([head, ...tail])."""
        bindings = {}
        if isinstance(match_value_type, ListType):
            element_type = match_value_type.element_type
            if hasattr(pattern, 'elements') and pattern.elements:
                for elem_pattern in pattern.elements:
                    bindings.update(self.infer_pattern_binding_type(elem_pattern, element_type))
            if hasattr(pattern, 'rest') and pattern.rest:
                bindings[pattern.rest] = ListType(element_type)
        else:
            if hasattr(pattern, 'elements') and pattern.elements:
                for elem_pattern in pattern.elements:
                    bindings.update(self.infer_pattern_binding_type(elem_pattern, ANY_TYPE))
            if hasattr(pattern, 'rest') and pattern.rest:
                bindings[pattern.rest] = ANY_TYPE
        return bindings

    def infer_pattern_binding_type(self, pattern, match_value_type: Type) -> Dict[str, Type]:
        """
        Infer types for variables bound in a pattern based on the matched value type.
        
        This enables precise type inference in pattern matching:
        - Literal patterns: no bindings
        - Identifier patterns: bind to full match value type
        - List patterns: bind elements and rest to appropriate types
        - Option/Result patterns: unwrap inner types
        - Variant patterns: bind with variant-specific types
        
        Args:
            pattern: Pattern AST node
            match_value_type: Type of the value being matched against
            
        Returns:
            Dictionary mapping variable names to their inferred types
        """
        from ..parser.ast import (
            LiteralPattern, IdentifierPattern, WildcardPattern,
            OptionPattern, ResultPattern, VariantPattern,
            TuplePattern, ListPattern
        )

        # Wildcard / literal patterns: no bindings
        if isinstance(pattern, (WildcardPattern, LiteralPattern)):
            return {}

        # Identifier pattern: binds the whole value
        if isinstance(pattern, IdentifierPattern):
            return {pattern.name: match_value_type}

        # Option pattern: Some(x) / None
        if isinstance(pattern, OptionPattern):
            return self._infer_option_pattern_bindings(pattern, match_value_type)

        # Result pattern: Ok(x) / Err(e)
        if isinstance(pattern, ResultPattern):
            return self._infer_result_pattern_bindings(pattern, match_value_type)

        # Variant pattern: generic enum variant
        if isinstance(pattern, VariantPattern):
            if hasattr(pattern, 'bindings') and pattern.bindings:
                return {name: ANY_TYPE for name in pattern.bindings}
            return {}

        # List pattern: [head, ...tail]
        is_list_pattern = isinstance(pattern, ListPattern) or (
            hasattr(pattern, 'elements') and
            pattern.__class__.__name__ == 'ListPattern'
        )
        if is_list_pattern:
            return self._infer_list_pattern_bindings(pattern, match_value_type)

        # Tuple pattern: (x, y, z)
        if isinstance(pattern, TuplePattern):
            bindings: Dict[str, Type] = {}
            if hasattr(pattern, 'elements'):
                for elem_pattern in pattern.elements:
                    bindings.update(self.infer_pattern_binding_type(elem_pattern, ANY_TYPE))
            return bindings

        # Unknown pattern type — no bindings
        return {}