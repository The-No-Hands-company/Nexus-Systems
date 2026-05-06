"""
Integration for the NexusLang type system.
This module connects all parts of the type system together.
"""

from typing import Type
from ..parser.ast import (
    Program, ClassDefinition, PropertyDeclaration, MethodDefinition, 
    FunctionDefinition, VariableDeclaration
)
from ..typesystem.types import (
    Type, PrimitiveType, ListType, DictionaryType, ClassType, 
    FunctionType, UnionType, AnyType, GenericType, GenericParameter,
    INTEGER_TYPE, FLOAT_TYPE, STRING_TYPE, BOOLEAN_TYPE, NULL_TYPE, ANY_TYPE,
    get_type_by_name, infer_type
)
from ..typesystem.typechecker import TypeChecker, TypeEnvironment
from ..typesystem.type_inference import TypeInferenceEngine
from ..typesystem.user_types import USER_TYPE_REGISTRY

class TypeSystem:
    """Main entry point for all type system functionality."""
    
    def __init__(self):
        self.type_checker = TypeChecker()
        self.type_inference = TypeInferenceEngine()
        self.type_registry = USER_TYPE_REGISTRY
    
    def analyze_program(self, program: Program) -> list:
        """Analyze a program, registering types and checking for errors."""
        # First pass: register all user-defined types
        self._register_types(program)
        
        # Second pass: type check the program
        return self.type_checker.check_program(program)
    
    def _register_types(self, program: Program) -> None:
        """Register all user-defined types in a program."""
        for statement in program.statements:
            if isinstance(statement, ClassDefinition):
                self._register_class(statement)
    
    def _register_class(self, class_def: ClassDefinition) -> None:
        """Register a class definition."""
        # Extract properties and their types
        properties = {}
        for prop in class_def.properties:
            if isinstance(prop, PropertyDeclaration) and prop.type_annotation:
                properties[prop.name] = get_type_by_name(prop.type_annotation)
        
        # Extract methods and their types
        methods = {}
        for method in class_def.methods:
            if isinstance(method, MethodDefinition):
                param_types = []
                for param in method.parameters:
                    param_type = ANY_TYPE
                    if param.type_annotation:
                        param_type = get_type_by_name(param.type_annotation)
                    param_types.append(param_type)
                
                return_type = ANY_TYPE
                if method.return_type:
                    return_type = get_type_by_name(method.return_type)
                
                methods[method.name] = FunctionType(param_types, return_type)
        
        # Check for generic type parameters
        if hasattr(class_def, 'generic_parameters') and class_def.generic_parameters:
            # Register as generic type
            self.type_registry.create_generic_class_type(
                class_def.name,
                class_def.generic_parameters,
                properties,
                methods
            )
        else:
            # Register as regular class
            parent_classes = class_def.parent_classes if hasattr(class_def, 'parent_classes') else None
            implemented_interfaces = class_def.implemented_interfaces if hasattr(class_def, 'implemented_interfaces') else None
            
            self.type_registry.create_class_type(
                class_def.name,
                properties,
                methods,
                parent_classes
            )
            
            # Register interface implementations
            if implemented_interfaces:
                for interface in implemented_interfaces:
                    self.type_registry.register_interface_implementation(class_def.name, interface)
    
    def infer_variable_type(self, var_decl: VariableDeclaration, env: dict = None) -> Type:
        """Infer the type of a variable declaration."""
        env = env or {}
        return self.type_inference.infer_variable_declaration(var_decl, env)
    
    def infer_function_return_type(self, func_def: FunctionDefinition, env: dict = None) -> Type:
        """Infer the return type of a function."""
        env = env or {}
        return self.type_inference.infer_function_return_type(func_def, env)
    
    def register_interface(self, name: str, required_methods: list) -> None:
        """Register an interface with its required methods."""
        self.type_registry.register_interface(name, required_methods)

# Global type system instance
TYPE_SYSTEM = TypeSystem() 