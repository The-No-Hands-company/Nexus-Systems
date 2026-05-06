"""
Module loader for the NexusLang language.
This module provides functionality for loading and caching modules.
"""

import os
import sys
import re
from typing import Dict, List, Optional, Set, Any, Tuple

from nexuslang.parser.lexer import Lexer
from nexuslang.parser.parser import Parser
from nexuslang.runtime.runtime import Runtime

class CircularImportError(ImportError):
    """Error raised when a circular import is detected."""
    
    def __init__(self, module_name: str, import_chain: List[str]):
        """Initialize the error with the module name and import chain."""
        self.module_name = module_name
        self.import_chain = import_chain
        super().__init__(f"Circular import detected for module {module_name}: {' -> '.join(import_chain)}")

class ModuleCache:
    """Cache for loaded modules to avoid reloading the same module multiple times."""
    
    def __init__(self):
        """Initialize the module cache."""
        self.modules: Dict[str, Any] = {}
        self.loading_modules: Set[str] = set()  # For detecting circular imports
    
    def has_module(self, module_name: str) -> bool:
        """Check if a module is in the cache."""
        return module_name in self.modules
    
    def get_module(self, module_name: str) -> Any:
        """Get a module from the cache."""
        return self.modules.get(module_name)
    
    def add_module(self, module_name: str, module: Any) -> None:
        """Add a module to the cache."""
        self.modules[module_name] = module
    
    def is_loading(self, module_name: str) -> bool:
        """Check if a module is currently being loaded (for circular import detection)."""
        return module_name in self.loading_modules
    
    def mark_loading(self, module_name: str) -> None:
        """Mark a module as being loaded."""
        self.loading_modules.add(module_name)
    
    def mark_loaded(self, module_name: str) -> None:
        """Mark a module as loaded (no longer loading)."""
        if module_name in self.loading_modules:
            self.loading_modules.remove(module_name)

class ModuleLoader:
    """Loader for NexusLang modules."""
    
    def __init__(self, runtime: Runtime, search_paths: List[str] = None):
        """
        Initialize the module loader.
        
        Args:
            runtime: The runtime environment
            search_paths: List of paths to search for modules (default: current directory)
        """
        self.runtime = runtime
        self.search_paths = search_paths or [os.getcwd()]
        self.cache = ModuleCache()
        self.current_module_path = None  # For relative imports
        self.module_path_map = {}  # Maps module names to their file paths
    
    def load_module(self, module_name: str, current_path: str = None) -> Any:
        """
        Load a module by name.
        
        Args:
            module_name: The name of the module to load
            current_path: The path of the current module (for relative imports)
            
        Returns:
            The loaded module
            
        Raises:
            ImportError: If the module cannot be found or loaded
            CircularImportError: If a circular import is detected
        """
        # Normalize the module name
        normalized_name, is_relative = self._normalize_module_name(module_name)
        
        # If it's a relative import, resolve it based on the current module path
        if is_relative and current_path:
            resolved_path = self._resolve_relative_path(normalized_name, current_path)
            cache_key = resolved_path
        else:
            resolved_path = normalized_name
            cache_key = normalized_name
        
        # Check if the module is already loaded
        if self.cache.has_module(cache_key):
            return self.cache.get_module(cache_key)
        
        # Check for circular imports
        if self.cache.is_loading(cache_key):
            raise CircularImportError(cache_key, self._get_import_chain(cache_key))
        
        # Mark the module as being loaded
        self.cache.mark_loading(cache_key)
        
        # Save the previous module path and set the current one
        previous_module_path = self.current_module_path
        # Also save the runtime's module_path so nested imports don't corrupt it
        previous_runtime_module_path = getattr(self.runtime, 'module_path', None)
        
        try:
            # Find the module file
            module_file = self._find_module_file(resolved_path)
            if not module_file:
                raise ImportError(f"Module '{module_name}' not found")
            
            # Set the current module path for relative imports
            self.current_module_path = module_file
            
            # Load the module
            module = self._load_module_from_file(module_file)
            
            # Add the module to the cache
            self.cache.add_module(cache_key, module)
            
            # Store the mapping from module name to file path
            self.module_path_map[cache_key] = module_file
            
            # Mark the module as loaded
            self.cache.mark_loaded(cache_key)
            
            return module
        finally:
            # Restore the previous module path (both loader and runtime)
            self.current_module_path = previous_module_path
            if previous_runtime_module_path is not None:
                self.runtime.module_path = previous_runtime_module_path
    
    def _normalize_module_name(self, module_name: str) -> Tuple[str, bool]:
        """
        Normalize a module name by removing file extensions and determining if it's a relative import.
        
        Args:
            module_name: The module name to normalize
            
        Returns:
            A tuple of (normalized_name, is_relative)
        """
        # Check if it's a relative import
        is_relative = module_name.startswith('./') or module_name.startswith('../')
        
        # Remove file extension if present
        if module_name.endswith('.nxl'):
            module_name = module_name[:-5]
        
        return module_name, is_relative
    
    def _resolve_relative_path(self, module_name: str, current_path: str) -> str:
        """
        Resolve a relative module path based on the current module path.
        
        Args:
            module_name: The relative module name (starting with ./ or ../)
            current_path: The path of the current module
            
        Returns:
            The resolved absolute path
        """
        if not current_path:
            raise ImportError(f"Relative import '{module_name}' used but no current module path set")
        
        # Get the directory of the current module
        current_dir = os.path.dirname(current_path)
        
        # Handle ./ imports (same directory)
        if module_name.startswith('./'):
            relative_path = module_name[2:]  # Remove './'
            return os.path.normpath(os.path.join(current_dir, relative_path))
        
        # Handle ../ imports (parent directory)
        elif module_name.startswith('../'):
            # Count the number of parent directory references
            parent_count = 0
            path = module_name
            while path.startswith('../'):
                parent_count += 1
                path = path[3:]  # Remove '../'
            
            # Navigate up parent_count directories
            result_path = current_dir
            for _ in range(parent_count):
                result_path = os.path.dirname(result_path)
            
            # Join with the remaining path
            return os.path.normpath(os.path.join(result_path, path))
        
        return module_name
    
    def _find_module_file(self, module_name: str) -> Optional[str]:
        """
        Find the file for a module.
        
        Args:
            module_name: The name of the module to find
            
        Returns:
            The path to the module file, or None if not found
        """
        # Convert dotted module names to path separators (test.module -> test/module)
        module_path = module_name.replace('.', os.sep)
        
        # Check if it's a standard library module
        stdlib_path = self._find_stdlib_module(module_path)
        if stdlib_path:
            return stdlib_path
        
        # Check if it's an absolute path
        if os.path.isabs(module_path):
            if os.path.isfile(module_path):
                return module_path
            if os.path.isfile(f"{module_path}.nxl"):
                return f"{module_path}.nxl"
            return None
        
        # Search in the search paths
        for path in self.search_paths:
            # Try with .nlpl extension
            full_path = os.path.join(path, f"{module_path}.nxl")
            if os.path.isfile(full_path):
                return full_path
            
            # Try without extension
            full_path = os.path.join(path, module_path)
            if os.path.isfile(full_path):
                return full_path
        
        return None
    
    def _find_stdlib_module(self, module_name: str) -> Optional[str]:
        """
        Find a module in the standard library.
        
        Args:
            module_name: The name of the module to find
            
        Returns:
            The path to the module file, or None if not found
        """
        # Check for direct match in stdlib directory
        stdlib_dir = os.path.join(os.path.dirname(__file__), '..', 'stdlib')
        
        # Try with .nlpl extension
        module_path = os.path.join(stdlib_dir, f"{module_name}.nxl")
        if os.path.isfile(module_path):
            return module_path
        
        # Try with .py extension (for Python-implemented stdlib modules)
        module_path = os.path.join(stdlib_dir, f"{module_name}.py")
        if os.path.isfile(module_path):
            return module_path
        
        # Try as a directory with __init__.py or __init__.nlpl
        module_dir = os.path.join(stdlib_dir, module_name)
        if os.path.isdir(module_dir):
            init_py = os.path.join(module_dir, "__init__.py")
            init_nxl = os.path.join(module_dir, "__init__.nxl")
            
            if os.path.isfile(init_py):
                return init_py
            if os.path.isfile(init_nxl):
                return init_nxl
        
        return None
    
    def _load_module_from_file(self, file_path: str) -> Any:
        """
        Load a module from a file.
        
        Args:
            file_path: The path to the module file
            
        Returns:
            The loaded module (a Module object with attributes)
            
        Raises:
            ImportError: If the module cannot be loaded
        """
        try:
            # Read the file
            with open(file_path, 'r') as f:
                source_code = f.read()
            
            # Parse the module
            lexer = Lexer(source_code)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Use the parent runtime for the module instead of creating a new one
            # This ensures modules have access to stdlib and other registered functions
            module_runtime = self.runtime
            
            # Store the module's file path in the runtime for relative imports
            module_runtime.module_path = file_path
            
            # Import Interpreter here to avoid circular imports
            from ..interpreter.interpreter import Interpreter
            
            # Create a new interpreter for the module using the shared runtime
            interpreter = Interpreter(module_runtime, enable_type_checking=False)
            
            # Execute the module
            interpreter.interpret(ast)
            
            # Create a Module object that contains all the module's definitions
            class Module:
                """A loaded NexusLang module."""
                pass
            
            module = Module()
            
            # Copy all variables and functions from the interpreter's scope to the module
            # Get the global scope (first scope in the stack)
            if interpreter.current_scope:
                global_scope = interpreter.current_scope[0]
                for name, value in global_scope.items():
                    setattr(module, name, value)
            
            # Also copy functions from the runtime
            for name, func in module_runtime.functions.items():
                if not hasattr(module, name):
                    setattr(module, name, func)
            
            # Return the module object
            return module
        except Exception as e:
            raise ImportError(f"Error loading module '{file_path}': {str(e)}") from e
    
    def get_module_path(self, module_name: str) -> Optional[str]:
        """
        Get the file path for a loaded module.
        
        Args:
            module_name: The name of the module
            
        Returns:
            The file path of the module, or None if not found
        """
        return self.module_path_map.get(module_name)

    def _get_import_chain(self, module_name: str) -> List[str]:
        """
        Get the import chain for a module.
        
        Args:
            module_name: The name of the module
            
        Returns:
            The import chain for the module
        """
        import_chain = []
        current_module = module_name
        while current_module:
            import_chain.append(current_module)
            current_module = self.module_path_map.get(current_module)
        return import_chain 