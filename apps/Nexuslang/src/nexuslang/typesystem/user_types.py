"""
User-defined types for the NexusLang language.
This module provides support for user-defined types with inheritance.
"""

from typing import Dict, List, Optional, Set, Any, Tuple, Type
from ..typesystem.types import (
    Type, ClassType, FunctionType, GenericType, GenericParameter,
    TypeKind, ANY_TYPE
)

class TypeRegistry:
    """Registry for user-defined types with inheritance tracking."""
    
    def __init__(self):
        self.types: Dict[str, Type] = {}
        self.inheritance_graph: Dict[str, List[str]] = {}  # Maps class to its direct parents
        self.interfaces: Dict[str, List[str]] = {}  # Maps interface to required methods
    
    def register_type(self, type_obj: Type) -> None:
        """Register a user-defined type."""
        self.types[type_obj.name] = type_obj
        
        # Initialize inheritance info if not present
        if isinstance(type_obj, ClassType) and type_obj.name not in self.inheritance_graph:
            self.inheritance_graph[type_obj.name] = []
    
    def register_inheritance(self, child_name: str, parent_name: str) -> None:
        """Register an inheritance relationship between classes."""
        if child_name not in self.inheritance_graph:
            self.inheritance_graph[child_name] = []
        
        if parent_name not in self.types:
            raise ValueError(f"Parent class not defined: {parent_name}")
            
        if parent_name not in self.inheritance_graph[child_name]:
            self.inheritance_graph[child_name].append(parent_name)
    
    def register_interface(self, interface_name: str, required_methods: List[str]) -> None:
        """Register an interface with its required methods."""
        self.interfaces[interface_name] = required_methods
    
    def register_interface_implementation(self, class_name: str, interface_name: str) -> None:
        """Register that a class implements an interface."""
        if interface_name not in self.interfaces:
            raise ValueError(f"Interface not defined: {interface_name}")
            
        # For type checking purposes, implementing an interface is like inheriting from it
        self.register_inheritance(class_name, interface_name)
    
    def get_type(self, name: str) -> Optional[Type]:
        """Get a type by name from the registry."""
        return self.types.get(name)
    
    def is_subtype(self, child_name: str, parent_name: str) -> bool:
        """Check if child is a subtype of parent."""
        # Same type is a subtype of itself
        if child_name == parent_name:
            return True
            
        # Check direct inheritance
        if child_name in self.inheritance_graph and parent_name in self.inheritance_graph[child_name]:
            return True
            
        # Check indirect inheritance (transitive closure)
        if child_name in self.inheritance_graph:
            for direct_parent in self.inheritance_graph[child_name]:
                if self.is_subtype(direct_parent, parent_name):
                    return True
                    
        return False
    
    def get_inherited_properties(self, class_name: str) -> Dict[str, Type]:
        """Get all properties inherited from parent classes."""
        result = {}
        
        if class_name not in self.types or not isinstance(self.types[class_name], ClassType):
            return result
            
        # Get properties from parent classes
        if class_name in self.inheritance_graph:
            for parent_name in self.inheritance_graph[class_name]:
                if parent_name in self.types and isinstance(self.types[parent_name], ClassType):
                    parent_class = self.types[parent_name]
                    # Add all properties from parent
                    for prop_name, prop_type in parent_class.properties.items():
                        result[prop_name] = prop_type
                        
                    # Add inherited properties from parent's parents
                    parent_inherited = self.get_inherited_properties(parent_name)
                    for prop_name, prop_type in parent_inherited.items():
                        result[prop_name] = prop_type
                        
        return result
    
    def get_inherited_methods(self, class_name: str) -> Dict[str, FunctionType]:
        """Get all methods inherited from parent classes."""
        result = {}
        
        if class_name not in self.types or not isinstance(self.types[class_name], ClassType):
            return result
            
        # Get methods from parent classes
        if class_name in self.inheritance_graph:
            for parent_name in self.inheritance_graph[class_name]:
                if parent_name in self.types and isinstance(self.types[parent_name], ClassType):
                    parent_class = self.types[parent_name]
                    # Add all methods from parent
                    for method_name, method_type in parent_class.methods.items():
                        result[method_name] = method_type
                        
                    # Add inherited methods from parent's parents
                    parent_inherited = self.get_inherited_methods(parent_name)
                    for method_name, method_type in parent_inherited.items():
                        result[method_name] = method_type
                        
        return result
    
    def check_interface_implementation(self, class_name: str, interface_name: str) -> List[str]:
        """Check if a class correctly implements an interface, returning missing methods."""
        if interface_name not in self.interfaces:
            raise ValueError(f"Interface not defined: {interface_name}")
            
        if class_name not in self.types or not isinstance(self.types[class_name], ClassType):
            raise ValueError(f"Class not defined: {class_name}")
            
        required_methods = self.interfaces[interface_name]
        class_type = self.types[class_name]
        
        # Get all methods, including inherited ones
        all_methods = dict(class_type.methods)
        inherited_methods = self.get_inherited_methods(class_name)
        all_methods.update(inherited_methods)
        
        # Check for missing methods
        missing_methods = []
        for method_name in required_methods:
            if method_name not in all_methods:
                missing_methods.append(method_name)
                
        return missing_methods
    
    def create_class_type(self, name: str, properties: Dict[str, Type] = None,
                        methods: Dict[str, FunctionType] = None,
                        parent_classes: List[str] = None) -> ClassType:
        """Create and register a class type with optional inheritance."""
        class_type = ClassType(name, properties or {}, methods or {})
        self.register_type(class_type)
        
        # Register inheritance relationships
        if parent_classes:
            for parent in parent_classes:
                self.register_inheritance(name, parent)
                
                # Inherit properties and methods from parent
                if parent in self.types and isinstance(self.types[parent], ClassType):
                    parent_class = self.types[parent]
                    
                    # Inherit properties
                    for prop_name, prop_type in parent_class.properties.items():
                        if prop_name not in class_type.properties:
                            class_type.properties[prop_name] = prop_type
                            
                    # Inherit methods
                    for method_name, method_type in parent_class.methods.items():
                        if method_name not in class_type.methods:
                            class_type.methods[method_name] = method_type
                            
        return class_type
    
    def create_generic_class_type(self, name: str, type_parameters: List[str],
                                properties: Dict[str, Type] = None,
                                methods: Dict[str, FunctionType] = None) -> GenericType:
        """Create and register a generic class type."""
        # Create generic parameters
        generic_params = [GenericParameter(param) for param in type_parameters]
        
        # Create generic type
        generic_type = GenericType(name, generic_params)
        self.register_type(generic_type)
        
        return generic_type

# Global registry for user-defined types
USER_TYPE_REGISTRY = TypeRegistry() 