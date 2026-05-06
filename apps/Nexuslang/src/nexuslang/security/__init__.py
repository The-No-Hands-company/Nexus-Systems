"""
NLPL Security Module

This package provides comprehensive security features for NexusLang including:

- Permission system (Deno-inspired deny-by-default)
- Path validation and sanitization
- Safe subprocess execution
- Input validation utilities
- Output sanitization
- Memory safety checks

Security hardening layer (8.4.3):
- Static analysis: taint tracking, CFI verification, memory safety validation
- Sandboxing: restricted mode, POSIX resource limits, seccomp-BPF (Linux)
- Runtime protections: stack canaries, bounds checking, integer overflow detection

The security model is deny-by-default: all dangerous operations require
explicit permission grants via command-line flags or runtime API.
"""

from nexuslang.security.permissions import (
    PermissionType,
    Permission,
    PermissionManager,
    PermissionDeniedError,
    parse_permission_flags,
    get_permission_manager,
    set_permission_manager,
)

from nexuslang.security.utils import (
    SecurityError,
    PathTraversalError,
    CommandInjectionError,
    ValidationError,
    normalize_path,
    validate_path,
    is_safe_path,
    get_safe_filename,
    safe_execute,
    validate_email,
    validate_url,
    validate_integer,
    sanitize_sql_identifier,
    escape_html,
    escape_shell_arg,
    is_safe_regex,
    check_rate_limit,
)

# --------------------------------------------------------------------------
# Static analysis (8.4.3 - Area 1)
# --------------------------------------------------------------------------
from nexuslang.security.analysis import (
    # Exceptions
    AnalysisViolation,
    TaintViolation,
    CFIViolation,
    MemorySafetyViolation,
    BoundsError,
    UseAfterFreeError,
    # Policy
    ViolationPolicy,
    AnalysisPolicy,
    get_analysis_policy,
    set_analysis_policy,
    # Taint analysis
    TaintLabel,
    TaintedValue,
    TaintSink,
    TaintTracker,
    unwrap,
    taint_label_of,
    is_tainted,
    # CFI
    CallSite,
    CallGraph,
    CFIChecker,
    # Memory safety
    MemorySafetyValidator,
    # Facade
    SecurityAnalyser,
)

# --------------------------------------------------------------------------
# Sandboxing (8.4.3 - Area 3)
# --------------------------------------------------------------------------
from nexuslang.security.sandbox import (
    SandboxPolicy,
    SandboxError,
    STRICT_POLICY,
    DEVELOPMENT_POLICY,
    RestrictedMode,
    ResourceLimits,
    SeccompFilter,
    Sandbox,
    check_aslr_status,
    warn_if_aslr_disabled,
)

# --------------------------------------------------------------------------
# Runtime protections (8.4.3 - Area 2)
# --------------------------------------------------------------------------
from nexuslang.security.runtime_protections import (
    # Exceptions
    RuntimeProtectionError,
    StackSmashingDetected,
    BoundsCheckError,
    IntegerOverflowError,
    # Stack canaries
    StackCanary,
    # Bounds checking
    BoundsChecker,
    # Integer overflow
    IntegerOverflowChecker,
    # ASLR helpers
    aslr_level,
    aslr_warning_message,
    check_and_warn_aslr,
    # Facade
    RuntimeProtectorConfig,
    RuntimeProtector,
)


__all__ = [
    # Permission system
    'PermissionType',
    'Permission',
    'PermissionManager',
    'PermissionDeniedError',
    'parse_permission_flags',
    'get_permission_manager',
    'set_permission_manager',

    # Security utilities
    'SecurityError',
    'PathTraversalError',
    'CommandInjectionError',
    'ValidationError',
    'normalize_path',
    'validate_path',
    'is_safe_path',
    'get_safe_filename',
    'safe_execute',
    'validate_email',
    'validate_url',
    'validate_integer',
    'sanitize_sql_identifier',
    'escape_html',
    'escape_shell_arg',
    'is_safe_regex',
    'check_rate_limit',

    # Analysis violations
    'AnalysisViolation',
    'TaintViolation',
    'CFIViolation',
    'MemorySafetyViolation',
    'BoundsError',
    'UseAfterFreeError',

    # Analysis policy
    'ViolationPolicy',
    'AnalysisPolicy',
    'get_analysis_policy',
    'set_analysis_policy',

    # Taint analysis
    'TaintLabel',
    'TaintedValue',
    'TaintSink',
    'TaintTracker',
    'unwrap',
    'taint_label_of',
    'is_tainted',

    # CFI
    'CallSite',
    'CallGraph',
    'CFIChecker',

    # Memory safety
    'MemorySafetyValidator',

    # Analysis facade
    'SecurityAnalyser',

    # Sandbox
    'SandboxPolicy',
    'SandboxError',
    'STRICT_POLICY',
    'DEVELOPMENT_POLICY',
    'RestrictedMode',
    'ResourceLimits',
    'SeccompFilter',
    'Sandbox',
    'check_aslr_status',
    'warn_if_aslr_disabled',

    # Runtime protection exceptions
    'RuntimeProtectionError',
    'StackSmashingDetected',
    'BoundsCheckError',
    'IntegerOverflowError',

    # Runtime protections
    'StackCanary',
    'BoundsChecker',
    'IntegerOverflowChecker',
    'aslr_level',
    'aslr_warning_message',
    'check_and_warn_aslr',
    'RuntimeProtectorConfig',
    'RuntimeProtector',
]

__version__ = '2.0.0'
