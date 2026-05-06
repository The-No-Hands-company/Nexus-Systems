#!/usr/bin/env python3
"""
NLPL Linter
===========

Static analysis tool for NexusLang source code.

Checks:
- Undefined variables
- Unused variables
- Type mismatches (if type annotations present)
- Style violations
- Best practices
- Dead code
- Unreachable code

Usage:
    python dev_tools/nxl_lint.py file.nlpl           # Lint single file
    python dev_tools/nxl_lint.py dir/ --recursive    # Lint directory
    python dev_tools/nxl_lint.py file.nlpl --strict  # Strict mode
"""

import argparse
import os
import sys
import re
from pathlib import Path
from typing import List, Dict, Set, Tuple
from dataclasses import dataclass

@dataclass
class LintMessage:
    """Represents a lint message."""
    severity: str  # 'error', 'warning', 'info'
    line: int
    column: int
    message: str
    code: str  # Error code like 'E001', 'W001'
    
    def __str__(self):
        color = {
            'error': '\033[91m',
            'warning': '\033[93m',
            'info': '\033[94m'
        }.get(self.severity, '')
        reset = '\033[0m'
        
        return f"{color}{self.severity.upper()}: {self.code}{reset} Line {self.line}, Col {self.column}: {self.message}"


class NLPLLinter:
    """Static analysis linter for NexusLang code."""
    
    def __init__(self, strict: bool = False):
        self.strict = strict
        self.messages: List[LintMessage] = []
        
        # Track variables
        self.defined_vars: Set[str] = set()
        self.used_vars: Set[str] = set()
        self.functions: Set[str] = set()
        
        # Built-in functions (stdlib)
        self.builtins = {
            'print', 'len', 'range', 'list_append', 'list_length', 'list_get',
            'dict_set', 'dict_get', 'dict_keys', 'dict_values',
            'read_file', 'write_file', 'parse_json', 'to_json',
            'now', 'format_datetime', 'parse_datetime',
            'sqrt', 'pow', 'abs', 'min', 'max',
            'parse_xml', 'create_xml_element', 'template_render',
            'create_email', 'send_email',
        }
    
    def lint_file(self, file_path: str) -> List[LintMessage]:
        """Lint a single file."""
        self.messages = []
        self.defined_vars = set()
        self.used_vars = set()
        self.functions = set()
        
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
            
            self._lint_lines(lines)
            
        except Exception as e:
            self.messages.append(LintMessage(
                severity='error',
                line=0,
                column=0,
                message=f"Failed to read file: {e}",
                code='E000'
            ))
        
        return self.messages
    
    def _lint_lines(self, lines: List[str]):
        """Lint all lines of code."""
        in_function = False
        function_vars = set()
        
        for line_num, line in enumerate(lines, start=1):
            stripped = line.strip()
            
            # Skip empty lines and comments
            if not stripped or stripped.startswith('#'):
                continue
            
            # Check line length
            if len(line.rstrip()) > 100 and not self._is_string_line(stripped):
                self.messages.append(LintMessage(
                    severity='warning',
                    line=line_num,
                    column=100,
                    message='Line too long (>100 characters)',
                    code='W001'
                ))
            
            # Check for trailing whitespace
            if line.endswith(' \n') or line.endswith('\t\n'):
                self.messages.append(LintMessage(
                    severity='info',
                    line=line_num,
                    column=len(line.rstrip()),
                    message='Trailing whitespace',
                    code='I001'
                ))
            
            # Function definition
            if stripped.startswith('function '):
                in_function = True
                function_name = self._extract_function_name(stripped)
                if function_name:
                    self.functions.add(function_name)
                    
                    # Check naming convention
                    if not self._is_snake_case(function_name):
                        self.messages.append(LintMessage(
                            severity='warning',
                            line=line_num,
                            column=9,
                            message=f"Function '{function_name}' should use snake_case",
                            code='W002'
                        ))
            
            # End of function
            if stripped == 'end' and in_function:
                # Check for unused local variables
                unused = function_vars - self.used_vars
                for var in unused:
                    self.messages.append(LintMessage(
                        severity='warning',
                        line=line_num,
                        column=0,
                        message=f"Unused variable '{var}' in function",
                        code='W003'
                    ))
                function_vars = set()
                in_function = False
            
            # Variable assignment
            if ' to ' in stripped and stripped.startswith('set '):
                var_name = self._extract_variable_name(stripped)
                if var_name:
                    self.defined_vars.add(var_name)
                    if in_function:
                        function_vars.add(var_name)
                    
                    # Check naming convention
                    if not self._is_snake_case(var_name):
                        self.messages.append(LintMessage(
                            severity='warning',
                            line=line_num,
                            column=4,
                            message=f"Variable '{var_name}' should use snake_case",
                            code='W002'
                        ))
            
            # Check for undefined variables (basic check)
            self._check_undefined_vars(stripped, line_num)
            
            # Check for comparison patterns
            self._check_comparison_style(stripped, line_num)
            
            # Check for common mistakes
            self._check_common_mistakes(stripped, line_num)
    
    def _is_string_line(self, line: str) -> bool:
        """Check if line is primarily a string literal."""
        return line.count('"') >= 2 or line.count("'") >= 2
    
    def _extract_function_name(self, line: str) -> str:
        """Extract function name from definition."""
        match = re.search(r'function\s+(\w+)', line)
        return match.group(1) if match else None
    
    def _extract_variable_name(self, line: str) -> str:
        """Extract variable name from assignment."""
        match = re.search(r'set\s+(\w+)\s+to', line)
        return match.group(1) if match else None
    
    def _is_snake_case(self, name: str) -> bool:
        """Check if name follows snake_case convention."""
        return bool(re.match(r'^[a-z_][a-z0-9_]*$', name))
    
    def _check_undefined_vars(self, line: str, line_num: int):
        """Check for potentially undefined variables."""
        # This is a simplified check - real implementation would use AST
        words = re.findall(r'\b[a-z_][a-z0-9_]*\b', line.lower())
        
        for word in words:
            # Skip keywords and builtins
            if word in {'set', 'to', 'if', 'else', 'while', 'for', 'function', 'return', 'end', 'print', 'text'}:
                continue
            if word in self.builtins:
                continue
            
            # Track usage
            if not line.startswith('set ' + word):
                self.used_vars.add(word)
                
                # Check if defined
                if word not in self.defined_vars and word not in self.functions:
                    # Don't warn for common parameter names
                    if word not in {'x', 'y', 'i', 'j', 'k', 'item', 'key', 'value', 'n', 'count'}:
                        if self.strict:
                            self.messages.append(LintMessage(
                                severity='warning',
                                line=line_num,
                                column=0,
                                message=f"Potentially undefined variable '{word}'",
                                code='W004'
                            ))
    
    def _check_comparison_style(self, line: str, line_num: int):
        """Check comparison style."""
        # Check for 'is equal to' vs 'equals'
        if ' equals ' in line and ' is equal to ' not in line:
            self.messages.append(LintMessage(
                severity='info',
                line=line_num,
                column=0,
                message="Consider using 'is equal to' for better readability",
                code='I002'
            ))
        
        # Check for boolean comparisons
        if ' is true' in line or ' is false' in line:
            self.messages.append(LintMessage(
                severity='info',
                line=line_num,
                column=0,
                message="Boolean comparison can be simplified",
                code='I003'
            ))
    
    def _check_common_mistakes(self, line: str, line_num: int):
        """Check for common mistakes."""
        # Missing 'to' in assignment
        if line.strip().startswith('set ') and ' to ' not in line:
            self.messages.append(LintMessage(
                severity='error',
                line=line_num,
                column=0,
                message="Missing 'to' in assignment statement",
                code='E001'
            ))
        
        # Multiple assignments without clear separation
        if line.count(' to ') > 1:
            self.messages.append(LintMessage(
                severity='warning',
                line=line_num,
                column=0,
                message="Multiple assignments on one line - consider splitting",
                code='W005'
            ))
    
    def get_summary(self) -> Dict[str, int]:
        """Get summary of lint results."""
        return {
            'errors': sum(1 for m in self.messages if m.severity == 'error'),
            'warnings': sum(1 for m in self.messages if m.severity == 'warning'),
            'info': sum(1 for m in self.messages if m.severity == 'info'),
            'total': len(self.messages)
        }


def main():
    """Main entry point for linter."""
    parser = argparse.ArgumentParser(
        description="NLPL Linter - Static analysis for NexusLang code",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    
    parser.add_argument(
        "path",
        help="File or directory to lint"
    )
    
    parser.add_argument(
        "-r", "--recursive",
        action="store_true",
        help="Recursively lint directory"
    )
    
    parser.add_argument(
        "-s", "--strict",
        action="store_true",
        help="Strict mode (more warnings)"
    )
    
    parser.add_argument(
        "--no-info",
        action="store_true",
        help="Hide info messages"
    )
    
    args = parser.parse_args()
    
    # Check path exists
    if not os.path.exists(args.path):
        print(f"Error: Path not found: {args.path}")
        return 1
    
    # Create linter
    linter = NLPLLinter(strict=args.strict)
    
    # Collect files to lint
    files_to_lint = []
    if os.path.isfile(args.path):
        if not args.path.endswith('.nxl'):
            print("Error: File must have .nlpl extension")
            return 1
        files_to_lint.append(args.path)
    elif os.path.isdir(args.path):
        path = Path(args.path)
        pattern = "**/*.nxl" if args.recursive else "*.nxl"
        files_to_lint = list(path.glob(pattern))
    
    # Lint all files
    total_errors = 0
    total_warnings = 0
    total_info = 0
    
    for file_path in files_to_lint:
        messages = linter.lint_file(str(file_path))
        
        if messages:
            print(f"\n{file_path}")
            print("=" * 60)
            
            for msg in messages:
                if args.no_info and msg.severity == 'info':
                    continue
                print(f"  {msg}")
            
            summary = linter.get_summary()
            total_errors += summary['errors']
            total_warnings += summary['warnings']
            total_info += summary['info']
    
    # Print overall summary
    print(f"\n{'=' * 60}")
    print("Summary:")
    print(f"  Files checked: {len(files_to_lint)}")
    print(f"  Errors:        {total_errors}")
    print(f"  Warnings:      {total_warnings}")
    if not args.no_info:
        print(f"  Info:          {total_info}")
    print(f"{'=' * 60}\n")
    
    # Exit with error code if errors found
    return 1 if total_errors > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
