"""
Ownership and Borrow Checker
=============================

Basic ownership tracking to prevent use-after-free and double-free errors.

Inspired by Rust's ownership system but simplified for NexusLang.
"""

from typing import Dict, Set, Optional, List
from dataclasses import dataclass
from enum import Enum


class OwnershipState(Enum):
    """State of a variable's ownership."""
    OWNED = "owned"           # Variable owns the value
    MOVED = "moved"           # Value has been moved
    BORROWED = "borrowed"     # Value is borrowed (read-only)
    MUT_BORROWED = "mut_borrowed"  # Value is mutably borrowed


@dataclass
class OwnershipInfo:
    """Ownership information for a variable."""
    name: str
    state: OwnershipState
    moved_to: Optional[str] = None  # Where it was moved to
    borrowed_by: Set[str] = None    # What borrows it
    
    def __post_init__(self):
        if self.borrowed_by is None:
            self.borrowed_by = set()


class OwnershipTracker:
    """
    Track ownership and moves to prevent use-after-move errors.
    
    Rules:
    1. Each value has exactly one owner
    2. When owner goes out of scope, value is dropped
    3. Moving transfers ownership
    4. Cannot use a value after it's moved
    5. Can have multiple immutable borrows OR one mutable borrow
    """
    
    def __init__(self):
        self.variables: Dict[str, OwnershipInfo] = {}
        self.scopes: List[Set[str]] = [set()]  # Stack of scopes
        self.errors: List[str] = []
        self.warnings: List[str] = []
    
    def enter_scope(self):
        """Enter a new scope."""
        self.scopes.append(set())
    
    def exit_scope(self):
        """Exit current scope and drop variables."""
        if len(self.scopes) <= 1:
            return  # Don't exit global scope
        
        scope_vars = self.scopes.pop()
        for var_name in scope_vars:
            if var_name in self.variables:
                del self.variables[var_name]
    
    def declare_variable(self, name: str):
        """Declare a new variable (owned)."""
        self.variables[name] = OwnershipInfo(
            name=name,
            state=OwnershipState.OWNED
        )
        self.scopes[-1].add(name)
    
    def move_variable(self, from_var: str, to_var: str, location: str = "") -> bool:
        """
        Move ownership from one variable to another.
        
        Returns:
            True if move is valid, False otherwise
        """
        if from_var not in self.variables:
            self.errors.append(
                f"{location}: Cannot move undefined variable '{from_var}'"
            )
            return False
        
        from_info = self.variables[from_var]
        
        # Check if already moved
        if from_info.state == OwnershipState.MOVED:
            self.errors.append(
                f"{location}: Use of moved value '{from_var}'. "
                f"Value was moved to '{from_info.moved_to}'"
            )
            return False
        
        # Check if borrowed
        if from_info.state in (OwnershipState.BORROWED, OwnershipState.MUT_BORROWED):
            self.errors.append(
                f"{location}: Cannot move '{from_var}' while it is borrowed"
            )
            return False
        
        # Perform move
        from_info.state = OwnershipState.MOVED
        from_info.moved_to = to_var
        
        # Create new owner
        self.variables[to_var] = OwnershipInfo(
            name=to_var,
            state=OwnershipState.OWNED
        )
        self.scopes[-1].add(to_var)
        
        return True
    
    def borrow_variable(self, var: str, mutable: bool = False, location: str = "") -> bool:
        """
        Borrow a variable.
        
        Args:
            var: Variable to borrow
            mutable: True for mutable borrow
            location: Source location
        
        Returns:
            True if borrow is valid, False otherwise
        """
        if var not in self.variables:
            self.errors.append(
                f"{location}: Cannot borrow undefined variable '{var}'"
            )
            return False
        
        var_info = self.variables[var]
        
        # Check if moved
        if var_info.state == OwnershipState.MOVED:
            self.errors.append(
                f"{location}: Cannot borrow moved value '{var}'"
            )
            return False
        
        # Mutable borrow rules
        if mutable:
            if var_info.state == OwnershipState.BORROWED:
                self.errors.append(
                    f"{location}: Cannot mutably borrow '{var}' while it has immutable borrows"
                )
                return False
            
            if var_info.state == OwnershipState.MUT_BORROWED:
                self.errors.append(
                    f"{location}: Cannot mutably borrow '{var}' while it is already mutably borrowed"
                )
                return False
            
            var_info.state = OwnershipState.MUT_BORROWED
        else:
            # Immutable borrow
            if var_info.state == OwnershipState.MUT_BORROWED:
                self.errors.append(
                    f"{location}: Cannot borrow '{var}' while it is mutably borrowed"
                )
                return False
            
            var_info.state = OwnershipState.BORROWED
        
        return True
    
    def use_variable(self, var: str, location: str = "") -> bool:
        """
        Check if using a variable is valid.
        
        Returns:
            True if use is valid, False otherwise
        """
        if var not in self.variables:
            self.errors.append(
                f"{location}: Use of undefined variable '{var}'"
            )
            return False
        
        var_info = self.variables[var]
        
        if var_info.state == OwnershipState.MOVED:
            self.errors.append(
                f"{location}: Use of moved value '{var}'. "
                f"Value was moved to '{var_info.moved_to}'"
            )
            return False
        
        return True
    
    def print_diagnostics(self):
        """Print all errors and warnings."""
        if self.errors:
            print("Ownership Errors:")
            for error in self.errors:
                print(f"   {error}")
        
        if self.warnings:
            print("\nOwnership Warnings:")
            for warning in self.warnings:
                print(f"   {warning}")


class MoveSemantics:
    """
    Helper class for implementing move semantics in NexusLang.
    
    Provides utilities for:
    - Detecting moves in assignments
    - Implementing copy-on-write
    - Managing ownership transfers
    """
    
    @staticmethod
    def is_move_assignment(node: any) -> bool:
        """Check if an assignment is a move (not a copy)."""
        # Simple heuristic: large types are moved, small types are copied
        if not hasattr(node, 'type_annotation'):
            return False
        
        type_ann = str(node.type_annotation)
        
        # These types are typically moved
        move_types = ['String', 'List', 'Dictionary', 'Class', 'Struct']
        
        return any(t in type_ann for t in move_types)
    
    @staticmethod
    def needs_clone(type_name: str) -> bool:
        """Check if a type needs explicit cloning."""
        # Types that should be explicitly cloned
        clone_types = ['String', 'List', 'Dictionary']
        return any(t in type_name for t in clone_types)


__all__ = [
    'OwnershipTracker', 'OwnershipState', 'OwnershipInfo',
    'MoveSemantics'
]
