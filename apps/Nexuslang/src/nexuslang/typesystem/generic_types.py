"""
Generic type system for NexusLang.
This module provides support for generic types and type parameters.
"""

from typing import Dict, List, Optional, Set, Union, Any, Tuple, Type
from ..typesystem.types import (
    Type, PrimitiveType, ListType, DictionaryType, ClassType, 
    FunctionType, UnionType, AnyType, GenericType, GenericParameter,
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, NULL_TYPE, ANY_TYPE
)

class GenericTypeRegistry:
    """Registry for managing generic types and their instantiations."""
    
    def __init__(self):
        self.generic_types: Dict[str, GenericType] = {}
        self.type_instances: Dict[Tuple[str, Tuple[Type, ...]], Type] = {}
    
    def register_generic_type(self, name: str, type_parameters: List[str], base_type: Type) -> None:
        """Register a new generic type."""
        self.generic_types[name] = GenericType(name, type_parameters, base_type)
    
    def create_type_instance(self, generic_name: str, type_args: List[Type]) -> Type:
        """Create a new instance of a generic type with specific type arguments."""
        # Check if this instance already exists
        key = (generic_name, tuple(type_args))
        if key in self.type_instances:
            return self.type_instances[key]
        
        # Get the generic type definition
        if generic_name not in self.generic_types:
            raise TypeError(f"Generic type '{generic_name}' not found")
        
        generic_type = self.generic_types[generic_name]
        
        # Check number of type arguments
        if len(type_args) != len(generic_type.type_parameters):
            raise TypeError(
                f"Generic type '{generic_name}' expects {len(generic_type.type_parameters)} "
                f"type arguments, got {len(type_args)}"
            )
        
        # Create type substitution map
        substitutions = dict(zip(generic_type.type_parameters, type_args))
        
        # Create new type instance by substituting type parameters
        instance = self._substitute_types(generic_type.base_type, substitutions)
        
        # Cache the instance
        self.type_instances[key] = instance
        return instance
    
    def _substitute_types(self, type_: Type, substitutions: Dict[str, Type]) -> Type:
        """Substitute type parameters with their actual types."""
        if isinstance(type_, GenericParameter):
            return substitutions.get(type_.name, type_)
        
        if isinstance(type_, ListType):
            return ListType(self._substitute_types(type_.element_type, substitutions))
        
        if isinstance(type_, DictionaryType):
            return DictionaryType(
                self._substitute_types(type_.key_type, substitutions),
                self._substitute_types(type_.value_type, substitutions)
            )
        
        if isinstance(type_, FunctionType):
            return FunctionType(
                [self._substitute_types(t, substitutions) for t in type_.param_types],
                self._substitute_types(type_.return_type, substitutions)
            )
        
        if isinstance(type_, ClassType):
            # Handle generic class types
            if isinstance(type_, GenericType):
                return self.create_type_instance(type_.name, [
                    self._substitute_types(t, substitutions) for t in type_.type_arguments
                ])
            
            # Handle regular class types
            return ClassType(
                type_.name,
                {k: self._substitute_types(v, substitutions) for k, v in type_.properties.items()},
                {k: self._substitute_types(v, substitutions) for k, v in type_.methods.items()},
                type_.parent_classes
            )
        
        if isinstance(type_, UnionType):
            return UnionType([
                self._substitute_types(t, substitutions) for t in type_.types
            ])
        
        return type_  # Primitive types and others don't need substitution

class GenericTypeConstraint:
    """Represents constraints on a generic type parameter."""
    
    def __init__(self, parameter: str, traits: List['TraitType']):
        """
        Initialize constraint with multiple trait bounds.
        
        Args:
            parameter: Name of the type parameter (e.g., 'T')
            traits: List of traits the type must implement
        """
        self.parameter = parameter
        self.traits = traits if isinstance(traits, list) else [traits]
    
    def check(self, type_: Type) -> bool:
        """Check if a type satisfies all trait constraints."""
        from ..typesystem.types import TraitType
        
        for trait in self.traits:
            if isinstance(trait, TraitType):
                if not trait.is_implemented_by(type_):
                    return False
            else:
                # For non-trait constraints, use compatibility check
                if not type_.is_compatible_with(trait):
                    return False
        return True
    
    def __str__(self) -> str:
        """String representation of constraint."""
        trait_names = [t.name if hasattr(t, 'name') else str(t) for t in self.traits]
        return f"{self.parameter}: {' + '.join(trait_names)}"

class GenericTypeContext:
    """Context for managing generic type parameters and constraints."""
    
    def __init__(self):
        self.type_parameters: Dict[str, GenericParameter] = {}
        self.constraints: List[GenericTypeConstraint] = []
    
    def add_type_parameter(self, name: str, constraints: Optional[List[Type]] = None) -> None:
        """Add a new type parameter with optional constraints.
        
        Args:
            name: Name of the type parameter
            constraints: List of trait constraints (supports multiple bounds)
        """
        self.type_parameters[name] = GenericParameter(name)
        if constraints:
            # Support both single constraint and list of constraints
            if not isinstance(constraints, list):
                constraints = [constraints]
            self.constraints.append(GenericTypeConstraint(name, constraints))
    
    def check_constraints(self, substitutions: Dict[str, Type]) -> bool:
        """Check if the given type substitutions satisfy all constraints."""
        for constraint in self.constraints:
            if constraint.parameter in substitutions:
                if not constraint.check(substitutions[constraint.parameter]):
                    return False
        return True
    
    def get_substituted_type(self, type_: Type, substitutions: Dict[str, Type]) -> Type:
        """Get a type with all generic parameters substituted."""
        if isinstance(type_, GenericParameter):
            return substitutions.get(type_.name, type_)
        
        if isinstance(type_, ListType):
            return ListType(self.get_substituted_type(type_.element_type, substitutions))
        
        if isinstance(type_, DictionaryType):
            return DictionaryType(
                self.get_substituted_type(type_.key_type, substitutions),
                self.get_substituted_type(type_.value_type, substitutions)
            )
        
        if isinstance(type_, FunctionType):
            return FunctionType(
                [self.get_substituted_type(t, substitutions) for t in type_.param_types],
                self.get_substituted_type(type_.return_type, substitutions)
            )
        
        if isinstance(type_, ClassType):
            return ClassType(
                type_.name,
                {k: self.get_substituted_type(v, substitutions) for k, v in type_.properties.items()},
                {k: self.get_substituted_type(v, substitutions) for k, v in type_.methods.items()},
                type_.parent_classes
            )
        
        if isinstance(type_, UnionType):
            return UnionType([
                self.get_substituted_type(t, substitutions) for t in type_.types
            ])
        
        return type_

# Create a global registry instance
GENERIC_TYPE_REGISTRY = GenericTypeRegistry() 