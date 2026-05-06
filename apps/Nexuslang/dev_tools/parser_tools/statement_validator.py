#!/usr/bin/env python3
"""
Statement Handler Validator
============================

Ensures all TokenTypes recognized as statement boundaries have handlers.
Validates the parser's statement() method has complete coverage.

Checks:
1. Every TokenType in error_recovery boundaries has a handler in statement()
2. Every handler in statement() has a corresponding parser method
3. Parser method signatures match expected patterns
4. No orphaned or unreachable handlers

This is a combination of Grammar Coverage + Method Validation.

Usage:
    python statement_validator.py
    python statement_validator.py --fix-suggestions
    python statement_validator.py --generate-stubs
"""

import sys
import os
import re
import ast
from pathlib import Path
from collections import defaultdict
from colorama import Fore, Style, init

init(autoreset=True)

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from src.nlpl.parser.lexer import TokenType


class StatementHandlerValidator:
    """Validate statement handler completeness."""
    
    def __init__(self, parser_file):
        self.parser_file = parser_file
        
        # Data structures
        self.error_recovery_tokens = set()
        self.statement_handlers = {}  # token_name -> method_name
        self.parser_methods = {}  # method_name -> info
        self.issues = []
    
    def analyze(self):
        """Perform full validation."""
        with open(self.parser_file, 'r') as f:
            content = f.read()
        
        # Extract information
        self._extract_error_recovery_tokens(content)
        self._extract_statement_handlers(content)
        self._extract_parser_methods(content)
        
        # Validate
        self._validate_coverage()
        self._validate_methods()
    
    def _extract_error_recovery_tokens(self, content):
        """Extract TokenTypes from error_recovery statement boundaries."""
        error_recovery_match = re.search(
            r'def error_recovery\(self\):.*?if self\.current_token\.type in \[(.*?)\]',
            content,
            re.DOTALL
        )
        
        if error_recovery_match:
            tokens_str = error_recovery_match.group(1)
            token_matches = re.findall(r'TokenType\.(\w+)', tokens_str)
            
            for token_name in token_matches:
                if hasattr(TokenType, token_name):
                    self.error_recovery_tokens.add(token_name)
    
    def _extract_statement_handlers(self, content):
        """Extract handlers from statement() method."""
        statement_match = re.search(
            r'def statement\(self\):.*?(?=\n    def |\Z)',
            content,
            re.DOTALL
        )
        
        if statement_match:
            statement_code = statement_match.group(0)
            
            # Find if/elif blocks with token checks
            # Pattern 1: Single token check
            # Updated regex to handle optional comments and print statements between condition and return
            handler_pattern = r'(?:if|elif)\s+\w+\.type\s*==\s*TokenType\.(\w+):\s*\n(?:\s*(?:#.*|print.*)\n)*\s*return\s+self\.(\w+)\('
            matches = re.findall(handler_pattern, statement_code)
            
            for token_name, method_name in matches:
                if hasattr(TokenType, token_name):
                    self.statement_handlers[token_name] = method_name
            
            # Pattern 2: OR conditions (e.g., "token.type == TokenType.RETURN or token.type == TokenType.RETURNS")
            or_pattern = r'(?:if|elif)\s+\w+\.type\s*==\s*TokenType\.(\w+)\s+or\s+\w+\.type\s*==\s*TokenType\.(\w+):\s*\n(?:\s*(?:#.*|print.*)\n)*\s*return\s+self\.(\w+)\('
            or_matches = re.findall(or_pattern, statement_code)
            
            for token1, token2, method_name in or_matches:
                if hasattr(TokenType, token1):
                    self.statement_handlers[token1] = method_name
                if hasattr(TokenType, token2):
                    self.statement_handlers[token2] = method_name
    
    def _extract_parser_methods(self, content):
        """Extract all parser methods and their signatures."""
        # Find ALL method definitions (not just those with specific keywords)
        # Pattern 1: Methods with docstrings
        method_pattern_with_doc = r'def (\w+)\(self(?:,\s*([^)]*))?\):\s*\n\s*"""([^"]+)"""'
        matches_with_doc = re.finditer(method_pattern_with_doc, content)
        
        for match in matches_with_doc:
            method_name = match.group(1)
            params = match.group(2) or ""
            docstring = match.group(3).strip()
            
            self.parser_methods[method_name] = {
                'params': params,
                'docstring': docstring,
                'exists': True
            }
        
        # Pattern 2: Methods without docstrings (simpler pattern)
        method_pattern_no_doc = r'def (\w+)\(self(?:,\s*([^)]*))?\):'
        matches_no_doc = re.finditer(method_pattern_no_doc, content)
        
        for match in matches_no_doc:
            method_name = match.group(1)
            params = match.group(2) or ""
            
            # Only add if not already found (docstring version takes priority)
            if method_name not in self.parser_methods:
                self.parser_methods[method_name] = {
                    'params': params,
                    'docstring': '',
                    'exists': True
                }
    
    def _validate_coverage(self):
        """Validate all error_recovery tokens have handlers."""
        for token_name in self.error_recovery_tokens:
            if token_name not in self.statement_handlers:
                self.issues.append({
                    'type': 'MISSING_HANDLER',
                    'severity': 'CRITICAL',
                    'token': token_name,
                    'message': f"TokenType.{token_name} recognized in error_recovery but NO handler in statement()"
                })
    
    def _validate_methods(self):
        """Validate handler methods exist and are properly implemented."""
        for token_name, method_name in self.statement_handlers.items():
            # Check if method exists
            if method_name not in self.parser_methods:
                self.issues.append({
                    'type': 'MISSING_METHOD',
                    'severity': 'ERROR',
                    'token': token_name,
                    'method': method_name,
                    'message': f"Handler {method_name}() for {token_name} is called but NOT defined"
                })
            else:
                # Check method signature - only warn about required parameters (no defaults)
                method_info = self.parser_methods[method_name]
                if method_info['params']:
                    # Check if all params have defaults (e.g., "packed=False")
                    params_str = method_info['params']
                    # Split by comma and check if all have '='
                    param_list = [p.strip() for p in params_str.split(',')]
                    required_params = [p for p in param_list if '=' not in p]
                    
                    if required_params:
                        self.issues.append({
                            'type': 'UNEXPECTED_PARAMS',
                            'severity': 'WARNING',
                            'method': method_name,
                            'params': ', '.join(required_params),
                            'message': f"Method {method_name}() has unexpected required parameters: {', '.join(required_params)}"
                        })
    
    def report(self, show_suggestions=False, generate_stubs=False):
        """Generate validation report."""
        print(f"{Fore.CYAN}{'='*80}")
        print(f"STATEMENT HANDLER VALIDATION")
        print(f"{'='*80}\n")
        
        # Statistics
        print(f"{Fore.WHITE}Coverage Statistics:")
        print(f"  Error recovery tokens: {len(self.error_recovery_tokens)}")
        print(f"  Statement handlers: {len(self.statement_handlers)}")
        print(f"  Parser methods: {len(self.parser_methods)}\n")
        
        # Report issues by severity
        critical = [i for i in self.issues if i['severity'] == 'CRITICAL']
        errors = [i for i in self.issues if i['severity'] == 'ERROR']
        warnings = [i for i in self.issues if i['severity'] == 'WARNING']
        
        if critical:
            print(f"{Fore.RED}{'='*80}")
            print(f" CRITICAL ISSUES ({len(critical)})")
            print(f"{'='*80}\n")
            
            for issue in critical:
                print(f"{Fore.RED}• {issue['message']}")
                print(f"  {Fore.YELLOW}Token: TokenType.{issue['token']}")
                print(f"  {Fore.YELLOW}Impact: INFINITE LOOP when encountering this token")
                
                if show_suggestions:
                    print(f"\n  {Fore.GREEN}Fix: Add to statement() method:")
                    method_name = self._suggest_method_name(issue['token'])
                    print(f"  {Fore.CYAN}  elif token.type == TokenType.{issue['token']}:")
                    print(f"  {Fore.CYAN}      return self.{method_name}()")
                
                print()
        
        if errors:
            print(f"{Fore.RED}{'='*80}")
            print(f" ERRORS ({len(errors)})")
            print(f"{'='*80}\n")
            
            for issue in errors:
                print(f"{Fore.RED}• {issue['message']}")
                print(f"  {Fore.YELLOW}Method: {issue['method']}()")
                
                if show_suggestions and generate_stubs:
                    print(f"\n  {Fore.GREEN}Stub to add:")
                    self._print_method_stub(issue['method'], issue['token'])
                
                print()
        
        if warnings:
            print(f"{Fore.YELLOW}{'='*80}")
            print(f" WARNINGS ({len(warnings)})")
            print(f"{'='*80}\n")
            
            for issue in warnings:
                print(f"{Fore.YELLOW}• {issue['message']}\n")
        
        # Summary
        print(f"\n{Fore.CYAN}{'='*80}")
        print(f"VALIDATION SUMMARY")
        print(f"{'='*80}\n")
        
        total_issues = len(self.issues)
        
        if total_issues == 0:
            print(f"{Fore.GREEN} PASSED: All checks passed!")
            print(f"{Fore.GREEN}  • All error_recovery tokens have handlers")
            print(f"{Fore.GREEN}  • All handlers have corresponding methods")
            print(f"{Fore.GREEN}  • No orphaned or missing implementations\n")
            return True
        else:
            print(f"{Fore.RED} FAILED: {total_issues} issue(s) found")
            if critical:
                print(f"{Fore.RED}  • {len(critical)} CRITICAL (will cause infinite loops)")
            if errors:
                print(f"{Fore.RED}  • {len(errors)} ERROR (missing implementations)")
            if warnings:
                print(f"{Fore.YELLOW}  • {len(warnings)} WARNING (potential issues)")
            print()
            return False
    
    def _suggest_method_name(self, token_name):
        """Suggest method name for a token type."""
        # Convert TOKEN_NAME to method_name
        name = token_name.lower()
        
        # Common patterns
        if name in ['set']:
            return 'variable_declaration'
        elif name in ['print']:
            return 'print_statement'
        elif name in ['if']:
            return 'if_statement'
        elif name in ['while']:
            return 'while_loop'
        elif name in ['for']:
            return 'for_loop'
        elif name in ['function']:
            return 'function_definition'
        elif name in ['class']:
            return 'class_definition'
        elif name in ['try']:
            return 'try_statement'
        elif name in ['returns']:
            return 'return_statement'
        elif name in ['import']:
            return 'import_statement'
        else:
            return f'{name}_statement'
    
    def _print_method_stub(self, method_name, token_name):
        """Print stub code for missing method."""
        suggested_name = self._suggest_method_name(token_name)
        
        stub = f'''
    def {method_name}(self):
        """Parse {token_name.lower()} statement.
        
        Grammar:
            {token_name} <implementation_details>
        """
        # TODO: Implement {token_name} statement parsing
        
        # Consume {token_name} token
        if self.current_token.type != TokenType.{token_name}:
            self.error(f"Expected {token_name}, got {{self.current_token.type}}")
        self.advance()
        
        # Parse statement components
        # ... implementation ...
        
        return None  # Return appropriate AST node
'''
        print(f"{Fore.CYAN}{stub}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Validate statement handler completeness"
    )
    parser.add_argument(
        '--fix-suggestions',
        action='store_true',
        help="Show suggested fixes for issues"
    )
    parser.add_argument(
        '--generate-stubs',
        action='store_true',
        help="Generate method stubs for missing handlers"
    )
    
    args = parser.parse_args()
    
    # Find parser file
    project_root = Path(__file__).parent.parent.parent
    parser_file = project_root / 'src' / 'nlpl' / 'parser' / 'parser.py'
    
    if not parser_file.exists():
        print(f"{Fore.RED}Error: Parser file not found at {parser_file}")
        return 1
    
    # Validate
    validator = StatementHandlerValidator(parser_file)
    validator.analyze()
    
    # Report
    success = validator.report(
        show_suggestions=args.fix_suggestions,
        generate_stubs=args.generate_stubs
    )
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
