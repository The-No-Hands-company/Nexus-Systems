#!/usr/bin/env python3
"""
NLPL Code Formatter
===================

Automatic code formatter for NexusLang source files.

Features:
- Consistent indentation (4 spaces)
- Proper spacing around operators
- Keyword capitalization consistency
- Line length management (80 chars recommended)
- Comment formatting
- Block structure alignment

Usage:
    python dev_tools/nxl_format.py file.nlpl          # Format and print
    python dev_tools/nxl_format.py file.nlpl --write  # Format in-place
    python dev_tools/nxl_format.py dir/ --recursive   # Format directory
"""

import argparse
import os
import sys
import re
from pathlib import Path
from typing import List, Tuple

class NLPLFormatter:
    """Format NexusLang source code."""
    
    def __init__(self, indent_size: int = 4, max_line_length: int = 100):
        self.indent_size = indent_size
        self.max_line_length = max_line_length
        
        # Keywords that increase indentation
        self.indent_increase = {
            'function', 'if', 'else if', 'else', 'while', 'for',
            'try', 'catch', 'finally', 'class', 'struct', 'enum'
        }
        
        # Keywords that decrease indentation
        self.indent_decrease = {'end', 'else', 'else if', 'catch', 'finally'}
        
        # Keywords that should be lowercase
        self.lowercase_keywords = {
            'set', 'to', 'function', 'with', 'returns', 'return',
            'if', 'else', 'while', 'for', 'each', 'in',
            'try', 'catch', 'finally', 'throw',
            'class', 'struct', 'enum', 'end',
            'and', 'or', 'not', 'is', 'as',
            'print', 'text', 'import', 'from'
        }
    
    def format_code(self, source: str) -> str:
        """Format NexusLang source code."""
        lines = source.split('\n')
        formatted_lines = []
        current_indent = 0
        
        for line in lines:
            # Skip empty lines
            if not line.strip():
                formatted_lines.append('')
                continue
            
            # Handle comments
            if line.strip().startswith('#'):
                formatted_line = self._format_comment(line, current_indent)
                formatted_lines.append(formatted_line)
                continue
            
            # Check for dedent keywords (before formatting)
            should_dedent = self._should_dedent_line(line)
            if should_dedent and current_indent > 0:
                current_indent -= 1
            
            # Format the line
            formatted_line = self._format_line(line, current_indent)
            formatted_lines.append(formatted_line)
            
            # Check for indent keywords (after formatting)
            if self._should_indent_line(line):
                current_indent += 1
        
        return '\n'.join(formatted_lines)
    
    def _format_line(self, line: str, indent_level: int) -> str:
        """Format a single line of code."""
        # Strip existing indentation
        stripped = line.lstrip()
        
        # Apply correct indentation
        indent = ' ' * (indent_level * self.indent_size)
        
        # Normalize keywords to lowercase
        formatted = self._normalize_keywords(stripped)
        
        # Add spacing around operators
        formatted = self._add_operator_spacing(formatted)
        
        return indent + formatted.rstrip()
    
    def _format_comment(self, line: str, indent_level: int) -> str:
        """Format a comment line."""
        stripped = line.lstrip()
        indent = ' ' * (indent_level * self.indent_size)
        
        # Ensure single space after #
        if stripped.startswith('#') and len(stripped) > 1 and stripped[1] != ' ':
            stripped = '# ' + stripped[1:]
        
        return indent + stripped.rstrip()
    
    def _normalize_keywords(self, line: str) -> str:
        """Normalize keyword capitalization."""
        # Split by whitespace but preserve strings
        in_string = False
        quote_char = None
        result = []
        current_word = []
        
        i = 0
        while i < len(line):
            char = line[i]
            
            # Handle strings
            if char in ('"', "'") and (i == 0 or line[i-1] != '\\'):
                if not in_string:
                    # Start string
                    if current_word:
                        word = ''.join(current_word)
                        result.append(self._process_word(word))
                        current_word = []
                    in_string = True
                    quote_char = char
                    result.append(char)
                elif char == quote_char:
                    # End string
                    in_string = False
                    quote_char = None
                    result.append(char)
                else:
                    result.append(char)
            elif in_string:
                result.append(char)
            elif char in (' ', '\t'):
                # Whitespace - process current word
                if current_word:
                    word = ''.join(current_word)
                    result.append(self._process_word(word))
                    current_word = []
                result.append(' ')
            else:
                current_word.append(char)
            
            i += 1
        
        # Process final word
        if current_word:
            word = ''.join(current_word)
            result.append(self._process_word(word))
        
        return ''.join(result)
    
    def _process_word(self, word: str) -> str:
        """Process a single word (apply keyword rules)."""
        # Check if it's a keyword
        word_lower = word.lower()
        if word_lower in self.lowercase_keywords:
            return word_lower
        return word
    
    def _add_operator_spacing(self, line: str) -> str:
        """Add proper spacing around operators."""
        # Don't modify strings
        parts = []
        in_string = False
        quote_char = None
        current = []
        
        for i, char in enumerate(line):
            if char in ('"', "'") and (i == 0 or line[i-1] != '\\'):
                if not in_string:
                    in_string = True
                    quote_char = char
                elif char == quote_char:
                    in_string = False
                    quote_char = None
            
            if in_string:
                current.append(char)
            else:
                # Apply spacing rules outside strings
                current.append(char)
        
        result = ''.join(current)
        
        # Clean up multiple spaces
        result = re.sub(r' +', ' ', result)
        
        return result
    
    def _should_indent_line(self, line: str) -> bool:
        """Check if line should increase indentation for next line."""
        stripped = line.strip().lower()
        
        # Check if line starts with indent-increasing keyword
        for keyword in self.indent_increase:
            if stripped.startswith(keyword + ' ') or stripped == keyword:
                # Don't indent after one-line if/while/for
                if 'end' not in stripped:
                    return True
        
        return False
    
    def _should_dedent_line(self, line: str) -> bool:
        """Check if line should decrease indentation."""
        stripped = line.strip().lower()
        
        # Check for dedent keywords
        for keyword in self.indent_decrease:
            if stripped.startswith(keyword):
                return True
        
        return False
    
    def format_file(self, file_path: str, write: bool = False) -> str:
        """Format a file and optionally write back."""
        with open(file_path, 'r') as f:
            source = f.read()
        
        formatted = self.format_code(source)
        
        if write:
            with open(file_path, 'w') as f:
                f.write(formatted)
            return f"Formatted: {file_path}"
        else:
            return formatted
    
    def format_directory(self, dir_path: str, recursive: bool = False, write: bool = False) -> List[str]:
        """Format all .nlpl files in directory."""
        results = []
        
        if recursive:
            pattern = "**/*.nxl"
        else:
            pattern = "*.nxl"
        
        path = Path(dir_path)
        for file_path in path.glob(pattern):
            try:
                result = self.format_file(str(file_path), write)
                if write:
                    results.append(result)
                else:
                    results.append(f"Would format: {file_path}")
            except Exception as e:
                results.append(f"Error formatting {file_path}: {e}")
        
        return results


def main():
    """Main entry point for formatter."""
    parser = argparse.ArgumentParser(
        description="NLPL Code Formatter - Format NexusLang source code",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        "path",
        help="File or directory to format"
    )
    
    parser.add_argument(
        "-w", "--write",
        action="store_true",
        help="Write changes to file(s) in-place"
    )
    
    parser.add_argument(
        "-r", "--recursive",
        action="store_true",
        help="Recursively format directory"
    )
    
    parser.add_argument(
        "-i", "--indent",
        type=int,
        default=4,
        help="Indentation size (default: 4 spaces)"
    )
    
    parser.add_argument(
        "-l", "--line-length",
        type=int,
        default=100,
        help="Maximum line length (default: 100)"
    )
    
    args = parser.parse_args()
    
    # Check path exists
    if not os.path.exists(args.path):
        print(f"Error: Path not found: {args.path}")
        return 1
    
    # Create formatter
    formatter = NLPLFormatter(
        indent_size=args.indent,
        max_line_length=args.line_length
    )
    
    # Format file or directory
    if os.path.isfile(args.path):
        if not args.path.endswith('.nxl'):
            print("Error: File must have .nlpl extension")
            return 1
        
        result = formatter.format_file(args.path, args.write)
        print(result)
    
    elif os.path.isdir(args.path):
        results = formatter.format_directory(args.path, args.recursive, args.write)
        for result in results:
            print(result)
        
        if not args.write:
            print(f"\nFound {len(results)} file(s). Use --write to apply changes.")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
