"""
Signal handling for NexusLang.
Handle OS signals (SIGINT, SIGTERM, etc.)
"""

import signal
import sys
from typing import Callable, Optional
from ...runtime.runtime import Runtime


# Signal handlers storage
_signal_handlers = {}


def handle_signal(sig: int, handler: Callable) -> bool:
    """
    Register a signal handler.
    Common signals: SIGINT (2), SIGTERM (15), SIGHUP (1), SIGQUIT (3)
    """
    try:
        signal.signal(sig, lambda signum, frame: handler())
        _signal_handlers[sig] = handler
        return True
    except (ValueError, OSError) as e:
        print(f"Failed to register signal handler: {e}")
        return False


def handle_sigint(handler: Callable) -> bool:
    """Handle Ctrl+C (SIGINT)."""
    return handle_signal(signal.SIGINT, handler)


def handle_sigterm(handler: Callable) -> bool:
    """Handle termination signal (SIGTERM)."""
    return handle_signal(signal.SIGTERM, handler)


def ignore_signal(sig: int) -> bool:
    """Ignore a signal."""
    try:
        signal.signal(sig, signal.SIG_IGN)
        return True
    except (ValueError, OSError) as e:
        print(f"Failed to ignore signal: {e}")
        return False


def default_signal(sig: int) -> bool:
    """Restore default signal handler."""
    try:
        signal.signal(sig, signal.SIG_DFL)
        if sig in _signal_handlers:
            del _signal_handlers[sig]
        return True
    except (ValueError, OSError) as e:
        print(f"Failed to restore default handler: {e}")
        return False


def send_signal(pid: int, sig: int) -> bool:
    """Send signal to process."""
    import os
    try:
        os.kill(pid, sig)
        return True
    except (ProcessLookupError, PermissionError) as e:
        print(f"Failed to send signal: {e}")
        return False


def get_signal_name(sig: int) -> str:
    """Get signal name from number."""
    try:
        return signal.Signals(sig).name
    except ValueError:
        return f"UNKNOWN_{sig}"


def register_signal_functions(runtime: Runtime) -> None:
    """Register signal functions with the runtime."""
    
    # Signal handling
    runtime.register_function("handle_signal", handle_signal)
    runtime.register_function("handle_sigint", handle_sigint)
    runtime.register_function("handle_sigterm", handle_sigterm)
    runtime.register_function("ignore_signal", ignore_signal)
    runtime.register_function("default_signal", default_signal)
    runtime.register_function("send_signal", send_signal)
    runtime.register_function("get_signal_name", get_signal_name)
    
    # Signal constants
    runtime.register_function("SIGINT", lambda: signal.SIGINT)
    runtime.register_function("SIGTERM", lambda: signal.SIGTERM)
    runtime.register_function("SIGHUP", lambda: signal.SIGHUP)
    runtime.register_function("SIGQUIT", lambda: signal.SIGQUIT)
