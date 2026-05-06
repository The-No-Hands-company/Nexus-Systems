"""
Workspace Symbol Indexing
==========================

Provides workspace-wide symbol indexing for cross-file navigation, workspace symbols,
and fast symbol lookup. This is the foundation for features like:
- Cross-file go-to-definition
- Find all references across workspace
- Workspace symbol search
- Call hierarchy
- Document outline

Architecture:
- WorkspaceIndex: Main class that manages the global symbol table
- SymbolInfo: Dataclass representing a symbol (function, class, variable, etc.)
- Fast lookup via hash tables (O(1) by name, O(1) by file)
- Incremental updates (only re-index changed files)
- Background indexing support (for large workspaces)

Usage:
    index = WorkspaceIndex(workspace_root="/path/to/workspace")
    index.scan_workspace()  # Index all .nlpl files
    
    symbols = index.get_symbol("my_function")  # Find all symbols named "my_function"
    file_symbols = index.get_symbols_in_file("file:///path/to/file.nxl")
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set
from pathlib import Path
import os
import re
import logging

from ..parser.lexer import Lexer
from ..parser.parser import Parser
from ..parser.ast import (
    Program, FunctionDefinition, ClassDefinition, StructDefinition,
    VariableDeclaration, ImportStatement, ASTNode, MethodDefinition,
    SwitchStatement, WhileLoop, ForLoop
)

logger = logging.getLogger('nexuslang-lsp.workspace-index')


@dataclass
class SymbolInfo:
    """
    Information about a symbol in the workspace.
    
    Attributes:
        name: Symbol name (e.g., "greet", "MyClass", "PI")
        kind: Symbol kind - one of:
            - 'function': Top-level function
            - 'class': Class definition
            - 'method': Method inside a class
            - 'variable': Variable (module-level or local)
            - 'struct': Struct definition
            - 'parameter': Function/method parameter
            - 'field': Struct field or class attribute
        file_uri: File URI (e.g., "file:///path/to/file.nxl")
        line: 0-indexed line number where symbol is defined
        column: 0-indexed column number (start of symbol name)
        scope: Optional scope qualifier (e.g., "MyClass" for methods, "MyModule" for module-level)
        signature: Optional signature (for functions/methods: "with name as String returns String")
        doc: Optional documentation string (from comments)
        type_annotation: Optional type (for variables: "Integer", "List of String")
    """
    name: str
    kind: str
    file_uri: str
    line: int
    column: int
    scope: Optional[str] = None
    signature: Optional[str] = None
    doc: Optional[str] = None
    type_annotation: Optional[str] = None
    
    def __hash__(self):
        """Make SymbolInfo hashable for use in sets."""
        return hash((self.name, self.kind, self.file_uri, self.line, self.column))
    
    def __eq__(self, other):
        """Compare symbols for equality."""
        if not isinstance(other, SymbolInfo):
            return False
        return (self.name == other.name and 
                self.kind == other.kind and
                self.file_uri == other.file_uri and
                self.line == other.line)


class WorkspaceIndex:
    """
    Indexes all symbols in the workspace for fast lookup.
    
    Data structures:
    - symbols: Dict[str, List[SymbolInfo]] - Maps symbol names to their definitions
    - files: Dict[str, List[SymbolInfo]] - Maps file URIs to symbols defined in them
    - imports: Dict[str, List[str]] - Maps files to list of imported module names
    - indexed_files: Set[str] - Tracks which files have been indexed
    
    Performance:
    - Symbol lookup by name: O(1) average case
    - Symbols in file: O(1)
    - Indexing 100 files: <1 second (target)
    - Memory: ~1KB per symbol (10K symbols = ~10MB)
    """
    
    def __init__(self, workspace_root: str):
        """
        Initialize workspace index.
        
        Args:
            workspace_root: Absolute path to workspace root directory
        """
        self.workspace_root = workspace_root
        self.symbols: Dict[str, List[SymbolInfo]] = {}  # name -> [SymbolInfo, ...]
        self.files: Dict[str, List[SymbolInfo]] = {}    # file_uri -> [SymbolInfo, ...]
        self.imports: Dict[str, List[str]] = {}         # file_uri -> [module_name, ...]
        self.indexed_files: Set[str] = set()
        
        logger.info(f"Initialized WorkspaceIndex for: {workspace_root}")
    
    def scan_workspace(self, progress_callback=None) -> None:
        """
        Scan workspace for all .nlpl files and index their symbols.
        
        This recursively walks the workspace directory, finds all .nlpl files,
        and indexes symbols from each file. Skips hidden directories (.git, __pycache__).
        
        Args:
            progress_callback: Optional callback(current, total, file_path) for progress reporting
        """
        logger.info(f"Scanning workspace: {self.workspace_root}")
        
        # Find all .nlpl files
        nxl_files = self._find_nxl_files()
        total_files = len(nxl_files)
        
        logger.info(f"Found {total_files} .nlpl files")
        
        # Index each file
        for idx, file_path in enumerate(nxl_files, 1):
            try:
                file_uri = self._path_to_uri(file_path)
                self.index_file(file_uri, file_path)
                
                if progress_callback:
                    progress_callback(idx, total_files, file_path)
                    
            except Exception as e:
                logger.error(f"Failed to index {file_path}: {e}")
                # Continue with other files even if one fails
        
        logger.info(f"Workspace indexing complete: {len(self.indexed_files)} files, {sum(len(syms) for syms in self.symbols.values())} symbols")
    
    def index_file(self, file_uri: str, file_path: Optional[str] = None) -> List[SymbolInfo]:
        """
        Parse a single file and extract all symbols.
        
        This method:
        1. Reads file content (or gets from cache)
        2. Parses to AST
        3. Walks AST to extract symbols (functions, classes, variables, etc.)
        4. Stores symbols in global table and per-file table
        5. Extracts imports
        
        Args:
            file_uri: File URI (e.g., "file:///path/to/file.nxl")
            file_path: Optional file system path (computed from URI if not provided)
            
        Returns:
            List of SymbolInfo objects extracted from this file
        """
        if file_path is None:
            file_path = self._uri_to_path(file_uri)
        
        logger.debug(f"Indexing file: {file_uri}")
        
        # Clear old symbols from this file
        self._clear_file_symbols(file_uri)
        
        try:
            # Read file content
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Parse to AST
            lexer = Lexer(content)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Extract symbols
            symbols = self._extract_symbols_from_ast(ast, file_uri)
            
            # Store symbols
            for symbol in symbols:
                # Add to global symbol table (by name)
                if symbol.name not in self.symbols:
                    self.symbols[symbol.name] = []
                self.symbols[symbol.name].append(symbol)
                
                # Add to per-file table
                if file_uri not in self.files:
                    self.files[file_uri] = []
                self.files[file_uri].append(symbol)

            # Post-process: resolve column=0 placeholders to real columns
            source_lines = content.split('\n')
            for symbol in symbols:
                if symbol.column == 0:
                    symbol.column = self._find_symbol_column(source_lines, symbol.line, symbol.name)
            
            # Mark as indexed
            self.indexed_files.add(file_uri)
            
            logger.debug(f"Indexed {len(symbols)} symbols from {file_uri}")
            return symbols
            
        except Exception as e:
            logger.error(f"Failed to index {file_uri}: {e}", exc_info=True)
            return []
    
    def _extract_function_symbol(self, stmt: FunctionDefinition, file_uri: str, scope: Optional[str]) -> List[SymbolInfo]:
        """Return SymbolInfo list for a FunctionDefinition, including its parameters."""
        symbols = []
        kind = 'method' if scope else 'function'
        signature = self._build_function_signature(stmt)
        symbol = SymbolInfo(
            name=stmt.name,
            kind=kind,
            file_uri=file_uri,
            line=stmt.line_number - 1 if stmt.line_number else 0,
            column=0,
            scope=scope,
            signature=signature,
            type_annotation=str(stmt.return_type) if stmt.return_type else None
        )
        symbols.append(symbol)
        for param in stmt.parameters:
            param_symbol = SymbolInfo(
                name=param.name,
                kind='parameter',
                file_uri=file_uri,
                line=stmt.line_number - 1 if stmt.line_number else 0,
                column=0,
                scope=f"{scope}.{stmt.name}" if scope else stmt.name,
                type_annotation=str(param.type_annotation) if hasattr(param, 'type_annotation') and param.type_annotation else None
            )
            symbols.append(param_symbol)

        function_scope = f"{scope}.{stmt.name}" if scope else stmt.name
        symbols.extend(self._extract_control_flow_symbols(stmt.body, file_uri, function_scope))
        return symbols

    def _extract_control_flow_symbols(self, statements: List[ASTNode], file_uri: str, scope: str) -> List[SymbolInfo]:
        """Extract labeled-loop and switch/case outline symbols from statement lists."""
        symbols: List[SymbolInfo] = []
        if not statements:
            return symbols

        for stmt in statements:
            if isinstance(stmt, WhileLoop) and getattr(stmt, 'label', None):
                line = stmt.line_number - 1 if stmt.line_number else 0
                symbols.append(SymbolInfo(
                    name=stmt.label,
                    kind='label',
                    file_uri=file_uri,
                    line=line,
                    column=0,
                    scope=scope,
                    signature='loop label'
                ))
                symbols.extend(self._extract_control_flow_symbols(getattr(stmt, 'body', []), file_uri, scope))
                symbols.extend(self._extract_control_flow_symbols(getattr(stmt, 'else_body', []), file_uri, scope))
                continue

            if isinstance(stmt, ForLoop) and getattr(stmt, 'label', None):
                line = stmt.line_number - 1 if stmt.line_number else 0
                symbols.append(SymbolInfo(
                    name=stmt.label,
                    kind='label',
                    file_uri=file_uri,
                    line=line,
                    column=0,
                    scope=scope,
                    signature='loop label'
                ))
                symbols.extend(self._extract_control_flow_symbols(getattr(stmt, 'body', []), file_uri, scope))
                symbols.extend(self._extract_control_flow_symbols(getattr(stmt, 'else_body', []), file_uri, scope))
                continue

            if isinstance(stmt, SwitchStatement):
                switch_line = stmt.line_number - 1 if stmt.line_number else 0
                symbols.append(SymbolInfo(
                    name='switch',
                    kind='switch',
                    file_uri=file_uri,
                    line=switch_line,
                    column=0,
                    scope=scope,
                    signature='switch statement'
                ))

                for case_stmt in getattr(stmt, 'cases', []):
                    case_line = case_stmt.line_number - 1 if case_stmt.line_number else switch_line
                    case_name = 'case'
                    if hasattr(case_stmt, 'value') and case_stmt.value is not None:
                        case_value = getattr(case_stmt.value, 'value', None)
                        if case_value is not None:
                            case_name = f'case {case_value}'
                    symbols.append(SymbolInfo(
                        name=case_name,
                        kind='case',
                        file_uri=file_uri,
                        line=case_line,
                        column=0,
                        scope=scope,
                        signature='switch case'
                    ))
                    symbols.extend(self._extract_control_flow_symbols(getattr(case_stmt, 'body', []), file_uri, scope))

                if getattr(stmt, 'default_case', None):
                    symbols.append(SymbolInfo(
                        name='default',
                        kind='case',
                        file_uri=file_uri,
                        line=switch_line,
                        column=0,
                        scope=scope,
                        signature='switch default'
                    ))
                    symbols.extend(self._extract_control_flow_symbols(getattr(stmt, 'default_case', []), file_uri, scope))
                continue

            # Descend into common nested blocks where statements can appear.
            nested_blocks = []
            if hasattr(stmt, 'body') and isinstance(getattr(stmt, 'body', None), list):
                nested_blocks.append(stmt.body)
            if hasattr(stmt, 'then_block') and isinstance(getattr(stmt, 'then_block', None), list):
                nested_blocks.append(stmt.then_block)
            if hasattr(stmt, 'else_block') and isinstance(getattr(stmt, 'else_block', None), list):
                nested_blocks.append(stmt.else_block)
            if hasattr(stmt, 'try_block') and isinstance(getattr(stmt, 'try_block', None), list):
                nested_blocks.append(stmt.try_block)
            if hasattr(stmt, 'catch_block') and isinstance(getattr(stmt, 'catch_block', None), list):
                nested_blocks.append(stmt.catch_block)

            for block in nested_blocks:
                symbols.extend(self._extract_control_flow_symbols(block, file_uri, scope))

        return symbols

    def _extract_class_symbol(self, stmt: ClassDefinition, file_uri: str, scope: Optional[str]) -> List[SymbolInfo]:
        """Return SymbolInfo list for a ClassDefinition, including methods and attributes."""
        symbols = []
        class_line = self._line_from_node(stmt)
        symbol = SymbolInfo(
            name=stmt.name,
            kind='class',
            file_uri=file_uri,
            line=class_line,
            column=0,
            scope=scope
        )
        symbols.append(symbol)
        class_scope = f"{scope}.{stmt.name}" if scope else stmt.name
        if hasattr(stmt, 'methods'):
            for method in stmt.methods:
                if isinstance(method, (FunctionDefinition, MethodDefinition)):
                    method_signature = self._build_function_signature(method)
                    method_symbol = SymbolInfo(
                        name=method.name,
                        kind='method',
                        file_uri=file_uri,
                        line=self._line_from_node(method, fallback=class_line),
                        column=0,
                        scope=class_scope,
                        signature=method_signature,
                        type_annotation=str(method.return_type) if method.return_type else None
                    )
                    symbols.append(method_symbol)
                    for param in method.parameters:
                        param_symbol = SymbolInfo(
                            name=param.name,
                            kind='parameter',
                            file_uri=file_uri,
                            line=self._line_from_node(param, fallback=self._line_from_node(method, fallback=class_line)),
                            column=0,
                            scope=f"{class_scope}.{method.name}",
                            type_annotation=str(param.type_annotation) if hasattr(param, 'type_annotation') and param.type_annotation else None
                        )
                        symbols.append(param_symbol)
                    method_scope = f"{class_scope}.{method.name}"
                    symbols.extend(self._extract_control_flow_symbols(getattr(method, 'body', []), file_uri, method_scope))
        # Class fields can appear as PropertyDeclaration (current AST) and
        # VariableDeclaration (legacy/compat parse shapes).
        class_fields = []
        if hasattr(stmt, 'properties') and isinstance(stmt.properties, list):
            class_fields.extend(stmt.properties)
        legacy_attributes = getattr(stmt, 'attributes', None)
        if isinstance(legacy_attributes, list):
            class_fields.extend(legacy_attributes)

        for field in class_fields:
            if not hasattr(field, 'name'):
                continue
            field_symbol = SymbolInfo(
                name=field.name,
                kind='field',
                file_uri=file_uri,
                line=self._line_from_node(field, fallback=class_line),
                column=0,
                scope=class_scope,
                type_annotation=str(getattr(field, 'type_annotation', None)) if getattr(field, 'type_annotation', None) else None
            )
            symbols.append(field_symbol)
        return symbols

    def _extract_symbols_from_ast(self, ast: Program, file_uri: str, scope: Optional[str] = None) -> List[SymbolInfo]:
        """
        Walk AST and extract all symbols (functions, classes, variables, etc.).

        This is the core symbol extraction logic. It recursively walks the AST
        and creates SymbolInfo objects for each symbol definition found.

        Args:
            ast: Program AST node
            file_uri: File URI
            scope: Current scope (e.g., class name for methods)

        Returns:
            List of SymbolInfo objects
        """
        symbols = []

        if not hasattr(ast, 'statements'):
            return symbols

        for stmt in ast.statements:
            if isinstance(stmt, FunctionDefinition):
                symbols.extend(self._extract_function_symbol(stmt, file_uri, scope))

            elif isinstance(stmt, ClassDefinition):
                symbols.extend(self._extract_class_symbol(stmt, file_uri, scope))

            elif isinstance(stmt, StructDefinition):
                struct_line = self._line_from_node(stmt)
                symbol = SymbolInfo(
                    name=stmt.name,
                    kind='struct',
                    file_uri=file_uri,
                    line=struct_line,
                    column=0,
                    scope=scope
                )
                symbols.append(symbol)
                if hasattr(stmt, 'fields'):
                    struct_scope = f"{scope}.{stmt.name}" if scope else stmt.name
                    for field in stmt.fields:
                        field_symbol = SymbolInfo(
                            name=field.name,
                            kind='field',
                            file_uri=file_uri,
                            line=self._line_from_node(field, fallback=struct_line),
                            column=0,
                            scope=struct_scope,
                            type_annotation=str(field.type_annotation) if hasattr(field, 'type_annotation') else None
                        )
                        symbols.append(field_symbol)

            elif isinstance(stmt, VariableDeclaration) and not scope:
                symbol = SymbolInfo(
                    name=stmt.name,
                    kind='variable',
                    file_uri=file_uri,
                    line=self._line_from_node(stmt),
                    column=0,
                    scope=scope,
                    type_annotation=str(stmt.type_annotation) if stmt.type_annotation else None
                )
                symbols.append(symbol)

            elif isinstance(stmt, ImportStatement):
                if file_uri not in self.imports:
                    self.imports[file_uri] = []
                self.imports[file_uri].append(stmt.module_name)

        return symbols
    
    def _build_function_signature(self, func: FunctionDefinition) -> str:
        """
        Build human-readable function signature.
        
        Example: "with name as String and age as Integer returns String"
        """
        parts = []
        
        if func.parameters:
            param_parts = []
            for param in func.parameters:
                param_str = param.name
                if hasattr(param, 'type_annotation') and param.type_annotation:
                    param_str += f" as {param.type_annotation}"
                param_parts.append(param_str)
            parts.append("with " + " and ".join(param_parts))
        
        if func.return_type:
            parts.append(f"returns {func.return_type}")
        
        return " ".join(parts) if parts else ""
    
    def get_symbol(self, name: str) -> List[SymbolInfo]:
        """
        Get all symbols with given name across workspace.
        
        Args:
            name: Symbol name to search for
            
        Returns:
            List of matching SymbolInfo objects (may be from different files)
        """
        return self.symbols.get(name, [])
    
    def get_symbols_in_file(self, file_uri: str) -> List[SymbolInfo]:
        """
        Get all symbols defined in a specific file.
        
        Args:
            file_uri: File URI
            
        Returns:
            List of SymbolInfo objects from this file
        """
        return self.files.get(file_uri, [])
    
    def get_imports(self, file_uri: str) -> List[str]:
        """
        Get list of module names imported by a file.
        
        Args:
            file_uri: File URI
            
        Returns:
            List of imported module names
        """
        return self.imports.get(file_uri, [])
    
    def find_symbols(self, query: str, kind: Optional[str] = None) -> List[SymbolInfo]:
        """
        Search for symbols matching a query (fuzzy search).
        
        Args:
            query: Search query (substring or fuzzy match)
            kind: Optional filter by symbol kind ('function', 'class', etc.)
            
        Returns:
            List of matching SymbolInfo objects, sorted by relevance
        """
        results = []
        query_lower = query.lower()
        
        for name, symbol_list in self.symbols.items():
            # Check if name matches query (case-insensitive substring)
            if query_lower in name.lower():
                for symbol in symbol_list:
                    # Apply kind filter if specified
                    if kind is None or symbol.kind == kind:
                        results.append(symbol)
        
        # Sort by name similarity (exact match first, then by name length)
        results.sort(key=lambda s: (
            s.name.lower() != query_lower,  # Exact matches first
            len(s.name),                     # Shorter names first
            s.name.lower()                   # Alphabetical
        ))
        
        return results
    
    def _clear_file_symbols(self, file_uri: str) -> None:
        """
        Remove all symbols associated with a file (for re-indexing).
        
        Args:
            file_uri: File URI to clear
        """
        # Get symbols from this file
        old_symbols = self.files.get(file_uri, [])
        
        # Remove from global symbol table
        for symbol in old_symbols:
            if symbol.name in self.symbols:
                self.symbols[symbol.name] = [
                    s for s in self.symbols[symbol.name] 
                    if s.file_uri != file_uri
                ]
                # Remove empty entries
                if not self.symbols[symbol.name]:
                    del self.symbols[symbol.name]
        
        # Clear per-file table
        if file_uri in self.files:
            del self.files[file_uri]
        
        # Clear imports
        if file_uri in self.imports:
            del self.imports[file_uri]
        
        # Remove from indexed set
        self.indexed_files.discard(file_uri)
    
    def _find_symbol_column(self, lines: List[str], line_idx: int, symbol_name: str) -> int:
        """
        Scan a source line to find the 0-indexed column of a symbol name.

        Uses a whole-word regex so keywords and partial matches are skipped.
        Returns 0 if the symbol cannot be found on the given line.
        """
        if line_idx >= len(lines):
            return 0
        match = re.search(r'\b' + re.escape(symbol_name) + r'\b', lines[line_idx])
        return match.start() if match else 0

    def _line_from_node(self, node, fallback: int = 0) -> int:
        """Return 0-indexed line from AST node metadata, with fallback."""
        line_number = getattr(node, 'line_number', None)
        if isinstance(line_number, int) and line_number > 0:
            return line_number - 1
        return fallback

    def _find_nxl_files(self) -> List[str]:
        """
        Recursively find all .nlpl files in workspace.
        
        Skips:
        - Hidden directories (.git, .vscode, etc.)
        - __pycache__ directories
        - build/ output directories
        
        Returns:
            List of absolute file paths
        """
        nxl_files = []
        skip_dirs = {'.git', '.vscode', '__pycache__', 'node_modules', '.mypy_cache', 'build', 'dist'}
        
        for root, dirs, files in os.walk(self.workspace_root):
            # Remove skip directories from traversal
            dirs[:] = [d for d in dirs if d not in skip_dirs and not d.startswith('.')]
            
            # Find .nlpl files
            for file in files:
                if file.endswith('.nxl'):
                    file_path = os.path.join(root, file)
                    nxl_files.append(file_path)
        
        return nxl_files
    
    def _path_to_uri(self, file_path: str) -> str:
        """
        Convert file system path to URI.
        
        Args:
            file_path: Absolute file system path
            
        Returns:
            File URI (e.g., "file:///path/to/file.nxl")
        """
        # Ensure absolute path
        abs_path = os.path.abspath(file_path)
        # Convert to URI (file:// prefix)
        return Path(abs_path).as_uri()
    
    def _uri_to_path(self, file_uri: str) -> str:
        """
        Convert file URI to file system path.
        
        Args:
            file_uri: File URI
            
        Returns:
            Absolute file system path
        """
        # Remove file:// prefix and convert to path
        if file_uri.startswith('file://'):
            return file_uri[7:]  # Remove 'file://'
        return file_uri
    
    def get_statistics(self) -> Dict[str, int]:
        """
        Get indexing statistics for debugging/reporting.
        
        Returns:
            Dictionary with statistics (files indexed, total symbols, etc.)
        """
        return {
            'files_indexed': len(self.indexed_files),
            'total_symbols': sum(len(syms) for syms in self.symbols.values()),
            'unique_names': len(self.symbols),
            'functions': sum(1 for syms in self.symbols.values() for s in syms if s.kind == 'function'),
            'classes': sum(1 for syms in self.symbols.values() for s in syms if s.kind == 'class'),
            'methods': sum(1 for syms in self.symbols.values() for s in syms if s.kind == 'method'),
            'variables': sum(1 for syms in self.symbols.values() for s in syms if s.kind == 'variable'),
            'structs': sum(1 for syms in self.symbols.values() for s in syms if s.kind == 'struct'),
        }
