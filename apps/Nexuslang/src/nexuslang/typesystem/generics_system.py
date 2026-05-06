"""
Enhanced Generic Type System for NexusLang
======================================

Complete implementation of generics with:
- Type parameters and constraints
- Type inference for generic functions
- Monomorphization for code generation
- Variance support (covariant, contravariant)
"""

from typing import Dict, List, Optional, Set, Tuple, Any, Type
from dataclasses import dataclass
from ..typesystem.types import (
    Type, PrimitiveType, ListType, DictionaryType, ClassType,
    FunctionType, GenericParameter, GenericType, AnyType
)


@dataclass
class TypeConstraint:
    """Represents a constraint on a generic type parameter."""
    parameter_name: str
    constraint_type: Type  # The type that parameter must satisfy
    constraint_kind: str  # 'subtype', 'interface', 'comparable', etc.
    
    def __str__(self):
        return f"{self.parameter_name} is {self.constraint_kind} {self.constraint_type}"


@dataclass
class TypeParameterInfo:
    """Information about a generic type parameter."""
    name: str
    constraints: List[TypeConstraint]
    variance: Optional[str]  # 'covariant', 'contravariant', 'invariant'
    default_type: Optional[Type] = None
    
    def __str__(self):
        if self.constraints:
            constraints_str = " where " + ", ".join(str(c) for c in self.constraints)
        else:
            constraints_str = ""
        return f"{self.name}{constraints_str}"


class GenericContext:
    """Manages generic type parameters and their substitutions during type checking."""
    
    def __init__(self, parent: Optional['GenericContext'] = None):
        self.parent = parent
        self.type_parameters: Dict[str, TypeParameterInfo] = {}
        self.type_substitutions: Dict[str, Type] = {}
        self.constraints: List[TypeConstraint] = []
    
    def add_type_parameter(self, param: TypeParameterInfo):
        """Register a type parameter."""
        self.type_parameters[param.name] = param
        if param.constraints:
            self.constraints.extend(param.constraints)
    
    def substitute_type(self, param_name: str, actual_type: Type):
        """Bind a type parameter to an actual type."""
        if param_name in self.type_parameters:
            # Validate constraints
            param_info = self.type_parameters[param_name]
            for constraint in param_info.constraints:
                if not self._check_constraint(actual_type, constraint):
                    raise TypeError(
                        f"Type {actual_type} does not satisfy constraint {constraint}"
                    )
            self.type_substitutions[param_name] = actual_type
        elif self.parent:
            self.parent.substitute_type(param_name, actual_type)
        else:
            raise ValueError(f"Unknown type parameter: {param_name}")
    
    def resolve_type(self, type_: Type) -> Type:
        """Resolve a type by applying current substitutions."""
        if isinstance(type_, GenericParameter):
            # Look up the substitution
            if type_.name in self.type_substitutions:
                return self.type_substitutions[type_.name]
            elif self.parent:
                return self.parent.resolve_type(type_)
            return type_
        
        if isinstance(type_, ListType):
            return ListType(self.resolve_type(type_.element_type))
        
        if isinstance(type_, DictionaryType):
            return DictionaryType(
                self.resolve_type(type_.key_type),
                self.resolve_type(type_.value_type)
            )
        
        if isinstance(type_, FunctionType):
            return FunctionType(
                [self.resolve_type(t) for t in type_.param_types],
                self.resolve_type(type_.return_type)
            )
        
        return type_
    
    def _check_constraint(self, actual_type: Type, constraint: TypeConstraint) -> bool:
        """Check if a type satisfies a constraint."""
        if constraint.constraint_kind == 'subtype':
            return actual_type.is_compatible_with(constraint.constraint_type)
        
        if constraint.constraint_kind == 'comparable':
            # Check if type implements comparison operators
            return isinstance(actual_type, (PrimitiveType, ClassType))
        
        if constraint.constraint_kind == 'equatable':
            # All types are equatable in NexusLang
            return True
        
        if constraint.constraint_kind == 'interface':
            # Check if type implements an interface
            if isinstance(actual_type, ClassType):
                # Check if interface is in parent classes
                return constraint.constraint_type.name in actual_type.parent_classes
            return False
        
        return False


class GenericTypeInference:
    """Infers type arguments for generic functions and classes."""
    
    def __init__(self):
        self.inference_cache: Dict[Tuple, List[Type]] = {}
    
    def infer_type_arguments(
        self,
        type_parameters: List[TypeParameterInfo],
        argument_types: List[Type],
        parameter_types: List[Type]
    ) -> Dict[str, Type]:
        """
        Infer type arguments from actual argument types.
        
        Example:
            function max<T>(a: T, b: T) -> T
            Called with: max(5, 10)
            Infers: T = Integer
        """
        substitutions: Dict[str, Type] = {}
        
        # Match each argument with its parameter
        for arg_type, param_type in zip(argument_types, parameter_types):
            self._unify_types(arg_type, param_type, substitutions)
        
        # Fill in defaults for unresolved parameters
        for param in type_parameters:
            if param.name not in substitutions:
                if param.default_type:
                    substitutions[param.name] = param.default_type
                else:
                    raise TypeError(f"Cannot infer type for parameter {param.name}")
        
        return substitutions
    
    def _unify_types(
        self,
        actual: Type,
        expected: Type,
        substitutions: Dict[str, Type]
    ):
        """Unify two types and update substitutions."""
        # If expected is a type parameter, bind it
        if isinstance(expected, GenericParameter):
            param_name = expected.name
            if param_name in substitutions:
                # Check compatibility with existing binding
                if not actual.is_compatible_with(substitutions[param_name]):
                    raise TypeError(
                        f"Type mismatch: {actual} vs {substitutions[param_name]}"
                    )
            else:
                substitutions[param_name] = actual
            return
        
        # Recurse into structured types
        if isinstance(expected, ListType) and isinstance(actual, ListType):
            self._unify_types(actual.element_type, expected.element_type, substitutions)
        
        elif isinstance(expected, DictionaryType) and isinstance(actual, DictionaryType):
            self._unify_types(actual.key_type, expected.key_type, substitutions)
            self._unify_types(actual.value_type, expected.value_type, substitutions)
        
        elif isinstance(expected, FunctionType) and isinstance(actual, FunctionType):
            for a, e in zip(actual.param_types, expected.param_types):
                self._unify_types(a, e, substitutions)
            self._unify_types(actual.return_type, expected.return_type, substitutions)


class Monomorphizer:
    """
    Generates specialized versions of generic functions/classes.
    
    This is the code generation strategy for generics:
    - Each unique set of type arguments creates a new specialized version
    - Specialized versions are generated at compile time
    - No runtime type information needed
    """
    
    def __init__(self):
        self.specializations: Dict[Tuple[str, Tuple[Type, ...]], str] = {}
    
    def get_specialized_name(
        self,
        generic_name: str,
        type_args: List[Type]
    ) -> str:
        """
        Get the mangled name for a specialized version.
        
        Example:
            max<Integer> -> max_Integer
            List<String> -> List_String
            Dict<String, Integer> -> Dict_String_Integer
        """
        key = (generic_name, tuple(type_args))
        
        if key in self.specializations:
            return self.specializations[key]
        
        # Create mangled name
        type_names = []
        for t in type_args:
            if isinstance(t, PrimitiveType):
                type_names.append(t.name.capitalize())
            elif isinstance(t, ClassType):
                type_names.append(t.name)
            elif isinstance(t, ListType):
                elem_name = self._get_type_name(t.element_type)
                type_names.append(f"List_{elem_name}")
            elif isinstance(t, DictionaryType):
                key_name = self._get_type_name(t.key_type)
                val_name = self._get_type_name(t.value_type)
                type_names.append(f"Dict_{key_name}_{val_name}")
            else:
                type_names.append("Any")
        
        specialized_name = f"{generic_name}_{'_'.join(type_names)}"
        self.specializations[key] = specialized_name
        
        return specialized_name
    
    def _get_type_name(self, type_: Type) -> str:
        """Get a string name for a type."""
        if isinstance(type_, PrimitiveType):
            return type_.name.capitalize()
        elif isinstance(type_, ClassType):
            return type_.name
        elif isinstance(type_, ListType):
            return f"List_{self._get_type_name(type_.element_type)}"
        elif isinstance(type_, DictionaryType):
            return f"Dict_{self._get_type_name(type_.key_type)}_{self._get_type_name(type_.value_type)}"
        return "Any"
    
    def needs_specialization(
        self,
        generic_name: str,
        type_args: List[Type]
    ) -> bool:
        """Check if this combination needs a new specialization."""
        key = (generic_name, tuple(type_args))
        return key not in self.specializations


# Built-in generic type constructors
def create_list_type(element_type: Type) -> ListType:
    """Create a generic List<T> type."""
    return ListType(element_type)


def create_dict_type(key_type: Type, value_type: Type) -> DictionaryType:
    """Create a generic Dictionary<K, V> type."""
    return DictionaryType(key_type, value_type)


# Export main classes
__all__ = [
    'TypeConstraint',
    'TypeParameterInfo',
    'GenericContext',
    'GenericTypeInference',
    'Monomorphizer',
    'create_list_type',
    'create_dict_type'
]
