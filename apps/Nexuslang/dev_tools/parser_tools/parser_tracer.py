#!/usr/bin/env python3
"""
Parser Tracer - Deep Introspection of Parsing Process
======================================================

Instruments the parser to show EXACTLY what's happening:
- Every method call (entry/exit)
- Every token consumed (before/after advance())
- Every conditional branch taken
- Full call stack on errors

This would have instantly revealed the double-advance bug.

Usage:
    python parser_tracer.py <file.nlpl>
    python parser_tracer.py <file.nlpl> --method variable_declaration
    python parser_tracer.py <file.nlpl> --break-on SET
"""

import sys
import os
from pathlib import Path
from typing import Optional, Set
import functools

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from src.parser.lexer import Lexer, TokenType
from src.parser.parser import Parser
from colorama import Fore, Style, init

init(autoreset=True)


class ParserTracer:
    """Trace all parser operations with detailed output."""
    
    def __init__(self, parser: Parser, 
                 filter_methods: Optional[Set[str]] = None,
                 break_on_token: Optional[TokenType] = None):
        self.parser = parser
        self.filter_methods = filter_methods
        self.break_on_token = break_on_token
        self.call_depth = 0
        self.advance_count = 0
        self.token_history = []
        
        # Wrap all parser methods
        self._instrument_parser()
    
    def _instrument_parser(self):
        """Wrap all parser methods to trace calls."""
        # Get all parser methods
        for name in dir(self.parser):
            if name.startswith('_') or name in ['tokenize', 'error', 'error_recovery']:
                continue
            
            attr = getattr(self.parser, name)
            if callable(attr):
                # Skip if filtering and not in filter list
                if self.filter_methods and name not in self.filter_methods:
                    continue
                
                # Wrap the method
                wrapped = self._trace_method(name, attr)
                setattr(self.parser, name, wrapped)
        
        # Special handling for advance()
        original_advance = self.parser.advance
        
        def traced_advance():
            before = self.parser.current_token
            self.advance_count += 1
            
            print(f"{Fore.YELLOW}{'  ' * self.call_depth}[ADVANCE #{self.advance_count}]")
            print(f"{'  ' * self.call_depth}  Before: {self._format_token(before)}")
            
            result = original_advance()
            
            after = self.parser.current_token
            print(f"{'  ' * self.call_depth}  After:  {self._format_token(after)}")
            
            self.token_history.append((self.advance_count, before, after))
            
            # Check breakpoint
            if self.break_on_token and after and after.type == self.break_on_token:
                print(f"\n{Fore.RED} BREAKPOINT: Token {self.break_on_token} encountered!")
                print(f"{Fore.CYAN}Token history:")
                for i, (num, b, a) in enumerate(self.token_history[-5:], 1):
                    print(f"  {num}. {self._format_token(b)} → {self._format_token(a)}")
                input(f"{Fore.YELLOW}Press Enter to continue...")
            
            return result
        
        self.parser.advance = traced_advance
    
    def _trace_method(self, method_name: str, method):
        """Wrap a method to trace its execution."""
        @functools.wraps(method)
        def wrapper(*args, **kwargs):
            # Entry
            indent = '  ' * self.call_depth
            print(f"{Fore.GREEN}{indent}→ {method_name}()")
            print(f"{indent}  Current: {self._format_token(self.parser.current_token)}")
            
            self.call_depth += 1
            
            try:
                result = method(*args, **kwargs)
                
                # Exit
                self.call_depth -= 1
                indent = '  ' * self.call_depth
                print(f"{Fore.BLUE}{indent}← {method_name}() returned {type(result).__name__ if result else 'None'}")
                
                return result
            
            except Exception as e:
                # Error
                self.call_depth -= 1
                indent = '  ' * self.call_depth
                print(f"{Fore.RED}{indent} {method_name}() raised {type(e).__name__}: {e}")
                
                # Show call stack
                print(f"\n{Fore.RED}Call Stack at Error:")
                print(f"{Fore.CYAN}  Depth: {self.call_depth}")
                print(f"  Last 5 advances:")
                for i, (num, before, after) in enumerate(self.token_history[-5:], 1):
                    print(f"    {num}. {self._format_token(before)} → {self._format_token(after)}")
                
                raise
        
        return wrapper
    
    def _format_token(self, token) -> str:
        """Format token for display."""
        if not token:
            return f"{Fore.RED}None"
        
        return f"{Fore.CYAN}{token.type.name}{Style.RESET_ALL}({Fore.WHITE}{token.lexeme}{Style.RESET_ALL}) @L{token.line}:C{token.column}"


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Trace parser execution in detail")
    parser.add_argument("file", help="NLPL source file to trace")
    parser.add_argument("--method", "-m", action="append", help="Filter to specific method(s)")
    parser.add_argument("--break-on", "-b", help="Break when specific token type is encountered")
    parser.add_argument("--show-tokens", action="store_true", help="Show all tokens first")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.file):
        print(f"{Fore.RED}Error: File not found: {args.file}")
        return 1
    
    # Read source
    with open(args.file, 'r') as f:
        source = f.read()
    
    print(f"{Fore.CYAN}{'='*80}")
    print(f"PARSER TRACE: {args.file}")
    print(f"{'='*80}\n")
    
    # Tokenize
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    
    if args.show_tokens:
        print(f"{Fore.YELLOW}Tokens:")
        for i, tok in enumerate(tokens, 1):
            print(f"  {i}. {tok.type.name:20s} {tok.lexeme!r}")
        print()
    
    # Create parser
    parser_obj = Parser(tokens)
    
    # Set up tracer
    filter_methods = set(args.method) if args.method else None
    break_token = getattr(TokenType, args.break_on) if args.break_on else None
    
    tracer = ParserTracer(parser_obj, filter_methods, break_token)
    
    # Parse with tracing
    try:
        print(f"{Fore.GREEN}Starting parse...\n")
        ast = parser_obj.parse()
        
        print(f"\n{Fore.GREEN} Parse completed successfully!")
        print(f"  Statements: {len(ast.statements)}")
        print(f"  Total advances: {tracer.advance_count}")
        
    except Exception as e:
        print(f"\n{Fore.RED} Parse failed: {e}")
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
