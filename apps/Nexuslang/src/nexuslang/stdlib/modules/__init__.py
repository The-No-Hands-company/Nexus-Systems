"""
Enhanced module system for NexusLang.
Adds namespace management, aliasing, and selective imports.
"""

from typing import Dict, List, Any, Optional, Set
from ...runtime.runtime import Runtime

class Namespace:
    """Represents a module namespace."""
    
    def __init__(self, name: str):
        self.name = name
        self.symbols: Dict[str, Any] = {}
        self.subnamespaces: Dict[str, 'Namespace'] = {}
        self.exports: Set[str] = set()  # Explicitly exported symbols
    
    def add_symbol(self, name: str, value: Any, export: bool = True):
        """Add a symbol to the namespace."""
        self.symbols[name] = value
        if export:
            self.exports.add(name)
    
    def get_symbol(self, name: str) -> Any:
        """Get a symbol from the namespace."""
        if name in self.symbols:
            return self.symbols[name]
        raise NameError(f"Symbol '{name}' not found in namespace '{self.name}'")
    
    def has_symbol(self, name: str) -> bool:
        """Check if a symbol exists in the namespace."""
        return name in self.symbols
    
    def get_exported_symbols(self) -> Dict[str, Any]:
        """Get all exported symbols."""
        return {k: v for k, v in self.symbols.items() if k in self.exports}
    
    def add_subnamespace(self, namespace: 'Namespace'):
        """Add a sub-namespace."""
        self.subnamespaces[namespace.name] = namespace

class NamespaceManager:
    """Manages module namespaces and imports."""
    
    def __init__(self):
        self.namespaces: Dict[str, Namespace] = {}
        self.current_namespace: Optional[Namespace] = None
        self.aliases: Dict[str, str] = {}  # Import aliases
    
    def create_namespace(self, name: str) -> Namespace:
        """Create a new namespace."""
        if name in self.namespaces:
            return self.namespaces[name]
        
        namespace = Namespace(name)
        self.namespaces[name] = namespace
        return namespace
    
    def get_namespace(self, name: str) -> Optional[Namespace]:
        """Get a namespace by name."""
        return self.namespaces.get(name)
    
    def set_current_namespace(self, name: str):
        """Set the current active namespace."""
        if name not in self.namespaces:
            self.create_namespace(name)
        self.current_namespace = self.namespaces[name]
    
    def add_alias(self, alias: str, module_name: str):
        """Add an import alias (e.g., import math as m)."""
        self.aliases[alias] = module_name
    
    def resolve_name(self, name: str) -> str:
        """Resolve a name through aliases."""
        return self.aliases.get(name, name)
    
    def import_module(self, module_name: str, alias: Optional[str] = None,
                     selective: Optional[List[str]] = None) -> Dict[str, Any]:
        """
        Import a module with various options.
        
        Args:
            module_name: Name of the module to import
            alias: Optional alias for the module
            selective: Optional list of specific symbols to import
        
        Returns:
            Dictionary of imported symbols
        """
        namespace = self.get_namespace(module_name)
        if not namespace:
            raise ImportError(f"Module '{module_name}' not found")
        
        if alias:
            self.add_alias(alias, module_name)
        
        if selective:
            # Import only selected symbols
            return {name: namespace.get_symbol(name) for name in selective}
        else:
            # Import all exported symbols
            return namespace.get_exported_symbols()
    
    def import_from(self, module_name: str, symbols: List[str],
                    aliases: Optional[Dict[str, str]] = None) -> Dict[str, Any]:
        """
        Import specific symbols from a module (from X import Y, Z).
        
        Args:
            module_name: Name of the module
            symbols: List of symbol names to import
            aliases: Optional dict mapping symbol names to aliases
        
        Returns:
            Dictionary of imported symbols (with aliases applied)
        """
        namespace = self.get_namespace(module_name)
        if not namespace:
            raise ImportError(f"Module '{module_name}' not found")
        
        result = {}
        for symbol in symbols:
            if not namespace.has_symbol(symbol):
                raise ImportError(f"Symbol '{symbol}' not found in module '{module_name}'")
            
            value = namespace.get_symbol(symbol)
            key = aliases.get(symbol, symbol) if aliases else symbol
            result[key] = value
        
        return result

def register_module_functions(runtime: Runtime) -> None:
    """Register enhanced module system functions."""
    
    # Create global namespace manager
    namespace_mgr = NamespaceManager()
    runtime.namespace_manager = namespace_mgr
    
    # Create stdlib namespace and populate it
    stdlib = namespace_mgr.create_namespace("stdlib")
    
    # Register stdlib functions in namespace
    for name, func in runtime.functions.items():
        stdlib.add_symbol(name, func)
    
    # Register stdlib constants in namespace
    if hasattr(runtime, 'constants'):
        for name, value in runtime.constants.items():
            stdlib.add_symbol(name, value)
    
    # Namespace management functions
    def create_namespace(name: str):
        """Create a new namespace."""
        return namespace_mgr.create_namespace(name)
    
    def get_namespace(name: str):
        """Get a namespace."""
        ns = namespace_mgr.get_namespace(name)
        if not ns:
            raise NameError(f"Namespace '{name}' not found")
        return ns
    
    def namespace_add(ns_name: str, symbol_name: str, value: Any):
        """Add a symbol to a namespace."""
        ns = namespace_mgr.get_namespace(ns_name)
        if not ns:
            raise NameError(f"Namespace '{ns_name}' not found")
        ns.add_symbol(symbol_name, value)
    
    def namespace_get(ns_name: str, symbol_name: str):
        """Get a symbol from a namespace."""
        ns = namespace_mgr.get_namespace(ns_name)
        if not ns:
            raise NameError(f"Namespace '{ns_name}' not found")
        return ns.get_symbol(symbol_name)
    
    def list_namespaces():
        """List all available namespaces."""
        return list(namespace_mgr.namespaces.keys())
    
    def list_namespace_symbols(ns_name: str):
        """List all symbols in a namespace."""
        ns = namespace_mgr.get_namespace(ns_name)
        if not ns:
            raise NameError(f"Namespace '{ns_name}' not found")
        return list(ns.symbols.keys())
    
    # Import functions
    def import_module(module_name: str, alias: Optional[str] = None):
        """Import a module, optionally with an alias."""
        return namespace_mgr.import_module(module_name, alias)
    
    def import_from(module_name: str, *symbols):
        """Import specific symbols from a module."""
        return namespace_mgr.import_from(module_name, list(symbols))
    
    # Package management (basic)
    def get_module_info(module_name: str) -> Dict[str, Any]:
        """Get information about a module."""
        ns = namespace_mgr.get_namespace(module_name)
        if not ns:
            raise NameError(f"Module '{module_name}' not found")
        
        return {
            'name': ns.name,
            'symbols': list(ns.symbols.keys()),
            'exported': list(ns.exports),
            'subnamespaces': list(ns.subnamespaces.keys())
        }
    
    # Register functions
    runtime.register_function("create_namespace", create_namespace)
    runtime.register_function("get_namespace", get_namespace)
    runtime.register_function("namespace_add", namespace_add)
    runtime.register_function("namespace_get", namespace_get)
    runtime.register_function("list_namespaces", list_namespaces)
    runtime.register_function("list_namespace_symbols", list_namespace_symbols)
    runtime.register_function("import_module", import_module)
    runtime.register_function("import_from", import_from)
    runtime.register_function("get_module_info", get_module_info)
