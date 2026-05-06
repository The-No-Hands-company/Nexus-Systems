"""
Debug Symbol Table for NexusLang
============================

Tracks symbols (variables, functions, classes) with their source locations
for debugger integration.
"""

from dataclasses import dataclass
from enum import Enum
from typing import Dict, List, Optional, Tuple


class SymbolType(Enum):
    """Type of debug symbol."""
    VARIABLE = "variable"
    FUNCTION = "function"
    CLASS = "class"
    STRUCT = "struct"
    PARAMETER = "parameter"
    LOCAL = "local"
    GLOBAL = "global"


@dataclass
class DebugSymbol:
    """Debug information for a single symbol."""
    name: str
    type: SymbolType
    data_type: str  # NexusLang type (Integer, String, etc.)
    file: str
    line: int
    column: int
    scope: str  # Function/class name or "global"
    llvm_name: Optional[str] = None  # LLVM IR name (%variable, @function, etc.)
    size: Optional[int] = None  # Size in bytes
    
    def to_dwarf_entry(self) -> Dict[str, any]:
        """Convert to DWARF debug info entry."""
        return {
            'name': self.name,
            'type': self.type.value,
            'data_type': self.data_type,
            'file': self.file,
            'line': self.line,
            'column': self.column,
            'scope': self.scope,
        }


class SymbolTable:
    """
    Symbol table for debug information.
    
    Tracks all symbols with their locations for debugger integration.
    """
    
    def __init__(self):
        self.symbols: Dict[str, DebugSymbol] = {}
        self.scopes: Dict[str, List[str]] = {}  # scope_name -> [symbol_names]
        self.line_map: Dict[int, List[str]] = {}  # line -> [symbol_names]
        self.source_file: Optional[str] = None
        
    def add_symbol(self, symbol: DebugSymbol):
        """Add a symbol to the table."""
        self.symbols[symbol.name] = symbol
        
        # Add to scope
        if symbol.scope not in self.scopes:
            self.scopes[symbol.scope] = []
        self.scopes[symbol.scope].append(symbol.name)
        
        # Add to line map
        if symbol.line not in self.line_map:
            self.line_map[symbol.line] = []
        self.line_map[symbol.line].append(symbol.name)
    
    def get_symbol(self, name: str) -> Optional[DebugSymbol]:
        """Get symbol by name."""
        return self.symbols.get(name)
    
    def get_symbols_in_scope(self, scope: str) -> List[DebugSymbol]:
        """Get all symbols in a scope."""
        names = self.scopes.get(scope, [])
        return [self.symbols[name] for name in names]
    
    def get_symbols_at_line(self, line: int) -> List[DebugSymbol]:
        """Get all symbols defined at a specific line."""
        names = self.line_map.get(line, [])
        return [self.symbols[name] for name in names]
    
    def get_all_functions(self) -> List[DebugSymbol]:
        """Get all function symbols."""
        return [s for s in self.symbols.values() if s.type == SymbolType.FUNCTION]
    
    def get_all_variables(self) -> List[DebugSymbol]:
        """Get all variable symbols (global, local, parameter)."""
        var_types = {SymbolType.VARIABLE, SymbolType.LOCAL, SymbolType.GLOBAL, SymbolType.PARAMETER}
        return [s for s in self.symbols.values() if s.type in var_types]
    
    def to_map(self) -> Dict[str, Dict]:
        """Export symbol table as dictionary."""
        return {
            'symbols': {name: sym.to_dwarf_entry() for name, sym in self.symbols.items()},
            'scopes': self.scopes,
            'line_map': self.line_map,
            'source_file': self.source_file,
        }
