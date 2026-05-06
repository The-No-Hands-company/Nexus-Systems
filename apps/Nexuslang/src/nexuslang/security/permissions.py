"""
NLPL Permission System - Deno-inspired security model

This module implements a comprehensive permission system for NexusLang that controls
access to sensitive system resources including:
- Filesystem (read/write)
- Network operations
- Subprocess execution
- Foreign Function Interface (FFI)
- Inline assembly
- Environment variables
- Remote imports

Permission model inspired by Deno's security architecture.
"""

from enum import Enum, auto
from typing import Set, Optional, List, Dict, Type
from dataclasses import dataclass, field
import os
import sys


class PermissionType(Enum):
    """Types of permissions that can be granted or denied."""
    READ = auto()  # File system read access
    WRITE = auto()  # File system write access
    NET = auto()  # Network access
    RUN = auto()  # Subprocess execution
    FFI = auto()  # Foreign Function Interface
    ASM = auto()  # Inline assembly
    ENV = auto()  # Environment variable access
    IMPORT = auto()  # Remote imports
    ALL = auto()  # All permissions


@dataclass
class Permission:
    """Represents a specific permission with optional scope restrictions."""
    type: PermissionType
    scope: Optional[Set[str]] = field(default_factory=set)
    granted: bool = False
    
    def allows(self, resource: str) -> bool:
        """Check if this permission allows access to the given resource."""
        if not self.granted:
            return False
        
        # If no scope specified, allow all
        if not self.scope:
            return True
        
        # Check if resource matches any scope pattern
        for pattern in self.scope:
            if self._matches_pattern(resource, pattern):
                return True
        
        return False
    
    def _matches_pattern(self, resource: str, pattern: str) -> bool:
        """Check if resource matches the permission pattern."""
        # Exact match
        if resource == pattern:
            return True
        
        # Wildcard match (simple glob-style)
        if '*' in pattern:
            import re
            regex_pattern = pattern.replace('.', '\\.').replace('*', '.*')
            return re.match(f'^{regex_pattern}$', resource) is not None
        
        # Directory prefix match (for file paths)
        if pattern.endswith('/') or pattern.endswith(os.sep):
            normalized_resource = os.path.normpath(resource)
            normalized_pattern = os.path.normpath(pattern)
            return normalized_resource.startswith(normalized_pattern)
        
        return False


class PermissionDeniedError(Exception):
    """Raised when an operation is attempted without proper permissions."""
    
    def __init__(self, permission_type: PermissionType, resource: Optional[str] = None):
        self.permission_type = permission_type
        self.resource = resource
        message = f"Permission denied: {permission_type.name}"
        if resource:
            message += f" for resource '{resource}'"
        message += f"\n\nHint: Run with --allow-{permission_type.name.lower()}"
        if resource:
            message += f"={resource}"
        super().__init__(message)


class PermissionManager:
    """
    Manages permissions for NexusLang runtime.
    
    This class is responsible for:
    - Storing granted permissions
    - Checking if operations are allowed
    - Prompting for permissions in interactive mode
    - Providing safe defaults (deny-by-default)
    """
    
    def __init__(self, prompt_mode: bool = False, allow_all: bool = False):
        """
        Initialize the permission manager.
        
        Args:
            prompt_mode: If True, prompt user when permission needed
            allow_all: If True, grant all permissions (DANGEROUS - dev only)
        """
        self.permissions: Dict[PermissionType, Permission] = {}
        self.prompt_mode = prompt_mode
        self.allow_all = allow_all
        self._prompted_resources: Set[str] = set()
        
        # Initialize all permissions as denied by default
        for perm_type in PermissionType:
            if perm_type != PermissionType.ALL:
                self.permissions[perm_type] = Permission(type=perm_type, granted=False)
        
        # If allow_all is set, grant everything
        if allow_all:
            self.grant_all()
    
    def grant(self, permission_type: PermissionType, scope: Optional[List[str]] = None):
        """
        Grant a permission with optional scope restriction.
        
        Args:
            permission_type: Type of permission to grant
            scope: Optional list of resources this permission applies to
        """
        if permission_type == PermissionType.ALL:
            self.grant_all()
            return
        
        if permission_type not in self.permissions:
            self.permissions[permission_type] = Permission(type=permission_type)
        
        perm = self.permissions[permission_type]
        perm.granted = True
        
        if scope:
            if not perm.scope:
                perm.scope = set()
            perm.scope.update(scope)
    
    def grant_all(self):
        """Grant all permissions (DANGEROUS - development only)."""
        for perm_type in PermissionType:
            if perm_type != PermissionType.ALL:
                if perm_type not in self.permissions:
                    self.permissions[perm_type] = Permission(type=perm_type)
                self.permissions[perm_type].granted = True
                self.permissions[perm_type].scope = None
    
    def revoke(self, permission_type: PermissionType):
        """Revoke a permission."""
        if permission_type in self.permissions:
            self.permissions[permission_type].granted = False
    
    def check(self, permission_type: PermissionType, resource: Optional[str] = None) -> bool:
        """
        Check if a permission is granted for the given resource.
        
        Args:
            permission_type: Type of permission to check
            resource: Optional resource identifier (path, URL, etc.)
        
        Returns:
            True if permission is granted, False otherwise
        
        Raises:
            PermissionDeniedError: If permission is denied and prompt_mode is False
        """
        # Check if permission exists and is granted
        if permission_type not in self.permissions:
            if self.prompt_mode and resource:
                return self._prompt_for_permission(permission_type, resource)
            raise PermissionDeniedError(permission_type, resource)
        
        perm = self.permissions[permission_type]
        
        # Check if permission allows this resource
        if resource:
            allowed = perm.allows(resource)
        else:
            allowed = perm.granted
        
        if not allowed:
            if self.prompt_mode and resource:
                return self._prompt_for_permission(permission_type, resource)
            raise PermissionDeniedError(permission_type, resource)
        
        return True
    
    def has_permission(self, permission_type: PermissionType, resource: Optional[str] = None) -> bool:
        """
        Non-throwing version of check(). Returns bool instead of raising exception.
        
        Args:
            permission_type: Type of permission to check
            resource: Optional resource identifier
        
        Returns:
            True if permission is granted, False otherwise
        """
        try:
            return self.check(permission_type, resource)
        except PermissionDeniedError:
            return False
    
    def _prompt_for_permission(self, permission_type: PermissionType, resource: str) -> bool:
        """
        Prompt the user to grant permission interactively.
        
        Args:
            permission_type: Type of permission being requested
            resource: Resource being accessed
        
        Returns:
            True if user granted permission, False otherwise
        """
        # Don't prompt twice for the same resource
        prompt_key = f"{permission_type.name}:{resource}"
        if prompt_key in self._prompted_resources:
            return False
        
        self._prompted_resources.add(prompt_key)
        
        # Display permission request
        print(f"\n⚠️  NexusLang requests {permission_type.name} permission", file=sys.stderr)
        print(f"   Resource: {resource}", file=sys.stderr)
        print(f"   Allow? [y/N/A(always)]: ", end='', file=sys.stderr)
        
        try:
            response = input().strip().lower()
        except (EOFError, KeyboardInterrupt):
            print("\nPermission denied.", file=sys.stderr)
            return False
        
        if response == 'y':
            # Grant for this execution only (temporary)
            return True
        elif response == 'a':
            # Grant permanently for this session
            self.grant(permission_type, [resource])
            return True
        else:
            return False
    
    def get_status(self) -> Dict[str, any]:
        """Get a dictionary describing current permission status."""
        status = {}
        for perm_type, perm in self.permissions.items():
            status[perm_type.name] = {
                'granted': perm.granted,
                'scope': list(perm.scope) if perm.scope else None
            }
        return status
    
    def __repr__(self) -> str:
        """String representation of permission manager state."""
        granted = [p.name for p, perm in self.permissions.items() if perm.granted]
        return f"PermissionManager(granted={granted}, prompt_mode={self.prompt_mode})"


def parse_permission_flags(args: List[str]) -> PermissionManager:
    """
    Parse command-line permission flags and create PermissionManager.
    
    Supported flags:
        --allow-read[=<paths>]
        --allow-write[=<paths>]
        --allow-net[=<hosts>]
        --allow-run[=<programs>]
        --allow-ffi[=<libraries>]
        --allow-asm
        --allow-env[=<vars>]
        --allow-import[=<urls>]
        --allow-all
        --prompt
    
    Args:
        args: Command-line arguments
    
    Returns:
        Configured PermissionManager instance
    """
    manager = PermissionManager()
    prompt_mode = False
    
    for arg in args:
        if arg == '--allow-all':
            manager.grant_all()
        elif arg == '--prompt':
            prompt_mode = True
        elif arg.startswith('--allow-'):
            # Parse permission flag
            parts = arg[8:].split('=', 1)  # Remove '--allow-' prefix
            perm_name = parts[0].upper()
            scope = parts[1].split(',') if len(parts) > 1 else None
            
            # Map to permission type
            perm_type_map = {
                'READ': PermissionType.READ,
                'WRITE': PermissionType.WRITE,
                'NET': PermissionType.NET,
                'RUN': PermissionType.RUN,
                'FFI': PermissionType.FFI,
                'ASM': PermissionType.ASM,
                'ENV': PermissionType.ENV,
                'IMPORT': PermissionType.IMPORT,
            }
            
            if perm_name in perm_type_map:
                manager.grant(perm_type_map[perm_name], scope)
    
    if prompt_mode:
        manager.prompt_mode = True
    
    return manager


# Global permission manager instance (will be set by main)
_global_permission_manager: Optional[PermissionManager] = None


def get_permission_manager() -> PermissionManager:
    """Get the global permission manager instance."""
    global _global_permission_manager
    if _global_permission_manager is None:
        # Default: deny all, no prompts
        _global_permission_manager = PermissionManager()
    return _global_permission_manager


def set_permission_manager(manager: PermissionManager):
    """Set the global permission manager instance."""
    global _global_permission_manager
    _global_permission_manager = manager
