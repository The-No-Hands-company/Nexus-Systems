#!/usr/bin/env python3
"""
Grammar Coverage Analyzer
==========================

Analyzes which grammar rules are implemented vs. recognized.
This would have caught the "PRINT recognized in error_recovery but not implemented" issue!

Checks:
1. Which TokenTypes are recognized in error_recovery()
2. Which TokenTypes have actual handler methods in statement()
3. Which parser methods exist vs. are actually called
4. Missing implementations

Usage:
    python grammar_coverage.py
    python grammar_coverage.py --verbose
    python grammar_coverage.py --show-unused
"""

import sys
import os
import ast
import re
from pathlib import Path
from collections import defaultdict
from colorama import Fore, Style, init

init(autoreset=True)

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from src.parser.lexer import TokenType


class GrammarAnalyzer:
    """Analyze parser grammar coverage."""
    
    def __init__(self, parser_file):
        self.parser_file = parser_file
        self.recognized_in_error_recovery = set()
        self.handled_in_statement = set()
        self.parser_methods = set()
        self.called_methods = set()
        
    def analyze(self):
        """Perform full grammar analysis."""
        with open(self.parser_file, 'r') as f:
            content = f.read()
        
        # Find TokenTypes recognized in error_recovery
        self._find_recognized_tokens(content)
        
        # Find TokenTypes handled in statement()
        self._find_handled_tokens(content)
        
        # Find all parser methods
        self._find_parser_methods(content)
        
        # Find called methods
        self._find_called_methods(content)
    
    def _find_recognized_tokens(self, content):
        """Find TokenTypes recognized as statement boundaries in error_recovery."""
        # Look for the error_recovery method
        error_recovery_match = re.search(
            r'def error_recovery\(self\):.*?(?=\n    def |\Z)',
            content,
            re.DOTALL
        )
        
        if error_recovery_match:
            error_recovery_code = error_recovery_match.group(0)
            
            # Find all TokenType references in the boundary check
            token_pattern = r'TokenType\.(\w+)'
            matches = re.findall(token_pattern, error_recovery_code)
            
            for token_name in matches:
                if hasattr(TokenType, token_name):
                    self.recognized_in_error_recovery.add(token_name)
    
    def _find_handled_tokens(self, content):
        """Find TokenTypes that have handlers in statement() method."""
        # Look for the statement method
        statement_match = re.search(
            r'def statement\(self\):.*?(?=\n    def |\Z)',
            content,
            re.DOTALL
        )
        
        if statement_match:
            statement_code = statement_match.group(0)
            
            # Find all token type checks (if token.type == TokenType.XXX or elif ... TokenType.XXX)
            if_pattern = r'(?:if|elif)\s+\w+\.type\s*==\s*TokenType\.(\w+)'
            matches = re.findall(if_pattern, statement_code)
            
            for token_name in matches:
                if hasattr(TokenType, token_name):
                    self.handled_in_statement.add(token_name)
    
    def _find_parser_methods(self, content):
        """Find all parser method definitions."""
        method_pattern = r'\n    def (\w+)\(self'
        matches = re.findall(method_pattern, content)
        
        # Filter to statement/expression parsing methods
        for method in matches:
            if any(keyword in method for keyword in [
                'statement', 'declaration', 'definition', 
                'expression', 'loop', 'block', 'clause'
            ]):
                self.parser_methods.add(method)
    
    def _find_called_methods(self, content):
        """Find methods that are actually called."""
        call_pattern = r'self\.(\w+)\('
        matches = re.findall(call_pattern, content)
        
        for method in matches:
            if method in self.parser_methods:
                self.called_methods.add(method)
    
    def report(self, verbose=False, show_unused=False):
        """Generate coverage report."""
        print(f"{Fore.CYAN}{'='*80}")
        print(f"GRAMMAR COVERAGE ANALYSIS")
        print(f"{'='*80}\n")
        
        # Check 1: Recognized but not handled
        missing_handlers = self.recognized_in_error_recovery - self.handled_in_statement
        
        if missing_handlers:
            print(f"{Fore.RED} CRITICAL: Recognized tokens WITHOUT handlers!")
            print(f"{Fore.RED}{''*80}\n")
            print(f"{Fore.YELLOW}These tokens are recognized as statement boundaries in error_recovery()")
            print(f"{Fore.YELLOW}but have NO implementation in statement() method:\n")
            
            for token in sorted(missing_handlers):
                print(f"  {Fore.RED}• TokenType.{token}")
                print(f"    {Fore.YELLOW}Impact: Parser enters infinite loop when encountering this token")
                print(f"    {Fore.GREEN}Fix: Add handler in statement() method\n")
            
            print(f"\n{Fore.RED}This is EXACTLY what happened with PRINT!")
            print(f"{Fore.YELLOW}PRINT was in error_recovery list but had no handler → infinite loop\n")
        else:
            print(f"{Fore.GREEN} All recognized tokens have handlers\n")
        
        # Check 2: Handled but not in error recovery
        handled_but_not_recognized = self.handled_in_statement - self.recognized_in_error_recovery
        
        if handled_but_not_recognized:
            print(f"{Fore.YELLOW} WARNING: Tokens handled but NOT in error_recovery list")
            print(f"{Fore.YELLOW}{''*80}\n")
            print(f"These tokens have handlers but won't be recognized for error recovery:\n")
            
            for token in sorted(handled_but_not_recognized):
                print(f"  {Fore.YELLOW}• TokenType.{token}")
            
            print(f"\n{Fore.CYAN}Consider adding these to error_recovery() statement boundaries\n")
        
        # Check 3: Coverage statistics
        print(f"\n{Fore.CYAN}{'='*80}")
        print(f"COVERAGE STATISTICS")
        print(f"{'='*80}\n")
        
        total_statement_tokens = len(self.recognized_in_error_recovery)
        implemented_tokens = len(self.handled_in_statement)
        coverage = (implemented_tokens / total_statement_tokens * 100) if total_statement_tokens > 0 else 0
        
        print(f"  Statement Token Coverage: {Fore.GREEN if coverage == 100 else Fore.YELLOW}{coverage:.1f}%")
        print(f"  Recognized tokens: {total_statement_tokens}")
        print(f"  Implemented handlers: {implemented_tokens}")
        print(f"  Missing handlers: {Fore.RED if missing_handlers else Fore.GREEN}{len(missing_handlers)}\n")
        
        # Check 4: Parser methods
        if verbose:
            print(f"\n{Fore.CYAN}{'='*80}")
            print(f"PARSER METHODS")
            print(f"{'='*80}\n")
            
            print(f"{Fore.GREEN}Implemented statement/expression methods: {len(self.parser_methods)}\n")
            for method in sorted(self.parser_methods)[:20]:
                called = "" if method in self.called_methods else " "
                color = Fore.GREEN if method in self.called_methods else Fore.YELLOW
                print(f"  {color}[{called}] {method}()")
            
            if len(self.parser_methods) > 20:
                print(f"\n  {Fore.CYAN}... and {len(self.parser_methods) - 20} more")
        
        # Check 5: Unused methods
        if show_unused:
            unused = self.parser_methods - self.called_methods
            
            if unused:
                print(f"\n{Fore.YELLOW}{'='*80}")
                print(f"UNUSED PARSER METHODS")
                print(f"{'='*80}\n")
                print(f"These methods exist but are never called:\n")
                
                for method in sorted(unused):
                    print(f"  {Fore.YELLOW}• {method}()")
                
                print(f"\n{Fore.CYAN}Consider removing unused methods or integrating them\n")
        
        # Summary
        print(f"\n{Fore.CYAN}{'='*80}")
        print(f"SUMMARY")
        print(f"{'='*80}\n")
        
        if missing_handlers:
            print(f"{Fore.RED} FAILED: {len(missing_handlers)} token(s) missing handlers")
            print(f"\n{Fore.YELLOW}Action Required:")
            print(f"  1. Implement handlers in statement() for:")
            for token in sorted(missing_handlers):
                print(f"     - TokenType.{token}")
            print(f"  2. OR remove from error_recovery() if not a statement type")
            return False
        else:
            print(f"{Fore.GREEN} PASSED: All recognized tokens have handlers")
            print(f"{Fore.GREEN} Parser grammar coverage is complete\n")
            return True


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Analyze parser grammar coverage"
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help="Show all parser methods"
    )
    parser.add_argument(
        '--show-unused',
        action='store_true',
        help="Show unused parser methods"
    )
    
    args = parser.parse_args()
    
    # Find parser file
    project_root = Path(__file__).parent.parent.parent
    parser_file = project_root / 'src' / 'parser' / 'parser.py'
    
    if not parser_file.exists():
        print(f"{Fore.RED}Error: Parser file not found at {parser_file}")
        return 1
    
    # Analyze
    analyzer = GrammarAnalyzer(parser_file)
    analyzer.analyze()
    
    # Report
    success = analyzer.report(verbose=args.verbose, show_unused=args.show_unused)
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
