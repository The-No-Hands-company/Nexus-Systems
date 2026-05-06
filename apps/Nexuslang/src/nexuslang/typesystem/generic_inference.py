"""
Generic type parameter inference for NexusLang.

Infers type arguments from function call context and performs type substitution.
Example: map([1, 2, 3], fn) infers T=Integer from the list argument.
"""

from typing import Dict, List, Optional, Set, Tuple, Type
from ..typesystem.types import (
    Type, ListType, DictionaryType, FunctionType,
    get_type_by_name, INTEGER_TYPE, STRING_TYPE, FLOAT_TYPE, BOOLEAN_TYPE, ANY_TYPE
)


class TypeVariable:
    """Represents a generic type parameter (e.g., T, K, V)."""
    
    def __init__(self, name: str, constraints: Optional[List[Type]] = None):
        self.name = name
        self.constraints = constraints or []
    
    def __repr__(self):
        return f"TypeVariable({self.name})"
    
    def __eq__(self, other):
        return isinstance(other, TypeVariable) and self.name == other.name
    
    def __hash__(self):
        return hash(self.name)


class TypeSubstitution:
    """Manages type variable substitutions during inference."""
    
    def __init__(self):
        self.substitutions: Dict[str, Type] = {}
    
    def bind(self, type_var: str, concrete_type: Type) -> bool:
        """Bind a type variable to a concrete type."""
        if type_var in self.substitutions:
            # Check if binding is consistent
            existing = self.substitutions[type_var]
            return existing.is_compatible_with(concrete_type)
        
        self.substitutions[type_var] = concrete_type
        return True
    
    def get(self, type_var: str) -> Optional[Type]:
        """Get the substitution for a type variable."""
        return self.substitutions.get(type_var)
    
    def apply(self, type_annotation: str) -> str:
        """Apply substitutions to a type annotation string."""
        result = type_annotation
        for var_name, concrete_type in self.substitutions.items():
            # Get the type name for substitution
            type_name = concrete_type.name if hasattr(concrete_type, 'name') else str(concrete_type)
            result = result.replace(var_name, type_name)
        return result
    
    def __repr__(self):
        return f"TypeSubstitution({self.substitutions})"


class GenericTypeInference:
    """Infers generic type arguments from usage context."""
    
    def __init__(self):
        self.substitution = TypeSubstitution()
    
    def infer_from_arguments(
        self,
        type_parameters: List[str],
        parameter_types: List[str],
        argument_types: List[Type]
    ) -> Dict[str, Type]:
        """
        Infer type arguments from function arguments.
        
        Args:
            type_parameters: Generic type parameter names (e.g., ['T', 'R'])
            parameter_types: Function parameter type annotations (e.g., ['List<T>', 'T'])
            argument_types: Actual argument types (e.g., [ListType(INTEGER_TYPE), INTEGER_TYPE])
        
        Returns:
            Dictionary mapping type parameter names to inferred types
        """
        self.substitution = TypeSubstitution()
        
        # Match each parameter with its argument
        for param_type_str, arg_type in zip(parameter_types, argument_types):
            self._unify(param_type_str, arg_type, type_parameters)
        
        return self.substitution.substitutions
    
    def _unify(self, param_type_str: str, arg_type: Type, type_params: List[str]) -> bool:
        """
        Unify a parameter type with an argument type to infer type variables.
        
        Args:
            param_type_str: Parameter type annotation (may contain type variables)
            arg_type: Actual argument type
            type_params: List of type parameter names to infer
        
        Returns:
            True if unification succeeded
        """
        # Check if param_type_str is a type variable
        if param_type_str in type_params:
            return self.substitution.bind(param_type_str, arg_type)
        
        # Handle List<T> pattern
        if param_type_str.startswith('List<') and param_type_str.endswith('>'):
            if not isinstance(arg_type, ListType):
                return False
            
            # Extract inner type: List<T> -> T
            inner_param = param_type_str[5:-1]
            return self._unify(inner_param, arg_type.element_type, type_params)
        
        # Handle Dictionary<K, V> pattern
        if param_type_str.startswith('Dictionary<') and param_type_str.endswith('>'):
            if not isinstance(arg_type, DictionaryType):
                return False
            
            # Extract key and value types: Dictionary<K, V> -> K, V
            inner = param_type_str[11:-1]
            parts = self._split_type_args(inner)
            if len(parts) != 2:
                return False
            
            key_param, value_param = parts
            return (self._unify(key_param.strip(), arg_type.key_type, type_params) and
                    self._unify(value_param.strip(), arg_type.value_type, type_params))
        
        # Handle nested generics recursively
        if '<' in param_type_str:
            # Extract base type and type arguments
            base_end = param_type_str.index('<')
            base_type = param_type_str[:base_end]
            type_args_str = param_type_str[base_end+1:-1]
            type_args = self._split_type_args(type_args_str)
            
            # Match with actual type structure
            if base_type.lower() == 'list' and isinstance(arg_type, ListType):
                if len(type_args) == 1:
                    return self._unify(type_args[0].strip(), arg_type.element_type, type_params)
            elif base_type.lower() == 'dictionary' and isinstance(arg_type, DictionaryType):
                if len(type_args) == 2:
                    return (self._unify(type_args[0].strip(), arg_type.key_type, type_params) and
                            self._unify(type_args[1].strip(), arg_type.value_type, type_params))
        
        # If no type variables, check for compatibility
        try:
            param_type_obj = get_type_by_name(param_type_str)
            return arg_type.is_compatible_with(param_type_obj)
        except:
            return True  # Allow if we can't parse the type
    
    def _split_type_args(self, args_str: str) -> List[str]:
        """Split type arguments by comma, respecting nesting."""
        parts = []
        current = ""
        depth = 0
        
        for char in args_str:
            if char == '<':
                depth += 1
                current += char
            elif char == '>':
                depth -= 1
                current += char
            elif char == ',' and depth == 0:
                parts.append(current.strip())
                current = ""
            else:
                current += char
        
        if current.strip():
            parts.append(current.strip())
        
        return parts
    
    def substitute_return_type(self, return_type: str, substitutions: Dict[str, Type]) -> Type:
        """
        Apply type substitutions to a return type annotation.
        
        Args:
            return_type: Return type annotation (may contain type variables)
            substitutions: Map of type variable names to concrete types
        
        Returns:
            Concrete return type with substitutions applied
        """
        # Check if return type is a type variable
        if return_type in substitutions:
            return substitutions[return_type]
        
        # Handle List<T> pattern
        if return_type.startswith('List<') and return_type.endswith('>'):
            inner = return_type[5:-1]
            if inner in substitutions:
                return ListType(substitutions[inner])
            else:
                # Recursively substitute
                inner_type = self.substitute_return_type(inner, substitutions)
                return ListType(inner_type)
        
        # Handle Dictionary<K, V> pattern
        if return_type.startswith('Dictionary<') and return_type.endswith('>'):
            inner = return_type[11:-1]
            parts = self._split_type_args(inner)
            if len(parts) == 2:
                key_type = self.substitute_return_type(parts[0].strip(), substitutions)
                value_type = self.substitute_return_type(parts[1].strip(), substitutions)
                return DictionaryType(key_type, value_type)
        
        # Try to parse as regular type
        try:
            return get_type_by_name(return_type)
        except:
            return ANY_TYPE


def infer_generic_types(
    type_parameters: List[str],
    parameter_types: List[str],
    argument_types: List[Type]
) -> Dict[str, Type]:
    """
    Convenience function for inferring generic type arguments.
    
    Example:
        type_parameters = ['T', 'R']
        parameter_types = ['List<T>', 'Function']
        argument_types = [ListType(INTEGER_TYPE), some_function]
        result = infer_generic_types(...)
        # result = {'T': INTEGER_TYPE}
    """
    inferrer = GenericTypeInference()
    return inferrer.infer_from_arguments(type_parameters, parameter_types, argument_types)
