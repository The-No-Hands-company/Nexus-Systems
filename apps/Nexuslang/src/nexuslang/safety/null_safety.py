"""
Null Safety Checker
===================

Enforces null safety at compile time:
- Non-nullable types by default
- Explicit Optional<T> for nullable values
- Warnings for unsafe null operations
"""

from typing import Set, Dict, Any, Optional
from dataclasses import dataclass
import copy


@dataclass
class NullabilityInfo:
    """Information about variable nullability."""
    name: str
    is_nullable: bool
    is_initialized: bool
    type_name: Optional[str] = None


class NullSafetyChecker:
    """
    Static analyzer for null safety.
    
    Checks:
    - Use of uninitialized variables
    - Null dereferences
    - Missing null checks
    - Optional<T> unwrapping safety
    """
    
    def __init__(self):
        self.variables: Dict[str, NullabilityInfo] = {}
        self.warnings: list[str] = []
        self.errors: list[str] = []
    
    def declare_variable(self, name: str, type_annotation: Optional[str], nullable: bool = False):
        """Declare a variable with nullability information."""
        self.variables[name] = NullabilityInfo(
            name=name,
            is_nullable=nullable,
            is_initialized=False,
            type_name=type_annotation
        )
    
    def initialize_variable(self, name: str):
        """Mark a variable as initialized."""
        if name in self.variables:
            self.variables[name].is_initialized = True
    
    def check_variable_use(self, name: str, location: str = "") -> bool:
        """
        Check if a variable is safe to use.
        
        Returns:
            True if safe, False if error
        """
        if name not in self.variables:
            self.errors.append(
                f"{location}: Use of undeclared variable '{name}'"
            )
            return False
        
        var_info = self.variables[name]
        
        # Check initialization
        if not var_info.is_initialized:
            self.errors.append(
                f"{location}: Use of uninitialized variable '{name}'"
            )
            return False
        
        return True
    
    def check_null_dereference(self, name: str, location: str = "") -> bool:
        """
        Check if dereferencing a variable is safe.
        
        Returns:
            True if safe, False if error/warning
        """
        if not self.check_variable_use(name, location):
            return False
        
        var_info = self.variables[name]
        
        # Warn about potential null dereference
        if var_info.is_nullable:
            self.warnings.append(
                f"{location}: Potential null dereference of '{name}'. "
                f"Consider using null check or Optional unwrapping."
            )
            return False
        
        return True
    
    def check_optional_unwrap(self, name: str, location: str = "", is_safe: bool = False) -> bool:
        """
        Check Optional<T> unwrapping.
        
        Args:
            name: Variable name
            location: Source location
            is_safe: True if using safe unwrap (unwrap_or, unwrap_or_else)
        
        Returns:
            True if safe, False if error
        """
        if not self.check_variable_use(name, location):
            return False
        
        var_info = self.variables[name]
        
        # Check if this is an Optional type
        if var_info.type_name and "Optional" not in var_info.type_name:
            self.warnings.append(
                f"{location}: Unwrapping non-Optional type '{var_info.type_name}'"
            )
            return False
        
        # Unsafe unwrap on nullable
        if var_info.is_nullable and not is_safe:
            self.warnings.append(
                f"{location}: Unsafe unwrap of Optional '{name}'. "
                f"Use 'unwrap_or' or check for null first."
            )
            return False
        
        return True
    
    def check_ast(self, ast: Any) -> tuple[list[str], list[str]]:
        """
        Check an entire AST for null safety violations.
        
        Returns:
            (errors, warnings)
        """
        self._walk_ast(ast)
        return (self.errors, self.warnings)
    
    def _walk_ast(self, node: Any):
        """Walk AST and perform null safety checks."""
        if node is None:
            return
        
        node_type = type(node).__name__
        
        # Variable declaration
        if node_type == 'VariableDeclaration':
            type_ann = getattr(node, 'type_annotation', None)
            nullable = type_ann and 'Optional' in str(type_ann)
            self.declare_variable(node.name, type_ann, nullable)
            
            # Check if initialized
            if hasattr(node, 'value') and node.value is not None:
                self.initialize_variable(node.name)
        
        # Variable use
        elif node_type == 'Identifier':
            name = getattr(node, 'name', None)
            if name:
                self.check_variable_use(name)
        
        # Member access (potential null dereference)
        elif node_type == 'MemberAccess':
            obj_name = getattr(node, 'object', None)
            if obj_name and hasattr(obj_name, 'name'):
                self.check_null_dereference(obj_name.name)
        
        # Recursively walk children
        for attr_name in dir(node):
            if attr_name.startswith('_'):
                continue
            
            attr = getattr(node, attr_name)
            
            if isinstance(attr, list):
                for item in attr:
                    self._walk_ast(item)
            elif hasattr(attr, '__class__') and hasattr(attr.__class__, '__name__'):
                self._walk_ast(attr)
    
    def print_diagnostics(self):
        """Print all errors and warnings."""
        if self.errors:
            print("Null Safety Errors:")
            for error in self.errors:
                print(f"   {error}")
        
        if self.warnings:
            print("\nNull Safety Warnings:")
            for warning in self.warnings:
                print(f"   {warning}")


__all__ = ['NullSafetyChecker', 'NullabilityInfo']
