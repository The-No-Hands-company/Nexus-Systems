"""Async utilities for NexusLang."""

from ...stdlib.asyncio_utils.promise import register_promise_functions
from ...stdlib.asyncio_utils.async_runtime import register_async_runtime_functions


def register_async_functions(runtime):
    """Register all async utility functions."""
    register_promise_functions(runtime)
    register_async_runtime_functions(runtime)


__all__ = [
    "register_async_functions",
]
