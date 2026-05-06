"""
Enhanced Type System Integration for NexusLang.

This module provides a unified interface for:
- Type inference (bidirectional, lambda, method chains)
- Generic types (instantiation, constraints, variance)
- User-defined types (classes with inheritance)
- Type checking (compatibility, constraint validation)

Purpose: Single entry point for all type system functionality.
"""

from typing import Dict, List, Optional, Any, Tuple, Type
from ..typesystem.types import (
    Type, PrimitiveType, ListType, DictionaryType, ClassType,
    FunctionType, UnionType, AnyType, GenericType, GenericParameter,
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, NULL_TYPE, ANY_TYPE,
    get_type_by_name, infer_type
)
from ..typesystem.type_inference import TypeInferenceEngine
from ..typesystem.generic_types import GenericTypeRegistry, GenericTypeContext
from ..typesystem.generics_system import (
    GenericContext, GenericTypeInference, TypeParameterInfo, TypeConstraint
)
from ..typesystem.user_types import TypeRegistry
from ..typesystem.typechecker import TypeChecker, TypeEnvironment
from ..parser.ast import (
    Expression, FunctionDefinition, ClassDefinition, VariableDeclaration,
    Parameter, Literal, Identifier, FunctionCall, BinaryOperation
)


class IntegratedTypeSystem:
    """
    Unified type system interface that integrates:
    - Type inference
    - Generic types
    - User-defined types
    - Type checking
    """
    
    def __init__(self, enable_type_checking: bool = True):
        """Initialize the integrated type system."""
        self.enable_type_checking = enable_type_checking
        
        # Core components
        self.inference_engine = TypeInferenceEngine()
        self.generic_registry = GenericTypeRegistry()
        self.type_registry = TypeRegistry()
        self.type_checker = TypeChecker() if enable_type_checking else None
        self.generic_inference = GenericTypeInference()
        
        # Type environment (maps variable names to types)
        self.type_environment: Dict[str, Type] = {}
        
        # Generic contexts (per-function for generic type parameters)
        self.generic_contexts: Dict[str, GenericContext] = {}
    
    # ========================================================================
    # Type Inference API
    # ========================================================================
    
    def infer_expression_type(self, expr: Expression, env: Optional[Dict[str, Type]] = None) -> Type:
        """
        Infer the type of an expression.
        
        Args:
            expr: Expression AST node
            env: Type environment (defaults to global environment)
        
        Returns:
            Inferred Type
        """
        env = env or self.type_environment
        return self.inference_engine.infer_expression_type(expr, env)
    
    def infer_with_expected_type(
        self, 
        expr: Expression, 
        expected: Optional[Type],
        env: Optional[Dict[str, Type]] = None
    ) -> Type:
        """
        Bidirectional type inference with expected type context.
        
        This is especially useful for:
        - Lambda expressions: infer parameter types from function signature
        - Literals: infer more specific types (e.g., List<Int> vs List<Any>)
        - Function arguments: guide inference from parameter types
        
        Args:
            expr: Expression to infer
            expected: Expected type from context
            env: Type environment
        
        Returns:
            Inferred Type
        """
        env = env or self.type_environment
        return self.inference_engine.infer_with_expected_type(expr, expected, env)
    
    def infer_function_signature(
        self,
        func_def: FunctionDefinition,
        env: Optional[Dict[str, Type]] = None
    ) -> FunctionType:
        """
        Infer complete function signature including return type.
        
        Args:
            func_def: FunctionDefinition AST node
            env: Type environment
        
        Returns:
            Complete FunctionType
        """
        env = env or self.type_environment
        
        # Build generic context if function has type parameters
        generic_context = None
        if func_def.type_parameters:
            generic_context = self._create_generic_context(
                func_def.name,
                func_def.type_parameters,
                func_def.type_constraints
            )
        
        return self.inference_engine.infer_function_signature(func_def, env, generic_context)
    
    def infer_lambda_type(
        self,
        lambda_expr,
        expected_func_type: Optional[FunctionType] = None,
        env: Optional[Dict[str, Type]] = None
    ) -> FunctionType:
        """
        Infer lambda expression type with bidirectional inference.
        
        Args:
            lambda_expr: LambdaExpression AST node
            expected_func_type: Expected function type from context
            env: Type environment
        
        Returns:
            Inferred FunctionType for the lambda
        """
        env = env or self.type_environment
        return self.inference_engine.infer_lambda_types(lambda_expr, expected_func_type, env)
    
    # ========================================================================
    # Generic Types API
    # ========================================================================
    
    def register_generic_type(
        self,
        name: str,
        type_parameters: List[str],
        base_type: Type
    ) -> None:
        """
        Register a generic type definition.
        
        Example:
            register_generic_type("List", ["T"], ListType(GenericParameter("T")))
        
        Args:
            name: Generic type name (e.g., "List", "Dictionary")
            type_parameters: List of type parameter names (e.g., ["T"], ["K", "V"])
            base_type: Base type with GenericParameter placeholders
        """
        self.generic_registry.register_generic_type(name, type_parameters, base_type)
    
    def instantiate_generic_type(
        self,
        generic_name: str,
        type_args: List[Type]
    ) -> Type:
        """
        Create a concrete instance of a generic type.
        
        Example:
            instantiate_generic_type("List", [INTEGER_TYPE]) -> ListType(INTEGER_TYPE)
        
        Args:
            generic_name: Name of generic type
            type_args: Concrete type arguments
        
        Returns:
            Instantiated concrete type
        """
        return self.generic_registry.create_type_instance(generic_name, type_args)
    
    def infer_generic_arguments(
        self,
        generic_func: FunctionDefinition,
        argument_types: List[Type]
    ) -> Dict[str, Type]:
        """
        Infer generic type arguments from function call arguments.
        
        Example:
            function map<T, R>(items: List<T>, fn: Function) -> List<R>
            map([1, 2, 3], fn) infers: T = Integer
        
        Args:
            generic_func: Generic function definition
            argument_types: Types of arguments in the call
        
        Returns:
            Dictionary mapping type parameter names to inferred types
        """
        # Extract parameter type annotations as strings
        param_type_strs = []
        for param in generic_func.parameters:
            if param.type_annotation:
                param_type_strs.append(param.type_annotation)
            else:
                param_type_strs.append("Any")
        
        return self.generic_inference.infer_from_arguments(
            generic_func.type_parameters,
            param_type_strs,
            argument_types
        )
    
    def create_generic_context(
        self,
        func_name: str,
        type_parameters: List[str],
        constraints: Optional[Dict[str, List[str]]] = None
    ) -> GenericContext:
        """
        Create a generic context for type parameter tracking.
        
        Args:
            func_name: Function name
            type_parameters: Type parameter names
            constraints: Optional constraints (param -> trait names)
        
        Returns:
            GenericContext for this function
        """
        context = GenericContext()
        
        for param_name in type_parameters:
            param_constraints = []
            
            if constraints and param_name in constraints:
                for constraint_name in constraints[param_name]:
                    # Create constraint (simplified - would lookup actual trait types)
                    constraint = TypeConstraint(
                        parameter_name=param_name,
                        constraint_type=get_type_by_name(constraint_name),
                        constraint_kind="interface"
                    )
                    param_constraints.append(constraint)
            
            param_info = TypeParameterInfo(
                name=param_name,
                constraints=param_constraints,
                variance=None  # Default to invariant
            )
            context.add_type_parameter(param_info)
        
        self.generic_contexts[func_name] = context
        return context
    
    # ========================================================================
    # User-Defined Types API
    # ========================================================================
    
    def register_class_type(
        self,
        class_def: ClassDefinition
    ) -> ClassType:
        """
        Register a user-defined class type.
        
        This extracts properties and methods from the ClassDefinition AST node
        and creates a ClassType that can be used for type checking.
        
        Args:
            class_def: ClassDefinition AST node
        
        Returns:
            Created ClassType
        """
        # Extract properties
        properties: Dict[str, Type] = {}
        for prop in class_def.properties:
            if prop.type_annotation:
                prop_type = get_type_by_name(prop.type_annotation)
                properties[prop.name] = prop_type
            else:
                properties[prop.name] = ANY_TYPE
        
        # Extract methods
        methods: Dict[str, FunctionType] = {}
        for method in class_def.methods:
            method_type = self.infer_function_signature(method)
            methods[method.name] = method_type
        
        # Create ClassType
        parent_classes = class_def.parent_classes if hasattr(class_def, 'parent_classes') else []
        class_type = ClassType(class_def.name, properties, methods, parent_classes)
        
        # Register with TypeRegistry
        self.type_registry.register_type(class_type)
        
        # Register inheritance relationships
        for parent_name in parent_classes:
            self.type_registry.register_inheritance(class_def.name, parent_name)
        
        return class_type
    
    def is_subtype(self, child_type_name: str, parent_type_name: str) -> bool:
        """
        Check if one type is a subtype of another (inheritance).
        
        Args:
            child_type_name: Name of potential child type
            parent_type_name: Name of potential parent type
        
        Returns:
            True if child is subtype of parent
        """
        return self.type_registry.is_subtype(child_type_name, parent_type_name)
    
    def get_type(self, name: str) -> Optional[Type]:
        """
        Get a type by name (user-defined or built-in).
        
        Args:
            name: Type name
        
        Returns:
            Type object or None if not found
        """
        # Try user-defined types first
        user_type = self.type_registry.get_type(name)
        if user_type:
            return user_type
        
        # Try built-in types
        return get_type_by_name(name)
    
    # ========================================================================
    # Type Checking API
    # ========================================================================
    
    def check_type_compatibility(self, actual: Type, expected: Type) -> bool:
        """
        Check if actual type is compatible with expected type.
        
        Args:
            actual: Actual type
            expected: Expected type
        
        Returns:
            True if compatible
        """
        return actual.is_compatible_with(expected)
    
    def check_function_call(
        self,
        func_call: FunctionCall,
        func_type: FunctionType,
        env: Optional[Dict[str, Type]] = None
    ) -> Tuple[bool, Optional[str]]:
        """
        Type check a function call.
        
        Args:
            func_call: FunctionCall AST node
            func_type: Expected function type
            env: Type environment
        
        Returns:
            (is_valid, error_message)
        """
        env = env or self.type_environment
        
        # Check argument count
        if len(func_call.arguments) != len(func_type.param_types):
            return (
                False,
                f"Function expects {len(func_type.param_types)} arguments, got {len(func_call.arguments)}"
            )
        
        # Check each argument type
        for i, (arg, expected_param_type) in enumerate(zip(func_call.arguments, func_type.param_types)):
            arg_type = self.infer_expression_type(arg, env)
            
            if not arg_type.is_compatible_with(expected_param_type):
                return (
                    False,
                    f"Argument {i+1} has type {arg_type}, expected {expected_param_type}"
                )
        
        return (True, None)
    
    # ========================================================================
    # Utility Methods
    # ========================================================================
    
    def _create_generic_context(
        self,
        name: str,
        type_parameters: List[str],
        constraints: Optional[Any] = None
    ):
        """Internal helper to create generic context."""
        context = GenericTypeContext()
        
        for param in type_parameters:
            # Parse constraints if provided
            param_constraints = None
            if constraints and param in constraints:
                param_constraints = constraints[param]
            
            context.add_type_parameter(param, param_constraints)
        
        return context
    
    def reset(self):
        """Reset all type system state."""
        self.inference_engine.reset()
        self.type_environment.clear()
        self.generic_contexts.clear()


# ============================================================================
# Global type system instance (singleton pattern)
# ============================================================================

_global_type_system: Optional[IntegratedTypeSystem] = None


def get_type_system(enable_type_checking: bool = True) -> IntegratedTypeSystem:
    """
    Get the global type system instance.
    
    Args:
        enable_type_checking: Whether to enable type checking
    
    Returns:
        Global IntegratedTypeSystem instance
    """
    global _global_type_system
    
    if _global_type_system is None:
        _global_type_system = IntegratedTypeSystem(enable_type_checking)
    
    return _global_type_system


def reset_type_system():
    """Reset the global type system instance."""
    global _global_type_system
    _global_type_system = None
