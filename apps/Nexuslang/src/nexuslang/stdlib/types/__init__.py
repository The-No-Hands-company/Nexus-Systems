"""
Type utilities for NexusLang standard library.
Provides Optional<T>, Result<T, E>, and other generic container types.
"""

from ...stdlib.types.optional import register_optional_functions
from ...stdlib.types.result import register_result_functions


def register_type_functions(runtime):
    """Register all type utility functions with the runtime."""
    register_optional_functions(runtime)
    register_result_functions(runtime)


__all__ = [
    'register_type_functions',
    'register_optional_functions',
    'register_result_functions'
]
