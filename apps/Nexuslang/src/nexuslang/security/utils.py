"""
NLPL Security Utilities

This module provides security utilities for safe handling of:
- File paths (path traversal prevention)
- Subprocess execution (command injection prevention)
- Input validation
- Output sanitization
"""

import os
import re
import subprocess
from typing import List, Optional, Union
from pathlib import Path


class SecurityError(Exception):
    """Base class for security-related errors."""
    pass


class PathTraversalError(SecurityError):
    """Raised when path traversal attempt is detected."""
    pass


class CommandInjectionError(SecurityError):
    """Raised when command injection attempt is detected."""
    pass


class ValidationError(SecurityError):
    """Raised when input validation fails."""
    pass


# =============================================================================
# Path Validation
# =============================================================================

def normalize_path(path: str) -> str:
    """
    Normalize a file path, resolving .. and . components.
    
    Args:
        path: Path to normalize
    
    Returns:
        Normalized absolute path
    """
    return os.path.normpath(os.path.abspath(path))


def validate_path(path: str, allowed_dirs: Optional[List[str]] = None, 
                  allow_absolute: bool = False) -> str:
    """
    Validate a file path to prevent path traversal attacks.
    
    Args:
        path: Path to validate
        allowed_dirs: Optional list of allowed directory prefixes
        allow_absolute: Whether to allow absolute paths
    
    Returns:
        Validated normalized path
    
    Raises:
        PathTraversalError: If path is unsafe
    """
    # Normalize the path
    normalized = normalize_path(path)
    
    # Check for null bytes (directory traversal trick)
    if '\x00' in path:
        raise PathTraversalError("Path contains null byte")
    
    # Check if absolute path is disallowed
    if not allow_absolute and os.path.isabs(path):
        raise PathTraversalError(f"Absolute paths not allowed: {path}")
    
    # Check for path traversal patterns
    dangerous_patterns = [
        '../',
        '..\\',
        '..',
    ]
    for pattern in dangerous_patterns:
        if pattern in path:
            raise PathTraversalError(f"Path contains dangerous pattern '{pattern}': {path}")
    
    # If allowed_dirs specified, ensure path is within one of them
    if allowed_dirs:
        normalized_dirs = [normalize_path(d) for d in allowed_dirs]
        is_allowed = False
        for allowed_dir in normalized_dirs:
            if normalized.startswith(allowed_dir):
                is_allowed = True
                break
        
        if not is_allowed:
            raise PathTraversalError(
                f"Path '{path}' is outside allowed directories: {allowed_dirs}"
            )
    
    return normalized


def is_safe_path(path: str, allowed_dirs: Optional[List[str]] = None) -> bool:
    """
    Non-throwing version of validate_path.
    
    Args:
        path: Path to check
        allowed_dirs: Optional list of allowed directories
    
    Returns:
        True if path is safe, False otherwise
    """
    try:
        validate_path(path, allowed_dirs)
        return True
    except PathTraversalError:
        return False


def get_safe_filename(filename: str) -> str:
    """
    Sanitize a filename by removing dangerous characters.
    
    Args:
        filename: Original filename
    
    Returns:
        Sanitized filename safe for filesystem use
    """
    # Remove path separators
    filename = os.path.basename(filename)
    
    # Remove or replace dangerous characters
    # Allow: letters, digits, dots, dashes, underscores
    safe_filename = re.sub(r'[^a-zA-Z0-9._-]', '_', filename)
    
    # Prevent hidden files (starting with .)
    if safe_filename.startswith('.'):
        safe_filename = '_' + safe_filename[1:]
    
    # Prevent empty filename
    if not safe_filename:
        safe_filename = 'unnamed_file'
    
    return safe_filename


# =============================================================================
# Subprocess Execution (Safe)
# =============================================================================

def safe_execute(program: str, args: List[str], 
                 allowed_programs: Optional[List[str]] = None,
                 capture_output: bool = True,
                 timeout: Optional[int] = None) -> subprocess.CompletedProcess:
    """
    Execute a subprocess safely without shell expansion.
    
    This function:
    - Never uses shell=True (prevents command injection)
    - Validates program path
    - Passes arguments as list (no string concatenation)
    - Supports whitelist of allowed programs
    
    Args:
        program: Path or name of program to execute
        args: List of arguments (each as separate string)
        allowed_programs: Optional whitelist of allowed program names/paths
        capture_output: Whether to capture stdout/stderr
        timeout: Optional timeout in seconds
    
    Returns:
        CompletedProcess instance with results
    
    Raises:
        CommandInjectionError: If program is not allowed or looks suspicious
        FileNotFoundError: If program doesn't exist
        subprocess.TimeoutExpired: If execution times out
    """
    # Validate program name doesn't contain shell metacharacters
    shell_metacharacters = ['&', '|', ';', '$', '`', '(', ')', '<', '>', '\\n', '\\r']
    for char in shell_metacharacters:
        if char in program:
            raise CommandInjectionError(
                f"Program name contains shell metacharacter '{char}': {program}"
            )
    
    # Check against whitelist if provided
    if allowed_programs:
        program_name = os.path.basename(program)
        if program_name not in allowed_programs and program not in allowed_programs:
            raise CommandInjectionError(
                f"Program '{program}' not in allowed list: {allowed_programs}"
            )
    
    # Build command list (never use shell)
    cmd = [program] + args
    
    # Execute safely
    try:
        result = subprocess.run(
            cmd,
            shell=False,  # CRITICAL: never use shell
            capture_output=capture_output,
            text=True,
            timeout=timeout,
            check=False  # Don't raise on non-zero exit
        )
        return result
    except FileNotFoundError:
        raise FileNotFoundError(f"Program not found: {program}")


# =============================================================================
# Input Validation
# =============================================================================

def validate_email(email: str) -> bool:
    """
    Validate email address format.
    
    Args:
        email: Email address to validate
    
    Returns:
        True if valid, False otherwise
    """
    pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
    return re.match(pattern, email) is not None


def validate_url(url: str, allowed_schemes: Optional[List[str]] = None) -> bool:
    """
    Validate URL format and optionally check scheme.
    
    Args:
        url: URL to validate
        allowed_schemes: Optional list of allowed schemes (e.g., ['http', 'https'])
    
    Returns:
        True if valid, False otherwise
    """
    # Basic URL pattern
    pattern = r'^[a-zA-Z][a-zA-Z0-9+.-]*://[^\s]+'
    if not re.match(pattern, url):
        return False
    
    # Check scheme if whitelist provided
    if allowed_schemes:
        scheme = url.split('://')[0].lower()
        if scheme not in allowed_schemes:
            return False
    
    return True


def validate_integer(value: str, min_val: Optional[int] = None, 
                     max_val: Optional[int] = None) -> bool:
    """
    Validate that string is a valid integer within optional bounds.
    
    Args:
        value: String to validate
        min_val: Optional minimum value
        max_val: Optional maximum value
    
    Returns:
        True if valid integer within bounds, False otherwise
    """
    try:
        num = int(value)
        if min_val is not None and num < min_val:
            return False
        if max_val is not None and num > max_val:
            return False
        return True
    except ValueError:
        return False


def sanitize_sql_identifier(identifier: str) -> str:
    """
    Sanitize a SQL identifier (table/column name).
    
    Args:
        identifier: SQL identifier to sanitize
    
    Returns:
        Sanitized identifier
    
    Raises:
        ValidationError: If identifier contains dangerous characters
    """
    # Only allow alphanumeric and underscores
    if not re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', identifier):
        raise ValidationError(
            f"SQL identifier contains invalid characters: {identifier}"
        )
    
    # Check for SQL keywords (basic list)
    sql_keywords = {
        'select', 'insert', 'update', 'delete', 'drop', 'create', 
        'alter', 'table', 'from', 'where', 'and', 'or', 'union'
    }
    if identifier.lower() in sql_keywords:
        raise ValidationError(
            f"SQL identifier cannot be a reserved keyword: {identifier}"
        )
    
    return identifier


# =============================================================================
# Output Sanitization
# =============================================================================

def escape_html(text: str) -> str:
    """
    Escape HTML special characters to prevent XSS.
    
    Args:
        text: Text to escape
    
    Returns:
        HTML-escaped text
    """
    escape_table = {
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#x27;',
        '/': '&#x2F;',
    }
    return ''.join(escape_table.get(c, c) for c in text)


def escape_shell_arg(arg: str) -> str:
    """
    Escape a string for safe use as shell argument.
    
    WARNING: This is a fallback. Prefer using safe_execute() instead.
    
    Args:
        arg: Argument to escape
    
    Returns:
        Shell-escaped argument
    """
    import shlex
    return shlex.quote(arg)


# =============================================================================
# Pattern Matching & Validation
# =============================================================================

def is_safe_regex(pattern: str, max_length: int = 1000) -> bool:
    """
    Check if a regex pattern is safe (prevents ReDoS attacks).
    
    Args:
        pattern: Regex pattern to check
        max_length: Maximum allowed pattern length
    
    Returns:
        True if pattern appears safe, False otherwise
    """
    # Check length
    if len(pattern) > max_length:
        return False
    
    # Check for dangerous patterns that can cause ReDoS
    dangerous_patterns = [
        r'\(\?.*\)\+',  # Nested quantifiers
        r'\(\.\*\)\+',  # Greedy quantifiers with wildcards
        r'\(.*\)\{.*,\}',  # Complex repetition
    ]
    
    for danger in dangerous_patterns:
        if re.search(danger, pattern):
            return False
    
    return True


# =============================================================================
# Utility Functions
# =============================================================================

def check_rate_limit(identifier: str, max_calls: int, window_seconds: int) -> bool:
    """
    Simple rate limiting check.
    
    Args:
        identifier: Identifier for rate limit (e.g., IP address, user ID)
        max_calls: Maximum calls allowed in window
        window_seconds: Time window in seconds
    
    Returns:
        True if within rate limit, False if exceeded
    
    Note: This is a simple in-memory implementation. Production systems
    should use Redis or similar for distributed rate limiting.
    """
    import time
    from collections import defaultdict, deque
    
    # In-memory storage (not thread-safe, for demo only)
    if not hasattr(check_rate_limit, 'call_history'):
        check_rate_limit.call_history = defaultdict(deque)
    
    now = time.time()
    calls = check_rate_limit.call_history[identifier]
    
    # Remove old calls outside window
    while calls and calls[0] < now - window_seconds:
        calls.popleft()
    
    # Check if limit exceeded
    if len(calls) >= max_calls:
        return False
    
    # Record this call
    calls.append(now)
    return True
