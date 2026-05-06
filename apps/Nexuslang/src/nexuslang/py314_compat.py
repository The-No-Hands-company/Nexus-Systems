"""
Python 3.14 Compatibility Layer

Workaround for Python 3.14 import system regression that causes hangs
when importing modules with large enums.

This module provides alternative import mechanisms that bypass the broken
import system in Python 3.14.
"""

import sys
import importlib.util
import os
from pathlib import Path


def load_module_direct(module_path: str, module_name: str):
    """
    Load a module directly using importlib.util.spec_from_file_location.
    
    This bypasses Python 3.14's broken import system for large enum modules.
    
    Args:
        module_path: Path to the .py file
        module_name: Name to give the module
    
    Returns:
        The loaded module
    """
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load spec for {module_name} from {module_path}")
    
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module  # Register in sys.modules
    spec.loader.exec_module(module)
    return module


def initialize_nxl_modules():
    """
    Initialize NexusLang modules using direct loading to avoid Python 3.14 import bugs.
    
    This function manually loads the core NexusLang modules in the correct order,
    bypassing the standard import system.
    """
    # Get the src/nlpl directory
    nxl_dir = Path(__file__).parent
    
    # Load modules in dependency order
    modules_to_load = [
        ('nexuslang.parser.lexer', nxl_dir / 'parser' / 'lexer.py'),
        ('nexuslang.parser.ast', nxl_dir / 'parser' / 'ast.py'),
        ('nexuslang.parser.parser', nxl_dir / 'parser' / 'parser.py'),
    ]
    
    loaded = {}
    for module_name, module_path in modules_to_load:
        if not module_path.exists():
            raise FileNotFoundError(f"Module file not found: {module_path}")
        
        # Load the module
        module = load_module_direct(str(module_path), module_name)
        loaded[module_name] = module
    
    return loaded


# Attempt to detect Python 3.14 and warn
if sys.version_info >= (3, 14):
    import warnings
    warnings.warn(
        "Python 3.14 has known import system bugs that may cause hangs. "
        "Using compatibility layer to work around the issue. "
        "Consider using Python 3.13 or earlier for best results.",
        RuntimeWarning,
        stacklevel=2
    )
