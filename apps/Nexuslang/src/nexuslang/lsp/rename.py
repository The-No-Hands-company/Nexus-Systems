"""
Rename Provider
===============

Provides symbol renaming across the workspace using AST-based analysis.
"""

from typing import List, Dict, Optional
import re
import logging
from ..parser.lexer import Lexer
from ..parser.parser import Parser
from ..analysis import ASTSymbolExtractor, SymbolTable


logger = logging.getLogger(__name__)


class RenameProvider:
    """
    Provides rename refactoring functionality using AST-based symbol resolution.
    
    Renames symbols across all files in the workspace:
    - Functions (definitions and all calls)
    - Classes (definitions and all instantiations)
    - Variables (declarations and all references)
    - Methods (definitions and all calls)
    
    Returns a WorkspaceEdit with all necessary changes.
    """
    
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
            logger.debug("Falling back to cached symbol table for %s", uri, exc_info=True)
            return self.symbol_tables.get(uri, None)
    
    def prepare_rename(
        self,
        text: str,
        position: 'Position',
        uri: str
    ) -> Optional[Dict]:
        """
        Check if rename is valid at this position using AST-based analysis.
        Returns range and placeholder text if rename is allowed.
        
        Args:
            text: Document text
            position: Cursor position
            uri: Document URI
            
        Returns:
            Dict with range and placeholder, or None if rename not allowed
        """
        # Build symbol table
        symbol_table = self._get_or_build_symbol_table(text, uri)
        if not symbol_table:
            return self._fallback_prepare_rename(text, position, uri)
        
        # Get symbol at position
        symbol = symbol_table.get_symbol_at_position(uri, position.line, position.character)
        if not symbol:
            return self._fallback_prepare_rename(text, position, uri)
        
        # All user-defined symbols are renameable
        return {
            "range": {
                "start": {"line": symbol.location.line, "character": symbol.location.column},
                "end": {"line": symbol.location.line, "character": symbol.location.column + len(symbol.name)}
            },
            "placeholder": symbol.name
        }
    
    def _fallback_prepare_rename(
        self,
        text: str,
        position: 'Position',
        uri: str
    ) -> Optional[Dict]:
        """Fallback to regex-based prepare rename."""
        # Get symbol at position
        symbol = self._get_symbol_at_position(text, position)
        if not symbol:
            return None
        
        # Check if it's a renameable symbol
        if not self._is_renameable(text, position, symbol):
            return None
        
        # Get symbol range
        lines = text.split('\n')
        line = lines[position.line]
        
        # Find exact bounds
        start = position.character
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        
        end = position.character
        while end < len(line) and (line[end].isalnum() or line[end] == '_'):
            end += 1
        
        return {
            "range": {
                "start": {"line": position.line, "character": start},
                "end": {"line": position.line, "character": end}
            },
            "placeholder": symbol
        }
    
    def rename(
        self,
        text: str,
        position: 'Position',
        uri: str,
        new_name: str
    ) -> Optional[Dict]:
        """
        Perform rename operation.
        
        Args:
            text: Document text
            position: Cursor position
            uri: Document URI
            new_name: New symbol name
            
        Returns:
            WorkspaceEdit with all changes, or None if rename not possible
        """
        # Validate new name
        if not self._is_valid_identifier(new_name):
            return None
        
        # Get symbol at position
        old_name = self._get_symbol_at_position(text, position)
        if not old_name:
            return None
        
        # Check if renameable
        if not self._is_renameable(text, position, old_name):
            return None
        
        # Get symbol type
        symbol_type = self._get_symbol_type(text, position, old_name)
        
        # Find all occurrences across workspace
        changes = {}
        
        for doc_uri, doc_text in self.server.documents.items():
            doc_changes = self._find_and_replace_in_document(
                doc_text,
                old_name,
                new_name,
                symbol_type,
                doc_uri
            )
            if doc_changes:
                changes[doc_uri] = doc_changes

        # Also rename in workspace files that are not currently open in memory
        if hasattr(self.server, 'workspace_index') and self.server.workspace_index:
            for file_uri in self.server.workspace_index.indexed_files:
                if file_uri in self.server.documents:
                    continue  # Already handled in the loop above
                file_path = self.server.workspace_index._uri_to_path(file_uri)
                try:
                    with open(file_path, 'r', encoding='utf-8') as fh:
                        file_text = fh.read()
                    doc_changes = self._find_and_replace_in_document(
                        file_text, old_name, new_name, symbol_type, file_uri
                    )
                    if doc_changes:
                        changes[file_uri] = doc_changes
                except Exception:
                    logger.warning("Skipping rename scan for %s", file_uri, exc_info=True)

        if not changes:
            return None

        return {
            "changes": changes
        }
    
    def _get_symbol_at_position(self, text: str, position: 'Position') -> Optional[str]:
        """Extract symbol name at cursor position."""
        lines = text.split('\n')
        if position.line >= len(lines):
            return None
        
        line = lines[position.line]
        
        if position.character >= len(line):
            return None
        
        # Find word boundaries
        start = position.character
        end = position.character
        
        # Move back to start of word
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        
        # Move forward to end of word
        while end < len(line) and (line[end].isalnum() or line[end] == '_'):
            end += 1
        
        symbol = line[start:end]
        return symbol if symbol else None
    
    def _is_renameable(self, text: str, position: 'Position', symbol: str) -> bool:
        """Check if symbol can be renamed."""
        # Don't rename keywords
        keywords = {
            'function', 'class', 'set', 'to', 'if', 'else', 'while', 'for',
            'each', 'in', 'return', 'break', 'continue', 'import', 'export',
            'as', 'of', 'with', 'returns', 'true', 'false', 'null', 'new',
            'print', 'text', 'match', 'case', 'end', 'then', 'do', 'struct',
            'union', 'enum', 'interface', 'extends', 'implements', 'public',
            'private', 'protected', 'static', 'async', 'await', 'try', 'catch',
            'finally', 'raise', 'error', 'and', 'or', 'not', 'is', 'plus',
            'minus', 'times', 'divided', 'by', 'modulo', 'equal', 'greater',
            'less', 'than'
        }
        
        if symbol.lower() in keywords:
            return False
        
        # Don't rename stdlib functions (unless user explicitly wants to)
        # For now, allow renaming everything except keywords
        return True
    
    def _is_valid_identifier(self, name: str) -> bool:
        """Check if new name is a valid NexusLang identifier."""
        if not name:
            return False
        
        # Must start with letter or underscore
        if not (name[0].isalpha() or name[0] == '_'):
            return False
        
        # Must contain only alphanumeric and underscores
        if not all(c.isalnum() or c == '_' for c in name):
            return False
        
        # Check against keywords
        keywords = {
            'function', 'class', 'set', 'to', 'if', 'else', 'while', 'for',
            'each', 'in', 'return', 'break', 'continue', 'import', 'export'
        }
        
        if name.lower() in keywords:
            return False
        
        return True
    
    def _get_symbol_type(
        self,
        text: str,
        position: 'Position',
        symbol: str
    ) -> str:
        """
        Determine symbol type (function, class, variable, method).
        
        Returns:
            'function' | 'class' | 'variable' | 'method'
        """
        lines = text.split('\n')
        if position.line >= len(lines):
            return 'variable'
        
        line = lines[position.line]
        
        # Check for function definition
        if re.search(rf'\bfunction\s+{re.escape(symbol)}\b', line, re.IGNORECASE):
            return 'function'
        
        # Check for class definition
        if re.search(rf'\bclass\s+{re.escape(symbol)}\b', line, re.IGNORECASE):
            return 'class'
        
        # Check for method definition (inside a class)
        for i in range(position.line - 1, max(0, position.line - 50), -1):
            if re.search(r'\bclass\s+\w+', lines[i], re.IGNORECASE):
                # Inside class, check for method
                if re.search(rf'\bmethod\s+{re.escape(symbol)}\b', line, re.IGNORECASE) or \
                   re.search(rf'\bfunction\s+{re.escape(symbol)}\b', line, re.IGNORECASE):
                    return 'method'
                break
        
        # Check for variable assignment
        if re.search(rf'\bset\s+{re.escape(symbol)}\s+to\b', line, re.IGNORECASE):
            return 'variable'
        
        # Default to variable
        return 'variable'
    
    def _find_and_replace_in_document(
        self,
        text: str,
        old_name: str,
        new_name: str,
        symbol_type: str,
        uri: str
    ) -> List[Dict]:
        """
        Find all occurrences and create TextEdit objects for replacement.
        
        Returns:
            List of TextEdit objects for this document
        """
        edits = []
        lines = text.split('\n')
        
        if symbol_type == 'function':
            edits.extend(self._replace_function_refs(lines, old_name, new_name))
        elif symbol_type == 'class':
            edits.extend(self._replace_class_refs(lines, old_name, new_name))
        elif symbol_type == 'method':
            edits.extend(self._replace_method_refs(lines, old_name, new_name))
        elif symbol_type == 'variable':
            edits.extend(self._replace_variable_refs(lines, old_name, new_name))
        
        return edits
    
    def _replace_function_refs(
        self,
        lines: List[str],
        old_name: str,
        new_name: str
    ) -> List[Dict]:
        """Find and replace function references."""
        edits = []
        
        # Pattern for function definition
        def_pattern = rf'\bfunction\s+{re.escape(old_name)}\b'
        
        # Pattern for function calls - NexusLang uses "function_name with args" syntax
        call_pattern = rf'\b{re.escape(old_name)}\s+with\b'
        # Also check for "call function_name" syntax
        call_pattern2 = rf'\bcall\s+{re.escape(old_name)}\b'
        # Simple reference
        ref_pattern = rf'\b{re.escape(old_name)}\b'
        
        for i, line in enumerate(lines):
            # Check for definition
            for match in re.finditer(def_pattern, line, re.IGNORECASE):
                # Find exact position of function name (after "function" keyword)
                func_start = match.start()
                func_text = match.group()
                # Find where the name actually starts
                name_start = func_text.lower().index(old_name.lower())
                actual_start = func_start + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for calls (with "with" keyword)
            for match in re.finditer(call_pattern, line, re.IGNORECASE):
                edits.append({
                    "range": {
                        "start": {"line": i, "character": match.start()},
                        "end": {"line": i, "character": match.start() + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for calls (with "call" keyword)
            for match in re.finditer(call_pattern2, line, re.IGNORECASE):
                # Find exact position after "call"
                call_text = match.group()
                name_start = call_text.lower().index(old_name.lower())
                actual_start = match.start() + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for standalone references (assignment to variable, etc.)
            # But avoid duplicates from above patterns
            for match in re.finditer(ref_pattern, line, re.IGNORECASE):
                # Skip if already covered by other patterns
                already_covered = any(
                    edit["range"]["start"]["line"] == i and
                    edit["range"]["start"]["character"] == match.start()
                    for edit in edits
                )
                if not already_covered:
                    # Additional check: is this actually a reference?
                    # (not part of another word, not in string, etc.)
                    if self._is_standalone_reference(line, match.start(), old_name):
                        edits.append({
                            "range": {
                                "start": {"line": i, "character": match.start()},
                                "end": {"line": i, "character": match.start() + len(old_name)}
                            },
                            "newText": new_name
                        })
        
        return edits
    
    def _replace_class_refs(
        self,
        lines: List[str],
        old_name: str,
        new_name: str
    ) -> List[Dict]:
        """Find and replace class references."""
        edits = []
        
        # Pattern for class definition
        def_pattern = rf'\bclass\s+{re.escape(old_name)}\b'
        
        # Pattern for class instantiation
        inst_pattern = rf'\bnew\s+{re.escape(old_name)}\b'
        
        # Pattern for type annotations
        type_pattern = rf'\bas\s+{re.escape(old_name)}\b'
        
        for i, line in enumerate(lines):
            # Check for definition
            for match in re.finditer(def_pattern, line, re.IGNORECASE):
                class_text = match.group()
                name_start = class_text.lower().index(old_name.lower())
                actual_start = match.start() + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for instantiation
            for match in re.finditer(inst_pattern, line, re.IGNORECASE):
                inst_text = match.group()
                name_start = inst_text.lower().index(old_name.lower())
                actual_start = match.start() + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for type annotations
            for match in re.finditer(type_pattern, line, re.IGNORECASE):
                type_text = match.group()
                name_start = type_text.lower().index(old_name.lower())
                actual_start = match.start() + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
        
        return edits
    
    def _replace_method_refs(
        self,
        lines: List[str],
        old_name: str,
        new_name: str
    ) -> List[Dict]:
        """Find and replace method references."""
        edits = []
        
        # Methods are like functions but called with dot notation
        # Pattern for method definition
        def_pattern = rf'\b(function|method)\s+{re.escape(old_name)}\b'
        
        # Pattern for method calls (obj.method syntax or "call obj.method")
        call_pattern = rf'\.{re.escape(old_name)}\b'
        
        for i, line in enumerate(lines):
            # Check for definition
            for match in re.finditer(def_pattern, line, re.IGNORECASE):
                method_text = match.group()
                name_start = method_text.lower().index(old_name.lower())
                actual_start = match.start() + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for calls (dot notation)
            for match in re.finditer(call_pattern, line, re.IGNORECASE):
                # Skip the dot, rename just the method name
                actual_start = match.start() + 1  # +1 to skip the dot
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
        
        return edits
    
    def _replace_variable_refs(
        self,
        lines: List[str],
        old_name: str,
        new_name: str
    ) -> List[Dict]:
        """Find and replace variable references."""
        edits = []
        
        # Pattern for variable declaration
        decl_pattern = rf'\bset\s+{re.escape(old_name)}\s+to\b'
        
        # Pattern for variable reference (standalone word)
        ref_pattern = rf'\b{re.escape(old_name)}\b'
        
        for i, line in enumerate(lines):
            # Check for declaration
            for match in re.finditer(decl_pattern, line, re.IGNORECASE):
                decl_text = match.group()
                name_start = decl_text.lower().index(old_name.lower())
                actual_start = match.start() + name_start
                
                edits.append({
                    "range": {
                        "start": {"line": i, "character": actual_start},
                        "end": {"line": i, "character": actual_start + len(old_name)}
                    },
                    "newText": new_name
                })
            
            # Check for all references
            for match in re.finditer(ref_pattern, line, re.IGNORECASE):
                # Skip if already covered by declaration pattern
                already_covered = any(
                    edit["range"]["start"]["line"] == i and
                    edit["range"]["start"]["character"] == match.start()
                    for edit in edits
                )
                if not already_covered:
                    # Verify it's a standalone reference
                    if self._is_standalone_reference(line, match.start(), old_name):
                        edits.append({
                            "range": {
                                "start": {"line": i, "character": match.start()},
                                "end": {"line": i, "character": match.start() + len(old_name)}
                            },
                            "newText": new_name
                        })
        
        return edits
    
    def _is_standalone_reference(self, line: str, start: int, symbol: str) -> bool:
        """Check if symbol at position is a standalone reference (not part of larger word)."""
        end = start + len(symbol)
        
        # Check character before (must be non-identifier char)
        if start > 0:
            char_before = line[start - 1]
            if char_before.isalnum() or char_before == '_':
                return False
        
        # Check character after (must be non-identifier char)
        if end < len(line):
            char_after = line[end]
            if char_after.isalnum() or char_after == '_':
                return False
        
        # Not inside a string (simple check)
        # Count quotes before this position
        quotes_before = line[:start].count('"') + line[:start].count("'")
        if quotes_before % 2 != 0:  # Odd number of quotes = inside string
            return False
        
        return True
