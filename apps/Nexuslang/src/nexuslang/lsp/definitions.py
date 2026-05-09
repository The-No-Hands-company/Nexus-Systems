"""
Definition Provider
===================

Provides go-to-definition functionality using AST-based symbol resolution.
"""

from typing import Optional, Tuple, Dict, List
import re
import os
import logging
from ..parser.lexer import Lexer
from ..parser.parser import Parser
from ..analysis import ASTSymbolExtractor, SymbolTable
from ..parser.ast import ClassDefinition
from ..errors import NxlError


logger = logging.getLogger(__name__)


_RECOVERABLE_LSP_EXCEPTIONS = (
    NxlError,
    RuntimeError,
    ValueError,
    TypeError,
    AttributeError,
    OSError,
    UnicodeError,
)


class DefinitionProvider:
    """
    Provides go-to-definition for NexusLang symbols using AST analysis.
    
    Supports:
    - Functions (cross-file)
    - Classes (cross-file)
    - Variables (local and module-level)
    - Methods (within classes)
    - Import resolution
    - Workspace-wide search
    """
    
    def __init__(self, server):
        self.server = server
        # Stdlib modules for quick lookup
        self.stdlib_modules = {
            'math', 'string', 'io', 'system', 'collections', 'network'
        }
        # Cache symbol tables per document
        self.symbol_tables: Dict[str, SymbolTable] = {}
        self._class_hierarchy_cache: Optional[Dict[str, List[str]]] = None
    
    def _get_word_at_position(self, text: str, position):
        """Alias for _get_symbol_at_position for workspace index code."""
        return self._get_symbol_at_position(text, position)
    
    def _get_or_build_symbol_table(self, text: str, uri: str) -> Optional[SymbolTable]:
        """
        Get cached symbol table or build new one from document text.
        
        Args:
            text: Document text
            uri: Document URI
            
        Returns:
            SymbolTable or None if parse fails
        """
        try:
            # Try to parse and extract symbols
            lexer = Lexer(text)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            extractor = ASTSymbolExtractor(uri)
            symbol_table = extractor.extract(ast)
            
            # Cache it
            self.symbol_tables[uri] = symbol_table
            return symbol_table
        except _RECOVERABLE_LSP_EXCEPTIONS:
            # Parse failed - return cached version if available
            return self.symbol_tables.get(uri, None)
    
    def get_definition(self, text: str, position, uri: str):
        """
        Find definition of symbol at position using AST-based resolution.
        
        Args:
            text: Document text
            position: Cursor position
            uri: Document URI
            
        Returns:
            Location or None
        """
        # First, try workspace index for cross-file lookup
        if hasattr(self.server, 'workspace_index') and self.server.workspace_index:
            word = self._get_word_at_position(text, position)
            if word:
                symbols = self.server.workspace_index.get_symbol(word)
                if symbols:
                    # Method override-aware navigation: when invoked on an overriding
                    # method definition, jump to the nearest base implementation.
                    method_symbols = [s for s in symbols if s.kind == 'method']
                    if method_symbols:
                        current_method = self._find_current_method_symbol(method_symbols, uri, position.line)
                        if current_method:
                            base_method = self._find_overridden_method(word, current_method.scope)
                            if base_method:
                                from ..lsp.server import Location, Range, Position
                                return Location(
                                    uri=base_method.file_uri,
                                    range=Range(
                                        start=Position(line=base_method.line, character=base_method.column),
                                        end=Position(line=base_method.line, character=base_method.column + len(base_method.name))
                                    )
                                )

                            # If no override target exists, keep local definition stable.
                            from ..lsp.server import Location, Range, Position
                            return Location(
                                uri=current_method.file_uri,
                                range=Range(
                                    start=Position(line=current_method.line, character=current_method.column),
                                    end=Position(line=current_method.line, character=current_method.column + len(current_method.name))
                                )
                            )

                    # Return first matching symbol (could enhance to handle multiple)
                    sym = symbols[0]
                    from ..lsp.server import Location, Range, Position
                    return Location(
                        uri=sym.file_uri,
                        range=Range(
                            start=Position(line=sym.line, character=sym.column),
                            end=Position(line=sym.line, character=sym.column + len(sym.name))
                        )
                    )
        # Fallback to AST-based symbol table
        symbol_table = self._get_or_build_symbol_table(text, uri)
        if not symbol_table:
            # Fallback to regex-based search
            return self._fallback_search(text, position, uri)
        
        # Get symbol at position from symbol table
        symbol = symbol_table.get_symbol_at_position(uri, position.line, position.character)
        if symbol:
            # Found symbol - return its definition location
            from ..lsp.server import Location, Range, Position
            return Location(
                uri=symbol.location.uri,
                range=Range(
                    start=Position(line=symbol.location.line, character=symbol.location.column),
                    end=Position(line=symbol.location.line, character=symbol.location.column + len(symbol.name))
                )
            )
        
        # Try fallback
        return self._fallback_search(text, position, uri)
    
    def _fallback_search(self, text: str, position, uri: str):
        """Fallback to regex-based search if AST analysis fails."""
        # Get symbol at position
        symbol = self._get_symbol_at_position(text, position)
        if not symbol:
            return None

        # Resolve member access through imported module aliases, e.g.:
        #   import utils.math as m
        #   set x to m.helper with 1
        member_access = self._get_member_access_at_position(text, position)
        if member_access:
            base_name, member_name = member_access
            if member_name == symbol:
                module_name = self._resolve_module_alias(text, base_name)
                if module_name:
                    location = self._resolve_symbol_in_module(module_name, member_name, uri)
                    if location:
                        return location
        
        # Check if it's an import
        import_target = self._get_import_target(text, position, symbol)
        if import_target:
            return self._resolve_import(import_target, uri)
        
        # Try current file first
        location = self._find_in_current_file(text, symbol, uri, position)
        if location:
            return location
        
        # Try imported modules
        location = self._find_in_imports(text, symbol, uri)
        if location:
            return location
        
        # Try all open documents
        location = self._find_in_workspace(symbol)
        if location:
            return location
        
        return None

    def _get_member_access_at_position(self, text: str, position) -> Optional[Tuple[str, str]]:
        """Return (base, member) if cursor is on `<base>.<member>`, else None."""
        lines = text.split('\n')
        if position.line >= len(lines):
            return None

        line = lines[position.line]
        if position.character >= len(line):
            return None

        # Expand token under cursor (member side)
        start = position.character
        end = position.character
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        while end < len(line) and (line[end].isalnum() or line[end] == '_'):
            end += 1

        if start == end:
            return None

        member = line[start:end]
        if start == 0 or line[start - 1] != '.':
            return None

        # Expand token left of '.' (base side)
        base_end = start - 1
        base_start = base_end
        while base_start > 0 and (line[base_start - 1].isalnum() or line[base_start - 1] == '_'):
            base_start -= 1

        if base_start == base_end:
            return None

        base = line[base_start:base_end]
        if not base or not member:
            return None

        return (base, member)

    def _resolve_module_alias(self, text: str, alias_or_module: str) -> Optional[str]:
        """Resolve alias/module token to full module name from import statements."""
        lines = text.split('\n')
        for line in lines:
            match = re.search(r'^\s*import\s+(.+)$', line, re.IGNORECASE)
            if not match:
                continue

            for part in match.group(1).split(','):
                item = part.strip()
                if not item:
                    continue

                alias_match = re.match(r'^([\w\.]+)\s+as\s+(\w+)$', item, re.IGNORECASE)
                if alias_match:
                    module_name, alias = alias_match.groups()
                    if alias == alias_or_module:
                        return module_name
                    continue

                module_name = item
                if module_name == alias_or_module:
                    return module_name

                # Support common short-form usage for dotted imports: pkg.mod -> mod
                short_name = module_name.split('.')[-1]
                if short_name == alias_or_module:
                    return module_name

        return None

    def _resolve_symbol_in_module(self, module_name: str, symbol: str, current_uri: str):
        """Resolve symbol definition inside the given module name."""
        current_dir = os.path.dirname(current_uri.replace('file://', ''))

        for module_path in self._find_module_paths(module_name, current_dir):
            module_uri = f"file://{module_path}"
            module_text = self._get_module_text(module_uri, module_path)
            if not module_text:
                continue

            location = self._find_in_current_file(module_text, symbol, module_uri, None)
            if location:
                return location

        return None
    
    def _get_import_target(self, text: str, position, symbol: str) -> Optional[str]:
        """Check if symbol is part of an import statement."""
        lines = text.split('\n')
        if position.line >= len(lines):
            return None
        
        line = lines[position.line]
        
        # Check for "import module_name" and comma-separated forms.
        import_match = re.search(r'^\s*import\s+(.+)$', line, re.IGNORECASE)
        if import_match:
            for part in import_match.group(1).split(','):
                item = part.strip()
                if not item:
                    continue
                alias_match = re.match(r'^([\w\.]+)\s+as\s+(\w+)$', item, re.IGNORECASE)
                if alias_match:
                    module_name, alias = alias_match.groups()
                    if symbol in (module_name, alias):
                        return module_name
                    continue
                if item == symbol:
                    return symbol

        # Check for "from module_name import a, b as c".
        from_match = re.search(r'^\s*from\s+([\w\.]+)\s+import\s+(.+)$', line, re.IGNORECASE)
        if from_match:
            module_name, imported_part = from_match.groups()
            if module_name == symbol:
                return module_name
            for item in imported_part.split(','):
                spec = item.strip()
                if not spec:
                    continue
                alias_match = re.match(r'^(\w+)\s+as\s+(\w+)$', spec, re.IGNORECASE)
                if alias_match:
                    imported_symbol, alias = alias_match.groups()
                    if symbol in (imported_symbol, alias):
                        return f"{module_name}.{imported_symbol}"
                elif spec == symbol:
                    return f"{module_name}.{spec}"
        
        return None
    
    def _resolve_import(self, import_target: str, current_uri: str):
        """Resolve import to file path."""
        from ..lsp.server import Location, Range, Position

        current_dir = os.path.dirname(current_uri.replace('file://', ''))

        # Resolve ambiguity for dotted targets:
        # - "pkg.sub.mod" (module path)
        # - "pkg.sub.mod.symbol" (module path + symbol)
        symbol_name = None
        module_name = import_target
        if '.' in import_target:
            as_module = self._find_module_paths(import_target, current_dir)
            if not as_module:
                module_name, symbol_name = import_target.rsplit('.', 1)
        
        # Skip stdlib modules
        if module_name.split('.')[0] in self.stdlib_modules:
            return None
        
        # Find module file across nested package paths.
        for module_path in self._find_module_paths(module_name, current_dir):
            module_uri = f"file://{module_path}"

            if symbol_name:
                module_text = self._get_module_text(module_uri, module_path)
                if module_text:
                    location = self._find_in_current_file(module_text, symbol_name, module_uri, None)
                    if location:
                        return location

            # Point to start of module when no symbol target or symbol not found.
            return Location(
                uri=module_uri,
                range=Range(
                    start=Position(0, 0),
                    end=Position(0, 0)
                )
            )
        
        return None
    
    def _find_in_current_file(self, text: str, symbol: str, uri: str, current_position):
        """Find definition in current file."""
        from ..lsp.server import Location, Range, Position
        
        line, column = self._find_definition(text, symbol, current_position)
        
        if line is not None and column is not None:
            return Location(
                uri=uri,
                range=Range(
                    start=Position(line, column),
                    end=Position(line, column + len(symbol))
                )
            )
        
        return None
    
    def _find_in_imports(self, text: str, symbol: str, current_uri: str):
        """Find definition in imported modules."""
        lines = text.split('\n')
        imports: List[Tuple[str, str]] = []
        
        # Extract import statements
        for line in lines:
            match = re.search(r'^\s*import\s+(.+)$', line, re.IGNORECASE)
            if match:
                for part in match.group(1).split(','):
                    item = part.strip()
                    if not item:
                        continue
                    alias_match = re.match(r'^([\w\.]+)\s+as\s+(\w+)$', item, re.IGNORECASE)
                    if alias_match:
                        module_name, alias = alias_match.groups()
                        if symbol in (module_name, alias):
                            imports.append((module_name, symbol))
                        continue
                    if item == symbol:
                        imports.append((item, symbol))

            match = re.search(r'^\s*from\s+([\w\.]+)\s+import\s+(.+)$', line, re.IGNORECASE)
            if match:
                module_name, imported_part = match.groups()
                for item in imported_part.split(','):
                    spec = item.strip()
                    if not spec:
                        continue
                    alias_match = re.match(r'^(\w+)\s+as\s+(\w+)$', spec, re.IGNORECASE)
                    if alias_match:
                        imported_symbol, alias = alias_match.groups()
                        if symbol in (imported_symbol, alias):
                            imports.append((module_name, imported_symbol))
                    elif spec == symbol:
                        imports.append((module_name, spec))
        
        # Search imported modules
        current_dir = os.path.dirname(current_uri.replace('file://', ''))
        
        for module_name, imported_symbol in imports:
            if module_name.split('.')[0] in self.stdlib_modules:
                continue

            for module_path in self._find_module_paths(module_name, current_dir):
                module_uri = f"file://{module_path}"
                module_text = self._get_module_text(module_uri, module_path)
                if not module_text:
                    continue

                location = self._find_in_current_file(module_text, imported_symbol, module_uri, None)
                if location:
                    return location
        
        return None

    def _candidate_base_dirs(self, current_dir: str) -> List[str]:
        """Return candidate base directories for module resolution."""
        bases: List[str] = []

        # Walk upwards from current file directory so sibling/parent packages resolve.
        cur = os.path.abspath(current_dir)
        workspace_root = None
        if getattr(self.server, 'workspace_index', None):
            workspace_root = os.path.abspath(getattr(self.server.workspace_index, 'root_path', '') or '')

        while True:
            if cur not in bases:
                bases.append(cur)
            if workspace_root and os.path.abspath(cur) == workspace_root:
                break
            parent = os.path.dirname(cur)
            if parent == cur:
                break
            cur = parent

        if workspace_root and workspace_root not in bases:
            bases.append(workspace_root)

        return bases

    def _find_module_paths(self, module_name: str, current_dir: str) -> List[str]:
        """Resolve dotted module names to candidate .nxl paths and __init__.nxl files."""
        rel_path = module_name.replace('.', os.sep)
        candidates: List[str] = []

        for base in self._candidate_base_dirs(current_dir):
            file_candidate = os.path.join(base, f"{rel_path}.nxl")
            init_candidate = os.path.join(base, rel_path, "__init__.nxl")
            if os.path.exists(file_candidate):
                candidates.append(file_candidate)
            if os.path.exists(init_candidate):
                candidates.append(init_candidate)

        # Keep first-seen order, remove duplicates.
        seen = set()
        unique = []
        for path in candidates:
            if path not in seen:
                unique.append(path)
                seen.add(path)
        return unique

    def _get_module_text(self, module_uri: str, module_path: str) -> Optional[str]:
        """Get module source from open docs cache or filesystem."""
        if module_uri in self.server.documents:
            return self.server.documents[module_uri]
        try:
            with open(module_path, 'r', encoding='utf-8') as f:
                return f.read()
        except _RECOVERABLE_LSP_EXCEPTIONS:
            logger.warning("Failed to read module text for %s", module_path, exc_info=True)
            return None
    
    def _find_in_workspace(self, symbol: str):
        """Find definition across all open documents."""
        for uri, text in self.server.documents.items():
            location = self._find_in_current_file(text, symbol, uri, None)
            if location:
                return location
        
        return None
    
    def _get_symbol_at_position(self, text: str, position) -> Optional[str]:
        """Extract symbol at cursor position."""
        lines = text.split('\n')
        if position.line >= len(lines):
            return None
        
        line = lines[position.line]
        if position.character >= len(line):
            return None
        
        # Find word boundaries
        start = position.character
        end = position.character
        
        # Expand left
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        
        # Expand right
        while end < len(line) and (line[end].isalnum() or line[end] == '_'):
            end += 1
        
        symbol = line[start:end]
        return symbol if symbol else None
    
    def _find_definition(self, text: str, symbol: str, current_position) -> Tuple[Optional[int], Optional[int]]:
        """
        Find definition line and column.
        
        Searches the full file for the most appropriate definition of the symbol.
        For functions/classes/structs: first non-current-line match wins.
        For variables: closest definition at or before the cursor wins; falls back
        to first occurrence anywhere in the file.

        Returns:
            (line, column) or (None, None)
        """
        lines = text.split('\n')
        current_line = current_position.line if current_position else len(lines)

        # Look for function definition (whole file, skip current line)
        func_pattern = rf'\bfunction\s+{re.escape(symbol)}\b'
        for i, line in enumerate(lines):
            if current_position and i == current_line:
                continue
            match = re.search(func_pattern, line, re.IGNORECASE)
            if match:
                return (i, match.start() + len('function '))

        # Look for class definition
        class_pattern = rf'\bclass\s+{re.escape(symbol)}\b'
        for i, line in enumerate(lines):
            if current_position and i == current_line:
                continue
            match = re.search(class_pattern, line, re.IGNORECASE)
            if match:
                return (i, match.start() + len('class '))

        # Look for method definition
        method_pattern = rf'\b(?:method|function)\s+{re.escape(symbol)}\b'
        for i, line in enumerate(lines):
            if current_position and i == current_line:
                continue
            match = re.search(method_pattern, line, re.IGNORECASE)
            if match:
                prefix = line[match.start():match.end()]
                kw_match = re.match(r'\b(?:method|function)\s+', prefix, re.IGNORECASE)
                offset = kw_match.end() if kw_match else len('function ')
                return (i, match.start() + offset)

        # Look for variable assignment — prefer closest definition at or before
        # the cursor, but fall back to the first occurrence in the file.
        var_pattern = rf'\bset\s+{re.escape(symbol)}\s+to\b'
        best_before = (None, None)   # closest definition <= current_line
        first_any = (None, None)     # first definition anywhere

        for i, line in enumerate(lines):
            match = re.search(var_pattern, line, re.IGNORECASE)
            if match:
                col = match.start() + len('set ')
                if first_any[0] is None:
                    first_any = (i, col)
                if i <= current_line:
                    best_before = (i, col)

        if best_before[0] is not None:
            return best_before
        if first_any[0] is not None:
            return first_any

        return (None, None)

    def _find_current_method_symbol(self, method_symbols, uri: str, line: int):
        """Find the method symbol that best matches the current cursor line."""
        same_file = [s for s in method_symbols if s.file_uri == uri]
        if not same_file:
            return None

        exact = [s for s in same_file if s.line == line]
        if exact:
            return exact[0]

        # Fallback to nearest method symbol in file.
        return min(same_file, key=lambda s: abs(s.line - line))

    def _find_overridden_method(self, method_name: str, method_scope: str):
        """Find nearest base-class method overridden by the given method scope."""
        if not method_scope:
            return None

        class_name = method_scope.split('.')[0]
        hierarchy = self._build_class_hierarchy()

        index = getattr(self.server, 'workspace_index', None)
        if not index:
            return None

        queue = list(hierarchy.get(class_name, []))
        visited = set()

        while queue:
            parent = queue.pop(0)
            if parent in visited:
                continue
            visited.add(parent)

            candidates = [s for s in index.get_symbol(method_name) if s.kind == 'method' and s.scope and s.scope.split('.')[0] == parent]
            if candidates:
                return candidates[0]

            queue.extend(hierarchy.get(parent, []))

        return None

    def _build_class_hierarchy(self) -> Dict[str, List[str]]:
        """Build class -> direct parent class names from indexed workspace files."""
        if self._class_hierarchy_cache is not None:
            return self._class_hierarchy_cache

        hierarchy: Dict[str, List[str]] = {}
        index = getattr(self.server, 'workspace_index', None)
        if not index:
            self._class_hierarchy_cache = hierarchy
            return hierarchy

        for file_uri in index.indexed_files:
            file_path = index._uri_to_path(file_uri)
            try:
                with open(file_path, 'r', encoding='utf-8') as fh:
                    source = fh.read()
                lexer = Lexer(source)
                tokens = lexer.tokenize()
                parser = Parser(tokens)
                ast = parser.parse()
            except _RECOVERABLE_LSP_EXCEPTIONS:
                logger.debug("Skipping class hierarchy parse for %s", file_uri, exc_info=True)
                continue

            for stmt in getattr(ast, 'statements', []):
                if isinstance(stmt, ClassDefinition):
                    hierarchy[stmt.name] = list(getattr(stmt, 'parent_classes', []) or [])

        self._class_hierarchy_cache = hierarchy
        return hierarchy


__all__ = ['DefinitionProvider']
