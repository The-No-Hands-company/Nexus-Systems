"""
Semantic Tokens Provider
=========================

Provides semantic token highlighting using AST-based symbol analysis.
"""

from typing import List, Dict, Optional, Tuple
import re
from ..parser.lexer import Lexer
from ..parser.parser import Parser
from ..analysis import ASTSymbolExtractor, SymbolTable, SymbolKind


class SemanticTokensProvider:
    """
    Provides semantic token highlighting for enhanced syntax coloring.
    
    Uses AST-based symbol resolution to provide accurate token types
    based on semantic meaning rather than just syntax patterns.
    
    Supported token types:
    - namespace, class, enum, interface, struct, typeParameter
    - parameter, variable, property, enumMember, function, method
    - keyword, comment, string, number, operator
    """
    
    # LSP Semantic Token Types (standard)
    TOKEN_TYPES = [
        "namespace",    # 0
        "class",        # 1
        "enum",         # 2
        "interface",    # 3
        "struct",       # 4
        "typeParameter",# 5
        "parameter",    # 6
        "variable",     # 7
        "property",     # 8
        "enumMember",   # 9
        "function",     # 10
        "method",       # 11
        "keyword",      # 12
        "comment",      # 13
        "string",       # 14
        "number",       # 15
        "operator",     # 16
    ]
    
    # LSP Semantic Token Modifiers (standard)
    TOKEN_MODIFIERS = [
        "declaration",      # 0x01
        "definition",       # 0x02
        "readonly",         # 0x04
        "static",           # 0x08
        "deprecated",       # 0x10
        "abstract",         # 0x20
        "async",            # 0x40
        "modification",     # 0x80
        "documentation",    # 0x100
        "defaultLibrary",   # 0x200
    ]
    
    def __init__(self, server):
        self.server = server
        # Cache symbol tables per document
        self.symbol_tables: Dict[str, SymbolTable] = {}
    
    def _get_or_build_symbol_table(self, text: str, uri: str) -> Optional[SymbolTable]:
        """Build symbol table from document text."""
        try:
            lexer = Lexer(text)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            extractor = ASTSymbolExtractor(uri)
            symbol_table = extractor.extract(ast)
            
            self.symbol_tables[uri] = symbol_table
            return symbol_table
        except Exception:
            return self.symbol_tables.get(uri, None)
    
    def _symbol_kind_to_token_type(self, kind: SymbolKind) -> int:
        """Convert SymbolKind to semantic token type index."""
        mapping = {
            SymbolKind.NAMESPACE: 0,
            SymbolKind.MODULE: 0,
            SymbolKind.CLASS: 1,
            SymbolKind.ENUM: 2,
            SymbolKind.INTERFACE: 3,
            SymbolKind.STRUCT: 4,
            SymbolKind.TYPE_PARAMETER: 5,
            SymbolKind.VARIABLE: 7,
            SymbolKind.PROPERTY: 8,
            SymbolKind.FIELD: 8,
            SymbolKind.ENUM_MEMBER: 9,
            SymbolKind.FUNCTION: 10,
            SymbolKind.METHOD: 11,
            SymbolKind.CONSTRUCTOR: 11,
        }
        return mapping.get(kind, 7)  # Default to variable
    
    def get_semantic_tokens(self, text: str, uri: str) -> List[int]:
        """
        Generate semantic tokens for the document.
        
        Returns tokens in LSP format: flat array of 5-element groups:
        [deltaLine, deltaStartChar, length, tokenType, tokenModifiers]
        
        Args:
            text: Document text
            uri: Document URI
            
        Returns:
            Flat array of semantic token data
        """
        # Build symbol table
        symbol_table = self._get_or_build_symbol_table(text, uri)
        absolute_tokens: List[Tuple[int, int, int, int, int]] = []
        if not symbol_table:
            all_symbols = []
        else:
            # Get all symbols from the symbol table
            all_symbols = self._collect_all_symbols(symbol_table)
            # Sort by position (line, then column)
            all_symbols.sort(key=lambda s: (s.location.line, s.location.column))
        
        # Convert symbol-derived tokens to absolute positions.
        prev_line = 0
        prev_char = 0
        
        for symbol in all_symbols:
            line = symbol.location.line
            char = symbol.location.column
            length = len(symbol.name)
            token_type = self._symbol_kind_to_token_type(symbol.kind)
            
            # Calculate modifiers
            modifiers = 0
            if symbol.is_exported:
                modifiers |= 0x01  # declaration
            
            # Delta encoding
            delta_line = line - prev_line
            delta_char = char - prev_char if delta_line == 0 else char
            
            absolute_tokens.append((line, char, length, token_type, modifiers))

            prev_line = line
            prev_char = char

        # Add control-flow keyword tokens for switch/label/fallthrough constructs.
        keyword_type = self.TOKEN_TYPES.index("keyword")
        control_flow_keywords = ("switch", "case", "default", "fallthrough", "label")
        keyword_pattern = r'\b(' + "|".join(control_flow_keywords) + r')\b'
        for line_num, line_text in enumerate(text.split('\n')):
            if line_text.strip().startswith('#'):
                continue
            for match in re.finditer(keyword_pattern, line_text, re.IGNORECASE):
                absolute_tokens.append((line_num, match.start(), len(match.group(1)), keyword_type, 0))

        # De-duplicate and sort before converting to delta-encoded LSP representation.
        absolute_tokens = sorted(set(absolute_tokens), key=lambda t: (t[0], t[1], t[2], t[3], t[4]))

        tokens: List[int] = []
        prev_line = 0
        prev_char = 0
        for line, char, length, token_type, modifiers in absolute_tokens:
            delta_line = line - prev_line
            delta_char = char - prev_char if delta_line == 0 else char
            tokens.extend([delta_line, delta_char, length, token_type, modifiers])
            
            prev_line = line
            prev_char = char
        
        return tokens
    
    def _collect_all_symbols(self, symbol_table: SymbolTable) -> List:
        """
        Collect all symbols from symbol table including nested symbols.
        
        Args:
            symbol_table: The symbol table to collect from
            
        Returns:
            List of all symbols
        """
        symbols = []
        
        # Traverse all scopes
        for scope in self._get_all_scopes(symbol_table.global_scope):
            for symbol in scope.symbols.values():
                symbols.append(symbol)
                # Add children recursively
                symbols.extend(self._collect_children(symbol))
        
        return symbols
    
    def _get_all_scopes(self, scope, scopes=None):
        """Recursively collect all scopes."""
        if scopes is None:
            scopes = []
        
        scopes.append(scope)
        for child_scope in getattr(scope, 'children', []):
            self._get_all_scopes(child_scope, scopes)
        
        return scopes
    
    def _collect_children(self, symbol) -> List:
        """Recursively collect all child symbols."""
        children = []
        for child in symbol.children:
            children.append(child)
            children.extend(self._collect_children(child))
        return children
    
    def get_semantic_tokens_legend(self) -> Dict:
        """
        Get the semantic tokens legend (token types and modifiers).
        
        Returns:
            Dict with tokenTypes and tokenModifiers arrays
        """
        return {
            "tokenTypes": self.TOKEN_TYPES,
            "tokenModifiers": self.TOKEN_MODIFIERS
        }


__all__ = ['SemanticTokensProvider']
