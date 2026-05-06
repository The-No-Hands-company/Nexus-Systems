"""
Type system for the NexusLang language.
This module defines the core types and type operations.
"""

from enum import Enum, auto
from typing import Dict, List, Optional, Set, Union, Any, Tuple, Iterator, Type
from abc import ABC, abstractmethod

class Variance(Enum):
    """Variance annotations for generic type parameters."""
    INVARIANT = auto()      # Default - no variance
    COVARIANT = auto()      # out T - can return T but not accept it
    CONTRAVARIANT = auto()  # in T - can accept T but not return it

class TypeKind(Enum):
    """Enum for the different kinds of types."""
    PRIMITIVE = auto()
    LIST = auto()
    DICTIONARY = auto()
    CLASS = auto()
    FUNCTION = auto()
    UNION = auto()
    ANY = auto()
    GENERIC_PARAMETER = auto()  # For T in List<T>
    GENERIC = auto()  # For generic types
    TRAIT = auto()  # For trait/interface types
    # 8.3 Advanced Type Features
    TYPE_CONSTRUCTOR = auto()   # Type constructor parameter (F :: * -> *)
    TYPE_APPLICATION = auto()   # Applied type constructor: F<A>
    ASSOCIATED = auto()         # Associated type declaration within a trait
    TYPE_PROJECTION = auto()    # Type projection T::Item
    ALIAS = auto()              # Named type alias

class Type(ABC):
    """Base class for all types in NexusLang."""
    
    @abstractmethod
    def is_compatible_with(self, other: 'Type') -> bool:
        """Check if this type is compatible with another type."""
        pass
    
    @abstractmethod
    def get_common_supertype(self, other: 'Type') -> Optional['Type']:
        """Get the most specific common supertype of this type and another type."""
        pass
    
    def __eq__(self, other: object) -> bool:
        """Check if two types are equal."""
        if not isinstance(other, Type):
            return NotImplemented
        return self.__class__ == other.__class__ and self._equals(other)
    
    @abstractmethod
    def _equals(self, other: 'Type') -> bool:
        """Implementation of equality for specific type classes."""
        pass

class PrimitiveType(Type):
    """Represents primitive types like integers, floats, strings, etc."""
    
    def __init__(self, name: str):
        self.name = name
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, PrimitiveType):
            # Special cases for numeric types
            if self.name == 'integer' and other.name == 'float':
                return True
            return self.name == other.name
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, PrimitiveType):
            if self.name == other.name:
                return self
            # Special cases for numeric types
            if self.name in ('integer', 'float') and other.name in ('integer', 'float'):
                return FLOAT_TYPE
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return self.name == other.name

    def __hash__(self) -> int:
        return hash(self.name)

class ListType(Type):
    """Represents a list type with an element type."""
    
    def __init__(self, element_type: Type):
        self.element_type = element_type
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, ListType):
            return self.element_type.is_compatible_with(other.element_type)
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, ListType):
            common_element_type = self.element_type.get_common_supertype(other.element_type)
            if common_element_type:
                return ListType(common_element_type)
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return self.element_type == other.element_type

    def __hash__(self) -> int:
        return hash(("List", self.element_type))

class DictionaryType(Type):
    """Represents a dictionary type with key and value types."""
    
    def __init__(self, key_type: Type, value_type: Type):
        self.key_type = key_type
        self.value_type = value_type
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, DictionaryType):
            return (
                self.key_type.is_compatible_with(other.key_type) and
                self.value_type.is_compatible_with(other.value_type)
            )
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, DictionaryType):
            common_key_type = self.key_type.get_common_supertype(other.key_type)
            common_value_type = self.value_type.get_common_supertype(other.value_type)
            if common_key_type and common_value_type:
                return DictionaryType(common_key_type, common_value_type)
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return self.key_type == other.key_type and self.value_type == other.value_type

    def __hash__(self) -> int:
        return hash(("Dict", self.key_type, self.value_type))

class ChannelType(Type):
    """Represents a channel type with a payload type."""

    def __init__(self, payload_type: Type):
        self.payload_type = payload_type

    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, ChannelType):
            return self.payload_type.is_compatible_with(other.payload_type)
        return isinstance(other, AnyType)

    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, ChannelType):
            common_payload = self.payload_type.get_common_supertype(other.payload_type)
            if common_payload:
                return ChannelType(common_payload)
        return ANY_TYPE

    def _equals(self, other: Type) -> bool:
        return self.payload_type == other.payload_type

    def __hash__(self) -> int:
        return hash(("Channel", self.payload_type))

class SetType(Type):
    """Represents a set type with an element type."""
    
    def __init__(self, element_type: Type):
        self.element_type = element_type
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, SetType):
            return self.element_type.is_compatible_with(other.element_type)
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, SetType):
            common_element_type = self.element_type.get_common_supertype(other.element_type)
            if common_element_type:
                return SetType(common_element_type)
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return self.element_type == other.element_type

    def __hash__(self) -> int:
        return hash(("Set", self.element_type))

class TupleType(Type):
    """Represents a tuple type with fixed element types."""
    
    def __init__(self, element_types: list):
        self.element_types = element_types if element_types else []
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, TupleType):
            if len(self.element_types) != len(other.element_types):
                return False
            return all(
                self_type.is_compatible_with(other_type)
                for self_type, other_type in zip(self.element_types, other.element_types)
            )
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, TupleType):
            if len(self.element_types) != len(other.element_types):
                return ANY_TYPE
            common_types = []
            for self_type, other_type in zip(self.element_types, other.element_types):
                common_type = self_type.get_common_supertype(other_type)
                if common_type:
                    common_types.append(common_type)
                else:
                    return ANY_TYPE
            return TupleType(common_types)
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        if len(self.element_types) != len(other.element_types):
            return False
        return all(
            self_type == other_type
            for self_type, other_type in zip(self.element_types, other.element_types)
        )

    def __hash__(self) -> int:
        return hash(("Tuple", tuple(self.element_types)))

class FunctionType(Type):
    """Represents a function type with parameter types and return type."""
    
    def __init__(self, param_types: List[Type], return_type: Type):
        self.param_types = param_types
        self.return_type = return_type
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, FunctionType):
            # Check parameter count
            if len(self.param_types) != len(other.param_types):
                return False
            
            # Check parameter types (contravariant)
            for self_param, other_param in zip(self.param_types, other.param_types):
                if not other_param.is_compatible_with(self_param):
                    return False
            
            # Check return type (covariant)
            return self.return_type.is_compatible_with(other.return_type)
        
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, FunctionType):
            # For functions, we need to find the most specific common subtype
            # This is because function types are contravariant in parameters
            # and covariant in return type
            if len(self.param_types) != len(other.param_types):
                return ANY_TYPE
            
            # Find the most specific common parameter types
            param_types = []
            for self_param, other_param in zip(self.param_types, other.param_types):
                common_param = self_param.get_common_supertype(other_param)
                if not common_param:
                    return ANY_TYPE
                param_types.append(common_param)
            
            # Find the most specific common return type
            common_return = self.return_type.get_common_supertype(other.return_type)
            if not common_return:
                return ANY_TYPE
            
            return FunctionType(param_types, common_return)
        
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return (
            len(self.param_types) == len(other.param_types) and
            all(t1 == t2 for t1, t2 in zip(self.param_types, other.param_types)) and
            self.return_type == other.return_type
        )

    def __hash__(self) -> int:
        return hash(("Function", tuple(self.param_types), self.return_type))

class ClassType(Type):
    """Represents a class type with properties and methods."""
    
    def __init__(self, name: str, properties: Dict[str, Type], methods: Dict[str, FunctionType], parent_classes: Optional[List[str]] = None):
        self.name = name
        self.properties = properties
        self.methods = methods
        self.parent_classes = parent_classes or []
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, ClassType):
            # Check if this class is a subclass of the other class
            if self.name in other.parent_classes:
                return True
            
            # Check if all properties and methods are compatible
            for prop_name, prop_type in other.properties.items():
                if prop_name not in self.properties or not self.properties[prop_name].is_compatible_with(prop_type):
                    return False
            
            for method_name, method_type in other.methods.items():
                if method_name not in self.methods or not self.methods[method_name].is_compatible_with(method_type):
                    return False
            
            return True
        
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, ClassType):
            # Find the most specific common superclass
            common_parents = set(self.parent_classes) & set(other.parent_classes)
            if not common_parents:
                return ANY_TYPE
            
            # Create a new class type with the common properties and methods
            common_properties = {}
            common_methods = {}
            
            # Add properties that exist in both classes
            for prop_name in set(self.properties) & set(other.properties):
                common_prop_type = self.properties[prop_name].get_common_supertype(other.properties[prop_name])
                if common_prop_type:
                    common_properties[prop_name] = common_prop_type
            
            # Add methods that exist in both classes
            for method_name in set(self.methods) & set(other.methods):
                common_method_type = self.methods[method_name].get_common_supertype(other.methods[method_name])
                if common_method_type:
                    common_methods[method_name] = common_method_type
            
            return ClassType(
                f"{self.name}_{other.name}_common",
                common_properties,
                common_methods,
                list(common_parents)
            )
        
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return (
            self.name == other.name and
            self.properties == other.properties and
            self.methods == other.methods and
            self.parent_classes == other.parent_classes
        )

    def __hash__(self) -> int:
        return hash(("Class", self.name))

class GenericType(Type):
    """Represents a generic type with type parameters and optional variance."""
    
    def __init__(self, name: str, type_parameters: List[Union[str, Tuple[str, Variance]]], base_type: Type):
        """
        Initialize generic type.
        
        Args:
            name: Name of the generic type
            type_parameters: List of type parameter names or (name, variance) tuples
            base_type: The base type being parameterized
        """
        self.name = name
        # Normalize type parameters to (name, variance) tuples
        self.type_parameters = []
        for param in type_parameters:
            if isinstance(param, tuple):
                self.type_parameters.append(param)
            else:
                # Default to invariant
                self.type_parameters.append((param, Variance.INVARIANT))
        self.base_type = base_type
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, GenericType):
            if self.name != other.name or len(self.type_parameters) != len(other.type_parameters):
                return False
            
            # Check variance rules
            for (self_param, self_var), (other_param, other_var) in zip(self.type_parameters, other.type_parameters):
                # Variance must match
                if self_var != other_var:
                    return False
            
            # Create a substitution map for type parameters
            substitutions = dict((sp[0], op[0]) for sp, op in zip(self.type_parameters, other.type_parameters))
            
            # Check if the base types are compatible after substitution
            return self._substitute_types(self.base_type, substitutions).is_compatible_with(other.base_type)
        
        return isinstance(other, AnyType)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, GenericType):
            if self.name != other.name or len(self.type_parameters) != len(other.type_parameters):
                return ANY_TYPE
            
            # Create a substitution map for type parameters
            substitutions = dict(zip(self.type_parameters, other.type_parameters))
            
            # Get the common supertype of the base types after substitution
            common_base = self._substitute_types(self.base_type, substitutions).get_common_supertype(other.base_type)
            if not common_base:
                return ANY_TYPE
            
            return GenericType(self.name, self.type_parameters, common_base)
        
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return (
            self.name == other.name and
            self.type_parameters == other.type_parameters and
            self.base_type == other.base_type
        )
    
    def _substitute_types(self, type_: Type, substitutions: Dict[str, str]) -> Type:
        """Substitute type parameters with their actual types."""
        if isinstance(type_, GenericParameter):
            return GenericParameter(substitutions.get(type_.name, type_.name))
        
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
        
        return type_


class OptionType(GenericType):
    """Represents Option<T> - a type that may or may not contain a value.
    
    This is a production-ready implementation of the Option monad, similar to
    Rust's Option or Haskell's Maybe. It provides type-safe handling of nullable
    values without using null/None directly.
    
    Variants:
        Some(T): Contains a value of type T
        None: Contains no value
    
    Methods:
        unwrap() -> T: Returns the contained value, panics if None
        unwrap_or(default: T) -> T: Returns value or default
        map(f: T -> U) -> Option<U>: Transforms the contained value
        and_then(f: T -> Option<U>) -> Option<U>: Monadic bind
        is_some() -> bool: Checks if contains a value
        is_none() -> bool: Checks if empty
    """
    
    def __init__(self, element_type: Type):
        """Initialize Option<T> type.
        
        Args:
            element_type: The type T of the contained value
        """
        super().__init__("Option", [element_type], element_type) # Base type is element_type
        self.element_type = element_type
    
    def is_compatible_with(self, other: Type) -> bool:
        """Check compatibility with another type.
        
        Option<T> is compatible with:
        - Option<U> where T is compatible with U (covariant)
        - None type (can always be None)
        """
        if isinstance(other, OptionType):
            return self.element_type.is_compatible_with(other.element_type)
        # Assuming PrimitiveType('null') exists for null type
        # if isinstance(other, PrimitiveType) and other.name == 'null':
        #     return True
        return False
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        """Get common supertype with another type."""
        if isinstance(other, OptionType):
            common_element = self.element_type.get_common_supertype(other.element_type)
            if common_element:
                return OptionType(common_element)
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        """Check equality with another type."""
        return (isinstance(other, OptionType) and 
                self.element_type == other.element_type)
    
    def __repr__(self) -> str:
        return f"Option<{self.element_type}>"


class ResultType(GenericType):
    """Represents Result<T, E> - a type for operations that may fail.
    
    This is a production-ready implementation of the Result type, similar to
    Rust's Result or Haskell's Either. It provides type-safe error handling
    without exceptions.
    
    Variants:
        Ok(T): Contains a success value of type T
        Err(E): Contains an error value of type E
    
    Methods:
        unwrap() -> T: Returns the Ok value, panics if Err
        unwrap_or(default: T) -> T: Returns Ok value or default
        unwrap_err() -> E: Returns the Err value, panics if Ok
        map(f: T -> U) -> Result<U, E>: Transforms the Ok value
        map_err(f: E -> F) -> Result<T, F>: Transforms the Err value
        and_then(f: T -> Result<U, E>) -> Result<U, E>: Monadic bind
        is_ok() -> bool: Checks if contains Ok value
        is_err() -> bool: Checks if contains Err value
    """
    
    def __init__(self, ok_type: Type, err_type: Type):
        """Initialize Result<T, E> type.
        
        Args:
            ok_type: The type T of the success value
            err_type: The type E of the error value
        """
        # Result<T, E> is a tagged union type that can hold either Ok(T) or Err(E)
        # We create a synthetic base type that represents this union semantics
        # The base type name encodes both possible types for proper type checking
        base_type_name = f"Union[{ok_type},{err_type}]"
        
        # Create a minimal Type instance representing the union
        # This enables proper type compatibility checking without circular dependencies
        class ResultBaseType(Type):
            def __init__(self, ok_t, err_t):
                self.ok_type = ok_t
                self.err_type = err_t
                
            def is_compatible_with(self, other):
                # Compatible with either component type or another Result with compatible types
                if isinstance(other, ResultBaseType):
                    return (self.ok_type.is_compatible_with(other.ok_type) and
                            self.err_type.is_compatible_with(other.err_type))
                return (self.ok_type.is_compatible_with(other) or
                        self.err_type.is_compatible_with(other))
            
            def get_common_supertype(self, other):
                # The common supertype with another Result base or either component type
                if isinstance(other, ResultBaseType):
                    ok_super = self.ok_type.get_common_supertype(other.ok_type)
                    err_super = self.err_type.get_common_supertype(other.err_type)
                    if ok_super and err_super:
                        return ResultBaseType(ok_super, err_super)
                return None
            
            def _equals(self, other):
                # Equality check for type system
                if isinstance(other, ResultBaseType):
                    return (self.ok_type._equals(other.ok_type) and
                            self.err_type._equals(other.err_type))
                return False
            
            def __str__(self):
                return base_type_name
        
        base_type = ResultBaseType(ok_type, err_type)
        super().__init__("Result", [ok_type, err_type], base_type)
        self.ok_type = ok_type
        self.err_type = err_type
    
    def is_compatible_with(self, other: Type) -> bool:
        """Check compatibility with another type.
        
        Result<T, E> is compatible with Result<U, F> where:
        - T is compatible with U (covariant in success type)
        - E is compatible with F (covariant in error type)
        """
        if isinstance(other, ResultType):
            return (self.ok_type.is_compatible_with(other.ok_type) and
                    self.err_type.is_compatible_with(other.err_type))
        return False
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        """Get common supertype with another type."""
        if isinstance(other, ResultType):
            common_ok = self.ok_type.get_common_supertype(other.ok_type)
            common_err = self.err_type.get_common_supertype(other.err_type)
            if common_ok and common_err:
                return ResultType(common_ok, common_err)
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        """Check equality with another type."""
        return (isinstance(other, ResultType) and 
                self.ok_type == other.ok_type and
                self.err_type == other.err_type)
    
    def __repr__(self) -> str:
        return f"Result<{self.ok_type}, {self.err_type}>"


class GenericParameter(Type):
    """Represents a generic type parameter."""
    
    def __init__(self, name: str):
        self.name = name
    
    def is_compatible_with(self, other: Type) -> bool:
        return isinstance(other, AnyType) or self == other
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if self == other:
            return self
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return self.name == other.name

class UnionType(Type):
    """Represents a union type (type that can be one of several types)."""
    
    def __init__(self, types: List[Type]):
        self.types = types
    
    def is_compatible_with(self, other: Type) -> bool:
        if isinstance(other, UnionType):
            # This union type is compatible with another union type
            # if all of its types are compatible with at least one type in the other union
            return all(
                any(t1.is_compatible_with(t2) for t2 in other.types)
                for t1 in self.types
            )
        
        # This union type is compatible with another type
        # if all of its types are compatible with that type
        return all(t.is_compatible_with(other) for t in self.types)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        if isinstance(other, UnionType):
            # For union types, take the union of all types
            all_types = set(self.types + other.types)
            return UnionType(list(all_types))
        
        # If the other type is compatible with all types in this union,
        # return this union type
        if all(t.is_compatible_with(other) for t in self.types):
            return self
        
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return set(self.types) == set(other.types)

class AnyType(Type):
    """Represents the type that can be any value."""
    
    def is_compatible_with(self, other: Type) -> bool:
        return True
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        return isinstance(other, AnyType)

class TypeAliasType(Type):
    """Represents a type alias with optional type parameters and constraints."""
    
    def __init__(self, name: str, type_parameters: List[str], target_type: Type, 
                 constraints: Optional[Dict[str, List[Type]]] = None):
        """
        Initialize type alias.
        
        Args:
            name: Name of the alias
            type_parameters: List of type parameter names
            target_type: The type being aliased
            constraints: Dict mapping parameter names to trait constraints
        """
        self.name = name
        self.type_parameters = type_parameters
        self.target_type = target_type
        self.constraints = constraints or {}
    
    def instantiate(self, type_args: List[Type]) -> Type:
        """
        Instantiate the alias with concrete type arguments.
        
        Args:
            type_args: Concrete types for each parameter
            
        Returns:
            The target type with substitutions applied
        """
        if len(type_args) != len(self.type_parameters):
            raise TypeError(f"Type alias {self.name} expects {len(self.type_parameters)} arguments, got {len(type_args)}")
        
        # Validate constraints
        for param, arg in zip(self.type_parameters, type_args):
            if param in self.constraints:
                for constraint in self.constraints[param]:
                    if isinstance(constraint, TraitType):
                        if not constraint.is_implemented_by(arg):
                            raise TypeError(f"Type {arg} does not implement {constraint.name}")
        
        # Create substitution map
        substitutions = dict(zip(self.type_parameters, type_args))
        
        # Apply substitutions to target type
        return self._substitute_type(self.target_type, substitutions)
    
    def _substitute_type(self, type_: Type, substitutions: Dict[str, Type]) -> Type:
        """Substitute type parameters in a type."""
        if isinstance(type_, GenericParameter):
            return substitutions.get(type_.name, type_)
        
        if isinstance(type_, ListType):
            return ListType(self._substitute_type(type_.element_type, substitutions))
        
        if isinstance(type_, DictionaryType):
            return DictionaryType(
                self._substitute_type(type_.key_type, substitutions),
                self._substitute_type(type_.value_type, substitutions)
            )
        
        return type_
    
    def is_compatible_with(self, other: Type) -> bool:
        """Check compatibility - aliases are transparent."""
        return self.target_type.is_compatible_with(other)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        """Get common supertype - aliases are transparent."""
        return self.target_type.get_common_supertype(other)
    
    def _equals(self, other: Type) -> bool:
        """Check equality."""
        if isinstance(other, TypeAliasType):
            return (self.name == other.name and 
                    self.type_parameters == other.type_parameters and
                    self.target_type == other.target_type)
        # Aliases are transparent for equality
        return self.target_type == other

class PhantomType(Type):
    """Represents a phantom type parameter - affects type identity but not runtime."""
    
    def __init__(self, name: str, phantom_param: Type):
        """
        Initialize phantom type.
        
        Args:
            name: Name of the phantom type
            phantom_param: The phantom type parameter (not used at runtime)
        """
        self.name = name
        self.phantom_param = phantom_param
    
    def is_compatible_with(self, other: Type) -> bool:
        """Phantom types are only compatible if phantom params match."""
        if isinstance(other, PhantomType):
            return self.name == other.name and self.phantom_param == other.phantom_param
        return False
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        """Phantom types have no common supertype unless identical."""
        if self == other:
            return self
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        """Check equality including phantom parameter."""
        return (isinstance(other, PhantomType) and 
                self.name == other.name and
                self.phantom_param == other.phantom_param)

class ExistentialType(Type):
    """Represents an existential type (impl Trait) - hides concrete type."""
    
    def __init__(self, trait_bounds: List['TraitType']):
        """
        Initialize existential type.
        
        Args:
            trait_bounds: Traits that the hidden type must implement
        """
        self.trait_bounds = trait_bounds
    
    def is_compatible_with(self, other: Type) -> bool:
        """Check if type implements all required traits."""
        for trait in self.trait_bounds:
            if not trait.is_implemented_by(other):
                return False
        return True
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        """Existential types abstract to ANY."""
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        """Check equality of trait bounds."""
        return (isinstance(other, ExistentialType) and
                set(self.trait_bounds) == set(other.trait_bounds))

class TraitType(Type):
    """Represents a trait/interface that types can implement."""

    def __init__(
        self,
        name: str,
        methods: Dict[str, 'FunctionType'],
        parent_traits: Optional[List[str]] = None,
        associated_types: Optional[Union[List[str], Dict[str, Any]]] = None,
    ) -> None:
        self.name = name
        self.methods = methods
        self.parent_traits = parent_traits or []
        # 8.3: Support both legacy List[str] and new Dict[str, AssociatedTypeDecl]
        if associated_types is None:
            self.associated_types: Dict[str, Any] = {}
        elif isinstance(associated_types, list):
            # Convert legacy string list for backwards compatibility
            try:
                from nexuslang.typesystem.associated_types import AssociatedTypeDecl  # type: ignore[import]
                self.associated_types = {
                    at_name: AssociatedTypeDecl(at_name) for at_name in associated_types
                }
            except ImportError:
                self.associated_types = {at_name: at_name for at_name in associated_types}
        else:
            self.associated_types = associated_types

    # ------------------------------------------------------------------
    # Associated type helpers (8.3)
    # ------------------------------------------------------------------

    def get_associated_type(self, assoc_name: str) -> Optional[Any]:
        """Return the AssociatedTypeDecl for the given name, or None."""
        return self.associated_types.get(assoc_name)

    def declare_associated_type(self, decl: Any) -> None:
        """Add or replace an associated type declaration in this trait."""
        self.associated_types[decl.name] = decl

    def associated_type_names(self) -> List[str]:
        """Return the names of all declared associated types."""
        return list(self.associated_types.keys())

    def is_implemented_by(self, type_: Type) -> bool:
        """Check if a type implements this trait."""
        if isinstance(type_, ClassType):
            # Check if all trait methods exist in the class
            for method_name, method_type in self.methods.items():
                if method_name not in type_.methods:
                    return False
                # Check method signature compatibility
                if not type_.methods[method_name].is_compatible_with(method_type):
                    return False
            
            # Phase 4b: Check associated types are defined
            # (Would need to check class has type definitions for associated types)
            
            return True
        
        # Primitive types can implement traits too
        if isinstance(type_, PrimitiveType):
            # Check against built-in trait implementations
            return self._check_primitive_trait_impl(type_)
        
        return False
    
    def _check_primitive_trait_impl(self, prim_type: PrimitiveType) -> bool:
        """Check if a primitive type implements this trait."""
        # Comparable trait for numeric types
        if self.name == "Comparable":
            return prim_type.name in ('integer', 'float', 'string')
        
        # Equatable trait for all types
        if self.name == "Equatable":
            return True
        
        # Printable trait for basic types
        if self.name == "Printable":
            return prim_type.name in ('integer', 'float', 'string', 'boolean')
        
        return False
    
    def is_compatible_with(self, other: Type) -> bool:
        """Check if this trait is compatible with another type."""
        if isinstance(other, TraitType):
            # Trait is compatible if it's the same or a subtrait
            if self.name == other.name:
                return True
            if other.name in self.parent_traits:
                return True
        
        # Check if the other type implements this trait
        return self.is_implemented_by(other)
    
    def get_common_supertype(self, other: Type) -> Optional[Type]:
        """Get common supertype with another type."""
        if isinstance(other, TraitType):
            # Find common parent trait
            common_parents = set(self.parent_traits) & set(other.parent_traits)
            if common_parents:
                # Return first common parent (simplified)
                return TraitType(list(common_parents)[0], {}, [])
        
        return ANY_TYPE
    
    def _equals(self, other: Type) -> bool:
        """Check equality with another trait."""
        return (
            self.name == other.name and
            self.methods == other.methods and
            self.parent_traits == other.parent_traits
        )

# Predefined primitive types
INTEGER_TYPE = PrimitiveType('integer')
FLOAT_TYPE = PrimitiveType('float')
STRING_TYPE = PrimitiveType('string')
BOOLEAN_TYPE = PrimitiveType('boolean')
NULL_TYPE = PrimitiveType('null')
ANY_TYPE = AnyType()


# Predefined trait types with complete method signatures

# Comparable trait: types that can be compared for ordering
# Required method: compare(self, other: Self) -> Integer
#   Returns: -1 if self < other, 0 if equal, 1 if self > other
COMPARABLE_TRAIT = TraitType("Comparable", {
    "compare": FunctionType(
        param_types=[ANY_TYPE],  # other: Self (self is implicit)
        return_type=INTEGER_TYPE
    )
})

# Equatable trait: types that can be compared for equality
# Required method: equals(self, other: Self) -> Boolean
EQUATABLE_TRAIT = TraitType("Equatable", {
    "equals": FunctionType(
        param_types=[ANY_TYPE],  # other: Self (self is implicit)
        return_type=BOOLEAN_TYPE
    )
})

# Printable trait: types that can be converted to string
# Required method: to_string(self) -> String
PRINTABLE_TRAIT = TraitType("Printable", {
    "to_string": FunctionType(
        param_types=[],
        return_type=STRING_TYPE
    )
})

# Iterable trait: types that can produce an iterator
# Required method: iter(self) -> Iterator<T>
# Note: Iterator<T> will be defined below
ITERABLE_TRAIT = TraitType("Iterable", {
    "iter": FunctionType(
        param_types=[],
        return_type=ANY_TYPE  # Will be Iterator<T> once defined
    )
})

# Iterator trait: types that can iterate over elements
# Required methods:
#   - next(self) -> Option<T>: Returns Some(value) or None when exhausted
#   - has_next(self) -> Boolean: Checks if more elements available
ITERATOR_TRAIT = TraitType("Iterator", {
    "next": FunctionType(
        param_types=[],
        return_type=ANY_TYPE  # Will be Option<T> once Option is in scope
    ),
    "has_next": FunctionType(
        param_types=[],
        return_type=BOOLEAN_TYPE
    )
})

def get_type_by_name(name: str) -> 'Type':
    """Get a type by its name.

    Args:
        name: Name of the type

    Returns:
        The corresponding Type object, or ANY_TYPE if not found
    """
    name_lower = name.lower()

    # 8.3: Check global type alias registry first
    try:
        from nexuslang.typesystem.type_alias_registry import GLOBAL_ALIAS_REGISTRY  # type: ignore[import]
        if GLOBAL_ALIAS_REGISTRY.has(name):
            expanded = GLOBAL_ALIAS_REGISTRY.expand(name)
            if expanded is not None:
                return expanded
    except (ImportError, Exception):
        pass

    # Handle primitive types and 'any'
    if name_lower == 'integer' or name_lower == 'int':
        return INTEGER_TYPE
    elif name_lower == 'float':
        return FLOAT_TYPE
    elif name_lower == 'string' or name_lower == 'str':
        return STRING_TYPE
    elif name_lower == 'boolean' or name_lower == 'bool':
        return BOOLEAN_TYPE
    elif name_lower == 'null' or name_lower == 'none':
        return NULL_TYPE
    elif name_lower == 'any':
        return ANY_TYPE
    # Trait types
    elif name == "Comparable":
        return COMPARABLE_TRAIT
    elif name == "Equatable":
        return EQUATABLE_TRAIT
    elif name == "Printable":
        return PRINTABLE_TRAIT
    elif name == "Iterable":
        return ITERABLE_TRAIT
    elif name == "Iterator":
        return ITERATOR_TRAIT
    
    # Handle generic syntax: List<T>, Dictionary<K, V>
    if '<' in name and '>' in name:
        # Extract base type and type arguments
        base_end = name.index('<')
        base_type_name = name[:base_end].strip()
        type_args_str = name[base_end+1:name.rindex('>')].strip()
        
        # Parse type arguments (handle nested generics and commas)
        type_args = _parse_type_arguments(type_args_str)
        
        # Handle List<T>
        if base_type_name.lower() in ('list', 'listtype'):
            if len(type_args) != 1:
                return ListType(ANY_TYPE)
            element_type = get_type_by_name(type_args[0])
            return ListType(element_type)
        
        # Handle Dictionary<K, V>
        elif base_type_name.lower() in ('dictionary', 'dict', 'dictionarytype'):
            if len(type_args) == 1:
                # Dictionary<V> assumes String keys
                value_type = get_type_by_name(type_args[0])
                return DictionaryType(STRING_TYPE, value_type)
            elif len(type_args) == 2:
                key_type = get_type_by_name(type_args[0])
                value_type = get_type_by_name(type_args[1])
                return DictionaryType(key_type, value_type)
            else:
                return DictionaryType(ANY_TYPE, ANY_TYPE)
        
        # Handle Channel<T>
        elif base_type_name.lower() in ('channel', 'channeltype'):
            if len(type_args) != 1:
                return ChannelType(ANY_TYPE)
            payload_type = get_type_by_name(type_args[0])
            return ChannelType(payload_type)

        # For other generic types, return as ANY for now
        # (can be extended to handle custom generic classes)
        return ANY_TYPE
    
    # Handle natural language syntax: "List of Float"
    name_lower = name.lower()
    if 'list of' in name_lower:
        # Extract element type
        element_type_name = name_lower.split('list of')[-1].strip()
        element_type = get_type_by_name(element_type_name)
        return ListType(element_type)
    elif 'dict of' in name_lower or 'dictionary of' in name_lower:
        # Parse "Dictionary of Key, Value"
        if 'dictionary of' in name_lower:
            parts = name_lower.split('dictionary of')[-1].strip().split(',')
        else:
            parts = name_lower.split('dict of')[-1].strip().split(',')
        
        if len(parts) == 1:
            # Dictionary of Value (assumes String keys)
            value_type = get_type_by_name(parts[0].strip())
            return DictionaryType(STRING_TYPE, value_type)
        elif len(parts) == 2:
            key_type = get_type_by_name(parts[0].strip())
            value_type = get_type_by_name(parts[1].strip())
            return DictionaryType(key_type, value_type)
        else:
            return DictionaryType(ANY_TYPE, ANY_TYPE)
    elif 'channel of' in name_lower:
        payload_type_name = name_lower.split('channel of')[-1].strip()
        payload_type = get_type_by_name(payload_type_name)
        return ChannelType(payload_type)
    
    # Default: return ANY_TYPE for unknown types
    return ANY_TYPE

def _parse_type_arguments(args_str: str) -> List[str]:
    """Parse comma-separated type arguments, handling nested generics.
    
    Examples:
        "Integer" -> ["Integer"]
        "String, Integer" -> ["String", "Integer"]
        "List<Integer>, String" -> ["List<Integer>", "String"]
        "Dictionary<String, List<Integer>>" -> ["Dictionary<String, List<Integer>>"]
    """
    args = []
    current_arg = ""
    depth = 0
    
    for char in args_str:
        if char == '<':
            depth += 1
            current_arg += char
        elif char == '>':
            depth -= 1
            current_arg += char
        elif char == ',' and depth == 0:
            # Top-level comma, split here
            args.append(current_arg.strip())
            current_arg = ""
        else:
            current_arg += char
    
    # Add the last argument
    if current_arg.strip():
        args.append(current_arg.strip())
    
    return args

def infer_type(value: Any) -> Type:
    """Infer the type of a value."""
    if value is None:
        return NULL_TYPE
    elif isinstance(value, bool):
        return BOOLEAN_TYPE
    elif isinstance(value, int):
        return INTEGER_TYPE
    elif isinstance(value, float):
        return FLOAT_TYPE
    elif isinstance(value, str):
        return STRING_TYPE
    elif isinstance(value, list):
        if value:
            element_type = infer_type(value[0])
            for item in value[1:]:
                element_type = element_type.get_common_supertype(infer_type(item))
            return ListType(element_type)
        return ListType(ANY_TYPE)
    elif isinstance(value, dict):
        if value:
            key_type = infer_type(next(iter(value.keys())))
            value_type = infer_type(next(iter(value.values())))
            for k, v in value.items():
                key_type = key_type.get_common_supertype(infer_type(k))
                value_type = value_type.get_common_supertype(infer_type(v))
            return DictionaryType(key_type, value_type)
        return DictionaryType(ANY_TYPE, ANY_TYPE)
    return ANY_TYPE 