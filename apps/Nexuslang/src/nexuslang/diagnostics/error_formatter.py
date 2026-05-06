"""
Enhanced Error Formatter
=========================

Beautiful, colorized error messages with context and suggestions.
"""

from enum import Enum
from dataclasses import dataclass
from typing import Optional, List
import sys


class DiagnosticLevel(Enum):
    """Severity level of a diagnostic."""
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"
    HINT = "hint"


@dataclass
class DiagnosticCode:
    """Error code with documentation."""
    code: str
    category: str
    url: Optional[str] = None


@dataclass
class Diagnostic:
    """A single diagnostic message."""
    level: DiagnosticLevel
    message: str
    file: str
    line: int
    column: int
    code: Optional[DiagnosticCode] = None
    suggestion: Optional[str] = None
    notes: List[str] = None
    
    def __post_init__(self):
        if self.notes is None:
            self.notes = []


class Colors:
    """ANSI color codes."""
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    
    # Foreground colors
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    
    # Bright variants
    BRIGHT_RED = "\033[91m"
    BRIGHT_GREEN = "\033[92m"
    BRIGHT_YELLOW = "\033[93m"
    BRIGHT_BLUE = "\033[94m"
    BRIGHT_MAGENTA = "\033[95m"
    BRIGHT_CYAN = "\033[96m"
    
    @staticmethod
    def is_supported() -> bool:
        """Check if terminal supports colors."""
        return sys.stdout.isatty()


class ErrorFormatter:
    """
    Format diagnostic messages with colors, context, and suggestions.
    
    Output format inspired by Rust compiler:
    
    error[E0001]: undefined variable 'nmae'
       example.nlpl:5:12
      
    5  print text nmae
                  ^^^^ not found in this scope
      
      = help: did you mean 'name'?
    """
    
    def __init__(self, use_colors: bool = True):
        self.use_colors = use_colors and Colors.is_supported()
        self.context_lines = 2  # Lines before/after error
    
    def format(self, diagnostic: Diagnostic, source_code: Optional[str] = None) -> str:
        """Format a diagnostic message."""
        lines = []
        
        # Header: error[E0001]: message
        lines.append(self._format_header(diagnostic))
        
        # Location:  file:line:col
        lines.append(self._format_location(diagnostic))
        
        # Source context with caret pointer
        if source_code:
            lines.extend(self._format_source_context(diagnostic, source_code))
        
        # Suggestion
        if diagnostic.suggestion:
            lines.append(self._format_suggestion(diagnostic.suggestion))
        
        # Notes
        for note in diagnostic.notes:
            lines.append(self._format_note(note))
        
        # Error code documentation
        if diagnostic.code and diagnostic.code.url:
            lines.append(self._format_doc_link(diagnostic.code))
        
        return '\n'.join(lines)
    
    def _format_header(self, diagnostic: Diagnostic) -> str:
        """Format: error[E0001]: message"""
        level = diagnostic.level.value
        
        # Color based on level
        if self.use_colors:
            if diagnostic.level == DiagnosticLevel.ERROR:
                color = Colors.BRIGHT_RED
            elif diagnostic.level == DiagnosticLevel.WARNING:
                color = Colors.BRIGHT_YELLOW
            elif diagnostic.level == DiagnosticLevel.INFO:
                color = Colors.BRIGHT_BLUE
            else:
                color = Colors.BRIGHT_CYAN
            
            level_text = f"{Colors.BOLD}{color}{level}{Colors.RESET}"
        else:
            level_text = level
        
        # Add error code if present
        if diagnostic.code:
            code_text = f"[{diagnostic.code.code}]"
            if self.use_colors:
                code_text = f"{Colors.BOLD}{code_text}{Colors.RESET}"
            level_text += code_text
        
        return f"{level_text}: {diagnostic.message}"
    
    def _format_location(self, diagnostic: Diagnostic) -> str:
        """Format:  file:line:col"""
        location = f"{diagnostic.file}:{diagnostic.line}:{diagnostic.column}"
        
        if self.use_colors:
            return f"  {Colors.BLUE}{Colors.RESET} {location}"
        return f"   {location}"
    
    def _format_source_context(self, diagnostic: Diagnostic, source_code: str) -> List[str]:
        """Format source code context with line numbers and caret pointer."""
        lines = []
        source_lines = source_code.split('\n')
        
        # Get context window
        start_line = max(1, diagnostic.line - self.context_lines)
        end_line = min(len(source_lines), diagnostic.line + self.context_lines)
        
        # Empty line with gutter
        lines.append(f"  {Colors.BLUE}{Colors.RESET}" if self.use_colors else "  ")
        
        # Show source lines
        for i in range(start_line, end_line + 1):
            line_num = str(i).rjust(4)
            line_text = source_lines[i - 1] if i <= len(source_lines) else ""
            
            # Highlight error line
            if i == diagnostic.line:
                if self.use_colors:
                    gutter = f"{Colors.BLUE}{line_num} {Colors.RESET}"
                else:
                    gutter = f"{line_num} "
                
                lines.append(f"{gutter} {line_text}")
                
                # Caret pointer
                spaces = ' ' * (diagnostic.column - 1)
                if self.use_colors:
                    pointer = f"  {Colors.BLUE}{Colors.RESET} {spaces}{Colors.BRIGHT_RED}^^^^ {diagnostic.message}{Colors.RESET}"
                else:
                    pointer = f"   {spaces}^^^^ {diagnostic.message}"
                lines.append(pointer)
            else:
                if self.use_colors:
                    gutter = f"{Colors.DIM}{line_num} {Colors.RESET}"
                else:
                    gutter = f"{line_num} "
                
                lines.append(f"{gutter} {line_text}")
        
        # Empty line
        lines.append(f"  {Colors.BLUE}{Colors.RESET}" if self.use_colors else "  ")
        
        return lines
    
    def _format_suggestion(self, suggestion: str) -> str:
        """Format: = help: suggestion"""
        if self.use_colors:
            return f"  {Colors.BOLD}{Colors.CYAN}= help:{Colors.RESET} {suggestion}"
        return f"  = help: {suggestion}"
    
    def _format_note(self, note: str) -> str:
        """Format: = note: message"""
        if self.use_colors:
            return f"  {Colors.BOLD}{Colors.WHITE}= note:{Colors.RESET} {note}"
        return f"  = note: {note}"
    
    def _format_doc_link(self, code: DiagnosticCode) -> str:
        """Format documentation link."""
        if self.use_colors:
            return f"  {Colors.DIM}For more information, see: {code.url}{Colors.RESET}"
        return f"  For more information, see: {code.url}"
    
    def format_multiple(self, diagnostics: List[Diagnostic], source_code: Optional[str] = None) -> str:
        """Format multiple diagnostics."""
        formatted = []
        
        for diagnostic in diagnostics:
            formatted.append(self.format(diagnostic, source_code))
        
        return '\n\n'.join(formatted)


__all__ = ['ErrorFormatter', 'Diagnostic', 'DiagnosticLevel', 'DiagnosticCode', 'Colors']
