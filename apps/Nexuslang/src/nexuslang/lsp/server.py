"""
NLPL Language Server
====================

Main LSP server implementation handling client-server communication.
"""

import json
import sys
from typing import Dict, List, Optional, Any
from dataclasses import dataclass
import logging

from ..tooling.analyzer.analyzer import create_default_analyzer, create_strict_analyzer

# Configure logging
logging.basicConfig(
    filename='/tmp/nexuslang-lsp.log',
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('nexuslang-lsp')


@dataclass
class Position:
    """Position in a text document (line, character)."""
    line: int
    character: int
    
    def to_dict(self):
        return {"line": self.line, "character": self.character}


@dataclass
class Range:
    """Range in a text document."""
    start: Position
    end: Position
    
    def to_dict(self):
        return {"start": self.start.to_dict(), "end": self.end.to_dict()}


@dataclass
class Location:
    """Location in a document."""
    uri: str
    range: Range
    
    def to_dict(self):
        return {"uri": self.uri, "range": self.range.to_dict()}


@dataclass
class TextDocumentIdentifier:
    """Text document identifier."""
    uri: str


@dataclass
class TextDocumentPosition:
    """Position in a text document."""
    text_document: TextDocumentIdentifier
    position: Position


class NexusLangLanguageServer:
    """
    NexusLang Language Server implementing LSP protocol.
    
    Capabilities:
    - textDocument/completion
    - textDocument/definition
    - textDocument/hover
    - textDocument/publishDiagnostics
    - textDocument/rename
    - textDocument/formatting
    - workspace/symbol
    """
    
    def __init__(self):
        self.documents: Dict[str, str] = {}  # URI -> content
        self.initialization_options = {}
        self.workspace_index = None  # Will be initialized after workspace root is known
        self._parse_cache: Dict[str, tuple] = {}  # uri -> (text_hash, ast)
        
        # Import providers
        from ..lsp.completions import CompletionProvider
        from ..lsp.definitions import DefinitionProvider
        from ..lsp.hover import HoverProvider
        from ..lsp.diagnostics import DiagnosticsProvider
        from ..lsp.symbols import SymbolProvider
        from ..lsp.formatter import NexusLangFormatter
        from ..lsp.code_actions import CodeActionsProvider
        from ..lsp.signature_help import SignatureHelpProvider
        from ..lsp.references import ReferencesProvider
        from ..lsp.rename import RenameProvider
        from ..lsp.semantic_tokens import SemanticTokensProvider
        from ..lsp.code_lens import CodeLensProvider
        from ..lsp.inlay_hints import InlayHintsProvider
        from ..lsp.dead_code import DeadCodeProvider
        
        self.completion_provider = CompletionProvider(self)
        self.definition_provider = DefinitionProvider(self)
        self.hover_provider = HoverProvider(self)
        self.diagnostics_provider = DiagnosticsProvider(self)
        self.symbol_provider = SymbolProvider(self)
        self.formatter = NexusLangFormatter()
        self.code_actions_provider = CodeActionsProvider(self)
        self.references_provider = ReferencesProvider(self)
        self.signature_help_provider = SignatureHelpProvider(self)
        self.rename_provider = RenameProvider(self)
        self.semantic_tokens_provider = SemanticTokensProvider(self)
        self.code_lens_provider = CodeLensProvider(self)
        self.inlay_hints_provider = InlayHintsProvider(self)
        self.dead_code_provider = DeadCodeProvider(self)
        
        logger.info("NexusLang Language Server initialized")
    
    def start(self):
        """Start the language server (stdio communication)."""
        logger.info("Starting NexusLang Language Server")
        
        while True:
            try:
                message = self._read_message()
                if message is None:
                    break
                
                response = self._handle_message(message)
                if response:
                    self._write_message(response)
                    
            except Exception as e:
                logger.error(f"Error handling message: {e}", exc_info=True)
    
    def _read_message(self) -> Optional[Dict]:
        """Read a JSON-RPC message from stdin."""
        try:
            # Read headers
            headers = {}
            while True:
                raw = sys.stdin.buffer.readline()
                if not raw:  # EOF
                    return None
                line = raw.decode('utf-8')
                if line in ('\r\n', '\n', ''):
                    break
                if ': ' not in line:
                    continue  # skip malformed header lines
                key, value = line.strip().split(': ', 1)
                headers[key] = value
            
            # Read content
            content_length = int(headers.get('Content-Length', 0))
            if content_length == 0:
                return None
            
            content = sys.stdin.buffer.read(content_length).decode('utf-8')
            return json.loads(content)
            
        except Exception as e:
            logger.error(f"Error reading message: {e}")
            return None
    
    def _write_message(self, message: Dict):
        """Write a JSON-RPC message to stdout."""
        try:
            content = json.dumps(message)
            content_bytes = content.encode('utf-8')
            
            response = (
                f"Content-Length: {len(content_bytes)}\r\n"
                f"\r\n"
                f"{content}"
            )
            
            sys.stdout.buffer.write(response.encode('utf-8'))
            sys.stdout.buffer.flush()
            
        except Exception as e:
            logger.error(f"Error writing message: {e}")
    
    def _handle_message(self, message: Dict) -> Optional[Dict]:
        """Handle incoming LSP message."""
        method = message.get('method')
        msg_id = message.get('id')
        params = message.get('params', {})
        
        logger.debug(f"Handling method: {method}")
        
        # Handle requests
        if method == 'initialize':
            return self._handle_initialize(msg_id, params)
        
        elif method == 'initialized':
            logger.info("Client initialized")
            return None
        
        elif method == 'textDocument/didOpen':
            self._handle_did_open(params)
            return None
        
        elif method == 'textDocument/didChange':
            self._handle_did_change(params)
            return None
        
        elif method == 'textDocument/didClose':
            self._handle_did_close(params)
            return None
        
        elif method == 'textDocument/completion':
            return self._handle_completion(msg_id, params)
        
        elif method == 'textDocument/definition':
            return self._handle_definition(msg_id, params)
        
        elif method == 'textDocument/hover':
            return self._handle_hover(msg_id, params)
        
        elif method == 'textDocument/references':
            return self._handle_references(msg_id, params)
        
        elif method == 'textDocument/prepareRename':
            return self._handle_prepare_rename(msg_id, params)
        
        elif method == 'textDocument/rename':
            return self._handle_rename(msg_id, params)
        
        elif method == 'textDocument/codeAction':
            return self._handle_code_action(msg_id, params)
        
        elif method == 'textDocument/signatureHelp':
            return self._handle_signature_help(msg_id, params)
        
        elif method == 'textDocument/formatting':
            return self._handle_formatting(msg_id, params)
        
        elif method == 'workspace/symbol':
            return self._handle_workspace_symbol(msg_id, params)
        
        elif method == 'textDocument/documentSymbol':
            return self._handle_document_symbol(msg_id, params)
        
        elif method == 'textDocument/prepareCallHierarchy':
            return self._handle_prepare_call_hierarchy(msg_id, params)
        
        elif method == 'callHierarchy/incomingCalls':
            return self._handle_incoming_calls(msg_id, params)
        
        elif method == 'callHierarchy/outgoingCalls':
            return self._handle_outgoing_calls(msg_id, params)
        
        elif method == 'textDocument/semanticTokens/full':
            return self._handle_semantic_tokens_full(msg_id, params)
        
        elif method == 'textDocument/codeLens':
            return self._handle_code_lens(msg_id, params)
        
        elif method == 'codeLens/resolve':
            return self._handle_code_lens_resolve(msg_id, params)
        
        elif method == 'textDocument/inlayHint':
            return self._handle_inlay_hint(msg_id, params)
        
        elif method == 'shutdown':
            logger.info("Shutdown requested")
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
        elif method == 'exit':
            logger.info("Exit requested")
            sys.exit(0)
        
        else:
            logger.warning(f"Unhandled method: {method}")
            return None
    
    def _handle_initialize(self, msg_id: int, params: Dict) -> Dict:
        """Handle initialize request."""
        self.initialization_options = params.get('initializationOptions', {})
        
        # Extract workspace root and initialize workspace index
        workspace_folders = params.get('workspaceFolders', [])
        if workspace_folders:
            workspace_root = workspace_folders[0]['uri'].replace('file://', '')
            logger.info(f"Initializing workspace index for: {workspace_root}")
            
            from ..lsp.workspace_index import WorkspaceIndex
            self.workspace_index = WorkspaceIndex(workspace_root)
            
            # Index workspace in background (don't block initialization)
            import threading
            def index_workspace():
                try:
                    logger.info("Starting workspace indexing...")
                    self.workspace_index.scan_workspace()
                    stats = self.workspace_index.get_statistics()
                    logger.info(f"Workspace indexed: {stats['files_indexed']} files, {stats['total_symbols']} symbols")
                except Exception as e:
                    logger.error(f"Error indexing workspace: {e}", exc_info=True)
            
            threading.Thread(target=index_workspace, daemon=True).start()
        else:
            # Try rootPath or rootUri as fallback
            workspace_root = params.get('rootPath') or (params.get('rootUri') or '').replace('file://', '')
            if workspace_root:
                logger.info(f"Initializing workspace index for: {workspace_root}")
                from ..lsp.workspace_index import WorkspaceIndex
                self.workspace_index = WorkspaceIndex(workspace_root)
                
                import threading
                def index_workspace():
                    try:
                        logger.info("Starting workspace indexing...")
                        self.workspace_index.scan_workspace()
                        stats = self.workspace_index.get_statistics()
                        logger.info(f"Workspace indexed: {stats['files_indexed']} files, {stats['total_symbols']} symbols")
                    except Exception as e:
                        logger.error(f"Error indexing workspace: {e}", exc_info=True)
                
                threading.Thread(target=index_workspace, daemon=True).start()
        
        capabilities = {
            "textDocumentSync": {
                "openClose": True,
                "change": 1,  # Full sync
                "save": {"includeText": True}
            },
            "completionProvider": {
                "resolveProvider": False,
                "triggerCharacters": [" ", "."]
            },
            "signatureHelpProvider": {
                "triggerCharacters": ["(", ",", " "],
                "retriggerCharacters": [","]
            },
            "codeActionProvider": {
                "codeActionKinds": [
                    "quickfix",
                    "refactor",
                    "refactor.extract",
                    "refactor.rewrite"
                ]
            },
            "definitionProvider": True,
            "hoverProvider": True,
            "referencesProvider": True,
            "documentFormattingProvider": True,
            "workspaceSymbolProvider": True,
            "documentSymbolProvider": True,
            "callHierarchyProvider": True,
            "renameProvider": {
                "prepareProvider": True
            },
            "semanticTokensProvider": {
                "legend": self.semantic_tokens_provider.get_semantic_tokens_legend(),
                "full": True
            },
            "codeLensProvider": {
                "resolveProvider": True
            },
            "inlayHintProvider": True
        }
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "capabilities": capabilities,
                "serverInfo": {
                    "name": "NexusLang Language Server",
                    "version": "0.1.0"
                }
            }
        }
    
    def _handle_did_open(self, params: Dict):
        """Handle textDocument/didOpen notification."""
        text_document = params['textDocument']
        uri = text_document['uri']
        text = text_document['text']
        
        self.documents[uri] = text
        logger.info(f"Opened document: {uri}")
        
        # Send diagnostics (syntax + type errors merged with dead-code warnings)
        diagnostics = self.diagnostics_provider.get_diagnostics(uri, text)
        lint_diagnostics = self._get_realtime_lint_diagnostics(uri, text)
        try:
            diagnostics = self.diagnostics_provider.merge_and_dedupe_diagnostics(
                diagnostics,
                self.dead_code_provider.get_diagnostics(uri, text),
                lint_diagnostics,
            )
        except Exception as e:
            logger.error(f"Dead code analysis error for {uri}: {e}", exc_info=True)
        self._publish_diagnostics(uri, diagnostics)
    
    def _handle_did_change(self, params: Dict):
        """Handle textDocument/didChange notification."""
        text_document = params['textDocument']
        uri = text_document['uri']
        changes = params['contentChanges']
        
        # Full sync - take the full text
        if changes:
            self.documents[uri] = changes[0]['text']
            
            # Re-index this file for workspace index
            if self.workspace_index and uri.endswith('.nxl'):
                try:
                    self.workspace_index.index_file(uri)
                    logger.debug(f"Re-indexed file: {uri}")
                except Exception as e:
                    logger.error(f"Error re-indexing file {uri}: {e}")
            
            # Send diagnostics (syntax + dead-code warnings)
            diagnostics = self.diagnostics_provider.get_diagnostics(uri, changes[0]['text'])
            lint_diagnostics = self._get_realtime_lint_diagnostics(uri, changes[0]['text'])
            try:
                diagnostics = self.diagnostics_provider.merge_and_dedupe_diagnostics(
                    diagnostics,
                    self.dead_code_provider.get_diagnostics(uri, changes[0]['text']),
                    lint_diagnostics,
                )
            except Exception as e:
                logger.error(f"Dead code analysis error for {uri}: {e}", exc_info=True)
            self._publish_diagnostics(uri, diagnostics)

    def _get_realtime_lint_diagnostics(self, uri: str, text: str) -> List[Dict]:
        """Run static analyzer checks for editor-time lint diagnostics (opt-in)."""
        lint_opts = self.initialization_options.get("linting", {}) or {}
        if not lint_opts.get("enabled", False):
            return []

        strict = bool(lint_opts.get("strict", False))
        errors_only = bool(lint_opts.get("errorsOnly", False))

        ast = self.get_or_parse(uri, text)
        if ast is None:
            # Parser diagnostics are already provided by diagnostics provider.
            return []

        analyzer = create_strict_analyzer() if strict else create_default_analyzer()
        lines = text.split('\n')
        diagnostics: List[Dict] = []

        severity_map = {
            "error": 1,
            "warning": 2,
            "info": 3,
            "hint": 4,
        }

        for checker in analyzer.checkers:
            try:
                issues = checker.check(ast, text, lines)
            except Exception as exc:
                logger.error(
                    "Realtime lint checker '%s' failed for %s: %s",
                    checker.__class__.__name__,
                    uri,
                    exc,
                    exc_info=True,
                )
                continue

            for issue in issues:
                severity_name = getattr(issue.severity, "value", str(issue.severity)).lower()
                if errors_only and severity_name != "error":
                    continue

                location = getattr(issue, "location", None)
                line = max(0, (getattr(location, "line", 1) or 1) - 1)
                col = max(0, (getattr(location, "column", 1) or 1) - 1)

                diagnostics.append(
                    {
                        "range": {
                            "start": {"line": line, "character": col},
                            "end": {"line": line, "character": col + 1},
                        },
                        "severity": severity_map.get(severity_name, 3),
                        "message": f"[{issue.code}] {issue.message}",
                        "source": "nlpl-lint",
                        "data": {
                            "origin": "nexuslang-lint",
                            "lintCode": issue.code,
                            "category": getattr(getattr(issue, "category", None), "value", None),
                        },
                    }
                )

        return diagnostics
    
    def _handle_did_close(self, params: Dict):
        """Handle textDocument/didClose notification."""
        uri = params['textDocument']['uri']
        if uri in self.documents:
            del self.documents[uri]
        self.invalidate_parse_cache(uri)
        logger.info(f"Closed document: {uri}")
    
    def _handle_completion(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/completion request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        
        text = self.documents.get(uri, '')
        completions = self.completion_provider.get_completions(text, position)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {"items": completions}
        }
    
    def _handle_definition(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/definition request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        
        text = self.documents.get(uri, '')
        location = self.definition_provider.get_definition(text, position, uri)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": location.to_dict() if location else None
        }
    
    def _handle_hover(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/hover request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        
        text = self.documents.get(uri, '')
        hover_info = self.hover_provider.get_hover(text, position)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": hover_info
        }
    
    def _handle_references(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/references request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        context = params.get('context', {})
        include_declaration = context.get('includeDeclaration', True)
        
        text = self.documents.get(uri, '')
        references = self.references_provider.find_references(
            text, 
            position, 
            uri, 
            include_declaration
        )
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": references
        }
    
    def _handle_prepare_rename(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/prepareRename request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        
        text = self.documents.get(uri, '')
        prepare_result = self.rename_provider.prepare_rename(text, position, uri)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": prepare_result
        }
    
    def _handle_rename(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/rename request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        new_name = params['newName']
        
        text = self.documents.get(uri, '')
        workspace_edit = self.rename_provider.rename(text, position, uri, new_name)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": workspace_edit
        }
    
    def _handle_code_action(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/codeAction request."""
        uri = params['textDocument']['uri']
        range_params = params['range']
        context = params.get('context', {})
        diagnostics = context.get('diagnostics', [])
        
        text = self.documents.get(uri, '')
        actions = self.code_actions_provider.get_code_actions(uri, text, range_params, diagnostics)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": actions
        }
    
    def _handle_signature_help(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/signatureHelp request."""
        uri = params['textDocument']['uri']
        position = Position(
            params['position']['line'],
            params['position']['character']
        )
        
        text = self.documents.get(uri, '')
        signature_help = self.signature_help_provider.get_signature_help(text, position)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": signature_help
        }
    
    def _handle_formatting(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/formatting request."""
        uri = params['textDocument']['uri']
        text = self.documents.get(uri, '')
        
        # Get formatting edits from the formatter
        edits = self.formatter.get_formatting_edits(text)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": edits
        }
    
    def _handle_workspace_symbol(self, msg_id: int, params: Dict) -> Dict:
        """Handle workspace/symbol request."""
        query = params.get('query', '')
        symbols = self.symbol_provider.find_symbols(query, self.documents)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": symbols
        }
    
    def _handle_semantic_tokens_full(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/semanticTokens/full request."""
        uri = params['textDocument']['uri']
        text = self.documents.get(uri, '')
        
        # Get semantic tokens
        tokens = self.semantic_tokens_provider.get_semantic_tokens(text, uri)
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": {
                "data": tokens
            }
        }
    
    # ------------------------------------------------------------------
    # Code Lens handlers
    # ------------------------------------------------------------------

    def _handle_code_lens(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/codeLens request."""
        uri = params['textDocument']['uri']
        text = self.documents.get(uri, '')
        try:
            lenses = self.code_lens_provider.get_code_lenses(uri, text)
        except Exception as e:
            logger.error(f"Error getting code lenses for {uri}: {e}", exc_info=True)
            lenses = []
        return {"jsonrpc": "2.0", "id": msg_id, "result": lenses}

    def _handle_code_lens_resolve(self, msg_id: int, params: Dict) -> Dict:
        """Handle codeLens/resolve request."""
        try:
            lens = self.code_lens_provider.resolve_code_lens(params)
        except Exception as e:
            logger.error(f"Error resolving code lens: {e}", exc_info=True)
            lens = params
        return {"jsonrpc": "2.0", "id": msg_id, "result": lens}

    # ------------------------------------------------------------------
    # Inlay Hints handler
    # ------------------------------------------------------------------

    def _handle_inlay_hint(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/inlayHint request."""
        uri = params['textDocument']['uri']
        text = self.documents.get(uri, '')
        range_ = params.get('range')
        try:
            hints = self.inlay_hints_provider.get_inlay_hints(uri, text, range_)
        except Exception as e:
            logger.error(f"Error getting inlay hints for {uri}: {e}", exc_info=True)
            hints = []
        return {"jsonrpc": "2.0", "id": msg_id, "result": hints}

    def _handle_document_symbol(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/documentSymbol request."""
        uri = params['textDocument']['uri']
        
        # Get symbols from workspace index
        if self.workspace_index:
            symbols = self.workspace_index.get_symbols_in_file(uri)
            
            # Convert to LSP DocumentSymbol format (hierarchical)
            result = []
            kind_map = {
                'function': 12,  # Function
                'class': 5,      # Class
                'method': 6,     # Method
                'struct': 23,    # Struct
                'variable': 13,  # Variable
                'field': 8,      # Field
                'parameter': 13, # Variable
                'switch': 2,     # Module-like control-flow group
                'case': 22,      # EnumMember-like branch entry
                'label': 20,     # Key-like loop label
            }
            
            # Build a hierarchical symbol tree using dotted scope paths.
            symbol_entries = []
            symbols_by_path = {}
            result = []

            for sym in symbols:
                lsp_symbol = {
                    'name': sym.name,
                    'kind': kind_map.get(sym.kind, 13),
                    'range': {
                        'start': {'line': sym.line, 'character': sym.column},
                        'end': {'line': sym.line, 'character': sym.column + len(sym.name)}
                    },
                    'selectionRange': {
                        'start': {'line': sym.line, 'character': sym.column},
                        'end': {'line': sym.line, 'character': sym.column + len(sym.name)}
                    }
                }

                if sym.signature:
                    lsp_symbol['detail'] = sym.signature

                symbol_path = f"{sym.scope}.{sym.name}" if sym.scope else sym.name
                symbol_entries.append((sym.scope, symbol_path, lsp_symbol))
                symbols_by_path[symbol_path] = lsp_symbol

            for scope, _symbol_path, lsp_symbol in symbol_entries:
                if scope and scope in symbols_by_path:
                    parent = symbols_by_path[scope]
                    parent.setdefault('children', []).append(lsp_symbol)
                else:
                    result.append(lsp_symbol)
        else:
            # No workspace index: use regex-based fallback directly on the document text
            text = self.documents.get(uri, '')
            result = []
            if text:
                import re as _re
                lines = text.split('\n')
                for i, line in enumerate(lines):
                    # Functions
                    m = _re.search(r'\bfunction\s+(\w+)', line, _re.IGNORECASE)
                    if m:
                        name = m.group(1)
                        col = m.start(1)
                        result.append({
                            'name': name, 'kind': 12,
                            'range': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}},
                            'selectionRange': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}}
                        })
                        continue
                    # Classes
                    m = _re.search(r'\bclass\s+(\w+)', line, _re.IGNORECASE)
                    if m:
                        name = m.group(1)
                        col = m.start(1)
                        result.append({
                            'name': name, 'kind': 5,
                            'range': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}},
                            'selectionRange': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}}
                        })
                        continue
                    # Structs
                    m = _re.search(r'\bstruct\s+(\w+)', line, _re.IGNORECASE)
                    if m:
                        name = m.group(1)
                        col = m.start(1)
                        result.append({
                            'name': name, 'kind': 23,
                            'range': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}},
                            'selectionRange': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}}
                        })
                        continue
                    # Variables (top-level only: not indented)
                    m = _re.match(r'^set\s+(\w+)\s+to', line, _re.IGNORECASE)
                    if m:
                        name = m.group(1)
                        col = m.start(1)
                        result.append({
                            'name': name, 'kind': 13,
                            'range': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}},
                            'selectionRange': {'start': {'line': i, 'character': col}, 'end': {'line': i, 'character': col + len(name)}}
                        })

        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": result
        }
    
    def _handle_prepare_call_hierarchy(self, msg_id: int, params: Dict) -> Dict:
        """Handle textDocument/prepareCallHierarchy request."""
        uri = params['textDocument']['uri']
        position = params['position']
        text = self.documents.get(uri, '')
        
        if not self.workspace_index:
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
        # Get word at position
        lines = text.split('\n')
        if position['line'] >= len(lines):
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
        line = lines[position['line']]
        if position['character'] >= len(line):
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
        # Find word boundaries
        start = position['character']
        end = position['character']
        
        while start > 0 and (line[start - 1].isalnum() or line[start - 1] == '_'):
            start -= 1
        while end < len(line) and (line[end].isalnum() or line[end] == '_'):
            end += 1
        
        word = line[start:end]
        if not word:
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
        # Find the symbol
        symbols = self.workspace_index.get_symbol(word)
        if not symbols:
            return {"jsonrpc": "2.0", "id": msg_id, "result": None}
        
        # Return call hierarchy items
        items = []
        for sym in symbols:
            if sym.kind in ('function', 'method'):
                kind_map = {'function': 12, 'method': 6}
                items.append({
                    'name': sym.name,
                    'kind': kind_map[sym.kind],
                    'detail': sym.signature or '',
                    'uri': sym.file_uri,
                    'range': {
                        'start': {'line': sym.line, 'character': sym.column},
                        'end': {'line': sym.line, 'character': sym.column + len(sym.name)}
                    },
                    'selectionRange': {
                        'start': {'line': sym.line, 'character': sym.column},
                        'end': {'line': sym.line, 'character': sym.column + len(sym.name)}
                    }
                })
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": items if items else None
        }
    
    def _handle_incoming_calls(self, msg_id: int, params: Dict) -> Dict:
        """Handle callHierarchy/incomingCalls request."""
        item = params['item']
        target_name = item['name']
        
        if not self.workspace_index:
            return {"jsonrpc": "2.0", "id": msg_id, "result": []}
        
        # Find all files that might call this function
        incoming = []
        seen_callers = set()  # Track (file_uri, function_name) to avoid duplicates
        
        # Search all indexed files for calls to target_name
        for file_uri in self.workspace_index.indexed_files:
            # Get file content
            file_path = self.workspace_index._uri_to_path(file_uri)
            try:
                with open(file_path, 'r') as f:
                    content = f.read()
                
                # Simple search for function calls (could be enhanced with AST analysis)
                lines = content.split('\n')
                
                # Build a map of line numbers to function names for accurate containment
                function_ranges = {}  # {function_name: (start_line, end_line)}
                current_function = None
                function_start = None
                
                for i, line in enumerate(lines):
                    stripped = line.strip()
                    # Detect function start
                    if stripped.startswith('function '):
                        if current_function and function_start is not None:
                            # End previous function
                            function_ranges[current_function] = (function_start, i - 1)
                        # Extract function name
                        parts = stripped.split()
                        if len(parts) >= 2:
                            current_function = parts[1]
                            function_start = i
                    # Detect function end
                    elif stripped == 'end' and current_function:
                        function_ranges[current_function] = (function_start, i)
                        current_function = None
                        function_start = None
                
                # Close last function if file ends without 'end'
                if current_function and function_start is not None:
                    function_ranges[current_function] = (function_start, len(lines) - 1)
                
                # Now search for calls
                for line_num, line in enumerate(lines):
                    # Look for function calls: "target_name(" or "target_name with"
                    if f"{target_name}(" in line or f"{target_name} with" in line:
                        # Skip if this is the function definition line itself
                        if line.strip().startswith('function ' + target_name):
                            continue
                        
                        # Find which function contains this line
                        calling_func_name = None
                        for func_name, (start, end) in function_ranges.items():
                            if start <= line_num <= end:
                                calling_func_name = func_name
                                break
                        
                        if calling_func_name and calling_func_name != target_name:
                            # Avoid duplicates
                            caller_key = (file_uri, calling_func_name)
                            if caller_key in seen_callers:
                                continue
                            seen_callers.add(caller_key)
                            
                            # Find the symbol info for this function
                            symbols = self.workspace_index.get_symbols_in_file(file_uri)
                            calling_func = None
                            for sym in symbols:
                                if sym.kind in ('function', 'method') and sym.name == calling_func_name:
                                    calling_func = sym
                                    break
                            
                            if calling_func:
                                kind_map = {'function': 12, 'method': 6}
                                incoming.append({
                                'from': {
                                    'name': calling_func.name,
                                    'kind': kind_map.get(calling_func.kind, 12),
                                    'detail': calling_func.signature or '',
                                    'uri': calling_func.file_uri,
                                    'range': {
                                        'start': {'line': calling_func.line, 'character': calling_func.column},
                                        'end': {'line': calling_func.line, 'character': calling_func.column + len(calling_func.name)}
                                    },
                                    'selectionRange': {
                                        'start': {'line': calling_func.line, 'character': calling_func.column},
                                        'end': {'line': calling_func.line, 'character': calling_func.column + len(calling_func.name)}
                                    }
                                },
                                'fromRanges': [{
                                    'start': {'line': line_num, 'character': line.find(target_name)},
                                    'end': {'line': line_num, 'character': line.find(target_name) + len(target_name)}
                                }]
                            })
            except Exception as e:
                logger.error(f"Error analyzing calls in {file_uri}: {e}")
                continue
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": incoming
        }
    
    def _handle_outgoing_calls(self, msg_id: int, params: Dict) -> Dict:
        """Handle callHierarchy/outgoingCalls request."""
        item = params['item']
        uri = item['uri']
        
        if not self.workspace_index:
            return {"jsonrpc": "2.0", "id": msg_id, "result": []}
        
        # Get the function's content
        file_path = self.workspace_index._uri_to_path(uri)
        try:
            with open(file_path, 'r') as f:
                content = f.read()
        except Exception:
            return {"jsonrpc": "2.0", "id": msg_id, "result": []}
        
        outgoing = []
        
        # Find function calls in this function's body
        # Simple approach: look for identifiers followed by "(" or "with"
        import re
        lines = content.split('\n')
        start_line = item['range']['start']['line']
        
        # Find end of function (naive: look for "end" keyword)
        end_line = start_line + 1
        while end_line < len(lines) and not lines[end_line].strip().startswith('end'):
            end_line += 1
        
        # Search for function calls in this range
        for line_num in range(start_line, end_line):
            if line_num >= len(lines):
                break
            line = lines[line_num]
            
            # Find function calls
            matches = re.finditer(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:\(|with)', line)
            for match in matches:
                func_name = match.group(1)
                
                # Look up this function in workspace index
                symbols = self.workspace_index.get_symbol(func_name)
                for sym in symbols:
                    if sym.kind in ('function', 'method'):
                        kind_map = {'function': 12, 'method': 6}
                        outgoing.append({
                            'to': {
                                'name': sym.name,
                                'kind': kind_map[sym.kind],
                                'detail': sym.signature or '',
                                'uri': sym.file_uri,
                                'range': {
                                    'start': {'line': sym.line, 'character': sym.column},
                                    'end': {'line': sym.line, 'character': sym.column + len(sym.name)}
                                },
                                'selectionRange': {
                                    'start': {'line': sym.line, 'character': sym.column},
                                    'end': {'line': sym.line, 'character': sym.column + len(sym.name)}
                                }
                            },
                            'fromRanges': [{
                                'start': {'line': line_num, 'character': match.start()},
                                'end': {'line': line_num, 'character': match.end() - 1}
                            }]
                        })
                        break  # Only add once per call
        
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "result": outgoing
        }
    
    def _publish_diagnostics(self, uri: str, diagnostics: List[Dict]):
        """Publish diagnostics to client."""
        notification = {
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
                "uri": uri,
                "diagnostics": diagnostics
            }
        }
        self._write_message(notification)

    def get_or_parse(self, uri: str, text: str):
        """
        Return a cached AST for the given document, or parse and cache it.

        The cache key is a short MD5 hash of the text; the AST is invalidated
        automatically whenever the text changes.  Returns None if parsing fails.
        """
        import hashlib
        text_hash = hashlib.md5(text.encode('utf-8')).hexdigest()[:12]
        cached = self._parse_cache.get(uri)
        if cached and cached[0] == text_hash:
            return cached[1]
        try:
            from ..parser.lexer import Lexer
            from ..parser.parser import Parser
            lexer = Lexer(text)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            self._parse_cache[uri] = (text_hash, ast)
            return ast
        except Exception:
            return None

    def invalidate_parse_cache(self, uri: str) -> None:
        """Remove the cached AST for a document (call on close or delete)."""
        self._parse_cache.pop(uri, None)


# Backward-compatible alias
NLPLLanguageServer = NexusLangLanguageServer

__all__ = ['NexusLangLanguageServer', 'NLPLLanguageServer', 'Position', 'Range', 'Location']
