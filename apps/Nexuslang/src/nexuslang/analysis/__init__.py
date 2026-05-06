"""
AST Analysis Module
====================

Provides symbol table management and AST analysis for IDE features.
"""

from .symbol_table import (
    SymbolTable,
    Symbol,
    SymbolKind,
    SymbolLocation,
    Scope
)
from .symbol_extractor import ASTSymbolExtractor

__all__ = [
    'SymbolTable',
    'Symbol',
    'SymbolKind',
    'SymbolLocation',
    'Scope',
    'ASTSymbolExtractor'
]
