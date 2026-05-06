"""
Diagnostics Provider
====================

Provides real-time error checking and warnings.
"""

from typing import List, Dict, Optional
import re
import logging

from nexuslang.error_codes import get_error_info, get_error_code_for_type
from nexuslang.lsp.telemetry import DiagnosticTelemetry


logger = logging.getLogger(__name__)


class DiagnosticsProvider:
    """
    Provides real-time diagnostics.
    
    Checks for:
    - Syntax errors (using NexusLang parser)
    - Type errors (using NexusLang type checker)
    - Undefined variables
    - Unused variables
    - Best practice violations
    """
    
    def __init__(self, server):
        self.server = server
        self.file_diagnostics_cache = {}  # Cache diagnostics per file
        self.workspace_files = set()  # Track all files in workspace
        self._telemetry = DiagnosticTelemetry()  # Local dev telemetry (opt-in)

    def _build_diagnostic(
        self,
        line: int,
        start_char: int,
        end_char: int,
        severity: int,
        message: str,
        source: str = "nexuslang",
        error_code: Optional[str] = None,
        error_type_key: Optional[str] = None,
        title: Optional[str] = None,
        category: Optional[str] = None,
        fixes: Optional[List[str]] = None,
        doc_link: Optional[str] = None,
    ) -> Dict:
        """Build normalized LSP diagnostic payload with NexusLang error metadata."""
        resolved_code = error_code or (get_error_code_for_type(error_type_key) if error_type_key else None)
        if not resolved_code:
            resolved_code = get_error_code_for_type("runtime_error")
        error_info = get_error_info(resolved_code) if resolved_code else None

        normalized_source = source
        if source in {"nexuslang", "parser", "typechecker", "nlpl-parser", "nlpl-typechecker"}:
            normalized_source = "nlpl"

        resolved_title = title or (error_info.title if error_info else None)
        resolved_category = category or (error_info.category if error_info else None)
        resolved_fixes = fixes if fixes is not None else (error_info.fixes[:3] if error_info and error_info.fixes else [])
        resolved_doc_link = doc_link or (error_info.doc_link if error_info else None)

        diagnostic = {
            "range": {
                "start": {"line": max(0, line), "character": max(0, start_char)},
                "end": {"line": max(0, line), "character": max(max(0, start_char), end_char)}
            },
            "severity": severity,
            "message": message,
            "source": normalized_source,
        }

        if resolved_code:
            diagnostic["code"] = resolved_code

        # Enriched payload for hover/code-actions/explain
        payload_data = {
            "title": resolved_title,
            "category": resolved_category,
            "fixes": resolved_fixes,
            "explainHint": f"nxl --explain {resolved_code}" if resolved_code else None,
            "docLink": resolved_doc_link,
        }
        # Remove empty keys
        payload_data = {k: v for k, v in payload_data.items() if v not in (None, [], "")}
        if payload_data:
            diagnostic["data"] = payload_data

        return diagnostic

    def _is_valid_error_code(self, code: Optional[str]) -> bool:
        """Return True when code follows NexusLang EXXX format."""
        if not isinstance(code, str):
            return False
        return bool(re.fullmatch(r"E\d{3}", code))

    def _tag_origin(self, diagnostics: List[Dict], origin: str) -> List[Dict]:
        """Annotate diagnostic stream origin for downstream merge/debugging."""
        tagged: List[Dict] = []
        for diagnostic in diagnostics or []:
            d = dict(diagnostic)
            data = dict(d.get("data", {}))
            data.setdefault("origin", origin)
            d["data"] = data
            tagged.append(d)
        return tagged

    def _diagnostic_sort_key(self, diagnostic: Dict) -> tuple:
        """Return a stable sort key for deterministic diagnostics ordering."""
        rng = diagnostic.get("range", {})
        start = rng.get("start", {})
        end = rng.get("end", {})
        return (
            start.get("line", 0),
            start.get("character", 0),
            end.get("line", 0),
            end.get("character", 0),
            diagnostic.get("severity", 1),
            str(diagnostic.get("code", "E309")),
            str(diagnostic.get("source", "nlpl")),
            str(diagnostic.get("message", "")),
        )

    def _normalize_diagnostic(self, diagnostic: Dict) -> Dict:
        """Normalize incoming diagnostic payload to stable source/code/data contract."""
        normalized = dict(diagnostic)

        source = normalized.get("source", "nlpl")

        data = dict(normalized.get("data", {}))
        if source in {"parser", "nexuslang-parser"}:
            data.setdefault("origin", "parser")
        elif source in {"typechecker", "nexuslang-typechecker"}:
            data.setdefault("origin", "typechecker")
        elif source in {"nexuslang", "nlpl"}:
            data.setdefault("origin", "diagnostics")

        if source in {"nexuslang", "parser", "typechecker", "nexuslang-parser", "nexuslang-typechecker"}:
            normalized["source"] = "nlpl"
        elif not source:
            normalized["source"] = "nlpl"

        code = normalized.get("code")
        if not self._is_valid_error_code(code):
            code = get_error_code_for_type("runtime_error")
            normalized["code"] = code

        info = get_error_info(str(code))
        if info:
            data.setdefault("title", info.title)
            data.setdefault("category", info.category)
            if info.fixes:
                data.setdefault("fixes", info.fixes[:3])
            if info.doc_link:
                data.setdefault("docLink", info.doc_link)
        data.setdefault("reference", f"docs/reference/error-codes.md#{str(code).lower()}")
        data.setdefault("explainHint", f"nxl --explain {code}")
        normalized["data"] = {k: v for k, v in data.items() if v not in (None, [], "")}

        return normalized

    def merge_and_dedupe_diagnostics(self, *streams: List[Dict]) -> List[Dict]:
        """Deterministically merge multiple diagnostic streams with payload normalization and deduplication."""
        merged: List[Dict] = []
        for stream in streams:
            if not stream:
                continue
            for diagnostic in stream:
                merged.append(self._normalize_diagnostic(diagnostic))

        deduped: List[Dict] = []
        seen = {}
        for diagnostic in sorted(merged, key=self._diagnostic_sort_key):
            identity = (
                diagnostic.get("range", {}).get("start", {}).get("line", 0),
                diagnostic.get("range", {}).get("start", {}).get("character", 0),
                diagnostic.get("range", {}).get("end", {}).get("line", 0),
                diagnostic.get("range", {}).get("end", {}).get("character", 0),
                diagnostic.get("severity", 1),
                str(diagnostic.get("code", "E309")),
                str(diagnostic.get("source", "nlpl")),
                str(diagnostic.get("message", "")),
            )
            existing_idx = seen.get(identity)
            if existing_idx is None:
                seen[identity] = len(deduped)
                deduped.append(diagnostic)
                continue

            # Merge origins for identical diagnostics emitted by multiple streams.
            existing = deduped[existing_idx]
            existing_data = dict(existing.get("data", {}))
            incoming_data = dict(diagnostic.get("data", {}))

            origins = set()
            existing_origin = existing_data.get("origin")
            if existing_origin:
                origins.add(existing_origin)
            incoming_origin = incoming_data.get("origin")
            if incoming_origin:
                origins.add(incoming_origin)
            origins.update(existing_data.get("origins", []))
            origins.update(incoming_data.get("origins", []))

            if origins:
                existing_data["origins"] = sorted(origins)
                if "origin" not in existing_data:
                    existing_data["origin"] = sorted(origins)[0]
                existing["data"] = existing_data

        return deduped
    
    def get_diagnostics(self, uri: str, text: str, check_imports: bool = True) -> List[Dict]:
        """
        Get diagnostics for document.
        
        Args:
            uri: Document URI
            text: Document text
            check_imports: Whether to check imported files
            
        Returns:
            List of diagnostics
        """
        parser_diagnostics: List[Dict] = []
        type_diagnostics: List[Dict] = []
        import_diagnostics: List[Dict] = []
        channel_diagnostics: List[Dict] = []
        macro_comptime_diagnostics: List[Dict] = []
        exception_diagnostics: List[Dict] = []
        async_diagnostics: List[Dict] = []
        unsafe_ffi_diagnostics: List[Dict] = []
        parallel_diagnostics: List[Dict] = []
        unused_var_diagnostics: List[Dict] = []
        
        # Track this file in workspace
        self.workspace_files.add(uri)
        
        # Try parser-based syntax checking first
        parser_diagnostics = self._check_parser_syntax_enhanced(text, uri)
        if not parser_diagnostics:
            # Fallback to basic syntax checks
            parser_diagnostics = self._check_syntax(text)
        
        # Try type checker diagnostics with enhanced positioning
        type_diagnostics = self._check_type_errors_enhanced(text, uri)
        
        # Check for import errors (multi-file)
        if check_imports:
            import_diagnostics = self._check_imports(text, uri)
        
        # Additional static checks
        channel_diagnostics = self._check_channel_operations(text)
        macro_comptime_diagnostics = self._check_macro_comptime_operations(text)
        exception_diagnostics = self._check_exception_scope_and_unreachable_catch(text)
        async_diagnostics = self._check_async_await_spawn_contexts(text)
        unsafe_ffi_diagnostics = self._check_unsafe_and_ffi_signatures(text)
        parallel_diagnostics = self._check_parallel_unsafe_captures(text)
        unused_var_diagnostics = self._check_unused_vars(text)

        diagnostics = self.merge_and_dedupe_diagnostics(
            self._tag_origin(parser_diagnostics, "parser"),
            self._tag_origin(type_diagnostics, "typechecker"),
            self._tag_origin(import_diagnostics, "imports"),
            self._tag_origin(channel_diagnostics, "channels"),
            self._tag_origin(macro_comptime_diagnostics, "macro-comptime"),
            self._tag_origin(exception_diagnostics, "exceptions"),
            self._tag_origin(async_diagnostics, "async"),
            self._tag_origin(unsafe_ffi_diagnostics, "unsafe-ffi"),
            self._tag_origin(parallel_diagnostics, "parallel-for"),
            self._tag_origin(unused_var_diagnostics, "unused-vars"),
        )
        
        # Cache diagnostics for this file
        self.file_diagnostics_cache[uri] = diagnostics

        # Record telemetry for every emitted code (local/dev only, best-effort)
        self._telemetry.record(diagnostics)

        return diagnostics
    
    def _check_parser_syntax_enhanced(self, text: str, uri: str) -> List[Dict]:
        """
        Check syntax using NexusLang parser with enhanced error positioning.
        
        Uses AST cache for performance.
        
        Returns:
            List of diagnostics with accurate line/column info
        """
        diagnostics = []
        
        try:
            from nexuslang.parser.cached_parser import parse_with_cache
            
            # Use cached parser for performance
            # Will automatically handle cache hits/misses based on content hash
            ast = parse_with_cache(uri, text, debug=False)
            
            # If we got here without exception, no syntax errors
            return []
            
        except Exception as e:
            # Parse error - extract detailed position info
            error_msg = str(e)
            
            # Try to extract line/column from error or token
            line = 0
            col = 0
            end_col = 0
            
            # Check if error has line_number attribute (from parser)
            if hasattr(e, 'line_number'):
                line = e.line_number - 1
            else:
                # Extract from error message
                line_match = re.search(r'line (\d+)', error_msg, re.IGNORECASE)
                if line_match:
                    line = int(line_match.group(1)) - 1
            
            if hasattr(e, 'column'):
                col = e.column - 1
            else:
                col_match = re.search(r'column (\d+)', error_msg, re.IGNORECASE)
                if col_match:
                    col = int(col_match.group(1)) - 1
            
            # Get the problematic line for better highlighting
            lines = text.split('\n')
            if line < len(lines):
                line_text = lines[line]
                # Try to highlight the actual error token
                if 'Unexpected token' in error_msg:
                    token_match = re.search(r"token '([^']+)'", error_msg)
                    if token_match:
                        token = token_match.group(1)
                        token_pos = line_text.find(token, col)
                        if token_pos >= 0:
                            col = token_pos
                            end_col = col + len(token)
                
                if end_col == 0:
                    end_col = min(col + 10, len(line_text))
            else:
                end_col = col + 1
            
            diagnostics.append({
                **self._build_diagnostic(
                    line=line,
                    start_char=col,
                    end_char=end_col,
                    severity=1,
                    message=f"Syntax error: {error_msg}",
                    source="nexuslang",
                    error_code=getattr(e, 'error_code', None),
                    error_type_key="unexpected_token",
                )
            })
        
        return diagnostics
    
    def _check_parser_syntax(self, text: str) -> List[Dict]:
        """
        Check syntax using NexusLang parser.
        
        Returns:
            List of diagnostics or empty list if parser not available
        """
        diagnostics = []
        
        try:
            from nexuslang.parser.lexer import Lexer
            from nexuslang.parser.parser import Parser
            
            # Try to parse
            lexer = Lexer(text)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            parser.parse()
            
            # If we got here without exception, no syntax errors
            return []
            
        except Exception as e:
            # Parse error - try to extract line/column info
            error_msg = str(e)
            
            # Try to extract line number from error message
            line_match = re.search(r'line (\d+)', error_msg, re.IGNORECASE)
            col_match = re.search(r'column (\d+)', error_msg, re.IGNORECASE)
            
            line = int(line_match.group(1)) - 1 if line_match else 0
            col = int(col_match.group(1)) - 1 if col_match else 0
            
            # Get the problematic line
            lines = text.split('\n')
            if line < len(lines):
                line_text = lines[line]
                end_col = min(col + 10, len(line_text))  # Highlight ~10 chars
            else:
                end_col = col + 1
            
            diagnostics.append(
                self._build_diagnostic(
                    line=line,
                    start_char=col,
                    end_char=end_col,
                    severity=1,
                    message=f"Syntax error: {error_msg}",
                    source="nexuslang",
                    error_type_key="unexpected_token",
                )
            )
        
        return diagnostics
    
    def _check_type_errors_enhanced(self, text: str, uri: str) -> List[Dict]:
        """
        Check for type errors using NexusLang type checker with AST-based positioning.
        
        Returns:
            List of diagnostics with accurate positioning from AST nodes
        """
        diagnostics = []
        
        try:
            # Use cached AST if available
            if hasattr(self, 'ast_cache') and uri in self.ast_cache:
                ast = self.ast_cache[uri]
            else:
                # Parse if not cached
                from nexuslang.parser.lexer import Lexer
                from nexuslang.parser.parser import Parser
                
                lexer = Lexer(text)
                tokens = lexer.tokenize()
                parser = Parser(tokens)
                ast = parser.parse()
            
            # Type check
            from nexuslang.typesystem.typechecker import TypeChecker
            typechecker = TypeChecker()
            typechecker.check_program(ast)
            
            # Convert type errors to diagnostics with AST node positions
            for error in typechecker.errors:
                # Try to find the AST node related to this error
                line, col, end_col = self._find_error_position(text, error, ast)

                contract_fixes = self._suggest_contract_fixes(error)
                asm_fixes = self._suggest_inline_asm_fixes(error)
                ffi_fixes = self._suggest_unsafe_ffi_fixes(error)
                is_contract_error = bool(contract_fixes)
                is_asm_error = bool(asm_fixes)
                is_ffi_error = bool(ffi_fixes)
                selected_fixes = contract_fixes or asm_fixes or ffi_fixes

                if is_contract_error or is_asm_error or is_ffi_error:
                    error_code = "E201"
                    error_type = "invalid_operation"
                else:
                    error_code = None
                    error_type = "type_mismatch"

                if is_contract_error:
                    category = "contract"
                elif is_asm_error:
                    category = "systems"
                elif is_ffi_error:
                    category = "ffi"
                else:
                    category = None
                
                diagnostics.append(
                    self._build_diagnostic(
                        line=line,
                        start_char=col,
                        end_char=end_col,
                        severity=1,
                        message=f"Type error: {error}",
                        source="nexuslang",
                        error_code=error_code,
                        error_type_key=error_type,
                        category=category,
                        fixes=selected_fixes,
                    )
                )
        
        except Exception:
            # If parsing fails, syntax errors are handled by parser diagnostics fallback.
            logger.debug("Enhanced type diagnostics failed", exc_info=True)
        
        return diagnostics

    def _suggest_contract_fixes(self, error: str) -> List[str]:
        """Return quick-fix suggestions for contract diagnostics."""
        text = (error or "").lower()
        if not any(k in text for k in ("require", "ensure", "guarantee", "invariant", "contract")):
            return []

        fixes: List[str] = []

        if "must be a boolean" in text:
            fixes.extend([
                "Use a boolean condition expression",
                "Convert contract condition to explicit boolean check",
                "Add contract failure message",
            ])

        if "must not contain assignments" in text or "must not contain assignments or mutations" in text:
            fixes.extend([
                "Move assignment or mutation outside contract condition",
                "Keep only pure boolean expressions in contract conditions",
                "Add contract failure message",
            ])

        if "message must be a string" in text:
            fixes.extend([
                "Use string literal for contract message",
                "Convert contract message to string",
            ])

        # Stable order, remove duplicates.
        seen = set()
        ordered: List[str] = []
        for fix in fixes:
            if fix in seen:
                continue
            seen.add(fix)
            ordered.append(fix)
        return ordered

    def _suggest_inline_asm_fixes(self, error: str) -> List[str]:
        """Return quick-fix suggestions for inline assembly diagnostics."""
        text = (error or "").lower()
        if "inline assembly" not in text:
            return []

        fixes: List[str] = []

        if "input constraint" in text:
            fixes.extend([
                "Use GCC-style input constraint tokens (e.g., r, m, i)",
                "Remove whitespace from asm constraint strings",
            ])

        if "output constraint" in text:
            fixes.extend([
                "Add output write marker (= or +) to asm output constraint",
                "Use GCC-style output constraint tokens (e.g., =r, +m)",
            ])

        if "output operand must be an identifier" in text:
            fixes.extend([
                "Use a variable identifier as asm output target",
                "Declare output variable before inline asm block",
            ])

        if "undefined output variable" in text:
            fixes.extend([
                "Declare output variable before inline asm block",
                "Use an existing variable name in asm outputs",
            ])

        if "clobber" in text and "duplicate" in text:
            fixes.extend([
                "Remove duplicate clobber entries",
            ])

        if "invalid inline assembly clobber" in text:
            fixes.extend([
                "Use valid clobbers such as memory, cc, or register names",
                "Remove special characters from clobber names",
            ])

        seen = set()
        ordered: List[str] = []
        for fix in fixes:
            if fix in seen:
                continue
            seen.add(fix)
            ordered.append(fix)
        return ordered

    def _suggest_unsafe_ffi_fixes(self, error: str) -> List[str]:
        """Return quick-fix suggestions for unsafe/FFI diagnostics."""
        text = (error or "").lower()
        if not any(k in text for k in ("extern", "foreign", "ffi", "unsafe", "calling convention", "library")):
            return []

        fixes: List[str] = []

        if "from library" in text or "library" in text:
            fixes.append("Add source library clause: from library \"c\"")

        if "calling convention" in text or "convention" in text:
            fixes.append("Use supported calling conventions: cdecl, stdcall, fastcall, sysv, win64, aapcs")

        if "unsafe" in text and "do" in text:
            fixes.append("Use unsafe blocks as: unsafe do ... end")

        if "return" in text and "extern function" in text:
            fixes.append("Declare explicit extern return type with returns <Type>")

        if "parameter" in text and "as" in text:
            fixes.append("Declare each extern parameter as: name as Type")

        if not fixes:
            fixes.extend([
                "Ensure extern signatures include parameter and return types",
                "Prefer explicit unsafe scopes around raw FFI operations",
            ])

        seen = set()
        ordered: List[str] = []
        for fix in fixes:
            if fix in seen:
                continue
            seen.add(fix)
            ordered.append(fix)
        return ordered
    
    def _find_error_position(self, text: str, error: str, ast) -> tuple:
        """
        Find accurate position for type error using AST.
        
        Returns:
            (line, col, end_col) tuple
        """
        lines = text.split('\n')
        
        # Extract identifiers from error message
        import re
        words = re.findall(r"'([^']+)'", error)
        
        # Try to find the first identifier in the source
        for word in words:
            for i, line_text in enumerate(lines):
                pos = line_text.find(word)
                if pos >= 0:
                    return (i, pos, pos + len(word))
        
        # Fallback: search for keywords in error
        error_lower = error.lower()
        for i, line_text in enumerate(lines):
            line_lower = line_text.lower()
            for word in error_lower.split():
                if len(word) > 3 and word.isalpha() and word in line_lower:
                    pos = line_lower.find(word)
                    return (i, pos, pos + len(word))
        
        # Last resort: beginning of first line
        return (0, 0, 10)
    
    def _check_type_errors(self, text: str) -> List[Dict]:
        """
        Check for type errors using NexusLang type checker.
        
        Returns:
            List of diagnostics or empty list if type checker not available
        """
        diagnostics = []
        
        try:
            from nexuslang.parser.lexer import Lexer
            from nexuslang.parser.parser import Parser
            from nexuslang.typesystem.typechecker import TypeChecker
            
            # Parse first
            lexer = Lexer(text)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            ast = parser.parse()
            
            # Type check
            typechecker = TypeChecker()
            typechecker.check_program(ast)
            
            # Convert type errors to diagnostics
            for error in typechecker.errors:
                # Try to extract position info from error
                line = 0
                col = 0
                
                # Simple heuristic: find the line containing the error
                error_lower = error.lower()
                for i, line_text in enumerate(text.split('\n')):
                    # Look for variable names, function names, etc. mentioned in error
                    if any(word in line_text.lower() for word in error_lower.split() 
                           if len(word) > 3 and word.isalpha()):
                        line = i
                        break
                
                diagnostics.append(
                    self._build_diagnostic(
                        line=line,
                        start_char=0,
                        end_char=100,
                        severity=1,
                        message=f"Type error: {error}",
                        source="nexuslang",
                        error_type_key="type_mismatch",
                    )
                )
        
        except Exception:
            # If parsing fails, syntax errors are handled by parser diagnostics fallback.
            logger.debug("Type checker diagnostics failed", exc_info=True)
        
        return diagnostics
    
    def _check_syntax(self, text: str) -> List[Dict]:
        """Check for syntax errors."""
        diagnostics = []
        lines = text.split('\n')
        
        for i, line in enumerate(lines):
            # Check for unclosed strings
            if line.count('"') % 2 != 0:
                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=0,
                        end_char=len(line),
                        severity=1,
                        message="Unclosed string",
                        source="nexuslang",
                        error_type_key="invalid_expression",
                    )
                )
            
            # Check for invalid characters in identifiers
            match = re.search(r'\bset\s+(\d\w*)', line, re.IGNORECASE)
            if match:
                var_name = match.group(1)
                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=match.start(1),
                        end_char=match.end(1),
                        severity=1,
                        message=f"Variable name '{var_name}' cannot start with a digit",
                        source="nexuslang",
                        error_type_key="invalid_expression",
                    )
                )
        
        return diagnostics
    
    def _check_undefined_vars(self, text: str) -> List[Dict]:
        """Check for undefined variables."""
        diagnostics = []
        lines = text.split('\n')
        
        # Extract defined variables
        defined_vars = set()
        var_pattern = r'set\s+(\w+)\s+to'
        for line in lines:
            matches = re.findall(var_pattern, line, re.IGNORECASE)
            defined_vars.update(matches)
        
        # Add common built-ins
        defined_vars.update(['true', 'false', 'null'])
        
        # Check for undefined variable usage
        # This is simplified - real implementation would track scope
        use_pattern = r'\b(\w+)\b'
        keywords = {'set', 'to', 'function', 'class', 'if', 'else', 'while', 'for', 'each', 'in', 
                   'return', 'print', 'text', 'as', 'Integer', 'String', 'Float', 'Boolean',
                   'plus', 'minus', 'times', 'divided', 'by', 'is', 'equal', 'greater', 'less', 'than'}
        
        for i, line in enumerate(lines):
            # Skip variable definitions and function definitions
            if re.search(r'set\s+\w+\s+to', line, re.IGNORECASE):
                continue
            if re.search(r'function\s+\w+', line, re.IGNORECASE):
                continue
            
            # Find potential variable uses
            matches = re.finditer(use_pattern, line)
            for match in matches:
                var = match.group(1)
                if var.lower() not in keywords and var not in defined_vars and not var[0].isupper():
                    # NOTE: Undefined variable detection disabled
                    # NLPL's natural language syntax creates too many false positives:
                    # - English words in natural syntax ("create", "called", "with", etc.)
                    # - Context-dependent parsing makes static analysis unreliable
                    # - Proper implementation requires full parser integration
                    # 
                    # This check is permanently removed until AST-based analysis is available.
                    # The interpreter provides runtime undefined variable errors instead.
                    pass
        
        return diagnostics
    
    def _check_unused_vars(self, text: str) -> List[Dict]:
        """Check for unused variables."""
        diagnostics = []
        lines = text.split('\n')
        
        # Extract defined variables with their line numbers
        defined_vars = {}
        var_pattern = r'set\s+(\w+)\s+to'
        for i, line in enumerate(lines):
            matches = re.finditer(var_pattern, line, re.IGNORECASE)
            for match in matches:
                var_name = match.group(1)
                if var_name not in defined_vars:
                    defined_vars[var_name] = (i, match.start(1))
        
        # Check which variables are used
        used_vars = set()
        for i, line in enumerate(lines):
            # Skip the definition line
            for var_name in defined_vars.keys():
                # Look for variable usage (not in definition)
                if not re.search(rf'set\s+{re.escape(var_name)}\s+to', line, re.IGNORECASE):
                    if re.search(rf'\b{re.escape(var_name)}\b', line):
                        used_vars.add(var_name)
        
        # Report unused variables
        for var_name, (line_num, col) in defined_vars.items():
            if var_name not in used_vars:
                diagnostics.append(
                    self._build_diagnostic(
                        line=line_num,
                        start_char=col,
                        end_char=col + len(var_name),
                        severity=2,
                        message=f"Unused variable '{var_name}'",
                        source="nexuslang",
                        error_type_key="unused_variable",
                    )
                )
        
        return diagnostics
    
    def _check_imports(self, text: str, uri: str) -> List[Dict]:
        """
        Check for import errors (multi-file diagnostics).
        
        Returns:
            List of diagnostics for missing/invalid imports
        """
        diagnostics = []
        lines = text.split('\n')
        
        # Find import statements
        import_pattern = r'import\s+(\w+)(?:\s+from\s+["\']([^"\']*)["\'])?'
        
        for i, line in enumerate(lines):
            match = re.search(import_pattern, line, re.IGNORECASE)
            if match:
                module_name = match.group(1)
                file_path = match.group(2) if match.group(2) else None
                
                # Check if module exists
                if file_path:
                    # Check if file exists
                    import os
                    base_dir = os.path.dirname(uri.replace('file://', ''))
                    full_path = os.path.join(base_dir, file_path)
                    
                    if not os.path.exists(full_path) and not os.path.exists(full_path + '.nxl'):
                        diagnostics.append(
                            self._build_diagnostic(
                                line=i,
                                start_char=0,
                                end_char=len(line),
                                severity=1,
                                message=f"Cannot find module '{file_path}'",
                                source="nexuslang",
                                error_type_key="module_not_found",
                            )
                        )
                else:
                    # Check stdlib modules
                    stdlib_modules = ['math', 'string', 'io', 'system', 'collections', 'network']
                    if module_name.lower() not in stdlib_modules:
                        diagnostics.append(
                            self._build_diagnostic(
                                line=i,
                                start_char=0,
                                end_char=len(line),
                                severity=2,
                                message=f"Unknown module '{module_name}' (not in stdlib)",
                                source="nexuslang",
                                error_type_key="import_name_not_found",
                            )
                        )
        
        return diagnostics

    def _check_channel_operations(self, text: str) -> List[Dict]:
        """Check channel send/receive operations for obvious non-channel misuse."""
        diagnostics = []
        lines = text.split('\n')

        variable_types = self._infer_variable_types(lines)

        send_pattern = re.compile(r'\bsend\s+.+\s+to\s+(\w+)\b', re.IGNORECASE)
        receive_pattern = re.compile(r'\breceive\s+from\s+(\w+)\b', re.IGNORECASE)

        for i, line in enumerate(lines):
            send_match = send_pattern.search(line)
            if send_match:
                target = send_match.group(1)
                target_type = variable_types.get(target)
                if target_type and target_type != "Channel":
                    start = send_match.start(1)
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=start,
                            end_char=start + len(target),
                            severity=1,
                            message=f"Cannot send to non-channel variable '{target}' of type {target_type}",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="type_mismatch",
                        )
                    )

            receive_match = receive_pattern.search(line)
            if receive_match:
                source_name = receive_match.group(1)
                source_type = variable_types.get(source_name)
                if source_type and source_type != "Channel":
                    start = receive_match.start(1)
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=start,
                            end_char=start + len(source_name),
                            severity=1,
                            message=f"Cannot receive from non-channel variable '{source_name}' of type {source_type}",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="type_mismatch",
                        )
                    )

        return diagnostics

    def _check_macro_comptime_operations(self, text: str) -> List[Dict]:
        """Check obvious macro/comptime misuse patterns in-document."""
        diagnostics = []
        lines = text.split('\n')

        macro_def_pattern = re.compile(r'^\s*macro\s+(\w+)\b', re.IGNORECASE)
        expand_pattern = re.compile(r'\bexpand\s+(\w+)\b', re.IGNORECASE)
        comptime_const_pattern = re.compile(r'^\s*comptime\s+const\s+(\w+)\s+is\b', re.IGNORECASE)
        reassignment_pattern = re.compile(r'^\s*set\s+(\w+)\s+to\b', re.IGNORECASE)

        defined_macros = set()
        comptime_consts = set()

        for i, line in enumerate(lines):
            macro_def_match = macro_def_pattern.match(line)
            if macro_def_match:
                defined_macros.add(macro_def_match.group(1))

            const_match = comptime_const_pattern.match(line)
            if const_match:
                comptime_consts.add(const_match.group(1))

            expand_match = expand_pattern.search(line)
            if expand_match:
                macro_name = expand_match.group(1)
                if macro_name not in defined_macros:
                    start = expand_match.start(1)
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=start,
                            end_char=start + len(macro_name),
                            severity=1,
                            message=f"Cannot expand undefined macro '{macro_name}'",
                            source="nexuslang",
                            error_code="E101",
                            error_type_key="undefined_function",
                        )
                    )

            reassign_match = reassignment_pattern.match(line)
            if reassign_match:
                var_name = reassign_match.group(1)
                if var_name in comptime_consts:
                    start = reassign_match.start(1)
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=start,
                            end_char=start + len(var_name),
                            severity=1,
                            message=f"Cannot reassign comptime constant '{var_name}'",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="invalid_operation",
                        )
                    )

        return diagnostics

    def _check_exception_scope_and_unreachable_catch(self, text: str) -> List[Dict]:
        """Check catch-variable scope misuse and likely unreachable catch blocks."""
        diagnostics: List[Dict] = []
        lines = text.split('\n')

        try_pattern = re.compile(r'^\s*try\b', re.IGNORECASE)
        catch_pattern = re.compile(r'^\s*catch\s+([A-Za-z_][A-Za-z0-9_]*)\b', re.IGNORECASE)
        end_pattern = re.compile(r'^\s*end\b', re.IGNORECASE)
        return_pattern = re.compile(r'^\s*return\b', re.IGNORECASE)

        def _indent(line_text: str) -> int:
            return len(line_text) - len(line_text.lstrip(' '))

        contexts: List[Dict] = []
        catch_scopes: List[Dict] = []

        for i, raw_line in enumerate(lines):
            line = raw_line.strip()
            if not line or line.startswith('#') or line.startswith('//'):
                continue

            indent = _indent(raw_line)

            if try_pattern.match(raw_line):
                contexts.append({
                    'try_indent': indent,
                    'try_terminal_line': None,
                    'try_terminal_kind': None,
                    'catch_var': None,
                    'catch_line': None,
                    'catch_indent': None,
                })
                continue

            if contexts:
                current = contexts[-1]

                if current['catch_var'] is None and current['try_terminal_line'] is None:
                    if return_pattern.match(raw_line):
                        current['try_terminal_line'] = i
                        current['try_terminal_kind'] = 'return'

                catch_match = catch_pattern.match(raw_line)
                if catch_match and current['catch_var'] is None:
                    catch_var = catch_match.group(1)
                    current['catch_var'] = catch_var
                    current['catch_line'] = i
                    current['catch_indent'] = indent

                    if current['try_terminal_kind'] == 'return':
                        start_char = raw_line.lower().find('catch')
                        diagnostics.append(
                            self._build_diagnostic(
                                line=i,
                                start_char=max(0, start_char),
                                end_char=max(5, start_char + 5),
                                severity=2,
                                message="Likely unreachable catch block: try body returns before catch",
                                source="nexuslang",
                                error_code="E309",
                                error_type_key="runtime_error",
                            )
                        )
                    continue

                if end_pattern.match(raw_line) and indent <= current['try_indent']:
                    finished = contexts.pop()
                    if finished['catch_var'] is not None and finished['catch_line'] is not None:
                        catch_scopes.append({
                            'var': finished['catch_var'],
                            'decl_line': finished['catch_line'],
                            'scope_start': finished['catch_line'] + 1,
                            'scope_end': i - 1,
                        })

        for scope in catch_scopes:
            var_name = scope['var']
            var_pattern = re.compile(rf'\b{re.escape(var_name)}\b')

            for i, raw_line in enumerate(lines):
                if i == scope['decl_line']:
                    continue

                in_scope = scope['scope_start'] <= i <= scope['scope_end']
                if in_scope:
                    continue

                stripped = raw_line.strip()
                if not stripped or stripped.startswith('#') or stripped.startswith('//'):
                    continue

                match = var_pattern.search(raw_line)
                if not match:
                    continue

                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=match.start(),
                        end_char=match.end(),
                        severity=1,
                        message=f"Catch variable '{var_name}' is only defined inside its catch block",
                        source="nexuslang",
                        error_code="E100",
                        error_type_key="undefined_variable",
                    )
                )

        return diagnostics

    def _check_async_await_spawn_contexts(self, text: str) -> List[Dict]:
        """Check await/spawn misuse patterns and async-scope violations."""
        diagnostics: List[Dict] = []
        lines = text.split('\n')

        async_function_pattern = re.compile(r'^\s*async\s+function\b', re.IGNORECASE)
        await_pattern = re.compile(r'\bawait\b', re.IGNORECASE)
        bare_await_pattern = re.compile(r'\bawait\s*$', re.IGNORECASE)
        bare_spawn_pattern = re.compile(r'^\s*spawn\s*$', re.IGNORECASE)

        async_scope_indents: List[int] = []

        def _indent(line_text: str) -> int:
            return len(line_text) - len(line_text.lstrip(' '))

        for i, raw_line in enumerate(lines):
            stripped = raw_line.strip()
            if not stripped or stripped.startswith('#') or stripped.startswith('//'):
                continue

            indent = _indent(raw_line)

            # Pop async scopes on dedent.
            while async_scope_indents and indent <= async_scope_indents[-1]:
                async_scope_indents.pop()

            if async_function_pattern.match(raw_line):
                async_scope_indents.append(indent)
                continue

            await_match = await_pattern.search(raw_line)
            if await_match and not async_scope_indents:
                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=await_match.start(),
                        end_char=await_match.end(),
                        severity=1,
                        message="Cannot use 'await' outside an async function",
                        source="nexuslang",
                        error_code="E201",
                        error_type_key="invalid_operation",
                    )
                )

            if await_match and bare_await_pattern.search(raw_line):
                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=await_match.start(),
                        end_char=await_match.end(),
                        severity=2,
                        message="Await expression is missing a task or awaitable value",
                        source="nexuslang",
                        error_code="E201",
                        error_type_key="invalid_operation",
                    )
                )

            if bare_spawn_pattern.match(raw_line):
                start = raw_line.lower().find('spawn')
                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=max(0, start),
                        end_char=max(5, start + 5),
                        severity=1,
                        message="Spawn expression is missing target function or block",
                        source="nexuslang",
                        error_code="E201",
                        error_type_key="invalid_operation",
                    )
                )

        return diagnostics

    def _check_unsafe_and_ffi_signatures(self, text: str) -> List[Dict]:
        """Check unsafe block structure and extern/foreign signature consistency."""
        diagnostics: List[Dict] = []
        lines = text.split('\n')

        unsafe_open_pattern = re.compile(r'^\s*unsafe\b', re.IGNORECASE)
        unsafe_do_pattern = re.compile(r'^\s*unsafe\s+do\b', re.IGNORECASE)
        end_pattern = re.compile(r'^\s*end\b', re.IGNORECASE)
        extern_function_pattern = re.compile(r'^\s*(extern|foreign)\s+function\s+([A-Za-z_][A-Za-z0-9_]*)\b(.*)$', re.IGNORECASE)
        extern_variable_pattern = re.compile(r'^\s*(extern|foreign)\s+variable\s+([A-Za-z_][A-Za-z0-9_]*)\b(.*)$', re.IGNORECASE)

        supported_calling_conventions = {"cdecl", "stdcall", "fastcall", "sysv", "win64", "aapcs", "vectorcall"}
        unsafe_stack: List[Dict] = []

        def _indent(line_text: str) -> int:
            return len(line_text) - len(line_text.lstrip(' '))

        for i, raw_line in enumerate(lines):
            stripped = raw_line.strip()
            if not stripped or stripped.startswith('#') or stripped.startswith('//'):
                continue

            indent = _indent(raw_line)

            if unsafe_open_pattern.match(raw_line) and not unsafe_do_pattern.match(raw_line):
                start = raw_line.lower().find('unsafe')
                diagnostics.append(
                    self._build_diagnostic(
                        line=i,
                        start_char=max(0, start),
                        end_char=max(6, start + 6),
                        severity=1,
                        message="Unsafe block must use 'unsafe do' opening syntax",
                        source="nexuslang",
                        error_code="E201",
                        error_type_key="invalid_operation",
                        category="ffi",
                        fixes=["Open unsafe blocks with: unsafe do", "Close unsafe blocks with end"],
                    )
                )

            if unsafe_do_pattern.match(raw_line):
                unsafe_stack.append({"line": i, "indent": indent})

            if end_pattern.match(raw_line) and unsafe_stack:
                if indent <= unsafe_stack[-1]["indent"]:
                    unsafe_stack.pop()

            extern_function_match = extern_function_pattern.match(raw_line)
            if extern_function_match:
                function_name = extern_function_match.group(2)
                remainder = extern_function_match.group(3) or ""
                remainder_lower = remainder.lower()
                name_start = raw_line.find(function_name)

                if " from library " not in remainder_lower:
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=max(0, name_start),
                            end_char=max(len(function_name), name_start + len(function_name)),
                            severity=1,
                            message=f"Extern function '{function_name}' must specify source library using 'from library \"name\"'",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="invalid_operation",
                            category="ffi",
                            fixes=["Add source library clause: from library \"c\""],
                        )
                    )

                if " returns " not in remainder_lower:
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=max(0, name_start),
                            end_char=max(len(function_name), name_start + len(function_name)),
                            severity=2,
                            message=f"Extern function '{function_name}' should declare an explicit return type",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="invalid_operation",
                            category="ffi",
                            fixes=["Declare explicit extern return type with returns <Type>"],
                        )
                    )

                call_conv_match = re.search(r'\bcalling\s+convention\s+([A-Za-z_][A-Za-z0-9_-]*)', remainder, re.IGNORECASE)
                if call_conv_match:
                    calling_convention = call_conv_match.group(1)
                    if calling_convention.lower() not in supported_calling_conventions:
                        diagnostics.append(
                            self._build_diagnostic(
                                line=i,
                                start_char=call_conv_match.start(1),
                                end_char=call_conv_match.end(1),
                                severity=1,
                                message=(
                                    f"Unsupported calling convention '{calling_convention}' for extern function '{function_name}'"
                                ),
                                source="nexuslang",
                                error_code="E201",
                                error_type_key="invalid_operation",
                                category="ffi",
                                fixes=["Use supported calling conventions: cdecl, stdcall, fastcall, sysv, win64, aapcs"],
                            )
                        )

                params_match = re.search(
                    r'\bwith\b\s*(.*?)(?=\breturns\b|\bfrom\s+library\b|\bcalling\s+convention\b|$)',
                    remainder,
                    re.IGNORECASE,
                )
                if params_match:
                    params_segment = params_match.group(1)
                    params = [p.strip() for p in re.split(r',|\band\b', params_segment, flags=re.IGNORECASE) if p.strip()]
                    for param in params:
                        if param == "...":
                            continue
                        if " as " not in param.lower():
                            diagnostics.append(
                                self._build_diagnostic(
                                    line=i,
                                    start_char=max(0, params_match.start(1)),
                                    end_char=max(params_match.start(1) + len(params_segment), params_match.start(1) + 1),
                                    severity=1,
                                    message=(
                                        f"Extern function '{function_name}' parameter '{param}' must declare type using 'as <Type>'"
                                    ),
                                    source="nexuslang",
                                    error_code="E201",
                                    error_type_key="invalid_operation",
                                    category="ffi",
                                    fixes=["Declare each extern parameter as: name as Type"],
                                )
                            )

            extern_variable_match = extern_variable_pattern.match(raw_line)
            if extern_variable_match:
                variable_name = extern_variable_match.group(2)
                remainder = extern_variable_match.group(3) or ""
                remainder_lower = remainder.lower()
                name_start = raw_line.find(variable_name)

                if " as " not in remainder_lower:
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=max(0, name_start),
                            end_char=max(len(variable_name), name_start + len(variable_name)),
                            severity=1,
                            message=f"Extern variable '{variable_name}' must declare a type using 'as <Type>'",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="invalid_operation",
                            category="ffi",
                            fixes=["Add type annotation: extern variable name as Type"],
                        )
                    )

                if " from library " not in remainder_lower:
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=max(0, name_start),
                            end_char=max(len(variable_name), name_start + len(variable_name)),
                            severity=2,
                            message=f"Extern variable '{variable_name}' should specify source library",
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="invalid_operation",
                            category="ffi",
                            fixes=["Add source library clause: from library \"c\""],
                        )
                    )

        for unsafe in unsafe_stack:
            line_index = unsafe["line"]
            raw_line = lines[line_index]
            start = raw_line.lower().find('unsafe')
            diagnostics.append(
                self._build_diagnostic(
                    line=line_index,
                    start_char=max(0, start),
                    end_char=max(6, start + 6),
                    severity=1,
                    message="Unsafe block opened here is not closed with 'end'",
                    source="nexuslang",
                    error_code="E201",
                    error_type_key="invalid_operation",
                    category="ffi",
                    fixes=["Add closing 'end' for unsafe block"],
                )
            )

        return diagnostics

    def _check_parallel_unsafe_captures(self, text: str) -> List[Dict]:
        """Check for likely unsafe outer-scope captures in parallel-for bodies."""
        diagnostics: List[Dict] = []
        lines = text.split('\n')

        parallel_open_pattern = re.compile(
            r'^\s*parallel\s+for(?:\s+each)?\s+([A-Za-z_][A-Za-z0-9_]*)\s+in\b',
            re.IGNORECASE,
        )
        end_pattern = re.compile(r'^\s*end\b', re.IGNORECASE)
        set_assign_pattern = re.compile(r'^\s*set\s+([A-Za-z_][A-Za-z0-9_]*)\b', re.IGNORECASE)
        member_assign_pattern = re.compile(r'^\s*set\s+([A-Za-z_][A-Za-z0-9_]*)\s*\.', re.IGNORECASE)
        index_assign_pattern = re.compile(r'^\s*set\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[', re.IGNORECASE)

        declared_before: set = set()
        contexts: List[Dict] = []

        def _indent(line_text: str) -> int:
            return len(line_text) - len(line_text.lstrip(' '))

        for i, raw_line in enumerate(lines):
            stripped = raw_line.strip()
            if not stripped or stripped.startswith('#') or stripped.startswith('//'):
                continue

            indent = _indent(raw_line)

            if contexts and end_pattern.match(raw_line):
                while contexts and indent <= contexts[-1]['indent']:
                    contexts.pop()
                continue

            open_match = parallel_open_pattern.match(raw_line)
            if open_match:
                loop_var = open_match.group(1)
                contexts.append({
                    'indent': indent,
                    'line': i,
                    'loop_var': loop_var,
                    'reported': set(),
                })
                continue

            if contexts:
                ctx = contexts[-1]

                def _emit_capture(var_name: str, start: int, end: int, detail: str) -> None:
                    key = (i, var_name, detail)
                    if key in ctx['reported']:
                        return
                    ctx['reported'].add(key)
                    diagnostics.append(
                        self._build_diagnostic(
                            line=i,
                            start_char=max(0, start),
                            end_char=max(start + 1, end),
                            severity=2,
                            message=(
                                f"Potential unsafe capture in parallel region: {detail} '{var_name}'"
                            ),
                            source="nexuslang",
                            error_code="E201",
                            error_type_key="invalid_operation",
                            category="concurrency",
                            fixes=[
                                "Prefer loop-local temporaries inside parallel regions",
                                "Use message passing/channels for shared-state coordination",
                                "Aggregate results after the parallel loop instead of mutating outer variables",
                            ],
                        )
                    )

                assign_match = set_assign_pattern.match(raw_line)
                if assign_match:
                    target = assign_match.group(1)
                    if target in declared_before and target != ctx['loop_var']:
                        _emit_capture(
                            target,
                            assign_match.start(1),
                            assign_match.end(1),
                            "writes to outer variable",
                        )

                member_match = member_assign_pattern.match(raw_line)
                if member_match:
                    base_obj = member_match.group(1)
                    if base_obj in declared_before and base_obj != ctx['loop_var']:
                        _emit_capture(
                            base_obj,
                            member_match.start(1),
                            member_match.end(1),
                            "mutates outer object",
                        )

                index_match = index_assign_pattern.match(raw_line)
                if index_match:
                    base_seq = index_match.group(1)
                    if base_seq in declared_before and base_seq != ctx['loop_var']:
                        _emit_capture(
                            base_seq,
                            index_match.start(1),
                            index_match.end(1),
                            "mutates outer collection",
                        )

            # Track declarations/assignments visible to subsequent lines.
            assign_match = set_assign_pattern.match(raw_line)
            if assign_match:
                declared_before.add(assign_match.group(1))

        return diagnostics

    def _infer_variable_types(self, lines: List[str]) -> Dict[str, str]:
        """Infer simple variable types for diagnostics checks."""
        types: Dict[str, str] = {}

        typed_decl_pattern = re.compile(
            r'^\s*set\s+(\w+)\s+as\s+([A-Za-z_][A-Za-z0-9_]*)(?:\s*<[^>]+>)?\s+to\s+(.+)$',
            re.IGNORECASE,
        )
        create_channel_pattern = re.compile(
            r'^\s*set\s+(\w+)\s+to\s+create\s+channel\b',
            re.IGNORECASE,
        )
        set_pattern = re.compile(r'^\s*set\s+(\w+)\s+to\s+(.+)$', re.IGNORECASE)

        for line in lines:
            typed_match = typed_decl_pattern.match(line)
            if typed_match:
                name = typed_match.group(1)
                declared_type = typed_match.group(2)
                if declared_type.lower() == "channel":
                    types[name] = "Channel"
                else:
                    types[name] = declared_type
                continue

            if create_channel_pattern.match(line):
                name = create_channel_pattern.match(line).group(1)
                types[name] = "Channel"
                continue

            set_match = set_pattern.match(line)
            if not set_match:
                continue

            name, value = set_match.groups()
            stripped = value.strip()

            if stripped.startswith('"') and stripped.endswith('"'):
                types[name] = "String"
            elif re.fullmatch(r'-?\d+', stripped):
                types[name] = "Integer"
            elif re.fullmatch(r'-?\d+\.\d+', stripped):
                types[name] = "Float"
            elif stripped.lower() in {"true", "false"}:
                types[name] = "Boolean"

        return types
    
    def get_workspace_diagnostics(self) -> Dict[str, List[Dict]]:
        """
        Get diagnostics for all files in workspace.
        
        Returns:
            Dict mapping URIs to their diagnostics
        """
        return self.file_diagnostics_cache.copy()


__all__ = ['DiagnosticsProvider']
