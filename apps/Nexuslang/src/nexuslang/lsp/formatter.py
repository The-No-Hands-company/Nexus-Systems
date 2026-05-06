"""
NexusLang Code Formatter
=======================

Formats NexusLang code according to the NexusLang style guide.

Formatting Rules:
- 4 spaces for indentation
- Consistent spacing around operators
- Line length under 100 characters
- Blank lines to separate logical sections
- Normalized keyword spacing

Pipeline:
1. Tokenise source with the NexusLang Lexer (AST-aware pass)
2. Reconstruct each line from canonical token forms with correct spacing
3. Track indentation using token types (not string heuristics)
4. Fall back to regex-based pass if tokenisation fails (partial/broken documents)
"""

import re
from typing import Dict, List, Optional, Tuple


class NexusLangFormatter:
    """
    Formats NexusLang code according to style guidelines.

    Rules:
    1. Indentation: 4 spaces per level
    2. Spacing: Consistent spacing derived from token types
    3. Line length: Keep under 100 characters (soft limit)
    4. Blank lines: Separate functions, classes, and logical sections
    5. Trailing whitespace: Remove all trailing whitespace
    """

    def __init__(self) -> None:
        self.indent_size = 4
        self.max_line_length = 100

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def format(self, code: str) -> str:
        """Format NexusLang code using AST-aware pipeline with regex fallback."""
        try:
            return self._format_with_tokens(code)
        except Exception:
            return self._format_regex(code)

    def get_formatting_edits(self, text: str) -> List[dict]:
        """Return LSP TextEdit list for the entire document, or [] if unchanged."""
        formatted = self.format(text)
        if formatted == text:
            return []
        lines = text.split('\n')
        return [{
            'range': {
                'start': {'line': 0, 'character': 0},
                'end': {'line': len(lines), 'character': 0},
            },
            'newText': formatted,
        }]

    # ------------------------------------------------------------------
    # AST-aware (token-based) pass
    # ------------------------------------------------------------------

    def _format_with_tokens(self, code: str) -> str:
        """Format using the NexusLang Lexer token stream."""
        from ..parser.lexer import Lexer, TokenType  # lazy import to avoid cycles

        lexer = Lexer(code)
        tokens = lexer.tokenize()

        # Structural tokens we skip when grouping by line
        _structural = {
            TokenType.INDENT, TokenType.DEDENT,
            TokenType.NEWLINE, TokenType.EOF,
        }

        # Group content tokens by 1-indexed source line
        lines_tokens: Dict[int, list] = {}
        for tok in tokens:
            if tok.type in _structural:
                continue
            lines_tokens.setdefault(tok.line, []).append(tok)

        source_lines = code.split('\n')
        result_lines: List[str] = []
        current_indent = 0
        prev_blank = False

        for line_num in range(1, len(source_lines) + 1):
            orig_line = source_lines[line_num - 1]
            toks = lines_tokens.get(line_num, [])
            stripped = orig_line.strip()

            # Empty line
            if not stripped:
                if not prev_blank:
                    result_lines.append('')
                    prev_blank = True
                continue

            # Comment-only line (regular # comments are not in the token stream;
            # ## DOC_COMMENT lines do appear in toks)
            if stripped.startswith('#'):
                prev_blank = False
                if toks:
                    # DOC_COMMENT token present - reconstruct canonically
                    text = self._reconstruct_from_tokens(toks, TokenType)
                else:
                    # Plain comment - preserve verbatim content, normalise indent
                    text = stripped
                result_lines.append(' ' * (current_indent * self.indent_size) + text)
                continue

            # Non-comment line with no tokens (edge case in partial documents)
            if not toks:
                prev_blank = False
                result_lines.append(' ' * (current_indent * self.indent_size) + stripped)
                continue

            prev_blank = False

            # Extract any trailing comment from the original source line
            code_part, comment_suffix = self._split_line_comment(orig_line)

            # Get the "effective" block-structure token (skip visibility/async modifiers)
            eff_tok = self._get_effective_block_token(toks, TokenType)

            # Apply dedent BEFORE placing this line
            if eff_tok and self._tok_dedent_before(eff_tok, TokenType):
                if current_indent > 0:
                    current_indent -= 1

            # Reconstruct normalised line text from tokens
            text = self._reconstruct_from_tokens(toks, TokenType)
            line_out = ' ' * (current_indent * self.indent_size) + text
            if comment_suffix:
                line_out += comment_suffix
            result_lines.append(line_out)

            # Apply indent AFTER this line
            if eff_tok and self._tok_indent_after(eff_tok, TokenType):
                current_indent += 1

            # Add separator blank line after block-ending tokens
            if eff_tok and self._tok_add_blank_after(eff_tok.type, TokenType):
                next_stripped = source_lines[line_num].strip() if line_num < len(source_lines) else ''
                if next_stripped and not next_stripped.startswith('#'):
                    result_lines.append('')
                    prev_blank = True

        return '\n'.join(result_lines).rstrip() + '\n'

    # ------------------------------------------------------------------
    # Token helpers
    # ------------------------------------------------------------------

    def _get_effective_block_token(self, toks: list, TokenType):
        """Return the first token that drives block-structure decisions.

        Skips leading visibility / modifier keywords so that
        `public function foo` is treated the same as `function foo`.
        """
        modifier_types = {
            TokenType.PUBLIC, TokenType.PRIVATE, TokenType.PROTECTED,
            TokenType.ASYNC, TokenType.INLINE,
        }
        for tok in toks:
            if tok.type not in modifier_types:
                return tok
        return toks[0]

    def _tok_indent_after(self, tok, TokenType) -> bool:
        """True if this token type opens a new indentation block."""
        ttype = tok.type
        indent_types = {
            TokenType.FUNCTION, TokenType.CLASS,
            TokenType.IF, TokenType.WHILE,
            TokenType.FOR, TokenType.FOR_EACH,
            TokenType.TRY, TokenType.CATCH,
            TokenType.STRUCT, TokenType.UNION, TokenType.ENUM,
            TokenType.INTERFACE, TokenType.TRAIT,
            TokenType.SWITCH, TokenType.CASE, TokenType.DEFAULT,
            TokenType.ELSE, TokenType.ELSE_IF,
            TokenType.DESCRIBE, TokenType.IT, TokenType.TEST,
            TokenType.MACRO, TokenType.SPEC,
        }
        if ttype in indent_types:
            return True
        # Handle keywords not in TokenType (e.g. 'finally' is an IDENTIFIER)
        if ttype == TokenType.IDENTIFIER and tok.lexeme.lower() in ('finally',):
            return True
        return False

    def _tok_dedent_before(self, tok, TokenType) -> bool:
        """True if this token type should close the previous indentation block."""
        ttype = tok.type
        dedent_types = {
            TokenType.END, TokenType.END_CLASS, TokenType.END_METHOD,
            TokenType.END_TRAIT, TokenType.END_INTERFACE,
            TokenType.END_IF, TokenType.END_WHILE,
            TokenType.END_LOOP, TokenType.END_CONCURRENT, TokenType.END_TRY,
            TokenType.ELSE, TokenType.ELSE_IF,
            TokenType.CATCH, TokenType.CASE, TokenType.DEFAULT,
        }
        if ttype in dedent_types:
            return True
        if ttype == TokenType.IDENTIFIER and tok.lexeme.lower() in ('finally',):
            return True
        return False

    def _tok_add_blank_after(self, ttype, TokenType) -> bool:
        """True if a blank separator line should follow this token's line."""
        return ttype in {
            TokenType.END, TokenType.END_CLASS, TokenType.END_METHOD,
            TokenType.END_TRAIT, TokenType.END_INTERFACE,
            TokenType.END_IF, TokenType.END_WHILE,
            TokenType.END_LOOP, TokenType.END_CONCURRENT, TokenType.END_TRY,
        }

    # ------------------------------------------------------------------
    # Token-to-text reconstruction
    # ------------------------------------------------------------------

    # Canonical text for multi-word token types
    _CANONICAL_FORMS: Dict[str, str] = {
        'GREATER_THAN': 'greater than',
        'LESS_THAN': 'less than',
        'EQUAL_TO': 'equal to',
        'NOT_EQUAL_TO': 'not equal to',
        'GREATER_THAN_OR_EQUAL_TO': 'greater than or equal to',
        'LESS_THAN_OR_EQUAL_TO': 'less than or equal to',
        'DIVIDED_BY': 'divided by',
        'FLOOR_DIVIDE': 'integer divided by',
        'FOR_EACH': 'for each',
        'ELSE_IF': 'else if',
        'BITWISE_AND': 'bitwise and',
        'BITWISE_OR': 'bitwise or',
        'BITWISE_XOR': 'bitwise xor',
        'BITWISE_NOT': 'bitwise not',
        'LEFT_SHIFT': 'shift left',
        'RIGHT_SHIFT': 'shift right',
        'ADDRESS_OF': 'address of',
        'SIZEOF': 'sizeof',
        'CONNECT_TO': 'connect to',
        'DISCONNECT_FROM': 'disconnect from',
        'END_CLASS': 'end class',
        'END_METHOD': 'end method',
        'END_TRAIT': 'end trait',
        'END_INTERFACE': 'end interface',
        'END_IF': 'end if',
        'END_WHILE': 'end while',
        'END_LOOP': 'end loop',
        'END_CONCURRENT': 'end concurrent',
        'END_TRY': 'end try',
        'BEFORE_EACH': 'before each',
        'AFTER_EACH': 'after each',
        'POWER': 'to the power of',
    }

    def _canonical(self, tok, TokenType) -> str:
        """Return the canonical text representation for a token."""
        ttype_name = tok.type.name
        if ttype_name in self._CANONICAL_FORMS:
            return self._CANONICAL_FORMS[ttype_name]

        # Preserve string/fstring literals verbatim
        if tok.type in (TokenType.STRING_LITERAL, TokenType.FSTRING_LITERAL):
            return tok.lexeme

        # DOC_COMMENT: restore ## prefix
        if tok.type == TokenType.DOC_COMMENT:
            content = tok.literal if tok.literal is not None else tok.lexeme
            return '## ' + str(content)

        # Numeric literals: use parsed value when available
        if tok.type == TokenType.INTEGER_LITERAL:
            return str(tok.literal) if tok.literal is not None else tok.lexeme
        if tok.type == TokenType.FLOAT_LITERAL:
            return str(tok.literal) if tok.literal is not None else tok.lexeme

        # Default: use original lexeme
        return tok.lexeme

    def _reconstruct_from_tokens(self, toks: list, TokenType) -> str:
        """Join token canonical forms with normalised inter-token spacing."""
        # Token types that suppress the space BEFORE them
        no_space_before = {
            TokenType.RIGHT_PAREN, TokenType.RIGHT_BRACKET,
            TokenType.DOT, TokenType.COMMA,
            TokenType.SEMICOLON, TokenType.COLON,
        }
        # Token types that suppress the space AFTER them
        no_space_after = {
            TokenType.LEFT_PAREN, TokenType.LEFT_BRACKET, TokenType.DOT,
        }

        parts: List[str] = []
        prev_type: Optional[object] = None

        for tok in toks:
            text = self._canonical(tok, TokenType)
            if parts:
                if prev_type not in no_space_after and tok.type not in no_space_before:
                    parts.append(' ')
            parts.append(text)
            prev_type = tok.type

        return ''.join(parts)

    # ------------------------------------------------------------------
    # Line-comment splitter
    # ------------------------------------------------------------------

    def _split_line_comment(self, line: str) -> Tuple[str, str]:
        """Split a source line into (code_part, trailing_comment).

        Correctly skips # characters inside string literals.
        Returns ('', '') for comment-only lines (caller handles those).
        """
        in_string = False
        escape_next = False
        quote_char: Optional[str] = None

        for i, c in enumerate(line):
            if escape_next:
                escape_next = False
                continue
            if c == '\\' and in_string:
                escape_next = True
                continue
            if not in_string and c in ('"', "'"):
                in_string = True
                quote_char = c
            elif in_string and c == quote_char:
                in_string = False
                quote_char = None
            elif not in_string and c == '#':
                comment_text = line[i:]
                return line[:i].rstrip(), '  ' + comment_text

        return line, ''

    # ------------------------------------------------------------------
    # Regex-based fallback pass (used when tokenisation fails)
    # ------------------------------------------------------------------

    def _format_regex(self, code: str) -> str:
        """Regex-based formatter used as a fallback for partial/broken documents."""
        lines = code.split('\n')
        formatted_lines: List[str] = []
        current_indent = 0
        prev_line_blank = False

        for i, line in enumerate(lines):
            line = line.rstrip()

            if not line.strip():
                if not prev_line_blank:
                    formatted_lines.append('')
                    prev_line_blank = True
                continue

            prev_line_blank = False
            line = self._normalize_spacing_regex(line)
            stripped = line.lstrip()

            if self._should_dedent_before(stripped) and current_indent > 0:
                current_indent -= 1

            formatted_lines.append(' ' * (current_indent * self.indent_size) + stripped)

            if self._should_indent_after(stripped):
                current_indent += 1

            if self._should_add_blank_after(stripped) and i < len(lines) - 1:
                next_line = lines[i + 1].strip() if i + 1 < len(lines) else ''
                if next_line and not next_line.startswith('#'):
                    formatted_lines.append('')

        return '\n'.join(formatted_lines).rstrip() + '\n'

    def _normalize_spacing_regex(self, line: str) -> str:
        """Normalise operator spacing using regex (fallback only)."""
        if line.strip().startswith('#'):
            return line.lstrip()
        if '"' in line or "'" in line:
            return line.strip()

        result = line.strip()
        result = re.sub(r'\b(set\s+\S+)\s+to\s+', r'\1 to ', result)
        result = re.sub(r'\s+is\s+equal\s+to\s+', ' is equal to ', result)
        result = re.sub(r'\s+is\s+not\s+equal\s+to\s+', ' is not equal to ', result)
        result = re.sub(r'\s+is\s+greater\s+than\s+or\s+equal\s+to\s+', ' is greater than or equal to ', result)
        result = re.sub(r'\s+is\s+less\s+than\s+or\s+equal\s+to\s+', ' is less than or equal to ', result)
        result = re.sub(r'\s+is\s+greater\s+than\s+', ' is greater than ', result)
        result = re.sub(r'\s+is\s+less\s+than\s+', ' is less than ', result)
        result = re.sub(r'\s+\bplus\b\s+', ' plus ', result)
        result = re.sub(r'\s+\bminus\b\s+', ' minus ', result)
        result = re.sub(r'\s+\btimes\b\s+', ' times ', result)
        result = re.sub(r'\s+divided\s+by\s+', ' divided by ', result)
        result = re.sub(r'\s+\band\b\s+', ' and ', result)
        result = re.sub(r'\s+\bor\b\s+', ' or ', result)
        result = re.sub(r'\s{2,}', ' ', result)
        return result

    def _should_indent_after(self, line: str) -> bool:
        line_lower = line.lower().strip()
        for prefix in (
            'function ', 'public function ', 'private function ',
            'static function ', 'async function ',
            'class ', 'struct ', 'union ', 'enum ',
            'interface ', 'trait ',
            'if ', 'else if ', 'while ', 'for ',
            'switch ', 'case ', 'default',
            'try', 'catch ', 'finally',
        ):
            if line_lower.startswith(prefix) or line_lower == prefix.rstrip():
                return True
        if line_lower == 'else':
            return True
        return False

    def _should_dedent_before(self, line: str) -> bool:
        line_lower = line.lower().strip()
        if line_lower == 'end' or line_lower.startswith('end '):
            return True
        if line_lower.startswith('else'):
            return True
        if line_lower.startswith('catch ') or line_lower.startswith('finally'):
            return True
        if line_lower.startswith('case ') or line_lower.startswith('default'):
            return True
        return False

    def _should_add_blank_after(self, line: str) -> bool:
        line_lower = line.lower().strip()
        return line_lower == 'end' or line_lower.startswith('end ')


# Backward-compatible alias
NLPLFormatter = NexusLangFormatter

__all__ = ['NexusLangFormatter', 'NLPLFormatter']
