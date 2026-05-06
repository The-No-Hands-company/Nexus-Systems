"""
Panic System for Unrecoverable Errors
======================================

Handles unrecoverable errors with stack unwinding and custom handlers.
"""

import sys
import traceback
from typing import Callable, Optional, NoReturn
from dataclasses import dataclass


@dataclass
class PanicInfo:
    """Information about a panic."""
    message: str
    location: Optional[str] = None
    stack_trace: Optional[str] = None


class Panic(Exception):
    """Exception raised on panic."""
    
    def __init__(self, info: PanicInfo):
        self.info = info
        super().__init__(info.message)


class PanicHandler:
    """
    Custom panic handler.
    
    Can be set to customize panic behavior:
    - Log to file
    - Send error reports
    - Custom cleanup
    - Custom error display
    """
    
    def __init__(self, handler: Optional[Callable[[PanicInfo], None]] = None):
        self.handler = handler or self.default_handler
    
    def handle(self, info: PanicInfo) -> NoReturn:
        """Handle a panic."""
        self.handler(info)
        sys.exit(1)
    
    @staticmethod
    def default_handler(info: PanicInfo):
        """Default panic handler - prints to stderr."""
        print("=" * 60, file=sys.stderr)
        print("PANIC: Unrecoverable error occurred", file=sys.stderr)
        print("=" * 60, file=sys.stderr)
        print(f"\nMessage: {info.message}", file=sys.stderr)
        
        if info.location:
            print(f"Location: {info.location}", file=sys.stderr)
        
        if info.stack_trace:
            print("\nStack trace:", file=sys.stderr)
            print(info.stack_trace, file=sys.stderr)
        
        print("\n" + "=" * 60, file=sys.stderr)


# Global panic handler
_panic_handler = PanicHandler()


def set_panic_handler(handler: Callable[[PanicInfo], None]):
    """Set custom panic handler."""
    global _panic_handler
    _panic_handler = PanicHandler(handler)


def panic(message: str, location: Optional[str] = None) -> NoReturn:
    """
    Trigger a panic with a message.
    
    This is for unrecoverable errors. For recoverable errors, use Result<T, E>.
    
    Args:
        message: Error message
        location: Source location (file:line)
    
    Raises:
        Panic: Always raises
    """
    # Capture stack trace
    stack_trace = ''.join(traceback.format_stack()[:-1])
    
    info = PanicInfo(
        message=message,
        location=location,
        stack_trace=stack_trace
    )
    
    _panic_handler.handle(info)


def assert_nxl(condition: bool, message: str = "Assertion failed"):
    """
    Assert with panic on failure.
    
    Args:
        condition: Condition to check
        message: Message to display on failure
    """
    if not condition:
        panic(message)


def unreachable(message: str = "Entered unreachable code") -> NoReturn:
    """
    Mark code that should never be reached.
    
    Args:
        message: Error message
    """
    panic(message)


def todo(message: str = "Not yet implemented") -> NoReturn:
    """
    Mark unimplemented code.
    
    Args:
        message: TODO message
    """
    panic(f"TODO: {message}")


# Panic recovery context manager
class PanicBoundary:
    """
    Context manager for catching panics.
    
    Usage:
        with PanicBoundary() as boundary:
            # Code that might panic
            risky_operation()
        
        if boundary.panicked:
            print(f"Caught panic: {boundary.panic_info.message}")
    """
    
    def __init__(self):
        self.panicked = False
        self.panic_info: Optional[PanicInfo] = None
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is Panic:
            self.panicked = True
            self.panic_info = exc_val.info
            return True  # Suppress the exception
        return False


__all__ = [
    'Panic', 'PanicInfo', 'PanicHandler', 'PanicBoundary',
    'panic', 'assert_nxl', 'unreachable', 'todo',
    'set_panic_handler'
]
