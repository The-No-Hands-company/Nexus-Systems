"""Unified error types for the NexusLang standard library.

Every stdlib module should raise errors from this hierarchy rather than raw
Python exceptions (ValueError, RuntimeError, etc.).  This gives NexusLang programs
a consistent, catchable error interface and enables the interpreter to provide
rich diagnostics.

Usage in stdlib modules::

    from nexuslang.stdlib.errors import (
        NLPLStdlibError,
        NLPLValueError,
        NLPLIOError,
        NLPLKeyError,
        NLPLIndexError,
        NLPLConnectionError,
        NLPLTimeoutError,
        NLPLPermissionError,
        NLPLNotImplementedError,
        NLPLOverflowError,
        NLPLFormatError,
        NLPLImportError,
    )

    # In a stdlib function:
    def stdlib_sqrt(x):
        if x < 0:
            raise NLPLValueError("sqrt requires a non-negative argument", value=x)
        ...

Error hierarchy
---------------
NLPLStdlibError (base for all stdlib errors)
    NLPLValueError          -- invalid argument value
    NLPLIOError             -- file/stream I/O failure
    NLPLKeyError            -- missing dictionary/map key
    NLPLIndexError          -- out-of-bounds index
    NLPLConnectionError     -- network connection failure
    NLPLTimeoutError        -- operation timed out
    NLPLPermissionError     -- insufficient permissions
    NLPLNotImplementedError -- feature not yet available
    NLPLOverflowError       -- numeric overflow
    NLPLFormatError         -- string/data formatting error
    NLPLImportError         -- module import failure
"""

from __future__ import annotations

from typing import Any, Optional


class NLPLStdlibError(Exception):
    """Base class for all NexusLang standard library errors.

    Parameters
    ----------
    message : str
        Human-readable description of the error.
    module : str, optional
        Name of the stdlib module that raised the error (e.g. "math", "io").
    function : str, optional
        Name of the stdlib function that raised the error.
    """

    def __init__(
        self,
        message: str,
        *,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.nxl_message = message
        self.module = module
        self.function = function
        prefix_parts: list[str] = []
        if module:
            prefix_parts.append(module)
        if function:
            prefix_parts.append(function)
        prefix = ".".join(prefix_parts)
        full = f"[{prefix}] {message}" if prefix else message
        super().__init__(full)


class NLPLValueError(NLPLStdlibError):
    """Raised when a stdlib function receives an argument with the right type
    but an inappropriate value (e.g. negative number for sqrt)."""

    def __init__(
        self,
        message: str,
        *,
        value: Any = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.value = value
        super().__init__(message, module=module, function=function)


class NLPLIOError(NLPLStdlibError):
    """Raised on file or stream I/O failures."""

    def __init__(
        self,
        message: str,
        *,
        path: Optional[str] = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.path = path
        super().__init__(message, module=module, function=function)


class NLPLKeyError(NLPLStdlibError):
    """Raised when a dictionary/map key is not found."""

    def __init__(
        self,
        message: str,
        *,
        key: Any = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.key = key
        super().__init__(message, module=module, function=function)


class NLPLIndexError(NLPLStdlibError):
    """Raised on out-of-bounds index access."""

    def __init__(
        self,
        message: str,
        *,
        index: Any = None,
        length: Optional[int] = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.index = index
        self.length = length
        super().__init__(message, module=module, function=function)


class NLPLConnectionError(NLPLStdlibError):
    """Raised when a network connection fails."""

    def __init__(
        self,
        message: str,
        *,
        host: Optional[str] = None,
        port: Optional[int] = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.host = host
        self.port = port
        super().__init__(message, module=module, function=function)


class NLPLTimeoutError(NLPLStdlibError):
    """Raised when an operation exceeds its time limit."""

    def __init__(
        self,
        message: str,
        *,
        timeout_seconds: Optional[float] = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.timeout_seconds = timeout_seconds
        super().__init__(message, module=module, function=function)


class NLPLPermissionError(NLPLStdlibError):
    """Raised when an operation lacks required permissions."""

    def __init__(
        self,
        message: str,
        *,
        path: Optional[str] = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.path = path
        super().__init__(message, module=module, function=function)


class NLPLNotImplementedError(NLPLStdlibError):
    """Raised when a stdlib feature is planned but not yet available."""

    pass


class NLPLOverflowError(NLPLStdlibError):
    """Raised on numeric overflow in arithmetic operations."""

    def __init__(
        self,
        message: str,
        *,
        value: Any = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.value = value
        super().__init__(message, module=module, function=function)


class NLPLFormatError(NLPLStdlibError):
    """Raised when string or data formatting fails."""

    pass


class NLPLImportError(NLPLStdlibError):
    """Raised when a module import fails."""

    def __init__(
        self,
        message: str,
        *,
        module_name: Optional[str] = None,
        module: Optional[str] = None,
        function: Optional[str] = None,
    ) -> None:
        self.module_name = module_name
        super().__init__(message, module=module, function=function)
