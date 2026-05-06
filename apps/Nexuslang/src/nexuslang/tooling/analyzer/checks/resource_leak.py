"""
Resource Leak Checker
=====================

Detects resource leaks:
- Files not closed
- Memory not freed
- Locks not released
- Network connections not closed
- Database connections not closed
"""

from typing import Dict, Set, List, Optional
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../..'))

from nexuslang.parser.ast import ASTNode
from .base import BaseChecker
from ..report import Issue, Severity, Category, SourceLocation


class ResourceInfo:
    """Track resource acquisition/release."""
    def __init__(self, var_name: str, resource_type: str, location: SourceLocation):
        self.var_name = var_name
        self.resource_type = resource_type  # 'file', 'memory', 'lock', 'connection'
        self.location = location
        self.is_released = False
        self.released_location: Optional[SourceLocation] = None


class ResourceLeakChecker(BaseChecker):

    CHECKER_NAME = "resource_leak"
    """
    Checks for resource leaks.
    
    Tracked resources:
    - Files (open → close)
    - Memory (alloc → dealloc)
    - Locks (lock → unlock)
    - Connections (connect → disconnect)
    """
    
    # Resource acquisition functions
    ACQUIRE_FUNCS = {
        'open': 'file',
        'open_file': 'file',
        'file_open': 'file',
        'alloc': 'memory',
        'allocate': 'memory',
        'malloc': 'memory',
        'lock': 'lock',
        'acquire_lock': 'lock',
        'connect': 'connection',
        'open_connection': 'connection',
    }
    
    # Resource release functions
    RELEASE_FUNCS = {
        'close': 'file',
        'close_file': 'file',
        'file_close': 'file',
        'dealloc': 'memory',
        'free': 'memory',
        'unlock': 'lock',
        'release_lock': 'lock',
        'disconnect': 'connection',
        'close_connection': 'connection',
    }
    
    def __init__(self):
        super().__init__()
        self.resources: Dict[str, ResourceInfo] = {}
        self.function_depth = 0
    
    def check(self, ast: ASTNode, source: str, lines: List[str]) -> List[Issue]:
        """Run resource leak analysis."""
        self.issues = []
        self.current_source = source
        self.current_lines = lines
        self.resources = {}
        self.function_depth = 0
        
        # Walk AST
        self.walk_ast(ast, self._check_node)
        
        # Check for leaks at end
        self._check_leaks()
        
        return self.issues
    
    def _check_node(self, node: ASTNode):
        """Check individual AST node."""
        node_type = type(node).__name__
        
        # Function definition
        if node_type == 'FunctionDefinition':
            self.function_depth += 1
            return
        
        # Variable assignment with resource acquisition
        if node_type == 'VariableDeclaration':
            self._check_acquisition(node)
        
        # Function call (might be release)
        elif node_type == 'FunctionCall':
            self._check_release(node)
        
        # Return statement
        elif node_type == 'ReturnStatement':
            self._check_leaks_on_return(node)
    
    def _check_acquisition(self, node: ASTNode):
        """Check resource acquisition."""
        if not hasattr(node, 'value'):
            return
        
        value = node.value
        if not value or type(value).__name__ != 'FunctionCall':
            return
        
        func_name = getattr(value, 'name', None)
        if not func_name or func_name not in self.ACQUIRE_FUNCS:
            return
        
        # This acquires a resource
        resource_type = self.ACQUIRE_FUNCS[func_name]
        var_name = getattr(node, 'name', None) or getattr(node, 'identifier', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if already holding resource
        if var_name in self.resources:
            old_resource = self.resources[var_name]
            if not old_resource.is_released:
                # Resource leak
                self.issues.append(Issue(
                    code="R001",
                    severity=Severity.WARNING,
                    category=Category.RESOURCE_LEAK,
                    message=f"Resource leak: '{var_name}' ({old_resource.resource_type}) not released before reacquisition",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=self._get_release_suggestion(old_resource),
                    related_locations=[old_resource.location]
                ))
        
        # Track resource
        self.resources[var_name] = ResourceInfo(var_name, resource_type, location)
    
    def _check_release(self, node: ASTNode):
        """Check resource release."""
        func_name = getattr(node, 'name', None)
        if not func_name or func_name not in self.RELEASE_FUNCS:
            return
        
        # This releases a resource
        resource_type = self.RELEASE_FUNCS[func_name]
        
        # Get resource variable
        if not hasattr(node, 'arguments') or not node.arguments:
            return
        
        arg = node.arguments[0]
        var_name = None
        
        if type(arg).__name__ in ['Identifier', 'VariableReference']:
            var_name = getattr(arg, 'name', None)
        
        if not var_name:
            return
        
        location = self.get_node_location(node)
        
        # Check if resource exists
        if var_name not in self.resources:
            # Releasing non-existent resource
            self.issues.append(Issue(
                code="R002",
                severity=Severity.WARNING,
                category=Category.RESOURCE_LEAK,
                message=f"Attempting to release non-existent {resource_type}: '{var_name}'",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Ensure '{var_name}' is acquired before releasing"
            ))
            return
        
        resource = self.resources[var_name]
        
        # Check type match
        if resource.resource_type != resource_type:
            self.issues.append(Issue(
                code="R003",
                severity=Severity.ERROR,
                category=Category.RESOURCE_LEAK,
                message=f"Type mismatch: trying to release {resource_type} but '{var_name}' is {resource.resource_type}",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Use correct release function for {resource.resource_type}"
            ))
            return
        
        # Check if already released
        if resource.is_released:
            # Double-release
            self.issues.append(Issue(
                code="R004",
                severity=Severity.ERROR,
                category=Category.RESOURCE_LEAK,
                message=f"Double-release: {resource_type} '{var_name}' already released",
                location=location,
                source_line=self.get_source_line(location.line),
                suggestion=f"Remove duplicate release",
                related_locations=[resource.released_location] if resource.released_location else []
            ))
            return
        
        # Mark as released
        resource.is_released = True
        resource.released_location = location
    
    def _check_leaks(self):
        """Check for unreleased resources."""
        for var_name, resource in self.resources.items():
            if not resource.is_released:
                self.issues.append(Issue(
                    code="R005",
                    severity=Severity.WARNING,
                    category=Category.RESOURCE_LEAK,
                    message=f"Resource leak: {resource.resource_type} '{var_name}' not released",
                    location=resource.location,
                    source_line=self.get_source_line(resource.location.line),
                    suggestion=self._get_release_suggestion(resource),
                    fix=self._get_release_fix(resource)
                ))
    
    def _check_leaks_on_return(self, node: ASTNode):
        """Check for leaks when returning."""
        location = self.get_node_location(node)
        
        for var_name, resource in self.resources.items():
            if not resource.is_released:
                self.issues.append(Issue(
                    code="R006",
                    severity=Severity.WARNING,
                    category=Category.RESOURCE_LEAK,
                    message=f"Resource leak on return: {resource.resource_type} '{var_name}' not released",
                    location=location,
                    source_line=self.get_source_line(location.line),
                    suggestion=self._get_release_suggestion(resource),
                    related_locations=[resource.location]
                ))
    
    def _get_release_suggestion(self, resource: ResourceInfo) -> str:
        """Get suggestion for releasing resource."""
        suggestions = {
            'file': f"Call 'close {resource.var_name}' before end of scope",
            'memory': f"Call 'dealloc {resource.var_name}' before end of scope",
            'lock': f"Call 'unlock {resource.var_name}' before end of scope",
            'connection': f"Call 'disconnect {resource.var_name}' before end of scope",
        }
        return suggestions.get(resource.resource_type, f"Release {resource.var_name}")
    
    def _get_release_fix(self, resource: ResourceInfo) -> str:
        """Get auto-fix for releasing resource."""
        fixes = {
            'file': f"call close with {resource.var_name}",
            'memory': f"call dealloc with {resource.var_name}",
            'lock': f"call unlock with {resource.var_name}",
            'connection': f"call disconnect with {resource.var_name}",
        }
        return fixes.get(resource.resource_type, "")
