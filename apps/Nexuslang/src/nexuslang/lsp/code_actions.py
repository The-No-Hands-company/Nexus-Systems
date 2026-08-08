"""
Code Actions Provider
=====================

Provides quick fixes and refactoring actions using AST-based analysis.
"""

import re
from typing import List, Dict, Optional
from ..parser.lexer import Lexer
from ..parser.parser import Parser
from ..analysis import ASTSymbolExtractor, SymbolTable, SymbolKind
from ..errors import NxlError
from .formatter import NexusLangFormatter


_RECOVERABLE_LSP_EXCEPTIONS = (
    NxlError,
    RuntimeError,
    ValueError,
    TypeError,
    AttributeError,
    OSError,
    UnicodeError,
)


class CodeActionsProvider:
    """
    Provides code actions and refactorings using AST-based analysis.
    
    Actions:
    - Fix unclosed strings
    - Remove unused variables
    - Add missing imports
    - Convert types
    - Extract function
    - Add type annotations
    - Organize imports
    - Rename symbol
    """
    
    # Action kinds (LSP standard)
    KIND_QUICKFIX = "quickfix"
    KIND_REFACTOR = "refactor"
    KIND_REFACTOR_EXTRACT = "refactor.extract"
    KIND_SOURCE_ORGANIZE_IMPORTS = "source.organizeImports"
    
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
        except _RECOVERABLE_LSP_EXCEPTIONS:
            return self.symbol_tables.get(uri, None)
    
    def get_code_actions(self, uri: str, text: str, range_params: Dict, diagnostics: List[Dict]) -> List[Dict]:
        """
        Get code actions for a range using AST-based analysis.
        
        Args:
            uri: Document URI
            text: Document text
            range_params: Range to check
            diagnostics: Diagnostics in range
            
        Returns:
            List of code actions
        """
        actions = []
        
        # Build symbol table
        symbol_table = self._get_or_build_symbol_table(text, uri)
        
        # Organize imports (actual document edit, not just a command dispatch)
        organize_action = self._organize_imports(uri, text)
        if organize_action:
            actions.append(organize_action)

        format_action = self._format_document_action(uri, text)
        if format_action:
            actions.append(format_action)

        # Toggle comment for the selected / cursor range
        toggle_action = self._toggle_comment(uri, text, range_params)
        if toggle_action:
            actions.append(toggle_action)

        # Check if we have a selection for extract actions
        if self._has_selection(range_params):
            # Extract selection into a function (command dispatch)
            actions.append({
                "title": "Extract function",
                "kind": self.KIND_REFACTOR_EXTRACT,
                "command": {
                    "title": "Extract function",
                    "command": "nexuslang.extractFunction",
                    "arguments": [uri, range_params]
                }
            })
            # Extract selection into a named variable
            extract_var = self._extract_variable(uri, text, range_params)
            if extract_var:
                actions.append(extract_var)
        
        # Inline variable when cursor sits on a single-use variable declaration
        start = range_params["start"]
        inline_action = self._inline_variable(uri, text, start["line"], start["character"])
        if inline_action:
            actions.append(inline_action)

        # Get symbol at cursor for targeted actions
        if symbol_table:
            symbol = symbol_table.get_symbol_at_position(uri, start["line"], start["character"])
            
            if symbol and not symbol.type_annotation:
                actions.append({
                    "title": f"Add type annotation to '{symbol.name}'",
                    "kind": self.KIND_QUICKFIX,
                    "edit": {
                        "changes": {
                            uri: [{
                                "range": {
                                    "start": {
                                        "line": symbol.location.line,
                                        "character": symbol.location.column + len(symbol.name)
                                    },
                                    "end": {
                                        "line": symbol.location.line,
                                        "character": symbol.location.column + len(symbol.name)
                                    }
                                },
                                "newText": " as Any"
                            }]
                        }
                    }
                })
        
        # Actions for specific diagnostics
        for diagnostic in diagnostics:
            message = diagnostic.get('message', '')
            message_lower = message.lower()
            diag_range = diagnostic.get('range', {})

            # Prefer structured fixes from diagnostic payload
            structured_actions = self._actions_from_structured_fixes(uri, text, diagnostic)
            if structured_actions:
                actions.extend(structured_actions)
                continue
            
            # Fix unclosed string
            if 'Unclosed string' in message or 'Unterminated string' in message:
                fix = self._fix_unclosed_string(uri, text, diag_range)
                if fix:
                    fix["diagnostics"] = [diagnostic]
                    actions.append(fix)
            
            # Fix undefined variable
            if 'undefined' in message_lower:
                match = re.search(r"'(\w+)'", message)
                if match:
                    var_name = match.group(1)
                    declare_action = self._declare_variable_action(uri, diag_range, var_name, diagnostic)
                    if declare_action:
                        actions.append(declare_action)

            # Parser quick fixes for common syntax diagnostics.
            if ('syntax error' in message_lower or 'expected' in message_lower):
                missing_end = self._add_missing_end_action(uri, text, diag_range, diagnostic)
                if missing_end:
                    actions.append(missing_end)

                missing_paren = self._add_missing_closing_paren_action(uri, text, diag_range, diagnostic)
                if missing_paren:
                    actions.append(missing_paren)

            # Generic typing quick fix from message alone (without structured fixes).
            if 'type error' in message_lower and "expected '" in message_lower:
                type_fix = self._add_type_annotation(uri, text, diag_range, message)
                if type_fix:
                    type_fix["diagnostics"] = [diagnostic]
                    actions.append(type_fix)

            # Boolean condition quick fix for control flow checks.
            if 'must be a boolean' in message_lower:
                bool_fix = self._convert_condition_to_boolean_check(uri, text, diag_range, diagnostic)
                if bool_fix:
                    actions.append(bool_fix)
        
        return actions

    def _add_missing_end_action(self, uri: str, text: str, diag_range: Dict, diagnostic: Dict) -> Optional[Dict]:
        """Offer a quick fix to add a missing 'end' when parser diagnostics suggest it."""
        message = diagnostic.get('message', '').lower()
        if 'end' not in message:
            return None

        insert_line = max(0, diag_range.get('end', {}).get('line', 0))
        return {
            "title": "Add missing end",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": insert_line, "character": 0},
                            "end": {"line": insert_line, "character": 0},
                        },
                        "newText": "end\n",
                    }]
                }
            },
        }

    def _add_missing_closing_paren_action(self, uri: str, text: str, diag_range: Dict, diagnostic: Dict) -> Optional[Dict]:
        """Offer a quick fix to insert a missing closing ')' for parser diagnostics."""
        message = diagnostic.get('message', '').lower()
        if "')'" not in message and 'parenthesis' not in message and 'paren' not in message:
            return None

        line_num = max(0, diag_range.get('start', {}).get('line', 0))
        lines = text.split('\n')
        if line_num >= len(lines):
            return None

        insert_char = len(lines[line_num])
        return {
            "title": "Add closing )",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": insert_char},
                            "end": {"line": line_num, "character": insert_char},
                        },
                        "newText": ")",
                    }]
                }
            },
        }

    def _convert_condition_to_boolean_check(self, uri: str, text: str, diag_range: Dict, diagnostic: Dict) -> Optional[Dict]:
        """Convert control-flow condition to explicit boolean check by appending 'is true'."""
        line_num = max(0, diag_range.get('start', {}).get('line', 0))
        lines = text.split('\n')
        if line_num >= len(lines):
            return None

        line = lines[line_num]
        match = re.match(r'^(\s*)(if|while)\s+(.+?)\s*$', line, re.IGNORECASE)
        if not match:
            return None

        indent, keyword, condition = match.groups()
        if re.search(r'\bis\s+(true|false)\b', condition, re.IGNORECASE):
            return None

        new_line = f"{indent}{keyword} {condition} is true"
        return {
            "title": "Convert condition to explicit boolean check",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num, "character": len(line)},
                        },
                        "newText": new_line,
                    }]
                }
            },
        }
    
    def _has_selection(self, range_params: Dict) -> bool:
        """Check if range represents a non-empty selection."""
        start = range_params["start"]
        end = range_params["end"]
        return start["line"] != end["line"] or start["character"] != end["character"]
    
    def _fix_unclosed_string(self, uri: str, text: str, diag_range: Dict) -> Optional[Dict]:
        """Fix unclosed string by adding closing quote."""
        line_num = diag_range.get('start', {}).get('line', 0)
        lines = text.split('\n')
        
        if line_num >= len(lines):
            return None
        
        line = lines[line_num]
        
        # Find the unclosed quote
        if '"' in line and line.count('"') % 2 != 0:
            # Add closing quote at end of line
            return {
                "title": "Add closing quote",
                "kind": "quickfix",
                "diagnostics": [{"range": diag_range}],
                "edit": {
                    "changes": {
                        uri: [{
                            "range": {
                                "start": {"line": line_num, "character": len(line)},
                                "end": {"line": line_num, "character": len(line)}
                            },
                            "newText": '"'
                        }]
                    }
                }
            }
        
        return None

    def _declare_variable_action(self, uri: str, diag_range: Dict, var_name: str, diagnostic: Optional[Dict] = None) -> Optional[Dict]:
        """Create quick fix action to declare a missing variable."""
        start_line = diag_range.get("start", {}).get("line")
        if start_line is None:
            return None

        action = {
            "title": f"Declare '{var_name}'",
            "kind": self.KIND_QUICKFIX,
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": start_line, "character": 0},
                            "end": {"line": start_line, "character": 0}
                        },
                        "newText": f"set {var_name} to Nothing\n"
                    }]
                }
            },
        }
        if diagnostic:
            action["diagnostics"] = [diagnostic]
        return action

    def _actions_from_structured_fixes(self, uri: str, text: str, diagnostic: Dict) -> List[Dict]:
        """Build code actions from structured diagnostic.data.fixes payload."""
        actions: List[Dict] = []
        data = diagnostic.get("data", {})
        if not isinstance(data, dict):
            return actions

        fixes = data.get("fixes", [])
        if not isinstance(fixes, list) or not fixes:
            return actions

        message = diagnostic.get("message", "")
        message_lower = message.lower()
        diag_range = diagnostic.get("range", {})
        ownership_ctx = data.get("ownership") if isinstance(data.get("ownership"), dict) else {}
        ownership_var = ownership_ctx.get("variable")
        ownership_op = ownership_ctx.get("operation")

        for fix_text in fixes:
            if not isinstance(fix_text, str):
                continue
            fix_lower = fix_text.lower()

            if "quote" in fix_lower and ("unclosed string" in message_lower or "unterminated string" in message_lower):
                fix = self._fix_unclosed_string(uri, text, diag_range)
                if fix:
                    fix["diagnostics"] = [diagnostic]
                    actions.append(fix)
                continue

            if "undefined" in message_lower and ("declare" in fix_lower or "define" in fix_lower or "initialize" in fix_lower):
                match = re.search(r"'(\w+)'", message)
                if match:
                    action = self._declare_variable_action(uri, diag_range, match.group(1), diagnostic)
                    if action:
                        actions.append(action)
                continue

            if "unused variable" in message_lower and ("remove" in fix_lower or "delete" in fix_lower):
                var_name = self._extract_variable_name(message)
                if var_name:
                    action = self._remove_unused_variable(uri, text, var_name, diag_range)
                    if action:
                        action["diagnostics"] = [diagnostic]
                        actions.append(action)
                continue

            if "type" in message_lower and ("annotation" in fix_lower or "add type" in fix_lower):
                action = self._add_type_annotation(uri, text, diag_range, message)
                if action:
                    action["diagnostics"] = [diagnostic]
                    actions.append(action)
                continue

            if "closed channel" in message_lower and "recreate channel" in fix_lower:
                match = re.search(r"'([^']+)'", message)
                if match:
                    action = self._recreate_channel_before_operation_action(
                        uri,
                        diag_range,
                        match.group(1),
                        diagnostic,
                    )
                    if action:
                        actions.append(action)
                continue

            if "closed channel" in message_lower and "move" in fix_lower and "close" in fix_lower:
                match = re.search(r"'([^']+)'", message)
                if match:
                    action = self._move_close_after_operation_action(
                        uri,
                        text,
                        diag_range,
                        match.group(1),
                        diagnostic,
                    )
                    if action:
                        actions.append(action)
                continue

            is_contract_context = any(k in message_lower for k in ("require", "ensure", "guarantee", "invariant", "contract"))

            if is_contract_context and ("add contract failure message" in fix_lower or "contract failure message" in fix_lower):
                action = self._add_contract_message_action(uri, text, diag_range, diagnostic)
                if action:
                    actions.append(action)
                continue

            if is_contract_context and ("explicit boolean check" in fix_lower or "boolean condition" in fix_lower):
                action = self._convert_contract_condition_to_boolean(uri, text, diag_range, diagnostic)
                if action:
                    actions.append(action)
                continue

            if is_contract_context and ("use string literal" in fix_lower or "convert contract message to string" in fix_lower):
                action = self._convert_contract_message_to_string(uri, text, diag_range, diagnostic)
                if action:
                    actions.append(action)

            if "ownership error:" in message_lower and ownership_var:
                if "drop active borrows" in fix_lower:
                    move_conflict = ownership_op == "move" or "move" in message_lower
                    specialized_move_actions: List[Dict] = []

                    if ownership_op == "borrow_mutable" or "mutable borrow" in message_lower:
                        mutable_action = self._insert_drop_mutable_borrow_before_operation_action(
                            uri,
                            diag_range,
                            ownership_var,
                            diagnostic,
                        )
                        if mutable_action:
                            actions.append(mutable_action)

                    if move_conflict:
                        narrow_scope_action = self._narrow_borrow_scope_before_move_action(
                            uri,
                            text,
                            diag_range,
                            ownership_var,
                            diagnostic,
                        )
                        if narrow_scope_action:
                            specialized_move_actions.append(narrow_scope_action)

                        reorder_action = self._reorder_move_after_drop_borrow_action(
                            uri,
                            text,
                            diag_range,
                            ownership_var,
                            diagnostic,
                        )
                        if reorder_action:
                            specialized_move_actions.append(reorder_action)

                    if move_conflict and specialized_move_actions:
                        specialized_move_actions.sort(key=self._specialized_move_fix_sort_key)
                        for idx, ranked_action in enumerate(specialized_move_actions):
                            ranked_action["isPreferred"] = idx == 0
                        actions.extend(specialized_move_actions)
                    else:
                        action = self._insert_drop_borrow_before_operation_action(
                            uri,
                            diag_range,
                            ownership_var,
                            diagnostic,
                        )
                        if action:
                            actions.append(action)
                    continue

                if "avoid mutable borrow" in fix_lower:
                    action = self._convert_mutable_borrow_to_immutable_action(
                        uri,
                        text,
                        diag_range,
                        ownership_var,
                        diagnostic,
                    )
                    if action:
                        actions.append(action)
                    continue

        return actions

    def _specialized_move_fix_sort_key(self, action: Dict) -> tuple:
        """Provide deterministic ordering for move-conflict specialized quick fixes.

        Preferred order:
        1) local reorder move-after-drop edits
        2) borrow-scope narrowing rewrites
        3) any future specialized actions (stable alphabetical fallback)
        """
        title = str(action.get("title", ""))
        title_lower = title.lower()
        if "reorder move" in title_lower:
            return (0, title_lower)
        if "narrow borrow scope" in title_lower:
            return (1, title_lower)
        return (2, title_lower)

    def _insert_drop_borrow_before_operation_action(
        self,
        uri: str,
        diag_range: Dict,
        var_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Insert `drop borrow <var>` immediately before the flagged operation."""
        line_num = diag_range.get("start", {}).get("line")
        if line_num is None:
            return None

        return {
            "title": f"Drop active borrow of '{var_name}' before this operation",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num, "character": 0},
                        },
                        "newText": f"drop borrow {var_name}\n",
                    }]
                }
            },
        }

    def _insert_drop_mutable_borrow_before_operation_action(
        self,
        uri: str,
        diag_range: Dict,
        var_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Insert `drop borrow mutable <var>` immediately before the flagged operation."""
        line_num = diag_range.get("start", {}).get("line")
        if line_num is None:
            return None

        return {
            "title": f"Drop mutable borrow of '{var_name}' before this operation",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num, "character": 0},
                        },
                        "newText": f"drop borrow mutable {var_name}\n",
                    }]
                }
            },
        }

    def _reorder_move_after_drop_borrow_action(
        self,
        uri: str,
        text: str,
        diag_range: Dict,
        var_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Reorder `move <var>` after a nearby `drop borrow <var>` when safe.

        Safety policy (conservative):
        - The flagged line must contain `move <var>`.
        - The nearest non-empty line below must be exactly `drop borrow <var>` or
          `drop borrow mutable <var>` in the same indentation block.
        - No inline comments or attached directive/comment metadata can be present
          on the statements being reordered.
        """
        move_line_num = diag_range.get("start", {}).get("line")
        if move_line_num is None:
            return None

        lines = text.split("\n")
        if move_line_num < 0 or move_line_num >= len(lines):
            return None

        move_line = lines[move_line_num]
        move_indent = len(move_line) - len(move_line.lstrip())
        move_pattern = rf"\bmove\s+{re.escape(var_name)}\b"
        if not re.search(move_pattern, move_line, re.IGNORECASE):
            return None

        drop_line_num = move_line_num + 1
        while drop_line_num < len(lines) and not lines[drop_line_num].strip():
            drop_line_num += 1
        if drop_line_num >= len(lines):
            return None

        drop_line = lines[drop_line_num]
        drop_indent = len(drop_line) - len(drop_line.lstrip())
        drop_pattern = rf"^\s*drop\s+borrow(?:\s+mutable)?\s+{re.escape(var_name)}\b\s*$"
        if not re.match(drop_pattern, drop_line, re.IGNORECASE):
            return None
        if drop_indent != move_indent:
            return None

        if self._has_attached_ownership_metadata(lines, move_line_num, var_name):
            return None
        if self._has_attached_ownership_metadata(lines, drop_line_num, var_name):
            return None

        segment_lines = lines[move_line_num:drop_line_num + 1]
        reordered = segment_lines[1:] + [segment_lines[0]]

        if drop_line_num + 1 < len(lines):
            replace_end = {"line": drop_line_num + 1, "character": 0}
            replacement = "\n".join(reordered) + "\n"
        else:
            replace_end = {"line": drop_line_num, "character": len(lines[drop_line_num])}
            replacement = "\n".join(reordered)

        return {
            "title": f"Reorder move of '{var_name}' after drop borrow (safe reorder)",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": move_line_num, "character": 0},
                            "end": replace_end,
                        },
                        "newText": replacement,
                    }]
                }
            },
        }

    def _narrow_borrow_scope_before_move_action(
        self,
        uri: str,
        text: str,
        diag_range: Dict,
        var_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Wrap a nearby borrow region into a tight scope before a conflicting move.

        This rewrite is intentionally conservative and only offered when:
        - The diagnostic line contains `move <var>`.
        - A preceding same-indentation `set <alias> to borrow( mutable) <var>` exists.
        - No explicit drop of `<var>` already exists in the candidate region.
        - The borrow alias is not referenced after the move line.
        - Neither statement has attached inline/directive comment metadata.
        """
        move_line_num = diag_range.get("start", {}).get("line")
        if move_line_num is None:
            return None

        lines = text.split("\n")
        if move_line_num < 0 or move_line_num >= len(lines):
            return None

        move_line = lines[move_line_num]
        move_indent_text = self._leading_whitespace(move_line)
        move_pattern = rf"\bmove\s+{re.escape(var_name)}\b"
        if not re.search(move_pattern, move_line, re.IGNORECASE):
            return None

        borrow_decl_re = re.compile(
            rf"^(?P<indent>\s*)set\s+(?P<alias>[A-Za-z_]\w*)\s+to\s+borrow(?P<mutable>\s+mutable)?\s+{re.escape(var_name)}\b\s*$",
            re.IGNORECASE,
        )
        drop_re = re.compile(
            rf"^\s*drop\s+borrow(?:\s+mutable)?\s+{re.escape(var_name)}\b",
            re.IGNORECASE,
        )

        borrow_line_num: Optional[int] = None
        borrow_alias: Optional[str] = None
        borrow_is_mutable = False

        for idx in range(move_line_num - 1, -1, -1):
            line = lines[idx]
            if not line.strip():
                continue

            if self._leading_whitespace(line) != move_indent_text:
                continue

            match = borrow_decl_re.match(line)
            if match:
                borrow_line_num = idx
                borrow_alias = match.group("alias")
                borrow_is_mutable = bool(match.group("mutable"))
                break

        if borrow_line_num is None or not borrow_alias:
            return None

        if self._has_attached_ownership_metadata(lines, move_line_num, var_name):
            return None
        if self._has_attached_ownership_metadata(lines, borrow_line_num, var_name):
            return None

        region = lines[borrow_line_num:move_line_num]
        if any(drop_re.match(region_line) for region_line in region):
            return None

        control_flow_delimiter_re = re.compile(
            r"^\s*(?:if\b|else\b|while\b|for\b|function\b|end\b)",
            re.IGNORECASE,
        )
        # Match writes to the owned root variable, including index/property chains.
        # Examples: set x to 1, set x[0] to 1, set x.field to 1, set x[0].y to 1
        write_to_owned_var_re = re.compile(
            rf"^\s*set\s+{re.escape(var_name)}(?:\s*(?:\[[^\]]+\]|\.[A-Za-z_]\w*))*\s+to\b",
            re.IGNORECASE,
        )
        write_to_alias_re = re.compile(
            rf"^\s*set\s+{re.escape(borrow_alias)}(?:\s*(?:\[[^\]]+\]|\.[A-Za-z_]\w*))*\s+to\b",
            re.IGNORECASE,
        )
        for idx, region_line in enumerate(region):
            if control_flow_delimiter_re.match(region_line):
                return None
            if write_to_owned_var_re.match(region_line):
                return None
            if idx > 0 and write_to_alias_re.match(region_line):
                return None

        for idx in range(move_line_num):
            if idx == borrow_line_num:
                continue
            line = lines[idx]
            if not line.strip():
                continue
            if self._leading_whitespace(line) != move_indent_text:
                continue
            if borrow_decl_re.match(line):
                return None

        alias_re = re.compile(rf"\b{re.escape(borrow_alias)}\b")
        for idx in range(move_line_num + 1, len(lines)):
            tail_line = lines[idx]
            if not tail_line.strip() or tail_line.lstrip().startswith("#"):
                continue
            if alias_re.search(tail_line):
                return None

        indent_unit = "\t" if "\t" in move_indent_text else "    "
        inner_prefix = move_indent_text + indent_unit

        scoped_lines = [f"{move_indent_text}if true"]
        for original in region:
            if original.startswith(move_indent_text):
                scoped_lines.append(inner_prefix + original[len(move_indent_text):])
            else:
                scoped_lines.append(inner_prefix + original.lstrip())

        drop_stmt = f"drop borrow{' mutable' if borrow_is_mutable else ''} {var_name}"
        scoped_lines.append(inner_prefix + drop_stmt)
        scoped_lines.append(f"{move_indent_text}end")

        return {
            "title": f"Narrow borrow scope of '{var_name}' before move",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": borrow_line_num, "character": 0},
                            "end": {"line": move_line_num, "character": 0},
                        },
                        "newText": "\n".join(scoped_lines) + "\n",
                    }]
                }
            },
        }

    def _leading_whitespace(self, line: str) -> str:
        """Return leading whitespace prefix for indentation-sensitive rewrites."""
        return line[:len(line) - len(line.lstrip())]

    def _has_attached_ownership_metadata(self, lines: List[str], line_num: int, var_name: str) -> bool:
        """Return True if a move/drop statement has attached metadata comments/directives."""
        if line_num < 0 or line_num >= len(lines):
            return False

        line = lines[line_num]
        indent = len(line) - len(line.lstrip())

        inline_ownership = re.match(
            rf"^\s*(?:drop\s+borrow(?:\s+mutable)?\s+{re.escape(var_name)}\b|.*\bmove\s+{re.escape(var_name)}\b).*#.+$",
            line,
            re.IGNORECASE,
        )
        if inline_ownership:
            return True

        comment_line_re = re.compile(r"^\s*#")
        directive_line_re = re.compile(r"^\s*(?:@\w+|pragma\b|directive\b)", re.IGNORECASE)
        directive_comment_re = re.compile(
            r"^\s*#\s*(?:noqa\b|nolint\b|lint:|pragma\b|directive\b|nlpl:|nexuslang:)",
            re.IGNORECASE,
        )

        idx = line_num - 1
        while idx >= 0:
            raw = lines[idx]
            if not raw.strip():
                break

            raw_indent = len(raw) - len(raw.lstrip())
            is_comment = bool(comment_line_re.match(raw))
            is_directive = bool(directive_line_re.match(raw) or directive_comment_re.match(raw))

            if raw_indent != indent:
                break
            if not (is_comment or is_directive):
                break

            return True

        return False

    def _convert_mutable_borrow_to_immutable_action(
        self,
        uri: str,
        text: str,
        diag_range: Dict,
        var_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Convert `borrow mutable <var>` to `borrow <var>` on the flagged line."""
        line_num = diag_range.get("start", {}).get("line")
        if line_num is None:
            return None

        lines = text.split("\n")
        if line_num < 0 or line_num >= len(lines):
            return None

        line = lines[line_num]
        pattern = rf"\bborrow\s+mutable\s+{re.escape(var_name)}\b"
        match = re.search(pattern, line, re.IGNORECASE)
        if not match:
            return None

        replacement = re.sub(pattern, f"borrow {var_name}", line, count=1, flags=re.IGNORECASE)
        return {
            "title": f"Convert mutable borrow of '{var_name}' to immutable borrow",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num, "character": len(line)},
                        },
                        "newText": replacement,
                    }]
                }
            },
        }

    def _add_contract_message_action(self, uri: str, text: str, diag_range: Dict, diagnostic: Dict) -> Optional[Dict]:
        """Append a default message clause to a contract statement if missing."""
        line_num = diag_range.get("start", {}).get("line", 0)
        lines = text.split("\n")
        if line_num >= len(lines):
            return None

        line = lines[line_num]
        m = re.match(r'^(\s*)(require|ensure|guarantee|invariant)\b(.+)$', line, re.IGNORECASE)
        if not m:
            return None
        if re.search(r'\bmessage\b', line, re.IGNORECASE):
            return None

        insert_at = len(line.rstrip())
        return {
            "title": "Add contract failure message",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": insert_at},
                            "end": {"line": line_num, "character": insert_at},
                        },
                        "newText": ' message "contract failed"',
                    }]
                }
            },
        }

    def _recreate_channel_before_operation_action(
        self,
        uri: str,
        diag_range: Dict,
        channel_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Insert channel recreation before a send/receive on a closed channel."""
        line_num = diag_range.get("start", {}).get("line")
        if line_num is None:
            return None

        return {
            "title": f"Recreate channel '{channel_name}' before operation",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num, "character": 0},
                        },
                        "newText": f"set {channel_name} to create channel\n",
                    }]
                }
            },
        }

    def _move_close_after_operation_action(
        self,
        uri: str,
        text: str,
        diag_range: Dict,
        channel_name: str,
        diagnostic: Dict,
    ) -> Optional[Dict]:
        """Move a channel close directly after the flagged operation when proven safe.

        Safety policy (conservative):
        - Operation must have an immediately preceding non-empty line in the same indentation block.
        - That line must be exactly `close <channel_name>`.
        - No block-boundary crossing is allowed by construction (single adjacent statement swap).
        """
        op_line_num = diag_range.get("start", {}).get("line")
        if op_line_num is None:
            return None

        lines = text.split("\n")
        if op_line_num < 0 or op_line_num >= len(lines):
            return None

        op_line = lines[op_line_num]
        op_indent = len(op_line) - len(op_line.lstrip())

        prev_non_empty = op_line_num - 1
        while prev_non_empty >= 0 and not lines[prev_non_empty].strip():
            prev_non_empty -= 1
        if prev_non_empty < 0:
            return None

        close_line = lines[prev_non_empty]
        close_indent = len(close_line) - len(close_line.lstrip())
        close_pattern = rf'^\s*close\s+{re.escape(channel_name)}\b\s*$'
        if not re.match(close_pattern, close_line, re.IGNORECASE):
            return None
        if close_indent != op_indent:
            return None
        if self._has_attached_close_metadata(lines, prev_non_empty, channel_name):
            return None

        # Rewrite only the local contiguous segment to keep edit deterministic.
        segment_lines = lines[prev_non_empty:op_line_num + 1]
        reordered = segment_lines[1:] + [segment_lines[0]]

        if op_line_num + 1 < len(lines):
            replace_end = {"line": op_line_num + 1, "character": 0}
            replacement = "\n".join(reordered) + "\n"
        else:
            replace_end = {"line": op_line_num, "character": len(lines[op_line_num])}
            replacement = "\n".join(reordered)

        return {
            "title": f"Move close '{channel_name}' after this operation (safe reorder)",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": prev_non_empty, "character": 0},
                            "end": replace_end,
                        },
                        "newText": replacement,
                    }]
                }
            },
        }

    def _has_attached_close_metadata(self, lines: List[str], close_line_num: int, channel_name: str) -> bool:
        """Return True if comments/directives are attached to the close statement.

        Attached metadata includes:
        - Inline trailing comments on the close line.
        - Immediately preceding comment/directive lines (same indentation block).
        """
        if close_line_num < 0 or close_line_num >= len(lines):
            return False

        close_line = lines[close_line_num]
        close_indent = len(close_line) - len(close_line.lstrip())

        # Inline comment on close line.
        inline = re.match(
            rf'^\s*close\s+{re.escape(channel_name)}\b\s*#.+$',
            close_line,
            re.IGNORECASE,
        )
        if inline:
            return True

        comment_line_re = re.compile(r'^\s*#')
        directive_line_re = re.compile(r'^\s*(?:@\w+|pragma\b|directive\b)', re.IGNORECASE)
        directive_comment_re = re.compile(r'^\s*#\s*(?:noqa\b|nolint\b|lint:|pragma\b|directive\b|nlpl:|nexuslang:)', re.IGNORECASE)

        idx = close_line_num - 1
        while idx >= 0:
            raw = lines[idx]
            if not raw.strip():
                break

            indent = len(raw) - len(raw.lstrip())
            stripped = raw.lstrip()
            is_comment = bool(comment_line_re.match(raw))
            is_directive = bool(directive_line_re.match(raw) or directive_comment_re.match(raw))

            if indent != close_indent:
                break
            if not (is_comment or is_directive):
                break

            # Any contiguous metadata line immediately above close blocks reordering.
            return True

        return False

    def _convert_contract_condition_to_boolean(self, uri: str, text: str, diag_range: Dict, diagnostic: Dict) -> Optional[Dict]:
        """Convert contract condition to explicit boolean check by appending 'is true'."""
        line_num = diag_range.get("start", {}).get("line", 0)
        lines = text.split("\n")
        if line_num >= len(lines):
            return None

        line = lines[line_num]
        m = re.match(r'^(\s*)(require|ensure|guarantee|invariant)\s+(.+?)(\s+message\s+.+)?\s*$', line, re.IGNORECASE)
        if not m:
            return None

        indent, keyword, condition, message_clause = m.groups()
        condition = condition.strip()
        if re.search(r'\bis\s+(true|false)\b', condition, re.IGNORECASE):
            return None

        rebuilt = f"{indent}{keyword} {condition} is true"
        if message_clause:
            rebuilt += message_clause

        return {
            "title": "Convert contract condition to explicit boolean check",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num, "character": len(line)},
                        },
                        "newText": rebuilt,
                    }]
                }
            },
        }

    def _convert_contract_message_to_string(self, uri: str, text: str, diag_range: Dict, diagnostic: Dict) -> Optional[Dict]:
        """Wrap non-string contract message expression in quotes as a safe fallback."""
        line_num = diag_range.get("start", {}).get("line", 0)
        lines = text.split("\n")
        if line_num >= len(lines):
            return None

        line = lines[line_num]
        message_match = re.search(r'\bmessage\s+(.+)$', line, re.IGNORECASE)
        if not message_match:
            return None

        message_expr = message_match.group(1).strip()
        if message_expr.startswith('"') and message_expr.endswith('"'):
            return None

        start = message_match.start(1)
        end = len(line)
        replacement = f'"{message_expr}"'

        return {
            "title": "Convert contract message to string literal",
            "kind": self.KIND_QUICKFIX,
            "diagnostics": [diagnostic],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": start},
                            "end": {"line": line_num, "character": end},
                        },
                        "newText": replacement,
                    }]
                }
            },
        }
    
    def _remove_unused_variable(self, uri: str, text: str, var_name: str, diag_range: Dict) -> Optional[Dict]:
        """Remove unused variable declaration."""
        line_num = diag_range.get('start', {}).get('line', 0)
        lines = text.split('\n')
        
        if line_num >= len(lines):
            return None
        
        return {
            "title": f"Remove unused variable '{var_name}'",
            "kind": "quickfix",
            "diagnostics": [{"range": diag_range}],
            "edit": {
                "changes": {
                    uri: [{
                        "range": {
                            "start": {"line": line_num, "character": 0},
                            "end": {"line": line_num + 1, "character": 0}
                        },
                        "newText": ""
                    }]
                }
            }
        }
    
    def _add_type_annotation(self, uri: str, text: str, diag_range: Dict, message: str) -> Optional[Dict]:
        """Add missing type annotation."""
        # Extract expected type from error message
        type_match = re.search(r"expected '([^']+)'", message, re.IGNORECASE)
        if not type_match:
            return None
        
        expected_type = type_match.group(1)
        line_num = diag_range.get('start', {}).get('line', 0)
        lines = text.split('\n')
        
        if line_num >= len(lines):
            return None
        
        line = lines[line_num]
        
        # Check if it's a variable declaration without type
        var_match = re.search(r'set\s+(\w+)\s+to', line, re.IGNORECASE)
        if var_match:
            var_name = var_match.group(1)
            # Insert type annotation after variable name
            insert_pos = var_match.end(1)
            
            return {
                "title": f"Add type annotation: {expected_type}",
                "kind": "quickfix",
                "diagnostics": [{"range": diag_range}],
                "edit": {
                    "changes": {
                        uri: [{
                            "range": {
                                "start": {"line": line_num, "character": insert_pos},
                                "end": {"line": line_num, "character": insert_pos}
                            },
                            "newText": f" as {expected_type}"
                        }]
                    }
                }
            }
        
        return None
    
    def _get_refactoring_actions(self, uri: str, text: str, range_params: Dict) -> List[Dict]:
        """Get general refactoring actions available in range."""
        actions = []
        
        start_line = range_params.get('start', {}).get('line', 0)
        end_line = range_params.get('end', {}).get('line', 0)
        
        lines = text.split('\n')
        selected_text = '\n'.join(lines[start_line:end_line + 1]) if start_line < len(lines) else ""
        
        # Extract function (if multiple lines selected)
        if end_line > start_line and len(selected_text.strip()) > 0:
            actions.append({
                "title": "Extract to function",
                "kind": "refactor.extract",
                "command": {
                    "title": "Extract to function",
                    "command": "nexuslang.extractFunction",
                    "arguments": [uri, range_params]
                }
            })
        
        # Convert to list comprehension (if it's a for loop creating a list)
        if 'for each' in selected_text.lower() and 'create list' in selected_text.lower():
            actions.append({
                "title": "Convert to list comprehension",
                "kind": "refactor.rewrite",
                "command": {
                    "title": "Convert to list comprehension",
                    "command": "nexuslang.convertToComprehension",
                    "arguments": [uri, range_params]
                }
            })
        
        return actions
    
    def _extract_variable_name(self, message: str) -> Optional[str]:
        """Extract variable name from diagnostic message."""
        match = re.search(r"variable '([^']+)'", message)
        return match.group(1) if match else None

    # ------------------------------------------------------------------
    # New Week-5 refactoring actions
    # ------------------------------------------------------------------

    def _format_document_action(self, uri: str, text: str) -> Optional[Dict]:
        """Return a format quick fix when formatter output differs from source."""
        formatter = NexusLangFormatter()
        edits = formatter.get_formatting_edits(text)
        if not edits:
            return None

        return {
            "title": "Format document (NexusLang style)",
            "kind": self.KIND_QUICKFIX,
            "edit": {"changes": {uri: edits}},
        }

    def _organize_imports(self, uri: str, text: str) -> Optional[Dict]:
        """
        Sort import statements alphabetically, preserving their relative
        block (contiguous import lines are treated as a group).

        Only produces an action when the order would actually change.
        """
        lines = text.split("\n")
        # Collect contiguous import blocks with their line ranges
        blocks: List[Dict] = []  # [{start, end, lines:[...]}]
        i = 0
        while i < len(lines):
            stripped = lines[i].strip()
            if re.match(r"^(import|from)\b", stripped, re.IGNORECASE):
                block_start = i
                block_lines = [lines[i]]
                j = i + 1
                while j < len(lines) and re.match(
                    r"^(import|from)\b", lines[j].strip(), re.IGNORECASE
                ):
                    block_lines.append(lines[j])
                    j += 1
                blocks.append({"start": block_start, "end": j - 1, "lines": block_lines})
                i = j
            else:
                i += 1

        if not blocks:
            return None

        # Build the edit changes
        changes = []
        changed = False
        for block in blocks:
            sorted_lines = sorted(block["lines"], key=lambda l: l.strip().lower())
            if sorted_lines != block["lines"]:
                changed = True
                changes.append({
                    "range": {
                        "start": {"line": block["start"], "character": 0},
                        "end": {"line": block["end"] + 1, "character": 0},
                    },
                    "newText": "\n".join(sorted_lines) + "\n",
                })

        if not changed:
            return None

        return {
            "title": "Organize imports",
            "kind": self.KIND_SOURCE_ORGANIZE_IMPORTS,
            "edit": {"changes": {uri: changes}},
        }

    def _toggle_comment(self, uri: str, text: str, range_params: Dict) -> Optional[Dict]:
        """
        Comment or uncomment every line in the range.

        If ALL non-empty lines in the range are already commented, the action
        removes the leading `# `.  Otherwise it adds `# `.
        """
        lines = text.split("\n")
        start_line = range_params["start"]["line"]
        end_line = range_params["end"]["line"]

        target_lines = [
            (i, lines[i])
            for i in range(start_line, min(end_line + 1, len(lines)))
        ]
        non_empty = [(i, l) for i, l in target_lines if l.strip()]
        if not non_empty:
            return None

        all_commented = all(l.lstrip().startswith("#") for _, l in non_empty)
        edits = []
        for i, line in non_empty:
            if all_commented:
                # Remove the leading `# ` (or `#`)
                new_line = re.sub(r"^(\s*)#\s?", r"\1", line)
            else:
                # Add `# ` at the indent level
                indent = len(line) - len(line.lstrip())
                new_line = line[:indent] + "# " + line[indent:]
            edits.append({
                "range": {
                    "start": {"line": i, "character": 0},
                    "end": {"line": i, "character": len(line)},
                },
                "newText": new_line,
            })

        title = "Uncomment lines" if all_commented else "Comment lines"
        return {
            "title": title,
            "kind": self.KIND_REFACTOR,
            "edit": {"changes": {uri: edits}},
        }

    def _extract_variable(self, uri: str, text: str, range_params: Dict) -> Optional[Dict]:
        """
        Wrap the selected expression in a new named variable.

        Inserts  ``set newValue to <selection>``  on the line above the
        selection start, and replaces the selection with ``newValue``.
        """
        lines = text.split("\n")
        start = range_params["start"]
        end = range_params["end"]
        start_line = start["line"]
        start_char = start["character"]
        end_char = end["character"]

        # Only handle single-line selections for simplicity
        if start["line"] != end["line"]:
            return None

        if start_line >= len(lines):
            return None

        selected = lines[start_line][start_char:end_char].strip()
        if not selected or len(selected) < 2:
            return None

        indent = len(lines[start_line]) - len(lines[start_line].lstrip())
        new_var = "newValue"
        insert_text = " " * indent + f"set {new_var} to {selected}\n"

        return {
            "title": f"Extract '{selected}' to variable",
            "kind": self.KIND_REFACTOR_EXTRACT,
            "edit": {
                "changes": {
                    uri: [
                        # Insert the new variable declaration above
                        {
                            "range": {
                                "start": {"line": start_line, "character": 0},
                                "end": {"line": start_line, "character": 0},
                            },
                            "newText": insert_text,
                        },
                        # Replace the selection with the variable name
                        # (line number shifts +1 after the insertion)
                        {
                            "range": {
                                "start": {"line": start_line + 1, "character": start_char},
                                "end": {"line": start_line + 1, "character": end_char},
                            },
                            "newText": new_var,
                        },
                    ]
                }
            },
        }

    def _inline_variable(self, uri: str, text: str, line_num: int, char_num: int) -> Optional[Dict]:
        """
        Replace all uses of a variable with its initializer value and remove
        the declaration.

        Triggered when the cursor is on the variable name in a
        ``set varname to value`` declaration line.
        Only acts when the variable has exactly one assignment.
        """
        lines = text.split("\n")
        if line_num >= len(lines):
            return None

        line = lines[line_num]
        m = re.match(r"^(\s*)set\s+(\w+)\s+to\s+(.+)$", line, re.IGNORECASE)
        if not m:
            return None

        var_name = m.group(2)
        value = m.group(3).strip()
        # Strip trailing comment
        value = re.sub(r"\s*#.*$", "", value).strip()

        # Cursor must be on the variable name
        indent_len = len(m.group(1))
        name_start = indent_len + len("set ")
        name_end = name_start + len(var_name)
        if not (name_start <= char_num < name_end):
            return None

        # Count all assignments to this variable (should be exactly 1)
        assign_pattern = re.compile(
            rf"^\s*set\s+{re.escape(var_name)}\s+to\b", re.IGNORECASE
        )
        assignments = sum(1 for l in lines if assign_pattern.match(l))
        if assignments != 1:
            return None

        # Build replacement edits:
        # 1. Delete the declaration line
        # 2. Replace each non-declaration use of var_name with value
        use_pattern = re.compile(rf"\b{re.escape(var_name)}\b")
        edits: List[Dict] = []

        for i, l in enumerate(lines):
            if i == line_num:
                # Delete the declaration
                edits.append({
                    "range": {
                        "start": {"line": i, "character": 0},
                        "end": {"line": i + 1, "character": 0},
                    },
                    "newText": "",
                })
                continue
            # Replace every occurrence of the variable name on this line
            if use_pattern.search(l):
                new_line = use_pattern.sub(value, l)
                edits.append({
                    "range": {
                        "start": {"line": i, "character": 0},
                        "end": {"line": i, "character": len(l)},
                    },
                    "newText": new_line,
                })

        if not edits:
            return None

        return {
            "title": f"Inline variable '{var_name}'",
            "kind": self.KIND_REFACTOR,
            "edit": {"changes": {uri: edits}},
        }


__all__ = ['CodeActionsProvider']
