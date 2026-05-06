"""
Symbol Table and AST-Based Symbol Resolution
=============================================

Provides comprehensive symbol tracking and resolution for NexusLang code.
Used by LSP for go-to-definition, find-references, rename, etc.
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set, Tuple
from enum import Enum, auto


class SymbolKind(Enum):
    """Symbol classification matching LSP SymbolKind."""
    FILE = 1
    MODULE = 2
    NAMESPACE = 3
    PACKAGE = 4
    CLASS = 5
    METHOD = 6
    PROPERTY = 7
    FIELD = 8
    CONSTRUCTOR = 9
    ENUM = 10
    INTERFACE = 11
    FUNCTION = 12
    VARIABLE = 13
    CONSTANT = 14
    STRING = 15
    NUMBER = 16
    BOOLEAN = 17
    ARRAY = 18
    OBJECT = 19
    KEY = 20
    NULL = 21
    ENUM_MEMBER = 22
    STRUCT = 23
    EVENT = 24
    OPERATOR = 25
    TYPE_PARAMETER = 26


@dataclass
class SymbolLocation:
    """Location of a symbol in source code."""
    uri: str
    line: int
    column: int
    end_line: Optional[int] = None
    end_column: Optional[int] = None


@dataclass
class Symbol:
    """
    Represents a symbol in the codebase.
    
    Tracks:
    - Name and kind (function, class, variable, etc.)
    - Definition location
    - Type information
    - Scope and parent symbol
    - References to this symbol
    - Documentation
    """
    name: str
    kind: SymbolKind
    location: SymbolLocation
    scope_level: int  # 0 = global, 1+ = nested
    parent: Optional['Symbol'] = None
    type_annotation: Optional[str] = None
    documentation: Optional[str] = None
    is_exported: bool = False
    references: List[SymbolLocation] = field(default_factory=list)
    children: List['Symbol'] = field(default_factory=list)
    
    def add_reference(self, location: SymbolLocation):
        """Add a reference to this symbol."""
        self.references.append(location)
    
    def add_child(self, child: 'Symbol'):
        """Add a child symbol (e.g., method to class)."""
        child.parent = self
        self.children.append(child)
    
    def get_qualified_name(self) -> str:
        """Get fully qualified name (e.g., 'MyClass.my_method')."""
        if self.parent:
            return f"{self.parent.get_qualified_name()}.{self.name}"
        return self.name


class Scope:
    """
    Represents a lexical scope in the program.
    
    Scopes are nested (global -> function -> block).
    """
    def __init__(self, parent: Optional['Scope'] = None, scope_type: str = "block"):
        self.parent = parent
        self.scope_type = scope_type  # "global", "function", "class", "block"
        self.symbols: Dict[str, Symbol] = {}
        self.children: List['Scope'] = []
        self.level = 0 if parent is None else parent.level + 1
    
    def define(self, symbol: Symbol):
        """Define a symbol in this scope."""
        self.symbols[symbol.name] = symbol
        symbol.scope_level = self.level
    
    def resolve(self, name: str) -> Optional[Symbol]:
        """
        Resolve a symbol by name, searching parent scopes.
        
        Returns:
            Symbol if found, None otherwise
        """
        if name in self.symbols:
            return self.symbols[name]
        
        if self.parent:
            return self.parent.resolve(name)
        
        return None
    
    def get_all_symbols(self) -> List[Symbol]:
        """Get all symbols in this scope (not recursive)."""
        return list(self.symbols.values())


class SymbolTable:
    """
    Master symbol table for a program or workspace.
    
    Provides:
    - Symbol definition tracking
    - Scope-aware symbol resolution
    - Reference tracking
    - AST walking and symbol extraction
    """
    
    def __init__(self):
        self.global_scope = Scope(scope_type="global")
        self.current_scope = self.global_scope
        self.all_symbols: List[Symbol] = []
        self.symbols_by_uri: Dict[str, List[Symbol]] = {}
    
    def enter_scope(self, scope_type: str = "block") -> Scope:
        """Enter a new scope."""
        new_scope = Scope(parent=self.current_scope, scope_type=scope_type)
        self.current_scope.children.append(new_scope)
        self.current_scope = new_scope
        return new_scope
    
    def exit_scope(self):
        """Exit current scope, return to parent."""
        if self.current_scope.parent:
            self.current_scope = self.current_scope.parent
    
    def define_symbol(self, symbol: Symbol):
        """
        Define a symbol in the current scope.
        
        Args:
            symbol: Symbol to define
        """
        self.current_scope.define(symbol)
        self.all_symbols.append(symbol)
        
        # Track by URI for quick lookup
        uri = symbol.location.uri
        if uri not in self.symbols_by_uri:
            self.symbols_by_uri[uri] = []
        self.symbols_by_uri[uri].append(symbol)
    
    def resolve_symbol(self, name: str) -> Optional[Symbol]:
        """
        Resolve a symbol by name in current scope chain.
        
        Args:
            name: Symbol name to resolve
            
        Returns:
            Symbol if found, None otherwise
        """
        return self.current_scope.resolve(name)
    
    def add_reference(self, symbol_name: str, location: SymbolLocation):
        """Add a reference to a symbol."""
        symbol = self.resolve_symbol(symbol_name)
        if symbol:
            symbol.add_reference(location)
    
    def get_symbols_in_file(self, uri: str) -> List[Symbol]:
        """Get all symbols defined in a file."""
        return self.symbols_by_uri.get(uri, [])
    
    def find_symbols_by_name(self, name: str, fuzzy: bool = False) -> List[Symbol]:
        """
        Find symbols by name (exact or fuzzy match).
        
        Args:
            name: Symbol name to search for
            fuzzy: If True, use fuzzy matching
            
        Returns:
            List of matching symbols
        """
        if fuzzy:
            name_lower = name.lower()
            return [s for s in self.all_symbols if name_lower in s.name.lower()]
        else:
            return [s for s in self.all_symbols if s.name == name]
    
    def find_symbols_by_kind(self, kind: SymbolKind) -> List[Symbol]:
        """Find all symbols of a specific kind."""
        return [s for s in self.all_symbols if s.kind == kind]
    
    def get_symbol_at_position(self, uri: str, line: int, column: int) -> Optional[Symbol]:
        """
        Find symbol at a specific position in a file.
        
        Args:
            uri: File URI
            line: Line number (0-indexed)
            column: Column number (0-indexed)
            
        Returns:
            Symbol at position if found, None otherwise
        """
        symbols = self.get_symbols_in_file(uri)
        
        for symbol in symbols:
            loc = symbol.location
            # Check if position is within symbol definition
            if loc.line == line and loc.column <= column:
                if loc.end_column is None or column <= loc.end_column:
                    return symbol
            elif loc.end_line is not None and loc.line <= line <= loc.end_line:
                return symbol
        
        return None
    
    def clear(self):
        """Clear all symbols and reset to empty state."""
        self.global_scope = Scope(scope_type="global")
        self.current_scope = self.global_scope
        self.all_symbols.clear()
        self.symbols_by_uri.clear()


__all__ = ['SymbolTable', 'Symbol', 'SymbolKind', 'SymbolLocation', 'Scope']
