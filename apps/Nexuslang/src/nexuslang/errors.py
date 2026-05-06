"""
Enhanced error reporting for NexusLang.
Provides detailed error messages with source context, suggestions, and formatting.
Inspired by Rust's error reporting system.
"""

from typing import Optional, List, Type
import difflib
from .error_codes import get_error_info, get_error_code_for_type
from .colors import red, yellow, green, cyan, magenta, bold, dim


class NxlError(Exception):
    """Base class for all NexusLang errors with enhanced formatting."""
    def __init__(self, message: str, line: Optional[int] = None, column: Optional[int] = None,
                 source_line: Optional[str] = None, error_type: str = "Error", nxl_type: str = "Error",
                 error_code: Optional[str] = None, full_source: Optional[str] = None,
                 context_lines: int = 2):
        self.message = message
        self.line = line
        self.column = column
        self.source_line = source_line
        self.error_type = error_type
        self.nxl_type = nxl_type
        self.error_code = error_code
        self.full_source = full_source
        self.context_lines = context_lines
        super().__init__(self._format_error())
    
    def format_error(self) -> str:
        """Public method to get formatted error (for catching in main)."""
        return self._format_error()
    
    def _format_error(self) -> str:
        """Format the error message with context (Rust-style with colors)."""
        parts = []
        
        # Header with error code (red and bold)
        if self.error_code:
            header = f"{self.error_type} [{self.error_code}]: {self.message}"
            parts.append(f"\n{red(bold(header))}")
        else:
            header = f"{self.error_type}: {self.message}"
            parts.append(f"\n{red(bold(header))}")
        
        # Location (cyan)
        if self.line is not None:
            location = f"  --> line {self.line}" + (f", column {self.column}" if self.column else "")
            parts.append(cyan(location))
        
        # Source context (multiple lines like Rust)
        if self.full_source and self.line:
            context = format_source_context(self.full_source, self.line, self.column or 1, self.context_lines)
            parts.append(f"\n{context}")
        elif self.source_line:
            # Fallback to single line
            line_str = str(self.line) if self.line else "?"
            parts.append(f"\n  {dim(line_str + ' |')} {self.source_line}")
            if self.column is not None:
                padding = len(line_str) + 3 + self.column - 1
                parts.append(f"  {' ' * padding}{red('^')}")
        
        # Error code help (green for suggestions)
        if self.error_code:
            error_info = get_error_info(self.error_code)
            if error_info and error_info.fixes:
                parts.append(f"\n\n{bold('How to fix:')}")
                for fix in error_info.fixes[:2]:  # Show top 2 fixes
                    parts.append(f"  {green('•')} {fix}")
                parts.append(f"\n{dim(f'For more help: nxl --explain {self.error_code}')}")
        
        return "".join(parts)



class NxlSyntaxError(NxlError):
    """Syntax error with suggestions and error codes."""
    def __init__(self, message: str, line: Optional[int] = None, column: Optional[int] = None,
                 source_line: Optional[str] = None, suggestion: Optional[str] = None,
                 expected: Optional[str] = None, got: Optional[str] = None,
                 error_type_key: Optional[str] = None, full_source: Optional[str] = None):
        self.suggestion = suggestion
        self.expected = expected
        self.got = got
        
        # Get error code
        error_code = None
        if error_type_key:
            error_code = get_error_code_for_type(error_type_key)
        
        super().__init__(message, line, column, source_line, "Syntax Error", "SyntaxError",
                        error_code=error_code, full_source=full_source)
    
    def _format_error(self) -> str:
        """Format syntax error with additional context."""
        base = super()._format_error()
        
        if self.expected and self.got:
            base += f"\n\nExpected: {self.expected}"
            base += f"\nGot: {self.got}"
        
        if self.suggestion:
            base += f"\n\nSuggestion: {self.suggestion}"
        
        return base


class NxlRuntimeError(NxlError):
    """Runtime error with stack trace."""
    def __init__(self, message: str, line: Optional[int] = None, column: Optional[int] = None,
                 source_line: Optional[str] = None, stack_trace: Optional[List[str]] = None,
                 variable_context: Optional[dict] = None, nxl_type: str = "RuntimeError",
                 error_type_key: Optional[str] = "runtime_error", full_source: Optional[str] = None):
        self.stack_trace = stack_trace or []
        self.variable_context = variable_context or {}
        
        # Get error code if type key provided
        error_code = get_error_code_for_type(error_type_key) if error_type_key else None
        
        super().__init__(message, line, column, source_line, "Runtime Error", nxl_type,
                        error_code=error_code, full_source=full_source)
    
    def _format_error(self) -> str:
        """Format runtime error with stack trace."""
        base = super()._format_error()
        
        if self.stack_trace:
            base += "\n\n  Call stack:"
            for i, frame in enumerate(reversed(self.stack_trace)):
                base += f"\n    {i + 1}. {frame}"
        
        if self.variable_context:
            base += "\n\n  Local variables:"
            for name, value in self.variable_context.items():
                value_str = repr(value)
                if len(value_str) > 50:
                    value_str = value_str[:47] + "..."
                base += f"\n    {name} = {value_str}"
        
        return base


class NxlNameError(NxlError):
    """Name error with did-you-mean suggestions and error codes."""
    def __init__(self, name: str = None, line: Optional[int] = None, column: Optional[int] = None,
                 source_line: Optional[str] = None, available_names: Optional[List[str]] = None,
                 error_type_key: str = "undefined_variable", full_source: Optional[str] = None,
                 message: Optional[str] = None, suggestions: Optional[List[str]] = None):
        # Support both calling styles: new (name=) and old (message=)
        if message:
            # Old style: explicit message provided
            error_message = message
            # Try to extract name from message if not provided
            if not name and "'" in message:
                parts = message.split("'")
                if len(parts) >= 2:
                    name = parts[1]
        else:
            # New style: build message from name
            if not name:
                name = "unknown"
            error_message = f"Name '{name}' is not defined"
        
        self.name = name
        self.available_names = available_names or []
        
        # Find similar names if not explicitly provided
        if suggestions:
            self.suggestions = suggestions
        else:
            self.suggestions = get_close_matches(name, self.available_names) if name else []
        
        # Determine specific error code
        error_code = get_error_code_for_type(error_type_key)
        
        super().__init__(error_message, line, column, source_line, "Name Error", "NameError",
                        error_code=error_code, full_source=full_source)
    
    def _format_error(self) -> str:
        """Format name error with suggestions."""
        base = super()._format_error()
        
        if self.suggestions:
            if len(self.suggestions) == 1:
                base += f"\n\n{bold('Did you mean:')} {cyan(repr(self.suggestions[0]))}?"
            else:
                base += f"\n\n{bold('Did you mean one of these?')}"
                for suggestion in self.suggestions[:3]:  # Show top 3
                    base += f"\n  {green('•')} {cyan(suggestion)}"
        
        return base


class NxlTypeError(NxlError):
    """Type error with type information and error codes."""
    def __init__(self, message: str, line: Optional[int] = None, column: Optional[int] = None,
                 source_line: Optional[str] = None, expected_type: Optional[str] = None,
                 got_type: Optional[str] = None, error_type_key: str = "type_mismatch",
                 full_source: Optional[str] = None):
        self.expected_type = expected_type
        self.got_type = got_type
        
        # Get error code
        error_code = get_error_code_for_type(error_type_key)
        
        super().__init__(message, line, column, source_line, "Type Error", "TypeError",
                        error_code=error_code, full_source=full_source)
    
    def _format_error(self) -> str:
        """Format type error with type info."""
        base = super()._format_error()
        
        if self.expected_type and self.got_type:
            base += f"\n\n{bold('Expected type:')} {magenta(self.expected_type)}"
            base += f"\n{bold('Got type:')} {magenta(self.got_type)}"
        
        return base


class NxlContractError(NxlError):
    """Contract violation error raised by require/ensure/guarantee statements."""
    def __init__(self, message: str, line: Optional[int] = None,
                 column: Optional[int] = None,
                 source_line: Optional[str] = None,
                 contract_kind: str = "contract",
                 full_source: Optional[str] = None):
        self.contract_kind = contract_kind
        super().__init__(message, line, column, source_line, "Contract Error",
                         "ContractError", full_source=full_source)


# Backward-compatible alias
NLPLContractError = NxlContractError


def get_close_matches(word: str, possibilities: List[str], n: int = 3, cutoff: float = 0.6) -> List[str]:
    """
    Get close matches to a word from a list of possibilities.
    Uses difflib for fuzzy string matching.

    Caps the candidate list at 256 entries (sorted by length similarity) to
    prevent O(n^2) difflib performance when the scope contains hundreds of
    stdlib names.
    """
    if not word or not possibilities:
        return []

    # Cap candidates to avoid difflib O(n^2) blow-up on large scopes.
    # Prefer candidates whose length is close to the target word.
    if len(possibilities) > 256:
        wlen = len(word)
        possibilities = sorted(possibilities, key=lambda p: abs(len(p) - wlen))[:256]

    # Use difflib to find similar strings
    matches = difflib.get_close_matches(word, possibilities, n=n, cutoff=cutoff)
    return matches


def format_source_context(source: str, line: int, column: int, context_lines: int = 2) -> str:
    """
    Format source code context around an error location.
    
    Args:
        source: Full source code
        line: Line number (1-indexed)
        column: Column number (1-indexed)
        context_lines: Number of lines to show before and after
    
    Returns:
        Formatted source context with line numbers and caret pointer
    """
    lines = source.split('\n')
    
    # Adjust for 0-indexed list
    error_line_idx = line - 1
    
    # Calculate range
    start_idx = max(0, error_line_idx - context_lines)
    end_idx = min(len(lines), error_line_idx + context_lines + 1)
    
    # Build context
    result = []
    max_line_num = end_idx
    num_width = len(str(max_line_num))
    
    for i in range(start_idx, end_idx):
        line_num = i + 1
        prefix = f"  {line_num:{num_width}} | "
        
        if i == error_line_idx:
            # Error line - highlight it
            result.append(f"{dim(prefix)}{lines[i]}")
            # Add caret pointer (red)
            padding = len(prefix) + column - 1
            result.append(f"  {' ' * padding}{red('^')}")
        else:
            # Context line (dimmed)
            result.append(f"{dim(prefix + lines[i])}")
    
    return '\n'.join(result)


def suggest_correction(error_type: str, context: dict) -> Optional[str]:
    """
    Suggest a correction based on the error type and context.
    
    Args:
        error_type: Type of error (e.g., "missing_end", "undefined_variable")
        context: Additional context information
    
    Returns:
        Suggestion string or None
    """
    suggestions = {
        "missing_end": "Make sure to close all blocks (if, while, for, function, class) with 'end'",
        "unexpected_token": "Check for missing or extra keywords, operators, or punctuation",
        "undefined_variable": "Declare variables with 'set <name> to <value>' before using them",
        "undefined_function": "Define functions with 'function <name>' or import them from a module",
        "type_mismatch": "Check that you're using compatible types in operations",
        "indentation_error": "Use consistent indentation (spaces or tabs, not mixed)",
        "missing_colon": "Some statements may need a colon ':' at the end",
        "invalid_syntax": "Review the language syntax - this construct is not valid",
        "division_by_zero": "Check that the divisor is not zero",
        "index_out_of_range": "Ensure the index is within the list/array bounds",
        "key_not_found": "Check that the dictionary key exists before accessing it",
    }
    
    return suggestions.get(error_type)
