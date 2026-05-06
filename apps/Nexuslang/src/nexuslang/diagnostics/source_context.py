"""
Source Code Context Extraction
===============================

Extract source code context for error messages.
"""

from dataclasses import dataclass
from typing import Optional, List


@dataclass
class SourceLocation:
    """A location in source code."""
    file: str
    line: int
    column: int
    length: int = 1


@dataclass
class SourceSpan:
    """A span of source code."""
    start: SourceLocation
    end: SourceLocation


class SourceContext:
    """
    Extract and manage source code context for error messages.
    """
    
    def __init__(self, source_code: str, file_path: str = "<input>"):
        self.source_code = source_code
        self.file_path = file_path
        self.lines = source_code.split('\n')
    
    def get_line(self, line_num: int) -> Optional[str]:
        """Get a specific line (1-indexed)."""
        if 1 <= line_num <= len(self.lines):
            return self.lines[line_num - 1]
        return None
    
    def get_lines(self, start: int, end: int) -> List[str]:
        """Get a range of lines (1-indexed, inclusive)."""
        lines = []
        for i in range(start, end + 1):
            line = self.get_line(i)
            if line is not None:
                lines.append(line)
        return lines
    
    def get_context(self, line: int, context_lines: int = 2) -> List[tuple[int, str]]:
        """
        Get context around a line.
        
        Returns:
            List of (line_number, line_text) tuples
        """
        start = max(1, line - context_lines)
        end = min(len(self.lines), line + context_lines)
        
        result = []
        for i in range(start, end + 1):
            line_text = self.get_line(i)
            if line_text is not None:
                result.append((i, line_text))
        
        return result
    
    def get_line_column(self, offset: int) -> tuple[int, int]:
        """
        Convert byte offset to (line, column).
        
        Args:
            offset: Byte offset in source
            
        Returns:
            (line, column) 1-indexed
        """
        current_offset = 0
        
        for line_num, line in enumerate(self.lines, 1):
            line_length = len(line) + 1  # +1 for newline
            
            if current_offset + line_length > offset:
                column = offset - current_offset + 1
                return (line_num, column)
            
            current_offset += line_length
        
        # Beyond end of file
        return (len(self.lines), len(self.lines[-1]) if self.lines else 0)
    
    def extract_word_at(self, line: int, column: int) -> Optional[str]:
        """Extract the word at a given position."""
        line_text = self.get_line(line)
        if not line_text or column < 1 or column > len(line_text):
            return None
        
        # Find word boundaries
        start = column - 1
        end = column - 1
        
        # Expand left
        while start > 0 and (line_text[start - 1].isalnum() or line_text[start - 1] == '_'):
            start -= 1
        
        # Expand right
        while end < len(line_text) and (line_text[end].isalnum() or line_text[end] == '_'):
            end += 1
        
        return line_text[start:end]
    
    def calculate_visual_column(self, line: int, column: int, tab_width: int = 4) -> int:
        """
        Calculate visual column accounting for tabs.
        
        Args:
            line: Line number (1-indexed)
            column: Column number (1-indexed)
            tab_width: Width of tab character
            
        Returns:
            Visual column position
        """
        line_text = self.get_line(line)
        if not line_text:
            return column
        
        visual_col = 0
        for i in range(min(column - 1, len(line_text))):
            if line_text[i] == '\t':
                visual_col += tab_width - (visual_col % tab_width)
            else:
                visual_col += 1
        
        return visual_col + 1


__all__ = ['SourceContext', 'SourceLocation', 'SourceSpan']
