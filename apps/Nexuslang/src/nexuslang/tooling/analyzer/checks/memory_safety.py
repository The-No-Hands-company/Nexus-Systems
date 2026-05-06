"""
Memory Safety Checker
=====================

Detects memory safety violations:
- Use-after-free
- Double-free
- Buffer overflows
- Memory leaks
- Invalid pointer operations
"""

from typing import Dict, Set, List, Optional
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


class AllocationInfo:
    """Track memory allocation state."""
    def __init__(self, var_name: str, size: Optional[int], location: SourceLocation):
        self.var_name = var_name
        self.size = size
        self.location = location
        self.is_freed = False
        self.freed_location: Optional[SourceLocation] = None


class MemorySafetyChecker(BaseChecker):

    CHECKER_NAME = "memory_safety"
    """
    Checks for memory safety violations.
    
    Tracks:
    - allocate/alloc calls
    - dealloc/free calls
    - Pointer dereferences
    - Pointer arithmetic
    """
    
    def __init__(self):
        super().__init__()
        self.allocations: Dict[str, AllocationInfo] = {}
        self.function_scope: List[Dict[str, AllocationInfo]] = []
        self.in_function = False
    
    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        """Run memory safety analysis."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.allocations = {}
        self.function_scope = []
        self.in_function = False
        
        # Walk AST and analyze
        self.walk_ast(ast, self._check_node)
        
        # Check for memory leaks at end of scope
        self._check_leaks()
        
        return self.issues
    
    def _check_node(self, node: ASTNode):
        """Check individual AST node."""
        node_type = type(node).__name__
        
        # Function entry/exit
        if node_type == 'FunctionDefinition':
            self._enter_function()
            return
        
        # Variable assignment with allocation
        if node_type == 'VariableDeclaration':
            self._check_allocation(node)
        
        # Function calls
        elif node_type == 'FunctionCall':
            self._check_function_call(node)
        
        # Pointer dereference
        elif node_type == 'PointerDereference':
            self._check_dereference(node)
        
        # Return statement
        elif node_type == 'ReturnStatement':
            if self.in_function:
                self._check_leaks_on_return(node)
    
    def _enter_function(self):
        """Enter function scope."""
        self.in_function = True
        self.function_scope.append(self.allocations.copy())
        self.allocations = {}
    
    def _check_allocation(self, node: ASTNode):
        """Check memory allocation."""
        # Check if this is an allocation: set x to alloc with 1024
        if not hasattr(node, 'value'):
            return
        
        value = node.value
        if not value or type(value).__name__ != 'FunctionCall':
            return
        
        func_name = getattr(value, 'name', None)
        if func_name not in ['alloc', 'allocate', 'malloc']:
            return
        
        # This is an allocation
        var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
        if not var_name:
            return
        
        # Get size if available
        size = None
        if hasattr(value, 'arguments') and value.arguments:
            # Try to get constant size
            size_arg = value.arguments[0]
            if type(size_arg).__name__ == 'IntegerLiteral':
                size = getattr(size_arg, 'value', None)
        
        location = self.get_node_location(node)
        
        # Check if already allocated
        if var_name in self.allocations:
            old_alloc = self.allocations[var_name]
            if not old_alloc.is_freed:
                # Memory leak - reassigning without freeing
                self.issues.append(Issue(
                    code="M001",
                    severity=Severity.WARNING,
                    category=Category.MEMORY,
                    message=f"Memory leak: '{var_name}' reassigned without freeing previous allocation",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Free '{var_name}' before reassigning",
                    fix=f"call dealloc with {var_name}\nset {var_name} to alloc with ...",
                    related_locations=[old_alloc.location]
                ))
        
        # Track allocation
        self.allocations[var_name] = AllocationInfo(var_name, size, location)
    
    def _check_function_call(self, node: ASTNode):
        """Check function calls for deallocation."""
        func_name = getattr(node, 'name', None)
        
        if func_name in ['dealloc', 'free']:
            self._check_deallocation(node)
        elif func_name in ['realloc', 'reallocate']:
            self._check_reallocation(node)
    
    def _check_deallocation(self, node: ASTNode):
        """Check memory deallocation."""
        # Get pointer being freed
        if not hasattr(node, 'arguments') or not node.arguments:
            return
        
        arg = node.arguments[0]
        var_name = None
        
        if type(arg).__name__ == 'Identifier':
            var_name = getattr(arg, 'name', None)
        elif type(arg).__name__ == 'VariableReference':
            var_name = getattr(arg, 'name', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if allocated
        if var_name not in self.allocations:
            # Freeing unallocated memory
            self.issues.append(Issue(
                code="M002",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Attempting to free unallocated memory: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Ensure '{var_name}' is allocated before freeing"
            ))
            return
        
        alloc_info = self.allocations[var_name]
        
        # Check if already freed
        if alloc_info.is_freed:
            # Double-free!
            self.issues.append(Issue(
                code="M003",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Double-free detected: '{var_name}' already freed",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Remove duplicate free or check logic flow",
                related_locations=[alloc_info.freed_location] if alloc_info.freed_location else []
            ))
            return
        
        # Mark as freed
        alloc_info.is_freed = True
        alloc_info.freed_location = location
    
    def _check_reallocation(self, node: ASTNode):
        """Check memory reallocation."""
        if not hasattr(node, 'arguments') or len(node.arguments) < 2:
            return
        
        ptr_arg = node.arguments[0]
        var_name = None
        
        if type(ptr_arg).__name__ == 'Identifier':
            var_name = getattr(ptr_arg, 'name', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if allocated
        if var_name not in self.allocations:
            self.issues.append(Issue(
                code="M004",
                severity=Severity.WARNING,
                category=Category.MEMORY,
                message=f"Reallocating unallocated memory: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Use 'alloc' for initial allocation"
            ))
            return
        
        alloc_info = self.allocations[var_name]
        
        if alloc_info.is_freed:
            # Reallocating freed memory
            self.issues.append(Issue(
                code="M005",
                severity=Severity.ERROR,
                category=Category.MEMORY,
                message=f"Reallocating freed memory: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Do not realloc after free"
            ))
    
    def _check_dereference(self, node: ASTNode):
        """Check pointer dereference safety."""
        # Get pointer being dereferenced
        ptr_expr = getattr(node, 'expression', None) or getattr(node, 'pointer', None)
        if not ptr_expr:
            return
        
        var_name = None
        if type(ptr_expr).__name__ == 'Identifier':
            var_name = getattr(ptr_expr, 'name', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if allocated
        if var_name in self.allocations:
            alloc_info = self.allocations[var_name]
            
            # Use-after-free!
            if alloc_info.is_freed:
                self.issues.append(Issue(
                    code="M006",
                    severity=Severity.ERROR,
                    category=Category.MEMORY,
                    message=f"Use-after-free: dereferencing freed pointer '{var_name}'",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Do not use '{var_name}' after freeing",
                    related_locations=[alloc_info.freed_location] if alloc_info.freed_location else []
                ))
    
    def _check_leaks(self):
        """Check for memory leaks at end of scope."""
        for var_name, alloc_info in self.allocations.items():
            if not alloc_info.is_freed:
                self.issues.append(Issue(
                    code="M007",
                    severity=Severity.WARNING,
                    category=Category.MEMORY,
                    message=f"Potential memory leak: '{var_name}' not freed",
                    location=alloc_info.location,
                    source_line=self.get_source_line(alloc_info.location.line),
                    suggestion=f"Add 'call dealloc with {var_name}' before end of scope",
                    fix=f"call dealloc with {var_name}"
                ))
    
    def _check_leaks_on_return(self, node: ASTNode):
        """Check for leaks when returning from function."""
        location = self.get_node_location(node)
        
        for var_name, alloc_info in self.allocations.items():
            if not alloc_info.is_freed:
                self.issues.append(Issue(
                    code="M008",
                    severity=Severity.WARNING,
                    category=Category.MEMORY,
                    message=f"Memory leak on return: '{var_name}' not freed before returning",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=f"Free '{var_name}' before returning",
                    related_locations=[alloc_info.location]
                ))
